#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define WIRE_CAPACITY (1024 * 1024)
typedef struct
{
    unsigned char *bytes;
    size_t at, length;
    int reading, ok;
} Wire;
static void Bytes(Wire *w, void *p, size_t n)
{
    if (n > w->length - w->at)
    {
        w->ok = 0;
        return;
    }
    if (w->reading)
        memcpy(p, w->bytes + w->at, n);
    else
        memcpy(w->bytes + w->at, p, n);
    w->at += n;
}
static void U32(Wire *w, uint32_t *p)
{
    unsigned char b[4];
    if (!w->reading)
        for (int i = 0; i < 4; i++)
            b[i] = (unsigned char)(*p >> (i * 8));
    Bytes(w, b, 4);
    if (w->reading && w->ok)
    {
        *p = 0;
        for (int i = 0; i < 4; i++)
            *p |= (uint32_t)b[i] << (i * 8);
    }
}
static void Int(Wire *w, int *p)
{
    uint32_t v = (uint32_t)*p;
    U32(w, &v);
    if (w->reading)
        *p = (int32_t)v;
}
static void U64(Wire *w, uint64_t *p)
{
    uint32_t lo = (uint32_t)*p, hi = (uint32_t)(*p >> 32);
    U32(w, &lo);
    U32(w, &hi);
    if (w->reading)
        *p = ((uint64_t)hi << 32) | lo;
}
static void State(Wire *w, GameState *g)
{
#define I(v) Int(w, &(v))
    U64(w, &g->tick);
    U32(w, &g->random);
    for (int i = 0; i < TILE_COUNT; i++)
    {
        Tile *t = &g->tiles[i];
        I(t->terrain);
        I(t->node);
        I(t->amount);
        I(t->occupied);
    }
    // Fixed-size arrays deliberately include idle slots: no pointers, padding or ABI-dependent bytes.
    for (int i = 0; i < MAX_CITIZENS; i++)
    {
        Citizen *c = &g->citizens[i];
        I(c->alive);
        I(c->role);
        I(c->x);
        I(c->y);
        I(c->hp);
        I(c->hunger);
        I(c->cold);
        I(c->job);
        I(c->target);
        I(c->work);
        for (int k = 0; k < RESOURCE_COUNT; k++)
            I(c->carry[k]);
        I(c->harvestCargo);
        for (int j = 0; j < MAX_PATH; j++)
            I(c->path[j]);
        I(c->pathLength);
        I(c->pathIndex);
        I(c->pathRevision);
        I(c->retry);
        I(c->attackCooldown);
    }
    for (int i = 0; i < MAX_BUILDINGS; i++)
    {
        Building *b = &g->buildings[i];
        I(b->type);
        I(b->x);
        I(b->y);
        I(b->progress);
        I(b->complete);
        I(b->upgraded);
        I(b->crop);
        I(b->cropProgress);
        I(b->priority);
    }
    for (int i = 0; i < MAX_RAIDERS; i++)
    {
        Raider *r = &g->raiders[i];
        I(r->active);
        I(r->x);
        I(r->y);
        I(r->hp);
        I(r->retreat);
        I(r->cooldown);
        I(r->stolen);
    }
    I(g->citizenCount);
    I(g->buildingCount);
    for (int k = 0; k < RESOURCE_COUNT; k++)
    {
        I(g->stock[k]);
        I(g->initial[k]);
        I(g->produced[k]);
        I(g->spent[k]);
        I(g->stolen[k]);
    }
    for (int k = 0; k < ROLE_COUNT; k++)
    {
        I(g->areaX[k]);
        I(g->areaY[k]);
        I(g->priority[k]);
    }
    I(g->rallyX);
    I(g->rallyY);
    I(g->mapRevision);
    for (int k = 0; k < DISCOVERY_COUNT; k++)
        I(g->discovery[k]);
    I(g->research);
    I(g->researchProgress);
    I(g->winterSurvived);
    I(g->harvestDelivered);
    I(g->raidResolved);
    I(g->won);
    I(g->lost);
    I(g->nextArrival);
    I(g->arrivals);
    I(g->deaths);
    I(g->nextRaid);
    I(g->raidWarning);
    I(g->raidActive);
    I(g->raidLosses);
    I(g->raidLoot);
    I(g->truceUntil);
    I(g->raidCount);
    I(g->warmth);
    I(g->foodWarning);
    I(g->coldWarning);
    I(g->shortageTicks);
    I(g->shortageRecoveries);
    I(g->hadShortage);
    Bytes(w, g->events, sizeof(g->events));
    I(g->eventCount);
#undef I
}
static uint64_t Digest(const unsigned char *p, size_t n)
{
    uint64_t h = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < n; i++)
    {
        h ^= p[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}
static bool Fail(char *r, int n, const char *s)
{
    if (r && n > 0)
        snprintf(r, (size_t)n, "%s", s);
    return false;
}
uint64_t GameHash(const GameState *g)
{
    unsigned char *data = malloc(WIRE_CAPACITY);
    if (!data)
        return 0;
    Wire w = {data, 0, WIRE_CAPACITY, 0, 1};
    State(&w, (GameState *)g);
    uint64_t h = Digest(data, w.at);
    free(data);
    return h;
}
bool GameSave(const GameState *g, const char *path, char *reason, int size)
{
    if (!GameValidate(g, reason, size))
        return false;
    unsigned char *data = calloc(1, WIRE_CAPACITY);
    if (!data)
        return Fail(reason, size, "Not enough memory to save.");
    Wire w = {data, 24, WIRE_CAPACITY, 0, 1};
    State(&w, (GameState *)g);
    size_t length = w.at;
    uint32_t version = SAVE_VERSION, bytes = (uint32_t)(length - 24);
    uint64_t hash = Digest(data + 24, bytes);
    w.at = 0;
    char magic[8] = "GFTRIBE";
    Bytes(&w, magic, 8);
    U32(&w, &version);
    U32(&w, &bytes);
    U64(&w, &hash);
    char temp[4096];
    if (snprintf(temp, sizeof(temp), "%s.tmp", path) >= (int)sizeof(temp))
    {
        free(data);
        return Fail(reason, size, "Save path is too long.");
    }
    FILE *f = fopen(temp, "wb");
    if (!f)
    {
        free(data);
        return Fail(reason, size, "Cannot open the save folder. Your previous save is intact.");
    }
    bool ok = fwrite(data, 1, length, f) == length;
    if (ok)
        ok = fflush(f) == 0;
    if (ok)
        ok = fsync(fileno(f)) == 0;
    if (fclose(f))
        ok = false;
    free(data);
    if (ok)
        ok = rename(temp, path) == 0;
    if (!ok)
    {
        remove(temp);
        return Fail(reason, size, "Save could not finish. Your previous save is intact.");
    }
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Village saved.");
    return true;
}
bool GameLoad(GameState *g, const char *path, char *reason, int size)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return Fail(reason, size, "No readable save yet. Start a new village.");
    unsigned char *data = malloc(WIRE_CAPACITY);
    GameState *candidate = calloc(1, sizeof(*candidate));
    if (!data || !candidate)
    {
        free(data);
        free(candidate);
        fclose(f);
        return Fail(reason, size, "Not enough memory to load.");
    }
    size_t n = fread(data, 1, WIRE_CAPACITY, f);
    bool io = ferror(f) == 0;
    if (io && !feof(f))
        io = fgetc(f) == EOF && ferror(f) == 0;
    fclose(f);
    Wire w = {data, 0, n, 1, 1};
    char magic[8] = {0};
    uint32_t version = 0, length = 0;
    uint64_t hash = 0;
    if (n < 24 || !io)
    {
        free(data);
        free(candidate);
        return Fail(reason, size, "Save is incomplete. Current village and save were left untouched.");
    }
    Bytes(&w, magic, 8);
    U32(&w, &version);
    U32(&w, &length);
    U64(&w, &hash);
    bool ok = memcmp(magic, "GFTRIBE", 8) == 0 && version == SAVE_VERSION && length == n - 24 &&
              hash == Digest(data + 24, n - 24);
    if (ok)
    {
        State(&w, candidate);
        ok = w.ok && w.at == n;
    }
    if (ok)
        ok = GameValidate(candidate, reason, size);
    if (ok)
    {
        for (int i = 0; i < 8; i++)
            candidate->events[i][127] = 0;
        *g = *candidate;
        if (reason && size > 0)
            snprintf(reason, (size_t)size, "Village restored.");
    }
    else
        Fail(reason, size, "Invalid or incompatible save. Nothing has been overwritten.");
    free(data);
    free(candidate);
    return ok;
}
