#include "citizens.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"
#include "orders.h"
#include "production.h"
#include <limits.h>
#include <string.h>
int GamePopulationFor(const GameState *g, int owner)
{
    int n = 0;
    for (int i = 0; i < g->citizenCount; i++)
        n += g->citizens[i].alive && g->citizens[i].owner == owner;
    return n;
}
int GameRoleCountFor(const GameState *g, int owner, int role)
{
    int n = 0;
    for (int i = 0; i < g->citizenCount; i++)
        n += g->citizens[i].alive && g->citizens[i].owner == owner && g->citizens[i].role == role;
    return n;
}
bool GameCitizenIsIdle(const GameState *g, int id)
{
    if (id < 0 || id >= g->citizenCount)
        return false;
    const Citizen *c = &g->citizens[id];
    return c->alive && c->unitType == CIVILIAN && c->training < 0 && c->job == JOB_IDLE &&
           c->pathIndex >= c->pathLength && !c->orderCount && !EconomyCargo(c);
}
int GameIdleCountFor(const GameState *g, int owner)
{
    int count = 0;
    for (int i = 0; i < g->citizenCount; i++)
        count += g->citizens[i].owner == owner && GameCitizenIsIdle(g, i);
    return count;
}
EntityRef GameCitizenRef(const GameState *g, int id)
{
    return (EntityRef){id, id >= 0 && id < g->citizenCount ? g->citizens[id].generation : 0};
}
EntityRef GameBuildingRef(const GameState *g, int id)
{
    return (EntityRef){id, id >= 0 && id < g->buildingCount ? g->buildings[id].generation : 0};
}
bool GameCitizenValid(const GameState *g, EntityRef r)
{
    return r.slot >= 0 && r.slot < g->citizenCount && g->citizens[r.slot].alive &&
           g->citizens[r.slot].generation == r.generation;
}
bool GameBuildingValid(const GameState *g, EntityRef r)
{
    return r.slot >= 0 && r.slot < g->buildingCount && g->buildings[r.slot].alive &&
           g->buildings[r.slot].generation == r.generation;
}
int CitizensAdd(GameState *g, int owner, int role)
{
    if (GamePopulationFor(g, owner) >= POPULATION_LIMIT)
        return -1;
    int id = 0;
    while (id < g->citizenCount && g->citizens[id].alive)
        id++;
    if (id >= MAX_CITIZENS)
        return -1;
    if (id == g->citizenCount)
        g->citizenCount++;
    Citizen *c = &g->citizens[id];
    int generation = c->generation + 1;
    memset(c, 0, sizeof(*c));
    c->generation = generation;
    c->alive = 1;
    c->owner = owner;
    c->role = role;
    c->hp = 1000;
    c->training = -1;
    int center = g->factions[owner].center;
    int x = owner ? 73 : 22, y = owner ? 73 : 22;
    if (center >= 0 && center < g->buildingCount)
    {
        x = g->buildings[center].x;
        y = g->buildings[center].y;
    }
    int best = -1, score = INT_MAX;
    for (int t = 0; t < TILE_COUNT; t++)
    {
        int d = Dist(t % MAP_SIZE, t / MAP_SIZE, x, y);
        if (d >= score || !GameWalkable(g, t % MAP_SIZE, t / MAP_SIZE))
            continue;
        bool occupied = false;
        for (int j = 0; j < g->citizenCount; j++)
            if (j != id && g->citizens[j].alive && g->citizens[j].x / 256 == t % MAP_SIZE &&
                g->citizens[j].y / 256 == t / MAP_SIZE)
                occupied = true;
        if (!occupied)
        {
            best = t;
            score = d;
        }
    }
    if (best < 0)
    {
        c->alive = 0;
        return -1;
    }
    c->x = best % MAP_SIZE * 256;
    c->y = best / MAP_SIZE * 256;
    c->hunger = (int)(SimRandom(g) % 300);
    JobsReset(c);
    return id;
}
void CitizensResolveDeaths(GameState *g)
{
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive || c->hp > 0)
            continue;
        EconomyDiscardCargo(g, c);
        for (int b = 0; b < g->buildingCount; b++)
            for (int q = g->buildings[b].queueCount - 1; q >= 0; q--)
            {
                Production *p = &g->buildings[b].queue[q];
                if (p->type != QUEUE_RECRUIT && p->citizen.slot == i &&
                    p->citizen.generation == c->generation)
                    ProductionCancel(g, &g->buildings[b], q, 2);
            }
        c->alive = 0;
        g->factions[c->owner].deaths++;
        if (!c->owner)
            GameEvent(g, "A citizen has died. Restore supplies or retreat from danger.");
    }
}
bool CitizensAssign(GameState *g, int owner, int role, int count, char *reason, int size)
{
    if (role < 0 || role >= ROLE_COUNT || count < 0 || count > GamePopulationFor(g, owner))
        return Reject(reason, size, "Invalid assignment.");
    int change = count - GameRoleCountFor(g, owner, role);
    if (change > GameRoleCountFor(g, owner, IDLE))
        return Reject(reason, size, "Select unassigned citizens first.");
    for (int i = 0; i < g->citizenCount && change; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive || c->owner != owner || c->unitType || c->training >= 0)
            continue;
        if (change > 0 && c->role == IDLE)
        {
            OrdersClear(g, c);
            c->role = role;
            change--;
        }
        else if (change < 0 && c->role == role)
        {
            OrdersClear(g, c);
            c->role = IDLE;
            change++;
        }
    }
    return true;
}
bool CitizensSetArea(GameState *g, int owner, int role, int x, int y, char *reason, int size)
{
    if (role < 0 || role >= ROLE_COUNT || ((x != -1 || y != -1) && GameTile(x, y) < 0))
        return Reject(reason, size, "Choose a tile in the valley.");
    g->factions[owner].areaX[role] = x;
    g->factions[owner].areaY[role] = y;
    for (int i = 0; i < g->citizenCount; i++)
        if (g->citizens[i].owner == owner && g->citizens[i].role == role && g->citizens[i].job != JOB_DELIVER)
            JobsReset(&g->citizens[i]);
    return true;
}
bool CitizensSetPriority(GameState *g, int owner, int role, int priority, char *reason, int size)
{
    if (role < 0 || role >= ROLE_COUNT || priority < 0 || priority > 2)
        return Reject(reason, size, "Invalid priority.");
    g->factions[owner].priority[role] = priority;
    return true;
}
