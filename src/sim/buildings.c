#include "buildings.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"

int GameHousing(const GameState *g)
{
    int n = 0;
    for (int i = 0; i < g->buildingCount; i++)
        if ((g->buildings[i].complete || g->buildings[i].upgraded) && g->buildings[i].type == SHELTER)
            n += 4;
    return n;
}

int GameBuildingCount(const GameState *g, int type, bool completeOnly)
{
    int n = 0;
    for (int i = 0; i < g->buildingCount; i++)
        n += g->buildings[i].type == type && (!completeOnly || g->buildings[i].complete);
    return n;
}

int BuildingsAdd(GameState *g, int type, int x, int y, bool done)
{
    int id = g->buildingCount++;
    Building *b = &g->buildings[id];
    *b = (Building){.type = type,
                    .x = x,
                    .y = y,
                    .progress = done ? BUILDINGS[type].labor : 0,
                    .complete = done,
                    .priority = 1};
    for (int yy = y; yy < y + BUILDINGS[type].h; yy++)
        for (int xx = x; xx < x + BUILDINGS[type].w; xx++)
            g->tiles[GameTile(xx, yy)].occupied = id;
    g->mapRevision++;
    return id;
}

bool BuildingsNearby(const GameState *g, int type, int x, int y, int range)
{
    for (int i = 0; i < g->buildingCount; i++)
    {
        const Building *b = &g->buildings[i];
        if (b->type == type && b->complete && Dist(b->x, b->y, x, y) <= range)
            return true;
    }
    return false;
}

bool GameCanPlace(const GameState *g, int type, int x, int y, char *reason, int size)
{
    if (type < 0 || type >= BUILDING_COUNT)
        return Reject(reason, size, "Unknown building.");
    if (g->buildingCount >= MAX_BUILDINGS)
        return Reject(reason, size, "The valley's building limit is reached.");
    if (type == FARM && (!g->discovery[FARMING] || g->tick < WINTER_END))
        return Reject(reason, size, "Wait for the thaw and discover Farming at a staffed workshop.");
    if (type == GRANARY && !g->discovery[FOOD_STORAGE])
        return Reject(reason, size, "Discover Food storage at a staffed workshop first.");
    const BuildingDef *d = &BUILDINGS[type];
    if (x < 0 || y < 0 || x > MAP_SIZE - d->w || y > MAP_SIZE - d->h)
        return Reject(reason, size, "Keep the whole footprint inside the valley.");
    for (int yy = y; yy < y + d->h; yy++)
        for (int xx = x; xx < x + d->w; xx++)
        {
            int t = GameTile(xx, yy);
            if (t < 0)
                return Reject(reason, size, "Keep the whole footprint inside the valley.");
            if (g->tiles[t].terrain == WATER || g->tiles[t].terrain == SHALLOWS)
                return Reject(reason, size, "Water and river crossings must stay clear.");
            if (xx >= 37 && yy < 13)
                return Reject(reason, size, "This land belongs to the rival camp.");
            if (g->tiles[t].occupied >= 0)
                return Reject(reason, size, "Another building occupies this footprint.");
            if (g->tiles[t].node != NODE_NONE)
                return Reject(reason, size, "Gather the natural resources on this tile first.");
            if (type == FARM && g->tiles[t].terrain != FERTILE)
                return Reject(reason, size, "Farm plots need the fertile soil beside the river.");
            for (int i = 0; i < g->citizenCount; i++)
                if (g->citizens[i].alive && g->citizens[i].x / 256 == xx && g->citizens[i].y / 256 == yy)
                    return Reject(reason, size, "A citizen is standing here. Wait until they move.");
            for (int i = 0; i < MAX_RAIDERS; i++)
                if (g->raiders[i].active && g->raiders[i].x / 256 == xx && g->raiders[i].y / 256 == yy)
                    return Reject(reason, size, "A raider occupies this tile.");
        }
    if (!GameCanAfford(g, d->cost))
        return Reject(reason, size, "Not enough stored materials. Carried goods must be delivered first.");
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Click to place %s", d->name);
    return true;
}

bool BuildingsPlace(GameState *g, int type, int x, int y, char *reason, int size)
{
    if (!GameCanPlace(g, type, x, y, reason, size))
        return false;
    EconomyPay(g, BUILDINGS[type].cost);
    BuildingsAdd(g, type, x, y, false);
    return true;
}

bool BuildingsUpgrade(GameState *g, int id, char *reason, int size)
{
    if (id < 0 || id >= g->buildingCount)
        return Reject(reason, size, "Select a completed shelter.");
    Building *b = &g->buildings[id];
    int cost[] = {0, 10, 0, 3, 0};
    if (b->type != SHELTER || !b->complete || b->upgraded || !g->discovery[INSULATION])
        return Reject(reason, size, "Discover Insulation and select a completed shelter.");
    if (!GameCanAfford(g, cost))
        return Reject(reason, size, "A hut upgrade needs 10 wood and 3 hides.");
    EconomyPay(g, cost);
    b->upgraded = 1;
    b->complete = 0;
    b->progress = 0;
    g->mapRevision++;
    return true;
}

void BuildingsWork(GameState *g, Citizen *c)
{
    Building *b = &g->buildings[c->target];
    if (b->complete)
    {
        JobsReset(c);
        return;
    }
    b->progress++;
    if (b->progress >= BUILDINGS[b->type].labor)
    {
        b->complete = 1;
        if (b->type == WATCH_POST)
        {
            g->rallyX = b->x;
            g->rallyY = b->y;
        }
        char msg[128];
        snprintf(msg, sizeof(msg), "%s completed.",
                 b->upgraded ? "Insulated hut" : BUILDINGS[b->type].name);
        GameEvent(g, msg);
        g->mapRevision++;
    }
}
