#define _POSIX_C_SOURCE 200809L
#include "game.h"
#include "sim/wildlife.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define WIRE_CAPACITY (4 * 1024 * 1024)
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
static void Ref(Wire *w, EntityRef *p)
{
    Int(w, &p->slot);
    Int(w, &p->generation);
}
static void WireTile(Wire *w, Tile *p)
{
    Int(w, &p->terrain);
    Int(w, &p->node);
    Int(w, &p->amount);
    Int(w, &p->occupied);
    Int(w, &p->natural);
    Int(w, &p->regrowth);
}
static void WireUnitOrder(Wire *w, UnitOrder *p)
{
    Int(w, &p->type);
    Int(w, &p->targetKind);
    Int(w, &p->x);
    Int(w, &p->y);
    Ref(w, &p->target);
}
static void WireProduction(Wire *w, Production *p)
{
    Int(w, &p->type);
    Int(w, &p->unitType);
    Int(w, &p->progress);
    Int(w, &p->duration);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->cost[i]);
    Ref(w, &p->citizen);
}
static void WireBuilding(Wire *w, Building *p)
{
    Int(w, &p->alive);
    Int(w, &p->owner);
    Int(w, &p->generation);
    Int(w, &p->type);
    Int(w, &p->x);
    Int(w, &p->y);
    Int(w, &p->hp);
    Int(w, &p->progress);
    Int(w, &p->complete);
    Int(w, &p->upgraded);
    Int(w, &p->crop);
    Int(w, &p->cropProgress);
    Int(w, &p->priority);
    Int(w, &p->started);
    Int(w, &p->queueCount);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->reserve[i]);
    for (int i = 0; i < MAX_QUEUE; i++)
        WireProduction(w, &p->queue[i]);
}
static void WireCitizen(Wire *w, Citizen *p)
{
    Int(w, &p->alive);
    Int(w, &p->owner);
    Int(w, &p->generation);
    Int(w, &p->role);
    Int(w, &p->unitType);
    Int(w, &p->equipped);
    Int(w, &p->x);
    Int(w, &p->y);
    Int(w, &p->hp);
    Int(w, &p->hunger);
    Int(w, &p->cold);
    Int(w, &p->job);
    Int(w, &p->target);
    Int(w, &p->work);
    Int(w, &p->harvestCargo);
    Int(w, &p->pathLength);
    Int(w, &p->pathIndex);
    Int(w, &p->pathRevision);
    Int(w, &p->retry);
    Int(w, &p->attackCooldown);
    Int(w, &p->orderCount);
    Int(w, &p->training);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->carry[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->recipe[i]);
    for (int i = 0; i < MAX_PATH; i++)
        Int(w, &p->path[i]);
    for (int i = 0; i < MAX_ORDERS; i++)
        WireUnitOrder(w, &p->orders[i]);
}
static void WireFaction(Wire *w, Faction *p)
{
    Int(w, &p->rallyX);
    Int(w, &p->rallyY);
    Int(w, &p->research);
    Int(w, &p->researchProgress);
    Int(w, &p->researchBuilding);
    Int(w, &p->age);
    Int(w, &p->advancing);
    Int(w, &p->advanceProgress);
    Int(w, &p->center);
    Int(w, &p->harvestDelivered);
    Int(w, &p->bronzeDelivered);
    Int(w, &p->village);
    Int(w, &p->villageTick);
    Int(w, &p->arrivals);
    Int(w, &p->deaths);
    Int(w, &p->warmth);
    Int(w, &p->foodWarning);
    Int(w, &p->coldWarning);
    Int(w, &p->shortageTicks);
    Int(w, &p->shortageRecoveries);
    Int(w, &p->hadShortage);
    Int(w, &p->aiEnabled);
    Int(w, &p->nextDecision);
    Int(w, &p->nextRaid);
    Int(w, &p->raidWarning);
    Int(w, &p->raidActive);
    Int(w, &p->raidCount);
    Int(w, &p->raidResolved);
    Int(w, &p->raidStarted);
    Int(w, &p->raidSize);
    Int(w, &p->defeated);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->stock[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->initial[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->produced[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->spent[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->imported[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->exported[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->reserved[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->lostGoods[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->researchReserve[i]);
    for (int i = 0; i < RESOURCE_COUNT; i++)
        Int(w, &p->advanceReserve[i]);
    for (int i = 0; i < ROLE_COUNT; i++)
        Int(w, &p->areaX[i]);
    for (int i = 0; i < ROLE_COUNT; i++)
        Int(w, &p->areaY[i]);
    for (int i = 0; i < ROLE_COUNT; i++)
        Int(w, &p->priority[i]);
    for (int i = 0; i < DISCOVERY_COUNT; i++)
        Int(w, &p->discovery[i]);
    WireUnitOrder(w, &p->rallyOrder);
}
static void State(Wire *w, GameState *g, uint32_t version)
{
    U64(w, &g->tick);
    U32(w, &g->random);
    for (int i = 0; i < TILE_COUNT; i++)
        WireTile(w, &g->tiles[i]);
    for (int i = 0; i < MAX_CITIZENS; i++)
        WireCitizen(w, &g->citizens[i]);
    for (int i = 0; i < MAX_BUILDINGS; i++)
        WireBuilding(w, &g->buildings[i]);
    for (int i = 0; i < FACTION_COUNT; i++)
        WireFaction(w, &g->factions[i]);
    Int(w, &g->citizenCount);
    Int(w, &g->buildingCount);
    Int(w, &g->mapRevision);
    Int(w, &g->winterSurvived);
    Int(w, &g->won);
    Int(w, &g->lost);
    Int(w, &g->truceUntil);
    Int(w, &g->treaty);
    Int(w, &g->lastHostileTick);
    Int(w, &g->eventCount);
    Bytes(w, g->events, sizeof(g->events));
    // Append-only v3 extension preserves the exact v2 prefix for safe loading.
    if (version >= 3)
    {
        for (int i = 0; i < MAX_CITIZENS; i++)
            Int(w, &g->citizens[i].facing);
        Int(w, &g->wildlifeCount);
        for (int i = 0; i < MAX_WILDLIFE; i++)
        {
            Wildlife *a = &g->wildlife[i];
            Int(w, &a->home);
            Int(w, &a->x);
            Int(w, &a->y);
            Int(w, &a->goalX);
            Int(w, &a->goalY);
            Int(w, &a->facing);
            Int(w, &a->state);
            Int(w, &a->wait);
            Int(w, &a->stepX);
            Int(w, &a->stepY);
            U32(w, &a->random);
        }
    }
    // Append-only v4: older country saves default to the original orientation.
    if (version >= 4)
        for (int i = 0; i < MAX_BUILDINGS; i++)
            Int(w, &g->buildings[i].rotation);
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
    State(&w, (GameState *)g, SAVE_VERSION);
    uint64_t h = w.ok ? Digest(data, w.at) : 0;
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
    State(&w, (GameState *)g, SAVE_VERSION);
    if (!w.ok)
    {
        free(data);
        return Fail(reason, size, "Save exceeds the format limit.");
    }
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
    bool ok = memcmp(magic, "GFTRIBE", 8) == 0 && (version >= 2 && version <= SAVE_VERSION) &&
              length == n - 24 && hash == Digest(data + 24, n - 24);
    if (ok)
    {
        State(&w, candidate, version);
        ok = w.ok && w.at == n;
    }
    if (ok)
        ok = GameValidate(candidate, reason, size);
    if (ok && version == 2)
        WildlifeInit(candidate);
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
