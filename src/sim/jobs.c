#include "jobs.h"
#include "buildings.h"
#include "common.h"
#include "economy.h"
#include "farming.h"
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
    if (c->job == JOB_GATHER && c->target >= 0 && c->target < TILE_COUNT)
        return WorldAdjacent(g, c->target % MAP_SIZE, c->target / MAP_SIZE, 1, 1, from);
    if (c->job == JOB_GUARD)
        return GameTile(g->rallyX, g->rallyY);
    if (c->target >= 0 && c->target < g->buildingCount)
    {
        Building *b = &g->buildings[c->target];
        return WorldAdjacent(g, b->x, b->y, BUILDINGS[b->type].w, BUILDINGS[b->type].h, from);
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
            bool match = c->role == BUILDER ? !b->complete
                         : c->role == CRAFTER
                             ? (b->type == WORKSHOP && b->complete)
                             : (b->type == FARM && b->complete && (b->crop == 0 || b->crop == 2));
            if (!match)
                continue;
            if (g->areaX[c->role] >= 0 && Dist(b->x, b->y, g->areaX[c->role], g->areaY[c->role]) > 9)
                continue;
            if (c->role == CRAFTER && !WorkshopHasWork(g))
                continue;
            int d = Dist(c->x / 256, c->y / 256, b->x, b->y);
            if (d >= score || WorldAdjacent(g, b->x, b->y, BUILDINGS[b->type].w, BUILDINGS[b->type].h, from) < 0)
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
        else if (g->research >= 0)
            c->job = JOB_RESEARCH;
        else
        {
            WorkshopBeginCraft(g, c);
        }
        return;
    }
    int wanted = c->role == FORAGER          ? NODE_BERRIES
                 : c->role == WOODCUTTER     ? NODE_TREE
                 : c->role == STONE_GATHERER ? NODE_STONE
                                             : NODE_GAME;
    for (int t = 0; t < TILE_COUNT; t++)
    {
        Tile *tile = &g->tiles[t];
        if (tile->node != wanted || tile->amount <= 0)
            continue;
        int x = t % MAP_SIZE, y = t / MAP_SIZE;
        if (g->areaX[c->role] >= 0 && Dist(x, y, g->areaX[c->role], g->areaY[c->role]) > 9)
            continue;
        int d = Dist(c->x / 256, c->y / 256, x, y);
        if (d >= score)
            continue;
        if (WorldAdjacent(g, x, y, 1, 1, from) < 0)
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
    if (c->job == JOB_GUARD && c->target >= 0)
        c->target = -1;
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
    if (c->job == JOB_DELIVER)
    {
        EconomyDeliver(g, c);
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
    if (c->job == JOB_CRAFT)
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
            if ((c->role == IDLE ? 1 : g->priority[c->role]) != priority || !c->alive ||
                c->job == JOB_FIGHT || (c->role == GUARD && c->hp <= 250))
                continue;
            if (c->retry > 0)
            {
                c->retry--;
                continue;
            }
            CitizenWork(g, c);
        }
}
