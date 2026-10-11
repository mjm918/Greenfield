#include "economy.h"
#include "buildings.h"
#include "common.h"
#include "jobs.h"
#include "world.h"
#include <limits.h>

int GameFoodDaysTenths(const GameState *g)
{
    int n = GamePopulation(g);
    return n ? g->stock[FOOD] * 10 / (n * 3) : 0;
}

bool GameCanAfford(const GameState *g, const int cost[RESOURCE_COUNT])
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
        if (g->stock[r] < cost[r])
            return false;
    return true;
}

void EconomySpend(GameState *g, int resource, int count)
{
    g->stock[resource] -= count;
    g->spent[resource] += count;
}

void EconomyPay(GameState *g, const int cost[RESOURCE_COUNT])
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
        EconomySpend(g, r, cost[r]);
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
        if (!b->complete || (b->type != STOCKPILE && !(b->type == GRANARY && c->carry[FOOD] == EconomyCargo(c))))
            continue;
        int t = WorldAdjacent(g, b->x, b->y, BUILDINGS[b->type].w, BUILDINGS[b->type].h, from);
        if (t >= 0)
        {
            int dd = Dist(t % MAP_SIZE, t / MAP_SIZE, c->x / 256, c->y / 256);
            if (dd < d)
            {
                best = t;
                d = dd;
            }
        }
    }
    return best;
}

void EconomyDeliver(GameState *g, Citizen *c)
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        g->stock[r] += c->carry[r];
        c->carry[r] = 0;
    }
    if (c->harvestCargo)
    {
        g->harvestDelivered = 1;
        c->harvestCargo = 0;
        GameEvent(g, "The first grain reaches storage. Farming can sustain a village.");
    }
    JobsReset(c);
}

static void Collect(GameState *g, Citizen *c, int resource, int count)
{
    c->carry[resource] += count;
    g->produced[resource] += count;
}

void EconomyGatherWork(GameState *g, Citizen *c)
{
    Tile *t = &g->tiles[c->target];
    if (t->amount <= 0)
    {
        JobsReset(c);
        return;
    }
    int duration = c->role == HUNTER           ? 450
                   : c->role == FORAGER        ? 300
                   : c->role == STONE_GATHERER ? 180
                                               : 140;
    if (g->stock[TOOLS] <= 0)
        duration *= 2;
    if (c->work < duration)
        return;
    int resource = c->role == WOODCUTTER ? WOOD : c->role == STONE_GATHERER ? STONE : FOOD;
    int quantity = c->role == HUNTER ? 6 : c->role == FORAGER ? 3 : 5;
    quantity += g->discovery[IMPROVED_TOOLS] ? (resource == FOOD ? 1 : 2) : 0;
    if (c->role == FORAGER && BuildingsNearby(g, FORAGING_POST, c->x / 256, c->y / 256, 10))
        quantity += 1;
    if (c->role == HUNTER && BuildingsNearby(g, HUNTING_CAMP, c->x / 256, c->y / 256, 12))
        quantity += 2;
    quantity = Min(quantity, t->amount);
    t->amount -= quantity;
    Collect(g, c, resource, quantity);
    if (c->role == HUNTER)
        Collect(g, c, HIDES, 2);
    if (g->stock[TOOLS] > 0 && SimRandom(g) % 20 == 0)
        EconomySpend(g, TOOLS, 1);
    if (t->amount == 0 && (t->node == NODE_TREE || t->node == NODE_STONE))
    {
        t->node = NODE_NONE;
        g->mapRevision++;
    }
    c->job = JOB_DELIVER;
    c->target = -1;
    c->work = 0;
}

void EconomyProduceBatch(GameState *g, Citizen *c, int resource, int count)
{
    // Completed crafting/harvest jobs replace their batch slot; gathering appends.
    c->carry[resource] = count;
    g->produced[resource] += count;
}


void EconomyDiscardCargo(GameState *g, Citizen *c)
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        g->spent[r] += c->carry[r];
        c->carry[r] = 0;
    }
}

void EconomySteal(GameState *g, int resource, int count)
{
    g->stock[resource] -= count;
    g->stolen[resource] += count;
}
