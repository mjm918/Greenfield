#include "jobs.h"
#include "buildings.h"
#include "common.h"
#include "economy.h"
#include "farming.h"
#include "orders.h"
#include "workshop.h"
#include "world.h"
#include <limits.h>

void JobsReset(Citizen *c)
{
    c->job = JOB_IDLE;
    c->target = -1;
    c->work = 0;
    c->pathLength = 0;
    c->pathIndex = 0;
    c->retry = 0;
}

static int Destination(GameState *g, Citizen *c)
{
    int from = GameTile(c->x / 256, c->y / 256);
    if (c->job == JOB_DELIVER)
        return EconomyDeliveryTile(g, c);
    if (c->job == JOB_MOVE)
        return c->target;
    if (c->job == JOB_GATHER && c->target >= 0 && c->target < TILE_COUNT)
        return WorldAdjacent(g, c->owner, c->target % MAP_SIZE, c->target / MAP_SIZE, 1, 1, from);
    if (c->job == JOB_GUARD)
        return c->target >= 0 ? c->target
                              : GameTile(g->factions[c->owner].rallyX, g->factions[c->owner].rallyY);
    if (c->target >= 0 && c->target < g->buildingCount)
    {
        Building *b = &g->buildings[c->target];
        return WorldAdjacent(g, c->owner, b->x, b->y, BUILDINGS[b->type].w, BUILDINGS[b->type].h, from);
    }
    return -1;
}

static void ChooseJob(GameState *g, Citizen *c)
{
    if (EconomyCargo(c))
    {
        c->job = JOB_DELIVER;
        c->target = -1;
        return;
    }
    EconomySettle(g, c->owner, c->recipe, 0);
    if (OrdersChoose(g, c))
        return;
    if (c->role == IDLE)
        return;
    int from = GameTile(c->x / 256, c->y / 256), best = -1, score = INT_MAX;
    if (c->role == GUARD)
    {
        c->job = JOB_GUARD;
        c->target = -1;
        return;
    }
    if (c->role == BUILDER || c->role == CRAFTER || c->role == FARMER)
    {
        for (int i = 0; i < g->buildingCount; i++)
        {
            Building *b = &g->buildings[i];
            if (!b->alive || b->owner != c->owner)
                continue;
            if (c->orderCount && c->orders[0].type == ORDER_WORK && c->orders[0].target.slot != i)
                continue;
            bool match =
                c->role == BUILDER
                    ? (!b->complete || (g->factions[c->owner].advancing && g->factions[c->owner].center == i))
                : c->role == CRAFTER ? ((b->type == WORKSHOP || b->type == SMELTER) && b->complete)
                                     : (b->type == FARM && b->complete && (b->crop == 0 || b->crop == 2));
            if (!match)
                continue;
            if (g->factions[c->owner].areaX[c->role] >= 0 &&
                Dist(b->x, b->y, g->factions[c->owner].areaX[c->role], g->factions[c->owner].areaY[c->role]) >
                    9)
                continue;
            if (c->role == CRAFTER && (b->type == SMELTER ? !GameCanAffordFor(g, c->owner, BRONZE_RECIPE.cost)
                                                          : !WorkshopHasWork(g, c->owner)))
                continue;
            int d = Dist(c->x / 256, c->y / 256, b->x, b->y);
            if (d >= score ||
                WorldAdjacent(g, c->owner, b->x, b->y, BUILDINGS[b->type].w, BUILDINGS[b->type].h, from) < 0)
                continue;
            best = i;
            score = d;
        }
        if (best < 0)
            return;
        c->target = best;
        c->work = 0;
        if (c->role == BUILDER)
            c->job = JOB_BUILD;
        else if (c->role == FARMER)
            c->job = g->buildings[best].crop == 2 ? JOB_HARVEST : JOB_PLANT;
        else if (g->factions[c->owner].research >= 0 && g->buildings[best].type == WORKSHOP)
            c->job = JOB_RESEARCH;
        else
        {
            WorkshopBeginCraft(g, c);
        }
        return;
    }
    int wanted = c->role == FORAGER          ? NODE_BERRIES
                 : c->role == WOODCUTTER     ? NODE_TREE
                 : c->role == MINER          ? NODE_ORE
                 : c->role == STONE_GATHERER ? NODE_STONE
                                             : NODE_GAME;
    if (c->role == MINER && !g->factions[c->owner].discovery[METALLURGY])
        return;
    for (int t = 0; t < TILE_COUNT; t++)
    {
        Tile *tile = &g->tiles[t];
        if (tile->node != wanted || tile->amount <= 0)
            continue;
        int x = t % MAP_SIZE, y = t / MAP_SIZE;
        if (g->factions[c->owner].areaX[c->role] >= 0 &&
            Dist(x, y, g->factions[c->owner].areaX[c->role], g->factions[c->owner].areaY[c->role]) > 9)
            continue;
        int d = Dist(c->x / 256, c->y / 256, x, y);
        if (d >= score)
            continue;
        if (WorldAdjacent(g, c->owner, x, y, 1, 1, from) < 0)
            continue;
        score = d;
        best = t;
    }
    if (best >= 0)
    {
        c->job = JOB_GATHER;
        c->target = best;
        c->work = 0;
    }
}

static void CitizenWork(GameState *g, Citizen *c)
{
    if (c->job == JOB_IDLE)
    {
        ChooseJob(g, c);
        if (c->job == JOB_IDLE)
        {
            c->retry = 20;
            return;
        }
    }
    if (c->job != JOB_GATHER && c->job != JOB_DELIVER && c->job != JOB_GUARD && c->job != JOB_MOVE &&
        c->job != JOB_IDLE &&
        (c->target < 0 || c->target >= g->buildingCount || !g->buildings[c->target].alive ||
         g->buildings[c->target].owner != c->owner))
    {
        EconomySettle(g, c->owner, c->recipe, 0);
        JobsReset(c);
        return;
    }
    if (c->pathIndex < c->pathLength)
    {
        WorldMoveCitizen(g, c);
        return;
    }
    int dest = Destination(g, c);
    if (dest < 0)
    {
        if (c->job != JOB_DELIVER)
            JobsReset(c);
        c->retry = 30;
        return;
    }
    if (c->x / 256 != dest % MAP_SIZE || c->y / 256 != dest / MAP_SIZE)
    {
        if (!WorldRoute(g, c, dest))
        {
            if (c->job != JOB_DELIVER)
                JobsReset(c);
            c->retry = 30;
        }
        return;
    }
    if (c->job == JOB_MOVE)
    {
        if (c->orderCount && (c->orders[0].type == ORDER_DEFEND ||
                              (c->orders[0].type == ORDER_ATTACK_MOVE && c->orderCount == 1)))
        {
            c->job = JOB_GUARD;
            return;
        }
        bool explicitOrder = c->orderCount > 0;
        OrdersPop(g, c);
        if (explicitOrder)
            c->role = c->unitType ? GUARD : IDLE;
        return;
    }
    if (c->job == JOB_REPAIR)
    {
        Building *b = &g->buildings[c->target];
        if (b->hp >= GameBuildingMaxHealth(g, b))
        {
            OrdersPop(g, c);
            return;
        }
        if (g->tick % 10 == 0 && g->factions[c->owner].stock[WOOD] > 0)
        {
            EconomySpend(g, c->owner, WOOD, 1);
            b->hp = Min(GameBuildingMaxHealth(g, b), b->hp + 100);
        }
        return;
    }
    if (c->job == JOB_DELIVER)
    {
        bool once = c->orderCount && c->orders[0].type == ORDER_DELIVER;
        EconomyDeliver(g, c);
        if (once)
            OrdersPop(g, c);
        return;
    }
    if (c->job == JOB_GUARD)
        return;
    c->work++;
    if (c->job == JOB_BUILD)
    {
        BuildingsWork(g, c);
        return;
    }
    if (c->job == JOB_RESEARCH)
    {
        WorkshopResearchWork(g, c);
        return;
    }
    if (c->job == JOB_CRAFT || c->job == JOB_SMELT)
    {
        WorkshopCraftWork(g, c);
        return;
    }
    if (c->job == JOB_PLANT || c->job == JOB_HARVEST)
    {
        FarmingWork(g, c);
        return;
    }
    if (c->job == JOB_GATHER)
    {
        EconomyGatherWork(g, c);
        return;
    }
}

void JobsUpdate(GameState *g)
{
    for (int priority = 2; priority >= 0; priority--)
        for (int i = 0; i < g->citizenCount; i++)
        {
            Citizen *c = &g->citizens[i];
            if ((c->role == IDLE ? 1 : g->factions[c->owner].priority[c->role]) != priority || !c->alive ||
                c->job == JOB_FIGHT || c->training >= 0)
                continue;
            if (c->retry > 0)
            {
                c->retry--;
                continue;
            }
            CitizenWork(g, c);
        }
}
