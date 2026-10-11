#include "common.h"
#include <limits.h>
#include <string.h>

bool GameValidate(const GameState *g, char *r, int n)
{
    if (g->tick > INT_MAX - 100000 || g->citizenCount < 0 || g->citizenCount > MAX_CITIZENS ||
        g->buildingCount < 2 || g->buildingCount > MAX_BUILDINGS || g->research < -1 ||
        g->research >= DISCOVERY_COUNT || g->eventCount < 0 || g->eventCount > 8)
        return Reject(r, n, "Invalid simulation header.");
    if (g->warmth < 0 || g->warmth > 100 || g->researchProgress < 0 || g->nextArrival < 0 ||
        g->nextRaid < 0 || g->truceUntil < 0 || g->raidLoot < 0 || g->raidLoot > 18 || g->raidLosses < 0 ||
        g->raidLosses > MAX_RAIDERS || g->researchProgress > 2400 || g->arrivals < 0 ||
        g->arrivals > MAX_CITIZENS || g->deaths < 0 || g->deaths > MAX_CITIZENS)
        return Reject(r, n, "Invalid progression state.");
    const int counters[] = {g->nextArrival, g->nextRaid,      g->truceUntil,        g->mapRevision,
                            g->raidCount,   g->shortageTicks, g->shortageRecoveries};
    for (unsigned i = 0; i < sizeof(counters) / sizeof(counters[0]); i++)
        if (counters[i] < 0 || counters[i] > INT_MAX - 100000)
            return Reject(r, n, "Invalid simulation counter.");
    const int flags[] = {g->winterSurvived, g->harvestDelivered, g->raidResolved, g->won,
                         g->lost,           g->raidWarning,      g->raidActive,   g->foodWarning,
                         g->coldWarning,    g->hadShortage};
    for (unsigned i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
        if (flags[i] < 0 || flags[i] > 1)
            return Reject(r, n, "Invalid simulation flag.");
    for (int i = 0; i < 8; i++)
        if (!memchr(g->events[i], 0, sizeof(g->events[i])))
            return Reject(r, n, "Invalid chronicle entry.");
    for (int k = 0; k < DISCOVERY_COUNT; k++)
        if (g->discovery[k] < 0 || g->discovery[k] > 1)
            return Reject(r, n, "Invalid discovery flag.");
    for (int k = 0; k < RESOURCE_COUNT; k++)
    {
        int64_t carried = 0;
        for (int i = 0; i < g->citizenCount; i++)
            carried += g->citizens[i].carry[k];
        if (g->stock[k] < 0 || g->stock[k] > 10000000 || g->initial[k] < 0 || g->initial[k] > 1000000000 ||
            g->produced[k] > 1000000000 || g->spent[k] > 1000000000 || g->stolen[k] > 1000000000 ||
            g->produced[k] < 0 || g->spent[k] < 0 || g->stolen[k] < 0 ||
            (int64_t)g->initial[k] + g->produced[k] - g->spent[k] - g->stolen[k] !=
                (int64_t)g->stock[k] + carried)
            return Reject(r, n, "Resource conservation failed.");
    }
    for (int i = 0; i < TILE_COUNT; i++)
    {
        const Tile *t = &g->tiles[i];
        if (t->terrain < 0 || t->terrain > ROCK_GROUND || t->node < 0 || t->node > NODE_GAME ||
            t->amount < 0 || t->amount > 1000000 || t->occupied < -1 || t->occupied >= g->buildingCount)
            return Reject(r, n, "Invalid map tile.");
        if (t->occupied >= 0)
        {
            const Building *b = &g->buildings[t->occupied];
            if (b->type < 0 || b->type >= BUILDING_COUNT || b->x < 0 || b->y < 0 || b->x >= MAP_SIZE ||
                b->y >= MAP_SIZE || i % MAP_SIZE < b->x || i % MAP_SIZE >= b->x + BUILDINGS[b->type].w ||
                i / MAP_SIZE < b->y || i / MAP_SIZE >= b->y + BUILDINGS[b->type].h)
                return Reject(r, n, "Unexpected building occupancy.");
        }
    }
    for (int i = 0; i < g->buildingCount; i++)
    {
        const Building *b = &g->buildings[i];
        if (b->type < 0 || b->type >= BUILDING_COUNT || b->x < 0 || b->y < 0 || b->x >= MAP_SIZE ||
            b->y >= MAP_SIZE || b->x + BUILDINGS[b->type].w > MAP_SIZE ||
            b->y + BUILDINGS[b->type].h > MAP_SIZE || b->progress < 0 ||
            b->progress > BUILDINGS[b->type].labor || b->crop < 0 || b->crop > 2 || b->cropProgress < 0 ||
            b->cropProgress > 2400 || b->complete < 0 || b->complete > 1 || b->upgraded < 0 ||
            b->upgraded > 1 || b->priority < 0 || b->priority > 2)
            return Reject(r, n, "Invalid building.");
        for (int y = b->y; y < b->y + BUILDINGS[b->type].h; y++)
            for (int x = b->x; x < b->x + BUILDINGS[b->type].w; x++)
                if (g->tiles[GameTile(x, y)].occupied != i)
                    return Reject(r, n, "Building occupancy disagrees with the map.");
    }
    for (int i = 0; i < g->citizenCount; i++)
    {
        const Citizen *c = &g->citizens[i];
        if (c->alive < 0 || c->alive > 1 || c->hunger < 0 || c->cold < 0 || c->work < 0 || c->role < 0 ||
            c->role > IDLE || c->x < 0 || c->y < 0 || c->x >= MAP_SIZE * 256 || c->y >= MAP_SIZE * 256 ||
            c->job < 0 || c->job > JOB_FIGHT || c->pathLength < 0 || c->pathLength > MAX_PATH ||
            c->pathIndex < 0 || c->pathIndex > c->pathLength || c->hp > 1000 || c->hp < -1000 ||
            c->hunger > 1000000 || c->cold > 1000000 || c->work > 1000000 || c->retry < 0 || c->retry > 30 ||
            c->attackCooldown < 0 || c->attackCooldown > 12 || c->pathRevision < 0 ||
            c->pathRevision > INT_MAX - 100000 || c->harvestCargo < 0 || c->harvestCargo > 1 ||
            c->target < -1 || c->target >= TILE_COUNT)
            return Reject(r, n, "Invalid citizen or job.");
        if (c->job != JOB_GATHER && c->job != JOB_DELIVER && c->job != JOB_GUARD && c->job != JOB_FIGHT &&
            c->job != JOB_IDLE && (c->target < 0 || c->target >= g->buildingCount))
            return Reject(r, n, "Job references a missing building.");
        for (int p = 0; p < c->pathLength; p++)
            if (c->path[p] < 0 || c->path[p] >= TILE_COUNT)
                return Reject(r, n, "Invalid route.");
        for (int k = 0; k < RESOURCE_COUNT; k++)
            if (c->carry[k] < 0 || c->carry[k] > 1000)
                return Reject(r, n, "Invalid carried goods.");
    }
    for (int i = 0; i < ROLE_COUNT; i++)
        if (g->areaX[i] < -1 || g->areaX[i] >= MAP_SIZE || g->areaY[i] < -1 || g->areaY[i] >= MAP_SIZE ||
            g->priority[i] < 0 || g->priority[i] > 2)
            return Reject(r, n, "Invalid work area.");
    for (int i = 0; i < MAX_RAIDERS; i++)
    {
        const Raider *a = &g->raiders[i];
        if (a->x < 0 || a->y < 0 || a->x >= MAP_SIZE * 256 || a->y >= MAP_SIZE * 256 || a->hp > 650 ||
            a->hp < -1000 || a->stolen < 0 || a->stolen > 18 || a->active < 0 || a->active > 1 ||
            a->retreat < 0 || a->retreat > 1 || a->cooldown < 0 || a->cooldown > 16)
            return Reject(r, n, "Invalid raider.");
    }
    if (GameTile(g->rallyX, g->rallyY) < 0)
        return Reject(r, n, "Invalid rally point.");
    return true;
}
