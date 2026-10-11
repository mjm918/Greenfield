#include "buildings.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"
#include "production.h"
#include "progression.h"

int GameBuildingMaxHealth(const GameState *g, const Building *b)
{
    return g->factions[b->owner].discovery[STONEWORK] ? 4500 : 3000;
}

int GameHousingFor(const GameState *g, int owner)
{
    int n = 0;
    for (int i = 0; i < g->buildingCount; i++)
        if (g->buildings[i].alive && g->buildings[i].owner == owner &&
            (g->buildings[i].complete || g->buildings[i].upgraded) && g->buildings[i].type == SHELTER)
            n += 4;
    return n;
}

int GameBuildingCountFor(const GameState *g, int owner, int type, bool completeOnly)
{
    int n = 0;
    for (int i = 0; i < g->buildingCount; i++)
        n += g->buildings[i].alive && g->buildings[i].owner == owner && g->buildings[i].type == type &&
             (!completeOnly || g->buildings[i].complete);
    return n;
}

int BuildingsAdd(GameState *g, int owner, int type, int x, int y, bool done)
{
    int id = 0;
    while (id < g->buildingCount && g->buildings[id].alive)
        id++;
    if (id == MAX_BUILDINGS)
        return -1;
    if (id == g->buildingCount)
        g->buildingCount++;
    int generation = g->buildings[id].generation + 1;
    Building *b = &g->buildings[id];
    *b = (Building){.alive = 1,
                    .owner = owner,
                    .generation = generation,
                    .hp = 3000,
                    .started = done,
                    .type = type,
                    .x = x,
                    .y = y,
                    .progress = done ? BUILDINGS[type].labor : 0,
                    .complete = done,
                    .priority = 1};
    b->hp = GameBuildingMaxHealth(g, b);
    for (int yy = y; yy < y + BUILDINGS[type].h; yy++)
        for (int xx = x; xx < x + BUILDINGS[type].w; xx++)
            g->tiles[GameTile(xx, yy)].occupied = id;
    g->mapRevision++;
    return id;
}

bool BuildingsNearby(const GameState *g, int owner, int type, int x, int y, int range)
{
    for (int i = 0; i < g->buildingCount; i++)
    {
        const Building *b = &g->buildings[i];
        if (b->alive && b->owner == owner && b->type == type && b->complete &&
            Dist(b->x, b->y, x, y) <= range)
            return true;
    }
    return false;
}

bool GameCanPlaceFor(const GameState *g, int owner, int type, int x, int y, char *reason, int size)
{
    if (owner < 0 || owner >= FACTION_COUNT || type < 0 || type >= BUILDING_COUNT)
        return Reject(reason, size, "Unknown building.");
    int slots = 0;
    for (int i = 0; i < g->buildingCount; i++)
        slots += g->buildings[i].alive;
    if (slots >= MAX_BUILDINGS)
        return Reject(reason, size, "The valley's building limit is reached.");
    if (type == FARM && (g->factions[owner].age < FARMING_AGE || !g->factions[owner].discovery[FARMING] ||
                         g->tick < WINTER_END))
        return Reject(reason, size, "Wait for the thaw and discover Farming at a staffed workshop.");
    if (type == GRANARY && !g->factions[owner].discovery[FOOD_STORAGE])
        return Reject(reason, size, "Discover Food storage at a staffed workshop first.");
    if ((type == WORKSHOP || type == TRAINING_GROUND) && g->factions[owner].age < SETTLED_AGE)
        return Reject(reason, size, "Advance to Settled Tribe first.");
    if (type == SMELTER && !g->factions[owner].discovery[METALLURGY])
        return Reject(reason, size, "Discover Metallurgy first.");
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
            if (g->tiles[t].occupied >= 0)
                return Reject(reason, size, "Another building occupies this footprint.");
            if (g->tiles[t].node != NODE_NONE)
                return Reject(reason, size, "Gather the natural resources on this tile first.");
            if (type == FARM && g->tiles[t].terrain != FERTILE)
                return Reject(reason, size, "Farm plots need the fertile soil beside the river.");
            for (int i = 0; i < g->citizenCount; i++)
                if (g->citizens[i].alive && g->citizens[i].x / 256 == xx && g->citizens[i].y / 256 == yy)
                    return Reject(reason, size, "A citizen is standing here. Wait until they move.");
        }
    if (!GameCanAffordFor(g, owner, d->cost))
        return Reject(reason, size, "Not enough stored materials. Carried goods must be delivered first.");
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Click to place %s", d->name);
    return true;
}

bool BuildingsPlace(GameState *g, int owner, int type, int x, int y, char *reason, int size)
{
    if (!GameCanPlaceFor(g, owner, type, x, y, reason, size))
        return false;
    int id = BuildingsAdd(g, owner, type, x, y, false);
    EconomyReserve(g, owner, BUILDINGS[type].cost, g->buildings[id].reserve);
    if (type == HEARTH && g->factions[owner].center < 0)
    {
        g->factions[owner].center = id;
        g->buildings[id].upgraded = g->factions[owner].age;
    }
    return true;
}

bool BuildingsUpgrade(GameState *g, int owner, int id, char *reason, int size)
{
    if (id < 0 || id >= g->buildingCount)
        return Reject(reason, size, "Select a completed shelter.");
    Building *b = &g->buildings[id];
    int cost[RESOURCE_COUNT] = {0, 10, 0, 3, 0};
    if (!b->alive || b->owner != owner || b->type != SHELTER || !b->complete || b->upgraded ||
        !g->factions[owner].discovery[INSULATION])
        return Reject(reason, size, "Discover Insulation and select a completed shelter.");
    if (!GameCanAffordFor(g, owner, cost))
        return Reject(reason, size, "A hut upgrade needs 10 wood and 3 hides.");
    EconomyReserve(g, owner, cost, b->reserve);
    b->upgraded = 1;
    b->complete = 0;
    b->progress = 0;
    g->mapRevision++;
    return true;
}

void BuildingsWork(GameState *g, Citizen *c)
{
    Building *b = &g->buildings[c->target];
    if (g->factions[c->owner].advancing && c->target == g->factions[c->owner].center)
    {
        ProgressionWork(g, c);
        return;
    }
    if (b->complete)
    {
        JobsReset(c);
        return;
    }
    if (!b->started)
    {
        b->started = 1;
        g->mapRevision++;
    }
    b->progress++;
    if (b->progress >= BUILDINGS[b->type].labor)
    {
        b->complete = 1;
        EconomySettle(g, b->owner, b->reserve, 1);
        if (b->type == WATCH_POST)
        {
            g->factions[b->owner].rallyX = b->x;
            g->factions[b->owner].rallyY = b->y;
        }
        char msg[128];
        snprintf(msg, sizeof(msg), "%s completed.", b->upgraded ? "Insulated hut" : BUILDINGS[b->type].name);
        if (!b->owner)
            GameEvent(g, msg);
        g->mapRevision++;
    }
}

void BuildingsDestroy(GameState *g, int id)
{
    Building *b = &g->buildings[id];
    if (!b->alive)
        return;
    EconomySettle(g, b->owner, b->reserve, 2);
    while (b->queueCount)
        ProductionCancel(g, b, 0, 2);
    Faction *f = &g->factions[b->owner];
    if (f->researchBuilding == id)
    {
        EconomySettle(g, b->owner, f->researchReserve, 2);
        f->research = -1;
        f->researchProgress = 0;
    }
    if (f->center == id)
    {
        EconomySettle(g, b->owner, f->advanceReserve, 2);
        f->advancing = 0;
        f->center = -1;
        if (b->owner)
            f->defeated = 1;
    }
    b->alive = 0;
    b->hp = 0;
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        if (c->target == id &&
            (c->job == JOB_BUILD || c->job == JOB_REPAIR || c->job == JOB_CRAFT || c->job == JOB_SMELT ||
             c->job == JOB_RESEARCH || c->job == JOB_PLANT || c->job == JOB_HARVEST))
        {
            EconomySettle(g, c->owner, c->recipe, 0);
            JobsReset(c);
        }
    }

    for (int t = 0; t < TILE_COUNT; t++)
        if (g->tiles[t].occupied == id)
            g->tiles[t].occupied = -1;
    g->mapRevision++;
}
