#include "common.h"
#include <limits.h>
#include <string.h>
static bool Flag(int v)
{
    return v == 0 || v == 1;
}
static bool OrderValid(const UnitOrder *o)
{
    if (o->type < 0 || o->type >= ORDER_COUNT || o->targetKind < 0 || o->targetKind > TARGET_BUILDING ||
        GameTile(o->x, o->y) < 0)
        return false;
    if (o->targetKind != TARGET_TILE &&
        (o->target.generation <= 0 || o->target.slot < 0 ||
         o->target.slot >= (o->targetKind == TARGET_CITIZEN ? MAX_CITIZENS : MAX_BUILDINGS)))
        return false;
    if ((o->type == ORDER_BUILD || o->type == ORDER_REPAIR || o->type == ORDER_WORK ||
         o->type == ORDER_PLUNDER) &&
        o->targetKind != TARGET_BUILDING)
        return false;
    if (o->type == ORDER_ATTACK && o->targetKind == TARGET_TILE)
        return false;
    if (o->type == ORDER_GATHER && o->targetKind != TARGET_TILE)
        return false;
    return true;
}
bool GameValidate(const GameState *g, char *r, int n)
{
    if (g->tick > INT_MAX - 100000 || !g->random || g->citizenCount < 0 || g->citizenCount > MAX_CITIZENS ||
        g->buildingCount < 0 || g->buildingCount > MAX_BUILDINGS || g->eventCount < 0 || g->eventCount > 8 ||
        g->mapRevision < 0 || g->truceUntil < 0 || g->lastHostileTick < 0 ||
        g->lastHostileTick > (int)g->tick || !Flag(g->won) || !Flag(g->lost) || !Flag(g->treaty) ||
        !Flag(g->winterSurvived) || g->wildlifeCount < 0 || g->wildlifeCount > MAX_WILDLIFE)
        return Reject(r, n, "Invalid simulation header.");
    for (int i = 0; i < g->wildlifeCount; i++)
    {
        const Wildlife *a = &g->wildlife[i];
        if (a->home < 0 || a->home >= TILE_COUNT || a->x < 0 || a->x >= MAP_SIZE * 256 || a->y < 0 ||
            a->y >= MAP_SIZE * 256 || a->goalX < 0 || a->goalX >= MAP_SIZE * 256 || a->goalY < 0 ||
            a->goalY >= MAP_SIZE * 256 || a->facing < FACING_SE || a->facing > FACING_NW ||
            a->state < ANIMAL_IDLE || a->state > ANIMAL_RUN || a->wait < 0 || a->wait > 100 ||
            a->stepX < -18 || a->stepX > 18 || a->stepY < -18 || a->stepY > 18 || !a->random ||
            Dist(a->x, a->y, a->home % MAP_SIZE * 256, a->home / MAP_SIZE * 256) > 512 ||
            Dist(a->goalX, a->goalY, a->home % MAP_SIZE * 256, a->home / MAP_SIZE * 256) > 512)
            return Reject(r, n, "Invalid wildlife.");
        for (int j = 0; j < i; j++)
            if (g->wildlife[j].home == a->home)
                return Reject(r, n, "Duplicate hunting ground animal.");
    }
    int64_t cargo[2][RESOURCE_COUNT] = {{0}}, reserved[2][RESOURCE_COUNT] = {{0}};
    int trainees[MAX_CITIZENS] = {0};
    for (int i = 0; i < 8; i++)
        if (!memchr(g->events[i], 0, 128))
            return Reject(r, n, "Invalid chronicle.");
    for (int i = 0; i < g->citizenCount; i++)
    {
        const Citizen *c = &g->citizens[i];
        if (!Flag(c->alive) || c->owner < 0 || c->owner >= 2 || c->generation <= 0 || c->role < 0 ||
            c->role > IDLE || c->unitType < 0 || c->unitType >= UNIT_COUNT || !Flag(c->equipped) ||
            c->x < 0 || c->x >= MAP_SIZE * 256 || c->y < 0 || c->y >= MAP_SIZE * 256 || c->hp > 1000 ||
            c->hp < -1000 || c->job < 0 || c->job >= JOB_COUNT || c->target < -1 || c->target >= TILE_COUNT ||
            c->pathLength < 0 || c->pathLength > MAX_PATH || c->pathIndex < 0 ||
            c->pathIndex > c->pathLength || c->orderCount < 0 || c->orderCount > MAX_ORDERS ||
            c->training < -1 || c->training >= g->buildingCount || c->work < 0 || c->work > 1000000 ||
            c->hunger < 0 || c->hunger > 10000000 || c->cold < 0 || c->cold > 10000000 || c->retry < 0 ||
            c->retry > 1000 || c->attackCooldown < 0 || c->attackCooldown > 100 || c->pathRevision < 0 ||
            !Flag(c->harvestCargo) || c->facing < FACING_SE || c->facing > FACING_NW)
            return Reject(r, n, "Invalid citizen.");
        if (c->job == JOB_BUILD || c->job == JOB_REPAIR || c->job == JOB_CRAFT || c->job == JOB_RESEARCH ||
            c->job == JOB_SMELT || c->job == JOB_PLANT || c->job == JOB_HARVEST)
            if (c->target < 0 || c->target >= g->buildingCount)
                return Reject(r, n, "Invalid job target.");
        for (int p = 0; p < c->pathLength; p++)
            if (c->path[p] < 0 || c->path[p] >= TILE_COUNT)
                return Reject(r, n, "Invalid route.");
        for (int p = 0; p < c->orderCount; p++)
            if (!OrderValid(&c->orders[p]))
                return Reject(r, n, "Invalid order.");
        for (int k = 0; k < RESOURCE_COUNT; k++)
        {
            if (c->carry[k] < 0 || c->carry[k] > 1000 || c->recipe[k] < 0 || c->recipe[k] > 10000 ||
                (!c->alive && (c->carry[k] || c->recipe[k])))
                return Reject(r, n, "Invalid cargo.");
            cargo[c->owner][k] += c->carry[k];
            reserved[c->owner][k] += c->recipe[k];
        }
    }
    for (int i = 0; i < g->buildingCount; i++)
    {
        const Building *b = &g->buildings[i];
        if (!Flag(b->alive) || b->owner < 0 || b->owner >= 2 || b->generation <= 0 || b->type < 0 ||
            b->type >= BUILDING_COUNT || b->x < 0 || b->y < 0 || b->x + GameBuildingWidth(b) > MAP_SIZE ||
            b->y + GameBuildingHeight(b) > MAP_SIZE || b->hp < 0 || b->hp > GameBuildingMaxHealth(g, b) ||
            b->progress < 0 || b->progress > BUILDINGS[b->type].labor || !Flag(b->complete) ||
            !Flag(b->rotation) || !Flag(b->started) || b->upgraded < 0 || b->upgraded > 3 || b->crop < 0 ||
            b->crop > 2 || b->cropProgress < 0 || b->cropProgress > 2400 || b->queueCount < 0 || b->queueCount > MAX_QUEUE)
            return Reject(r, n, "Invalid building.");
        if ((!b->alive && b->queueCount) || b->priority < 0 || b->priority > 2)
            return Reject(r, n, "Invalid building lifecycle.");
        if (b->alive)
            for (int y = b->y; y < b->y + GameBuildingHeight(b); y++)
                for (int x = b->x; x < b->x + GameBuildingWidth(b); x++)
                    if (g->tiles[GameTile(x, y)].occupied != i)
                        return Reject(r, n, "Building occupancy mismatch.");
        for (int k = 0; k < RESOURCE_COUNT; k++)
        {
            if (b->reserve[k] < 0 || b->reserve[k] > 10000)
                return Reject(r, n, "Invalid building reservation.");
            reserved[b->owner][k] += b->reserve[k];
        }
        for (int q = 0; q < b->queueCount; q++)
        {
            const Production *p = &b->queue[q];
            if (p->type < 0 || p->type > QUEUE_EQUIP || p->unitType < 0 || p->unitType >= UNIT_COUNT ||
                p->duration < 1 || p->duration > 10000 || p->progress < 0 || p->progress > p->duration)
                return Reject(r, n, "Invalid production queue.");
            if (p->type == QUEUE_RECRUIT && b->type != HEARTH)
                return Reject(r, n, "Invalid recruitment building.");
            if (p->type != QUEUE_RECRUIT && (!GameCitizenValid(g, p->citizen) || b->type != TRAINING_GROUND ||
                                             g->citizens[p->citizen.slot].owner != b->owner ||
                                             g->citizens[p->citizen.slot].training != i))
                return Reject(r, n, "Invalid trainee.");
            if (p->type != QUEUE_RECRUIT)
            {
                const Citizen *c = &g->citizens[p->citizen.slot];
                if (++trainees[p->citizen.slot] > 1 || p->unitType == CIVILIAN ||
                    (p->type == QUEUE_TRAIN && c->unitType != CIVILIAN) ||
                    (p->type == QUEUE_EQUIP && (c->unitType == CIVILIAN || c->equipped)))
                    return Reject(r, n, "Invalid training lifecycle.");
            }
            for (int previous = 0; previous < q; previous++)
                if (p->type != QUEUE_RECRUIT && b->queue[previous].type != QUEUE_RECRUIT &&
                    b->queue[previous].citizen.slot == p->citizen.slot)
                    return Reject(r, n, "Duplicate trainee.");
            for (int k = 0; k < RESOURCE_COUNT; k++)
            {
                if (p->cost[k] < 0 || p->cost[k] > 10000)
                    return Reject(r, n, "Invalid queue reservation.");
                reserved[b->owner][k] += p->cost[k];
            }
        }
    }
    for (int i = 0; i < g->citizenCount; i++)
        if (g->citizens[i].alive && (g->citizens[i].training >= 0) != (trainees[i] == 1))
            return Reject(r, n, "Missing training queue.");
    for (int i = 0; i < TILE_COUNT; i++)
    {
        const Tile *t = &g->tiles[i];
        if (t->terrain < 0 || t->terrain > ROCK_GROUND || t->node < 0 || t->node > NODE_ORE ||
            t->natural < 0 || t->natural > NODE_ORE || t->amount < 0 || t->amount > 1000000 ||
            t->occupied < -1 || t->occupied >= g->buildingCount || t->regrowth < 0 || t->regrowth > 10)
            return Reject(r, n, "Invalid terrain.");
        if (t->occupied >= 0)
        {
            const Building *b = &g->buildings[t->occupied];
            if (!b->alive || i % MAP_SIZE < b->x || i % MAP_SIZE >= b->x + GameBuildingWidth(b) ||
                i / MAP_SIZE < b->y || i / MAP_SIZE >= b->y + GameBuildingHeight(b))
                return Reject(r, n, "Invalid occupied tile.");
        }
    }
    for (int o = 0; o < 2; o++)
    {
        const Faction *f = &g->factions[o];
        if (!Flag(f->aiEnabled) || !Flag(f->raidWarning) || !Flag(f->raidActive) || !Flag(f->raidResolved) ||
            !Flag(f->defeated) || !Flag(f->village) || !Flag(f->foodWarning) || !Flag(f->coldWarning) ||
            !Flag(f->hadShortage) || f->villageTick < 0 || f->villageTick > (int)g->tick ||
            f->raidStarted < 0 || f->raidStarted > (int)g->tick || f->raidSize < 0 ||
            f->raidSize > POPULATION_LIMIT || f->nextDecision < 0 || f->nextRaid < 0 || f->raidCount < 0 ||
            f->shortageTicks < 0 || f->shortageRecoveries < 0 || f->arrivals < 0 || f->deaths < 0 ||
            f->harvestDelivered < 0 || f->bronzeDelivered < 0 || GameTile(f->rallyX, f->rallyY) < 0)
            return Reject(r, n, "Invalid faction counters.");
        if (f->age < 0 || f->age >= AGE_COUNT || !Flag(f->advancing) ||
            (f->advancing && f->age == BRONZE_AGE) || f->advanceProgress < 0 || f->advanceProgress > 3600 ||
            f->center < -1 || f->center >= g->buildingCount || f->research < -1 ||
            f->research >= DISCOVERY_COUNT || f->researchProgress < 0 || f->researchProgress > 6000 ||
            f->researchBuilding < -1 || f->researchBuilding >= g->buildingCount || f->warmth < 0 ||
            f->warmth > 100 || GamePopulationFor(g, o) > POPULATION_LIMIT || !OrderValid(&f->rallyOrder))
            return Reject(r, n, "Invalid faction progression.");
        if (f->center >= 0 && (!g->buildings[f->center].alive || g->buildings[f->center].owner != o ||
                               g->buildings[f->center].type != HEARTH))
            return Reject(r, n, "Invalid communal center.");
        if ((f->advancing && f->center < 0) ||
            (f->research >= 0 && (f->researchBuilding < 0 || !g->buildings[f->researchBuilding].alive ||
                                  g->buildings[f->researchBuilding].owner != o ||
                                  g->buildings[f->researchBuilding].type != WORKSHOP)))
            return Reject(r, n, "Missing project building.");
        for (int d = 0; d < DISCOVERY_COUNT; d++)
            if (!Flag(f->discovery[d]))
                return Reject(r, n, "Invalid discovery.");
        for (int k = 0; k < ROLE_COUNT; k++)
            if (f->areaX[k] < -1 || f->areaX[k] >= MAP_SIZE || f->areaY[k] < -1 || f->areaY[k] >= MAP_SIZE ||
                f->priority[k] < 0 || f->priority[k] > 2)
                return Reject(r, n, "Invalid work area.");
        for (int k = 0; k < RESOURCE_COUNT; k++)
        {
            const int values[] = {f->stock[k],           f->initial[k],       f->produced[k], f->spent[k],
                                  f->imported[k],        f->exported[k],      f->reserved[k], f->lostGoods[k],
                                  f->researchReserve[k], f->advanceReserve[k]};
            for (unsigned j = 0; j < sizeof(values) / sizeof(values[0]); j++)
                if (values[j] < 0 || values[j] > 1000000000)
                    return Reject(r, n, "Invalid resource ledger.");
            reserved[o][k] += f->researchReserve[k] + f->advanceReserve[k];
            if (reserved[o][k] != f->reserved[k] ||
                (int64_t)f->initial[k] + f->produced[k] + f->imported[k] !=
                    (int64_t)f->stock[k] + cargo[o][k] + f->reserved[k] + f->spent[k] + f->exported[k] +
                        f->lostGoods[k])
                return Reject(r, n, "Resource conservation failed.");
            if (f->imported[k] != g->factions[1 - o].exported[k])
                return Reject(r, n, "Faction transfer mismatch.");
        }
    }
    return true;
}
