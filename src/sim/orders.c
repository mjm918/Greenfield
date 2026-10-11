#include "orders.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"
#include <string.h>
void OrdersClear(GameState *g, Citizen *c)
{
    EconomySettle(g, c->owner, c->recipe, 0);
    c->orderCount = 0;
    JobsReset(c);
}
void OrdersPop(GameState *g, Citizen *c)
{
    if (c->orderCount)
    {
        memmove(c->orders, c->orders + 1, (size_t)(--c->orderCount) * sizeof(UnitOrder));
    }
    EconomySettle(g, c->owner, c->recipe, 0);
    JobsReset(c);
}
bool OrdersApply(GameState *g, GameCommand cmd, char *r, int n)
{
    if (!GameCitizenValid(g, cmd.entity) || g->citizens[cmd.entity.slot].owner != cmd.owner)
        return Reject(r, n, "Select one of your living citizens.");
    Citizen *c = &g->citizens[cmd.entity.slot];
    UnitOrder o = cmd.order;
    if (c->training >= 0)
        return Reject(r, n, "Cancel training before issuing another order.");
    if (cmd.type == CMD_ROLE)
    {
        if (cmd.a < 0 || cmd.a > IDLE || c->unitType)
            return Reject(r, n, "Choose a civilian role.");
        OrdersClear(g, c);
        c->role = cmd.a;
        return true;
    }
    if (cmd.type == CMD_STOP)
    {
        OrdersClear(g, c);
        c->role = c->unitType ? GUARD : IDLE;
        return true;
    }
    if (o.type < 0 || o.type >= ORDER_COUNT || o.targetKind < TARGET_TILE || o.targetKind > TARGET_BUILDING)
        return Reject(r, n, "Invalid order.");
    if (GameTile(o.x, o.y) < 0)
        return Reject(r, n, "Choose a destination in the valley.");
    if (o.targetKind == TARGET_BUILDING && !GameBuildingValid(g, o.target))
        return Reject(r, n, "That building no longer exists.");
    if (o.targetKind == TARGET_CITIZEN && !GameCitizenValid(g, o.target))
        return Reject(r, n, "That citizen no longer exists.");
    bool hostile = o.type == ORDER_ATTACK || o.type == ORDER_PLUNDER;
    int targetOwner = o.targetKind == TARGET_BUILDING  ? g->buildings[o.target.slot].owner
                      : o.targetKind == TARGET_CITIZEN ? g->citizens[o.target.slot].owner
                                                       : cmd.owner;
    if (hostile && (targetOwner == cmd.owner || g->treaty || g->truceUntil > (int)g->tick))
        return Reject(r, n, "A treaty or truce forbids that attack.");
    if ((o.type == ORDER_BUILD || o.type == ORDER_REPAIR || o.type == ORDER_WORK) &&
        (o.targetKind != TARGET_BUILDING || targetOwner != cmd.owner || c->unitType))
        return Reject(r, n, "Order your civilian to a friendly building.");
    if (o.type == ORDER_GATHER &&
        (c->unitType || o.targetKind != TARGET_TILE || !g->tiles[GameTile(o.x, o.y)].node))
        return Reject(r, n, "Choose a natural resource for a civilian.");
    if (o.type == ORDER_GATHER && g->tiles[GameTile(o.x, o.y)].node == NODE_ORE &&
        !g->factions[c->owner].discovery[METALLURGY])
        return Reject(r, n, "Discover Metallurgy before mining ore.");
    if (o.type == ORDER_WORK &&
        (g->buildings[o.target.slot].type != FARM && g->buildings[o.target.slot].type != WORKSHOP &&
         g->buildings[o.target.slot].type != SMELTER))
        return Reject(r, n, "Work at a farm, workshop or smelter.");
    if (cmd.append && c->orderCount == MAX_ORDERS)
        return Reject(r, n, "The order queue is full.");
    if (!cmd.append)
        OrdersClear(g, c);
    c->orders[c->orderCount++] = o;
    return true;
}
bool OrdersChoose(GameState *g, Citizen *c)
{
    if (!c->orderCount)
        return false;
    UnitOrder *o = &c->orders[0];
    if ((o->targetKind == TARGET_CITIZEN && !GameCitizenValid(g, o->target)) ||
        (o->targetKind == TARGET_BUILDING && !GameBuildingValid(g, o->target)))
    {
        OrdersPop(g, c);
        return true;
    }
    switch (o->type)
    {
    case ORDER_GATHER:
    {
        Tile *t = &g->tiles[GameTile(o->x, o->y)];
        if (!t->node || !t->amount)
        {
            OrdersPop(g, c);
            c->role = IDLE;
            break;
        }
        c->role = t->node == NODE_TREE    ? WOODCUTTER
                  : t->node == NODE_STONE ? STONE_GATHERER
                  : t->node == NODE_GAME  ? HUNTER
                  : t->node == NODE_ORE   ? MINER
                                          : FORAGER;
        c->job = JOB_GATHER;
        c->target = GameTile(o->x, o->y);
        break;
    }
    case ORDER_BUILD:
    case ORDER_REPAIR:
    {
        Building *b = &g->buildings[o->target.slot];
        // Repairing an unfinished structure first resumes its reserved project.
        // Resolve this when executing, including queued orders and hut upgrades.
        bool build = o->type == ORDER_BUILD || !b->complete;
        if ((o->type == ORDER_BUILD && b->complete &&
             !(g->factions[c->owner].advancing && g->factions[c->owner].center == o->target.slot)) ||
            (!build && b->hp >= GameBuildingMaxHealth(g, b)))
        {
            OrdersPop(g, c);
            break;
        }
        c->role = BUILDER;
        c->job = build ? JOB_BUILD : JOB_REPAIR;
        c->target = o->target.slot;
        break;
    }
    case ORDER_WORK:
    {
        Building *b = &g->buildings[o->target.slot];
        c->role = b->type == FARM ? FARMER : CRAFTER;
        return false;
    }
    case ORDER_DELIVER:
        c->job = JOB_DELIVER;
        break;
    case ORDER_HOLD:
        c->job = JOB_GUARD;
        c->target = GameTile(o->x, o->y);
        break;
    case ORDER_ATTACK:
    case ORDER_PLUNDER:
        c->job = JOB_FIGHT;
        break;
    case ORDER_RETREAT:
    case ORDER_MOVE:
    case ORDER_ATTACK_MOVE:
    case ORDER_DEFEND:
        c->job = JOB_MOVE;
        c->target = GameTile(o->x, o->y);
        break;
    default:
        return false;
    }
    return true;
}

bool OrdersRally(GameState *g, GameCommand cmd, char *r, int n)
{
    UnitOrder o = cmd.order;
    if (o.type < ORDER_AUTO || o.type >= ORDER_COUNT || o.targetKind < TARGET_TILE ||
        o.targetKind > TARGET_BUILDING || GameTile(o.x, o.y) < 0 || GameTile(cmd.a, cmd.b) < 0)
        return Reject(r, n, "Choose a valid rally instruction.");
    if (o.targetKind == TARGET_CITIZEN || o.type == ORDER_ATTACK || o.type == ORDER_PLUNDER)
        return Reject(r, n, "New recruits need a safe rally instruction.");
    if (o.targetKind == TARGET_BUILDING &&
        (!GameBuildingValid(g, o.target) || g->buildings[o.target.slot].owner != cmd.owner))
        return Reject(r, n, "Choose one of your buildings.");
    if ((o.type == ORDER_WORK || o.type == ORDER_BUILD || o.type == ORDER_REPAIR) &&
        o.targetKind != TARGET_BUILDING)
        return Reject(r, n, "That rally instruction requires a building.");
    if (o.type == ORDER_WORK && g->buildings[o.target.slot].type != FARM &&
        g->buildings[o.target.slot].type != WORKSHOP && g->buildings[o.target.slot].type != SMELTER)
        return Reject(r, n, "Work at a farm, workshop or smelter.");
    if (o.type == ORDER_GATHER &&
        (o.targetKind != TARGET_TILE || !g->tiles[GameTile(o.x, o.y)].node ||
         (g->tiles[GameTile(o.x, o.y)].node == NODE_ORE && !g->factions[cmd.owner].discovery[METALLURGY])))
        return Reject(r, n, "Choose an unlocked resource.");
    Faction *f = &g->factions[cmd.owner];
    f->rallyX = cmd.a;
    f->rallyY = cmd.b;
    f->rallyOrder = o;
    return true;
}
