#include "rivals.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"
#include "world.h"
#include <limits.h>

static void MoveRaider(GameState *g, Raider *r, int tx, int ty)
{
    int from = GameTile(r->x / 256, r->y / 256), path[MAX_PATH],
        n = GameFindPath(g, from, GameTile(tx, ty), path, MAX_PATH);
    if (n <= 0)
        return;
    int xx = path[0] % MAP_SIZE * 256, yy = path[0] / MAP_SIZE * 256;
    // Align to the current tile before changing axis, avoiding corner cutting.
    int step = 32;
    if (r->x != xx)
        r->x += r->x < xx ? Min(step, xx - r->x) : -Min(step, r->x - xx);
    else if (r->y != yy)
        r->y += r->y < yy ? Min(step, yy - r->y) : -Min(step, r->y - yy);
}

void RivalsUpdate(GameState *g)
{
    if (g->truceUntil > 0 && g->tick == (uint64_t)g->truceUntil)
        GameEvent(g, "The truce has expired. Another raid may approach in two minutes.");
    if (!g->raidActive && g->truceUntil <= (int)g->tick)
    {
        if ((int)g->tick >= g->nextRaid - 900 && !g->raidWarning)
        {
            g->raidWarning = 1;
            GameEvent(g, "Scouts warn of a raid in 90 seconds. Rally guards or offer a truce.");
        }
        if ((int)g->tick >= g->nextRaid)
        {
            g->raidActive = 1;
            g->raidLosses = 0;
            g->raidLoot = 0;
            g->raidWarning = 0;
            g->raidCount++;
            int count = Min(5, 2 + g->raidCount);
            for (int i = 0; i < count; i++)
                g->raiders[i] = (Raider){1, (40 + i % 2) * 256, (8 + i) * 256, 650, 0, 0, 0};
            GameEvent(g, "Raiders enter the valley. They seek food, then retreat.");
        }
    }
    int active = 0;
    for (int i = 0; i < MAX_RAIDERS; i++)
    {
        Raider *r = &g->raiders[i];
        if (!r->active)
            continue;
        active++;
        // A raid is bounded even if later construction encloses a retreating raider.
        // At six minutes, anyone unable to leave surrenders rather than blocking
        // future rival events forever.
        if ((int)g->tick > g->nextRaid + 3600)
        {
            r->active = 0;
            if (r->hp >= 220)
                g->raidLosses++;
            continue;
        }
        if (r->cooldown > 0)
            r->cooldown--;
        if (r->hp <= 0)
        {
            r->active = 0;
            g->raidLosses++;
            continue;
        }
        if (r->hp < 220 && !r->retreat)
        {
            r->retreat = 1;
            g->raidLosses++;
        }
        if (g->raidLosses >= 2 || g->raidLoot >= 18 || g->truceUntil > (int)g->tick)
            r->retreat = 1;
        if (r->retreat)
        {
            MoveRaider(g, r, 40, 8);
            if (Dist(r->x / 256, r->y / 256, 40, 8) < 2)
                r->active = 0;
            continue;
        }
        int foe = -1, d = INT_MAX;
        for (int j = 0; j < g->citizenCount; j++)
        {
            Citizen *c = &g->citizens[j];
            if (!c->alive || c->role != GUARD)
                continue;
            int dd = Dist(r->x, r->y, c->x, c->y);
            if (dd < d)
            {
                d = dd;
                foe = j;
            }
        }
        if (foe >= 0 && d < 400)
        {
            if (!r->cooldown)
            {
                g->citizens[foe].hp -= 45;
                r->cooldown = 16;
            }
            continue;
        }
        int destination = -1;
        for (int b = 0; b < g->buildingCount; b++)
            if (g->buildings[b].type == STOCKPILE && g->buildings[b].complete)
            {
                destination = b;
                break;
            }
        if (destination >= 0)
        {
            Building *b = &g->buildings[destination];
            if (Dist(r->x / 256, r->y / 256, b->x, b->y) <= 2)
            {
                int n = Min(6, Min(g->stock[FOOD], 18 - g->raidLoot));
                EconomySteal(g, FOOD, n);
                g->raidLoot += n;
                r->stolen += n;
                r->retreat = 1;
            }
            else
                MoveRaider(g, r, b->x, b->y);
        }
        if ((int)g->tick > g->nextRaid + 2400)
            r->retreat = 1;
    }
    for (int j = 0; j < g->citizenCount; j++)
    {
        Citizen *c = &g->citizens[j];
        if (!c->alive || c->role != GUARD)
            continue;
        if (c->attackCooldown)
            c->attackCooldown--;
        int foe = -1, d = INT_MAX;
        for (int i = 0; i < MAX_RAIDERS; i++)
        {
            Raider *r = &g->raiders[i];
            if (!r->active || r->retreat)
                continue;
            int dd = Dist(c->x, c->y, r->x, r->y);
            if (dd < d && Dist(g->rallyX, g->rallyY, r->x / 256, r->y / 256) < 16)
            {
                d = dd;
                foe = i;
            }
        }
        if (foe >= 0 && c->hp > 250)
        {
            c->job = JOB_FIGHT;
            Raider *r = &g->raiders[foe];
            if (d < 450)
            {
                if (!c->attackCooldown)
                {
                    r->hp -= 95;
                    c->attackCooldown = 12;
                }
            }
            else
            {
                if (c->pathIndex >= c->pathLength || g->tick % 10 == 0)
                    WorldRoute(g, c, GameTile(r->x / 256, r->y / 256));
                WorldMoveCitizen(g, c);
            }
        }
        else
        {
            if (c->job == JOB_FIGHT)
                JobsReset(c);
            if (c->hp <= 250)
            {
                int home = GameTile(22, 23);
                if (c->pathIndex >= c->pathLength)
                    WorldRoute(g, c, home);
                WorldMoveCitizen(g, c);
            }
        }
    }
    if (g->raidActive && !active)
    {
        g->raidActive = 0;
        if (g->raidLosses >= 2 || g->raidLoot == 0)
        {
            g->raidResolved = 1;
            GameEvent(g, "The raid is over and your supplies are protected. The tribe stands together.");
        }
        else
            GameEvent(g, "The raiders retreat. Supplies lost are shown in the chronicle.");
        g->nextRaid = g->truceUntil > (int)g->tick ? g->truceUntil + 1200 : (int)g->tick + 6600;
    }
}

bool RivalsOfferTruce(GameState *g, char *reason, int size)
{
    if (g->truceUntil > (int)g->tick)
        return Reject(reason, size, "The truce is already active.");
    if (g->stock[FOOD] < 40)
        return Reject(reason, size, "A truce costs 40 stored food.");
    EconomySpend(g, FOOD, 40);
    g->truceUntil = (int)g->tick + 7200;
    g->raidResolved = 1;
    g->raidWarning = 0;
    // During an active raid this is its start deadline, used by the retreat bound.
    // Schedule the next threat once the last current raider has left.
    if (!g->raidActive)
        g->nextRaid = g->truceUntil + 1200;
    for (int i = 0; i < MAX_RAIDERS; i++)
        if (g->raiders[i].active)
            g->raiders[i].retreat = 1;
    GameEvent(g, "The rival camp accepts 40 food. A twelve-minute truce begins.");
    return true;
}

bool RivalsSetRally(GameState *g, int x, int y, char *reason, int size)
{
    if (!GameWalkable(g, x, y))
        return Reject(reason, size, "Choose a walkable rally tile.");
    g->rallyX = x;
    g->rallyY = y;
    return true;
}
