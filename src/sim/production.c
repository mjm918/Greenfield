#include "production.h"
#include "citizens.h"
#include "common.h"
#include "economy.h"
#include "orders.h"
#include "world.h"
#include <string.h>
void ProductionCancel(GameState *g, Building *b, int index, int disposition)
{
    if (index < 0 || index >= b->queueCount)
        return;
    Production *p = &b->queue[index];
    if (p->type != QUEUE_RECRUIT && GameCitizenValid(g, p->citizen))
        g->citizens[p->citizen.slot].training = -1;
    EconomySettle(g, b->owner, p->cost, disposition);
    memmove(p, p + 1, (size_t)(b->queueCount - index - 1) * sizeof(*p));
    memset(&b->queue[--b->queueCount], 0, sizeof(*p));
}
bool ProductionCommand(GameState *g, GameCommand cmd, char *r, int n)
{
    Faction *f = &g->factions[cmd.owner];
    if (cmd.type == CMD_DEMOBILIZE)
    {
        if (!GameCitizenValid(g, cmd.entity) || g->citizens[cmd.entity.slot].owner != cmd.owner)
            return Reject(r, n, "Select your soldier.");
        Citizen *c = &g->citizens[cmd.entity.slot];
        if (c->training >= 0)
            return Reject(r, n, "Cancel training first.");
        OrdersClear(g, c);
        c->unitType = CIVILIAN;
        c->role = IDLE;
        return true;
    }
    EntityRef ref = cmd.type == CMD_TRAIN || cmd.type == CMD_EQUIP ? cmd.order.target : cmd.entity;
    if (!GameBuildingValid(g, ref))
        return Reject(r, n, "Select a living production building.");
    Building *b = &g->buildings[ref.slot];
    if (b->owner != cmd.owner)
        return Reject(r, n, "This is not your building.");
    if (cmd.type == CMD_CANCEL)
    {
        if (cmd.a < 0 || cmd.a >= b->queueCount)
            return Reject(r, n, "Select a queue item.");
        ProductionCancel(g, b, cmd.a, 0);
        return true;
    }
    if (!b->complete || b->queueCount >= MAX_QUEUE)
        return Reject(r, n, "Complete the building or free a queue slot.");
    Production p = {0};
    p.citizen = (EntityRef){-1, 0};
    p.duration = 300;
    if (cmd.type == CMD_RECRUIT)
    {
        if (b->type != HEARTH || f->center != ref.slot)
            return Reject(r, n, "Recruit at your communal center.");
        int queued = b->queueCount;
        if (GamePopulationFor(g, cmd.owner) + queued >= Min(POPULATION_LIMIT, GameHousingFor(g, cmd.owner)))
            return Reject(r, n, "Build more housing before recruiting.");
        p.type = QUEUE_RECRUIT;
        p.duration = RECRUIT_TICKS[f->age];
        memcpy(p.cost, RECRUIT_COST, sizeof(p.cost));
    }
    else
    {
        if (b->type != TRAINING_GROUND || f->age < SETTLED_AGE || !GameCitizenValid(g, cmd.entity))
            return Reject(r, n, "Select a citizen and a completed training ground.");
        Citizen *c = &g->citizens[cmd.entity.slot];
        if (c->owner != cmd.owner || c->training >= 0)
            return Reject(r, n, "Citizen is already training or belongs to the rival.");
        if (cmd.type == CMD_EQUIP)
        {
            if (f->age < BRONZE_AGE || !c->unitType || c->equipped)
                return Reject(r, n, "Bronze equipment needs a Bronze Age soldier.");
            p.type = QUEUE_EQUIP;
            p.unitType = c->unitType;
            memcpy(p.cost, EQUIPMENT_COST, sizeof(p.cost));
        }
        else
        {
            if (cmd.a <= CIVILIAN || cmd.a >= UNIT_COUNT || c->unitType)
                return Reject(r, n, "Choose a civilian and military class.");
            if (cmd.a == AXEMAN && f->age < FARMING_AGE)
                return Reject(r, n, "Axemen require the Farming Village age.");
            p.type = QUEUE_TRAIN;
            p.unitType = cmd.a;
            memcpy(p.cost, UNITS[cmd.a].cost, sizeof(p.cost));
            p.duration = UNITS[cmd.a].labor;
        }
        p.citizen = cmd.entity;
    }
    int cost[RESOURCE_COUNT];
    memcpy(cost, p.cost, sizeof(cost));
    if (!EconomyReserve(g, cmd.owner, cost, p.cost))
        return Reject(r, n, "Not enough resources for this queue item.");
    b->queue[b->queueCount++] = p;
    if (p.type != QUEUE_RECRUIT)
    {
        Citizen *c = &g->citizens[p.citizen.slot];
        OrdersClear(g, c);
        c->training = ref.slot;
        c->role = IDLE;
    }
    return true;
}
void ProductionUpdate(GameState *g)
{
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *b = &g->buildings[i];
        if (!b->alive || !b->complete || !b->queueCount)
            continue;
        Production *p = &b->queue[0];
        Faction *f = &g->factions[b->owner];
        if (p->type == QUEUE_RECRUIT)
        {
            if (GamePopulationFor(g, b->owner) >= Min(POPULATION_LIMIT, GameHousingFor(g, b->owner)))
                continue;
        }
        else
        {
            if (!GameCitizenValid(g, p->citizen))
            {
                ProductionCancel(g, b, 0, 2);
                continue;
            }
            Citizen *c = &g->citizens[p->citizen.slot];
            if (EconomyCargo(c))
            {
                int dest = EconomyDeliveryTile(g, c);
                if (dest < 0)
                    continue;
                if (c->x / 256 == dest % MAP_SIZE && c->y / 256 == dest / MAP_SIZE)
                    EconomyDeliver(g, c);
                else if (c->pathIndex < c->pathLength)
                    WorldMoveCitizen(g, c);
                else
                    WorldRoute(g, c, dest);
                continue;
            }
            const BuildingDef *def = &BUILDINGS[b->type];
            int dx = Max(0, Max(b->x - c->x / 256, c->x / 256 - (b->x + def->w - 1)));
            int dy = Max(0, Max(b->y - c->y / 256, c->y / 256 - (b->y + def->h - 1)));
            // WorldAdjacent includes the corners of the footprint. Every tile
            // it chooses must be a valid arrival, or the head of the queue stalls.
            if (dx > 1 || dy > 1)
            {
                if (c->pathIndex < c->pathLength)
                    WorldMoveCitizen(g, c);
                else
                    WorldRoute(g, c,
                               WorldAdjacent(g, c->owner, b->x, b->y, def->w, def->h,
                                             GameTile(c->x / 256, c->y / 256)));
                continue;
            }
        }
        if (++p->progress < p->duration)
            continue;
        int id = p->type == QUEUE_RECRUIT ? CitizensAdd(g, b->owner, IDLE) : p->citizen.slot;
        if (id < 0)
            continue;
        Citizen *c = &g->citizens[id];
        if (p->type == QUEUE_RECRUIT)
        {
            f->arrivals++;
            c->orders[0] = f->rallyOrder;
            c->orderCount = f->rallyOrder.type != ORDER_AUTO;
        }
        else if (p->type == QUEUE_TRAIN)
        {
            c->unitType = p->unitType;
            c->role = GUARD;
        }
        else
            c->equipped = 1;
        ProductionCancel(g, b, 0, 1);
    }
}
