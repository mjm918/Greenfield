#include "citizens.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"
#include <limits.h>
#include <string.h>

int GamePopulation(const GameState *g)
{
    int n = 0;
    for (int i = 0; i < g->citizenCount; i++)
        n += g->citizens[i].alive;
    return n;
}

int GameRoleCount(const GameState *g, int role)
{
    int n = 0;
    for (int i = 0; i < g->citizenCount; i++)
        n += g->citizens[i].alive && g->citizens[i].role == role;
    return n;
}

void CitizensAdd(GameState *g, int role)
{
    if (g->citizenCount >= MAX_CITIZENS)
        return;
    Citizen *c = &g->citizens[g->citizenCount++];
    memset(c, 0, sizeof(*c));
    c->alive = 1;
    c->hp = 1000;
    c->role = role;
    c->x = (20 + (g->citizenCount % 3)) * 256;
    c->y = (23 + (g->citizenCount % 2)) * 256;
    if (!GameWalkable(g, c->x / 256, c->y / 256))
    {
        int distance = INT_MAX, tile = -1;
        for (int t = 0; t < TILE_COUNT; t++)
        {
            int d = Dist(t % MAP_SIZE, t / MAP_SIZE, c->x / 256, c->y / 256);
            if (d < distance && GameWalkable(g, t % MAP_SIZE, t / MAP_SIZE))
            {
                tile = t;
                distance = d;
            }
        }
        if (tile >= 0)
        {
            c->x = tile % MAP_SIZE * 256;
            c->y = tile / MAP_SIZE * 256;
        }
    }
    c->hunger = (int)(SimRandom(g) % 300);
    JobsReset(c);
}

void CitizensResolveDeaths(GameState *g)
{
    // Both survival and combat use this one inventory/death transition. Run it
    // before arrivals and victory so zero-health citizens cannot win or revive.
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive || c->hp > 0)
            continue;
        EconomyDiscardCargo(g, c);
        c->alive = 0;
        g->deaths++;
        GameEvent(g, "A citizen has died. Restore food and warmth to protect the tribe.");
    }
}

void CitizensUpdate(GameState *g)
{
    if ((int)g->tick < g->nextArrival)
        return;
    int pop = GamePopulation(g);
    if (!pop)
        return;
    if (pop <= MAX_CITIZENS - 2 && g->citizenCount <= MAX_CITIZENS - 2 && GameHousing(g) >= pop + 2 &&
        g->stock[FOOD] >= pop * 3)
    {
        CitizensAdd(g, IDLE);
        CitizensAdd(g, IDLE);
        g->arrivals += 2;
        g->nextArrival = (int)g->tick + ARRIVAL_INTERVAL;
        GameEvent(g, "Two travelers join the tribe. Assign their work in the workforce panel.");
    }
    else
        g->nextArrival = (int)g->tick + 600;
}

bool CitizensAssign(GameState *g, int role, int count, char *reason, int size)
{
    if (role < 0 || role >= ROLE_COUNT || count < 0 || count > GamePopulation(g))
        return Reject(reason, size, "Invalid worker assignment.");
    int current = GameRoleCount(g, role), change = count - current;
    if (change > GameRoleCount(g, IDLE))
        return Reject(reason, size, "No unassigned citizens. Reduce another role first.");
    for (int i = 0; i < g->citizenCount && change; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive)
            continue;
        if (change > 0 && c->role == IDLE)
        {
            c->role = role;
            change--;
        }
        else if (change < 0 && c->role == role)
        {
            c->role = IDLE;
            change++;
        }
        else
            continue;
        int cargo = EconomyCargo(c);
        JobsReset(c);
        if (cargo)
            c->job = JOB_DELIVER;
    }
    return true;
}

bool CitizensSetArea(GameState *g, int role, int x, int y, char *reason, int size)
{
    if (role < 0 || role >= ROLE_COUNT || ((x != -1 || y != -1) && GameTile(x, y) < 0))
        return Reject(reason, size, "Choose a tile in the valley.");
    g->areaX[role] = x;
    g->areaY[role] = y;
    for (int i = 0; i < g->citizenCount; i++)
        if (g->citizens[i].role == role && g->citizens[i].job != JOB_DELIVER)
            JobsReset(&g->citizens[i]);
    return true;
}

bool CitizensSetPriority(GameState *g, int role, int priority, char *reason, int size)
{
    if (role < 0 || role >= ROLE_COUNT || priority < 0 || priority > 2)
        return Reject(reason, size, "Priority must be low, normal or high.");
    g->priority[role] = priority;
    return true;
}
