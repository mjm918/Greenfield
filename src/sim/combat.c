#include "combat.h"
#include "buildings.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"
#include "orders.h"
#include "world.h"
static bool Hostile(const GameState *g)
{
    return !g->treaty && g->truceUntil <= (int)g->tick && !g->factions[1].defeated;
}
static int Damage(const Citizen *a, const Citizen *b)
{
    int damage = UNITS[a->unitType].damage;
    if (a->equipped)
        damage += 25;
    if ((a->unitType == SPEARMAN && b->unitType == SLINGER) ||
        (a->unitType == SLINGER && b->unitType == AXEMAN) ||
        (a->unitType == AXEMAN && b->unitType == SPEARMAN))
        damage *= 2;
    return Max(5, damage - UNITS[b->unitType].armor - (b->equipped ? 15 : 0));
}
static void Approach(GameState *g, Citizen *c, int x, int y, int w, int h)
{
    if (c->pathIndex < c->pathLength)
        WorldMoveCitizen(g, c);
    else
    {
        int t = WorldAdjacent(g, c->owner, x, y, w, h, GameTile(c->x / 256, c->y / 256));
        WorldRoute(g, c, t);
    }
}
void CombatUpdate(GameState *g)
{
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive || c->hp <= 0)
            continue;
        if (c->attackCooldown)
            c->attackCooldown--;
        if (g->factions[c->owner].defeated)
        {
            EconomyDiscardCargo(g, c);
            c->alive = 0;
            continue;
        }
        if (!Hostile(g))
        {
            if (c->job == JOB_FIGHT)
            {
                OrdersClear(g, c);
                c->role = c->unitType ? GUARD : IDLE;
            }
            continue;
        }
        if (c->training >= 0)
            continue;
        UnitOrder *o = c->orderCount ? &c->orders[0] : NULL;
        if (c->unitType && c->hp <= 250 && (!o || o->type != ORDER_RETREAT))
        {
            UnitOrder retreat = {
                .type = ORDER_RETREAT, .x = g->factions[c->owner].rallyX, .y = g->factions[c->owner].rallyY};
            OrdersClear(g, c);
            c->orders[0] = retreat;
            c->orderCount = 1;
            continue;
        }
        if (o && (o->type == ORDER_MOVE || o->type == ORDER_RETREAT || o->type == ORDER_DELIVER))
            continue;
        Citizen *enemy = NULL;
        Building *building = NULL;
        if (o && (o->type == ORDER_ATTACK || o->type == ORDER_PLUNDER))
        {
            if (o->targetKind == TARGET_CITIZEN && GameCitizenValid(g, o->target))
                enemy = &g->citizens[o->target.slot];
            else if (o->targetKind == TARGET_BUILDING && GameBuildingValid(g, o->target))
                building = &g->buildings[o->target.slot];
            else
            {
                OrdersPop(g, c);
                continue;
            }
        }
        else
        {
            int best = c->unitType ? 7 : 2;
            if (o && o->type == ORDER_HOLD)
                best = c->unitType == SLINGER ? 5 : 2;
            for (int j = 0; j < g->citizenCount; j++)
            {
                Citizen *t = &g->citizens[j];
                if (!t->alive || t->hp <= 0 || t->owner == c->owner)
                    continue;
                int d = Dist(c->x / 256, c->y / 256, t->x / 256, t->y / 256);
                if (d < best)
                {
                    best = d;
                    enemy = t;
                }
            }
        }
        if (!enemy && !building)
        {
            if (c->job == JOB_FIGHT)
                JobsReset(c);
            continue;
        }
        int range = UNITS[c->unitType].range;
        int x = enemy ? enemy->x / 256 : building->x, y = enemy ? enemy->y / 256 : building->y;
        int w = building ? BUILDINGS[building->type].w : 1, h = building ? BUILDINGS[building->type].h : 1;
        int dx = Max(0, Max(x - c->x / 256, c->x / 256 - (x + w - 1))),
            dy = Max(0, Max(y - c->y / 256, c->y / 256 - (y + h - 1)));
        c->job = JOB_FIGHT;
        c->facing = GameFacing((enemy ? enemy->x : x * 256 + (w - 1) * 128) - c->x,
                               (enemy ? enemy->y : y * 256 + (h - 1) * 128) - c->y, c->facing);
        if (dx + dy > range)
        {
            if (!o || o->type != ORDER_HOLD)
                Approach(g, c, x, y, w, h);
            continue;
        }
        c->pathLength = c->pathIndex = 0;
        if (c->attackCooldown)
            continue;
        c->attackCooldown = UNITS[c->unitType].cooldown;
        g->lastHostileTick = (int)g->tick;
        if (building && o && o->type == ORDER_PLUNDER)
        {
            if (building->type != STOCKPILE && building->type != GRANARY && building->type != HEARTH)
            {
                OrdersPop(g, c);
                continue;
            }
            EconomyLoot(g, c, building->owner);
            OrdersClear(g, c);
            c->job = JOB_DELIVER;
            c->role = GUARD;
            g->factions[c->owner].raidResolved = 1;
            continue;
        }
        if (enemy)
            enemy->hp -= Damage(c, enemy);
        else
        {
            building->hp -= UNITS[c->unitType].buildingDamage;
            if (building->hp <= 0)
                BuildingsDestroy(g, (int)(building - g->buildings));
        }
    }
}
