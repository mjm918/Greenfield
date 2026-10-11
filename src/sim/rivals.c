#include "rivals.h"
#include "buildings.h"
#include "common.h"
#include "economy.h"
#include "orders.h"
int GameTruceCost(const GameState *g)
{
    return Max(40, GamePopulation(g) * 3);
}
bool RivalsOfferTruce(GameState *g, char *r, int n)
{
    if (g->treaty || g->factions[1].defeated || g->truceUntil > (int)g->tick)
        return Reject(r, n, "Peace already holds.");
    int cost = GameTruceCost(g);
    if (g->factions[0].stock[FOOD] < cost)
        return Reject(r, n, "Not enough food for the displayed tribute.");
    int payment[RESOURCE_COUNT] = {cost};
    EconomyTransfer(g, 0, 1, payment);
    g->truceUntil = (int)g->tick + 7200;
    for (int o = 0; o < 2; o++)
    {
        Faction *f = &g->factions[o];
        f->raidResolved = 1;
        f->raidWarning = f->raidActive = 0;
        f->nextRaid = g->truceUntil + 1200;
    }
    GameEvent(g, "A twelve-minute truce is accepted. Both armies stand down.");
    return true;
}
bool GameCanTreaty(const GameState *g, char *r, int n)
{
    if (g->treaty || g->factions[1].defeated)
        return Reject(r, n, "The valley is already secure.");
    if (g->factions[0].age < BRONZE_AGE)
        return Reject(r, n, "Reach the Bronze Age first.");
    if ((int)g->tick - g->lastHostileTick < 6000)
        return Reject(r, n, "Keep the peace for ten uninterrupted minutes.");
    const int cost[RESOURCE_COUNT] = {200, 0, 0, 0, 0, 0, 30};
    if (!GameCanAfford(g, cost))
        return Reject(r, n, "The treaty requires 200 food and 30 bronze.");
    if (r && n > 0)
        snprintf(r, (size_t)n, "Treaty available: 200 food and 30 bronze.");
    return true;
}
bool RivalsOfferTreaty(GameState *g, char *r, int n)
{
    if (!GameCanTreaty(g, r, n))
        return false;
    const int cost[RESOURCE_COUNT] = {200, 0, 0, 0, 0, 0, 30};
    EconomyTransfer(g, 0, 1, cost);
    g->treaty = 1;
    for (int o = 0; o < 2; o++)
        g->factions[o].raidActive = g->factions[o].raidWarning = 0;
    GameEvent(g, "A lasting treaty secures the valley.");
    return true;
}
void RivalsUpdate(GameState *g)
{
    if (g->factions[1].defeated)
        for (int i = 0; i < g->buildingCount; i++)
            if (g->buildings[i].alive && g->buildings[i].owner)
                BuildingsDestroy(g, i);
    for (int o = 0; o < 2; o++)
    {
        Faction *f = &g->factions[o];
        if (!f->raidActive)
            continue;
        int active = 0;
        for (int i = 0; i < g->citizenCount; i++)
        {
            Citizen *c = &g->citizens[i];
            if (c->alive && c->owner == o && c->unitType && c->orderCount &&
                (c->orders[0].type == ORDER_ATTACK || c->orders[0].type == ORDER_PLUNDER))
                active++;
        }
        if (!active || active * 2 < f->raidSize || (int)g->tick - f->raidStarted >= 2400)
        {
            for (int i = 0; i < g->citizenCount; i++)
            {
                Citizen *c = &g->citizens[i];
                if (c->alive && c->owner == o && c->orderCount &&
                    (c->orders[0].type == ORDER_PLUNDER || c->orders[0].type == ORDER_ATTACK))
                    GameCommandApply(
                        g,
                        (GameCommand){.type = CMD_ORDER,
                                      .owner = o,
                                      .entity = GameCitizenRef(g, i),
                                      .order = {.type = ORDER_RETREAT, .x = f->rallyX, .y = f->rallyY}},
                        NULL, 0);
            }
            f->raidActive = 0;
            g->factions[1 - o].raidResolved = 1;
        }
    }
}
