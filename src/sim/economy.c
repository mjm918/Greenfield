#include "economy.h"
#include "buildings.h"
#include "common.h"
#include "jobs.h"
#include "world.h"
#include <limits.h>
int GameFoodDaysTenthsFor(const GameState *g, int owner)
{
    int n = GamePopulationFor(g, owner);
    return n ? (int)((int64_t)g->factions[owner].stock[FOOD] * 10 / (n * 3)) : 0;
}
bool GameCanAffordFor(const GameState *g, int owner, const int *cost)
{
    if (owner < 0 || owner >= FACTION_COUNT)
        return false;
    for (int r = 0; r < RESOURCE_COUNT; r++)
        if (cost[r] < 0 || g->factions[owner].stock[r] < cost[r])
            return false;
    return true;
}
void EconomySpend(GameState *g, int owner, int resource, int count)
{
    Faction *f = &g->factions[owner];
    f->stock[resource] -= count;
    f->spent[resource] += count;
}
void EconomyPay(GameState *g, int owner, const int *cost)
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
        EconomySpend(g, owner, r, cost[r]);
}
bool EconomyTransfer(GameState *g, int from, int to, const int *cost)
{
    if (to < 0 || to >= FACTION_COUNT || from == to || !GameCanAffordFor(g, from, cost))
        return false;
    for (int k = 0; k < RESOURCE_COUNT; k++)
    {
        g->factions[from].stock[k] -= cost[k];
        g->factions[from].exported[k] += cost[k];
        g->factions[to].stock[k] += cost[k];
        g->factions[to].imported[k] += cost[k];
    }
    return true;
}
bool EconomyReserve(GameState *g, int owner, const int *cost, int *receipt)
{
    if (!GameCanAffordFor(g, owner, cost))
        return false;
    Faction *f = &g->factions[owner];
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        f->stock[r] -= cost[r];
        f->reserved[r] += cost[r];
        receipt[r] = cost[r];
    }
    return true;
}
// 0 refund, 1 completion/consumption, 2 destroyed. Receipts are cleared exactly once.
void EconomySettle(GameState *g, int owner, int *receipt, int disposition)
{
    Faction *f = &g->factions[owner];
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        f->reserved[r] -= receipt[r];
        if (disposition == 0)
            f->stock[r] += receipt[r];
        else if (disposition == 1)
            f->spent[r] += receipt[r];
        else
            f->lostGoods[r] += receipt[r];
        receipt[r] = 0;
    }
}
int EconomyCargo(const Citizen *c)
{
    int n = 0;
    for (int r = 0; r < RESOURCE_COUNT; r++)
        n += c->carry[r];
    return n;
}
int EconomyDeliveryTile(GameState *g, Citizen *c)
{
    int best = -1, d = INT_MAX, from = GameTile(c->x / 256, c->y / 256);
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *b = &g->buildings[i];
        if (!b->alive || b->owner != c->owner || !b->complete ||
            (b->type != STOCKPILE && b->type != HEARTH &&
             !(b->type == GRANARY && c->carry[FOOD] == EconomyCargo(c))))
            continue;
        int dd = Dist(b->x, b->y, c->x / 256, c->y / 256);
        if (dd >= d)
            continue;
        int t = WorldAdjacent(g, c->owner, b->x, b->y, GameBuildingWidth(b), GameBuildingHeight(b), from);
        if (t >= 0)
        {
            best = t;
            d = dd;
        }
    }
    return best;
}
void EconomyDeliver(GameState *g, Citizen *c)
{
    Faction *f = &g->factions[c->owner];
    if (c->carry[BRONZE])
        f->bronzeDelivered += c->carry[BRONZE];
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        f->stock[r] += c->carry[r];
        c->carry[r] = 0;
    }
    if (c->harvestCargo)
    {
        bool first = !f->harvestDelivered;
        f->harvestDelivered = 1;
        c->harvestCargo = 0;
        if (first && !c->owner)
            GameEvent(g, "The first grain reaches storage.");
    }
    JobsReset(c);
}
void EconomyProduceBatch(GameState *g, Citizen *c, int resource, int count)
{
    c->carry[resource] += count;
    g->factions[c->owner].produced[resource] += count;
}
void EconomyGatherWork(GameState *g, Citizen *c)
{
    Tile *t = &g->tiles[c->target];
    Faction *f = &g->factions[c->owner];
    if (t->amount <= 0)
    {
        JobsReset(c);
        return;
    }
    int duration = c->role == HUNTER                                 ? 450
                   : c->role == FORAGER                              ? 300
                   : (c->role == STONE_GATHERER || c->role == MINER) ? 180
                                                                     : 140;
    if (!f->stock[TOOLS])
        duration *= 2;
    if ((t->node == NODE_STONE || t->node == NODE_ORE) && t->regrowth)
        duration *= 4;
    if (c->work < duration)
        return;
    int resource = t->node == NODE_TREE    ? WOOD
                   : t->node == NODE_STONE ? STONE
                   : t->node == NODE_ORE   ? ORE
                                           : FOOD;
    int quantity = c->role == HUNTER ? 6 : c->role == FORAGER ? 3 : 5;
    quantity += f->discovery[IMPROVED_TOOLS] ? (resource == FOOD ? 1 : 2) : 0;
    if (c->role == FORAGER && BuildingsNearby(g, c->owner, FORAGING_POST, c->x / 256, c->y / 256, 10))
        quantity++;
    if (c->role == HUNTER && BuildingsNearby(g, c->owner, HUNTING_CAMP, c->x / 256, c->y / 256, 12))
        quantity += 2;
    quantity = Min(quantity, t->amount);
    t->amount -= quantity;
    EconomyProduceBatch(g, c, resource, quantity);
    if (c->role == HUNTER)
        EconomyProduceBatch(g, c, HIDES, 2);
    if (f->stock[TOOLS] > 0 && SimRandom(g) % 20 == 0)
        EconomySpend(g, c->owner, TOOLS, 1);
    if (t->amount == 0 && (t->node == NODE_TREE || t->node == NODE_STONE || t->node == NODE_ORE))
    {
        t->node = NODE_NONE;
        t->regrowth = 1;
        g->mapRevision++;
    }
    c->job = JOB_DELIVER;
    c->target = -1;
    c->work = 0;
}
void EconomyDiscardCargo(GameState *g, Citizen *c)
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        g->factions[c->owner].lostGoods[r] += c->carry[r];
        c->carry[r] = 0;
    }
    EconomySettle(g, c->owner, c->recipe, 2);
}
void EconomyLoot(GameState *g, Citizen *c, int victim)
{
    int capacity = 18 - EconomyCargo(c);
    for (int r = 0; r < RESOURCE_COUNT && capacity > 0; r++)
    {
        int n = Min(capacity, g->factions[victim].stock[r]);
        g->factions[victim].stock[r] -= n;
        g->factions[victim].exported[r] += n;
        g->factions[c->owner].imported[r] += n;
        c->carry[r] += n;
        capacity -= n;
    }
}
