#include "buildings.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"
#include "production.h"
#include "progression.h"

int GameBuildingWidth(const Building *b)
{
    return b->rotation ? BUILDINGS[b->type].h : BUILDINGS[b->type].w;
}

int GameBuildingHeight(const Building *b)
{
    return b->rotation ? BUILDINGS[b->type].w : BUILDINGS[b->type].h;
}

static void Occupy(GameState *g, int id)
{
    const Building *b = &g->buildings[id];
    for (int y = b->y; y < b->y + GameBuildingHeight(b); y++)
        for (int x = b->x; x < b->x + GameBuildingWidth(b); x++)
            g->tiles[GameTile(x, y)].occupied = id;
}

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

static int Add(GameState *g, int owner, int type, int x, int y, int rotation, bool done)
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
                    .rotation = rotation,
                    .x = x,
                    .y = y,
                    .progress = done ? BUILDINGS[type].labor : 0,
                    .complete = done,
                    .priority = 1};
    b->hp = GameBuildingMaxHealth(g, b);
    Occupy(g, id);
    g->mapRevision++;
    return id;
}

int BuildingsAdd(GameState *g, int owner, int type, int x, int y, bool done)
{
    return Add(g, owner, type, x, y, 0, done);
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

bool GameBuildingUnlockedFor(const GameState *g, int owner, int type, char *reason, int size)
{
    if (owner < 0 || owner >= FACTION_COUNT || type < 0 || type >= BUILDING_COUNT)
        return Reject(reason, size, "Unknown building.");
    if (type == FARM && g->factions[owner].age < FARMING_AGE)
        return Reject(reason, size, "Advance to Farming Village first.");
    if (type == FARM && g->tick < WINTER_END)
        return Reject(reason, size, "Wait for the winter thaw.");
    if (type == FARM && !g->factions[owner].discovery[FARMING])
        return Reject(reason, size, "Discover Farming at a staffed workshop.");
    if (type == GRANARY && !g->factions[owner].discovery[FOOD_STORAGE])
        return Reject(reason, size, "Requires Food storage research at a Settled Tribe workshop.");
    if ((type == WORKSHOP || type == TRAINING_GROUND) && g->factions[owner].age < SETTLED_AGE)
        return Reject(reason, size, "Advance to Settled Tribe first.");
    if (type == SMELTER && !g->factions[owner].discovery[METALLURGY])
        return Reject(reason, size, "Requires Metallurgy research in the Farming Village age.");
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Unlocked");
    return true;
}

// Shared terrain/occupancy validation for both new sites and existing structures.
static bool CanOccupy(const GameState *g, const Building *b, int ignore, char *reason, int size)
{
    int w = GameBuildingWidth(b), h = GameBuildingHeight(b);
    if (b->rotation < 0 || b->rotation > 1)
        return Reject(reason, size, "Choose one of the two building orientations.");
    if (b->x < 0 || b->y < 0 || b->x > MAP_SIZE - w || b->y > MAP_SIZE - h)
        return Reject(reason, size, "Keep the whole footprint inside the valley.");
    for (int y = b->y; y < b->y + h; y++)
        for (int x = b->x; x < b->x + w; x++)
        {
            const Tile *t = &g->tiles[GameTile(x, y)];
            if (t->terrain == WATER || t->terrain == SHALLOWS)
                return Reject(reason, size, "Water and river crossings must stay clear.");
            if (t->occupied >= 0 && t->occupied != ignore)
                return Reject(reason, size, "Another building occupies this footprint.");
            if (t->node != NODE_NONE)
                return Reject(reason, size, "Gather the natural resources on this tile first.");
            if (b->type == FARM && t->terrain != FERTILE)
                return Reject(reason, size, "Farm plots need the fertile soil beside the river.");
            // Existing occupants are safe on retained footprint tiles. Do not
            // trap anyone by moving/expanding a solid structure onto a new tile.
            if (ignore >= 0 && t->occupied == ignore)
                continue;
            for (int i = 0; i < g->citizenCount; i++)
                if (g->citizens[i].alive && g->citizens[i].x / 256 == x && g->citizens[i].y / 256 == y)
                    return Reject(reason, size, "A citizen is standing here. Wait until they move.");
        }
    return true;
}

bool GameCanPlaceRotatedFor(const GameState *g, int owner, int type, int x, int y, int rotation,
                            char *reason, int size)
{
    if (!GameBuildingUnlockedFor(g, owner, type, reason, size))
        return false;
    int slots = 0;
    for (int i = 0; i < g->buildingCount; i++)
        slots += g->buildings[i].alive;
    if (slots >= MAX_BUILDINGS)
        return Reject(reason, size, "The valley's building limit is reached.");
    Building candidate = {.type = type, .x = x, .y = y, .rotation = rotation};
    if (!CanOccupy(g, &candidate, -1, reason, size))
        return false;
    if (!GameCanAffordFor(g, owner, BUILDINGS[type].cost))
        return Reject(reason, size, "Not enough stored materials. Carried goods must be delivered first.");
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Click to place %s", BUILDINGS[type].name);
    return true;
}

bool GameCanPlaceFor(const GameState *g, int owner, int type, int x, int y, char *reason, int size)
{
    return GameCanPlaceRotatedFor(g, owner, type, x, y, 0, reason, size);
}

bool BuildingsPlace(GameState *g, int owner, int type, int x, int y, int rotation, char *reason, int size)
{
    if (!GameCanPlaceRotatedFor(g, owner, type, x, y, rotation, reason, size))
        return false;
    int id = Add(g, owner, type, x, y, rotation, false);
    EconomyReserve(g, owner, BUILDINGS[type].cost, g->buildings[id].reserve);
    if (type == HEARTH && g->factions[owner].center < 0)
    {
        g->factions[owner].center = id;
        g->buildings[id].upgraded = g->factions[owner].age;
    }
    return true;
}

bool GameCanTransformBuildingFor(const GameState *g, int owner, EntityRef ref, int x, int y,
                                int rotation, char *reason, int size)
{
    if (owner < 0 || owner >= FACTION_COUNT || !GameBuildingValid(g, ref) ||
        g->buildings[ref.slot].owner != owner)
        return Reject(reason, size, "Select an existing building in your settlement.");
    Building candidate = g->buildings[ref.slot];
    candidate.x = x;
    candidate.y = y;
    candidate.rotation = rotation;
    if (!CanOccupy(g, &candidate, ref.slot, reason, size))
        return false;
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Click to move %s. Progress and queues are kept.",
                 BUILDINGS[candidate.type].name);
    return true;
}

bool BuildingsEdit(GameState *g, GameCommand cmd, char *reason, int size)
{
    if (!GameBuildingValid(g, cmd.entity) || g->buildings[cmd.entity.slot].owner != cmd.owner)
        return Reject(reason, size, "Select an existing building in your settlement.");
    Building *b = &g->buildings[cmd.entity.slot];
    if (cmd.type == CMD_DEMOLISH)
    {
        BuildingsDestroy(g, cmd.entity.slot);
        return true;
    }
    int x = cmd.type == CMD_ROTATE_BUILDING ? b->x : cmd.a;
    int y = cmd.type == CMD_ROTATE_BUILDING ? b->y : cmd.b;
    int rotation = cmd.type == CMD_ROTATE_BUILDING ? 1 - b->rotation : cmd.rotation;
    if (!GameCanTransformBuildingFor(g, cmd.owner, cmd.entity, x, y, rotation, reason, size))
        return false;
    if (x == b->x && y == b->y && rotation == b->rotation)
        return true;
    for (int t = 0; t < TILE_COUNT; t++)
        if (g->tiles[t].occupied == cmd.entity.slot)
            g->tiles[t].occupied = -1;
    b->x = x;
    b->y = y;
    b->rotation = rotation;
    Occupy(g, cmd.entity.slot);
    // Handles, work, cargo, recipes and queues are retained. Jobs and trainees
    // resolve their destination from the live building after route invalidation.
    g->mapRevision++;
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        for (int q = 0; q < c->orderCount; q++)
            if (c->orders[q].targetKind == TARGET_BUILDING &&
                c->orders[q].target.slot == cmd.entity.slot &&
                c->orders[q].target.generation == cmd.entity.generation)
            {
                c->orders[q].x = x;
                c->orders[q].y = y;
            }
    }
    for (int o = 0; o < FACTION_COUNT; o++)
    {
        Faction *f = &g->factions[o];
        if (f->rallyOrder.targetKind == TARGET_BUILDING && f->rallyOrder.target.slot == cmd.entity.slot &&
            f->rallyOrder.target.generation == cmd.entity.generation)
        {
            f->rallyOrder.x = f->rallyX = x;
            f->rallyOrder.y = f->rallyY = y;
        }
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
