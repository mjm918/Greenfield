#include "sim/ai.h"
#include "sim/buildings.h"
#include "sim/citizens.h"
#include "sim/combat.h"
#include "sim/common.h"
#include "sim/economy.h"
#include "sim/farming.h"
#include "sim/jobs.h"
#include "sim/orders.h"
#include "sim/production.h"
#include "sim/progression.h"
#include "sim/rivals.h"
#include "sim/survival.h"
#include "sim/workshop.h"
#include "sim/world.h"
#include "sim/wildlife.h"
#include <string.h>
uint32_t SimRandom(GameState *g)
{
    g->random ^= g->random << 13;
    g->random ^= g->random >> 17;
    g->random ^= g->random << 5;
    return g->random;
}
void GameEvent(GameState *g, const char *text)
{
    for (int i = 7; i > 0; i--)
        memcpy(g->events[i], g->events[i - 1], 128);
    snprintf(g->events[0], 128, "%s", text);
    g->eventCount = Min(8, g->eventCount + 1);
}
void GameInit(GameState *g)
{
    memset(g, 0, sizeof(*g));
    g->random = 0x47f1e1d;
    WorldInit(g);
    WildlifeInit(g);
    const int supplies[RESOURCE_COUNT] = {60, 100, 40, 24, 15};
    for (int o = 0; o < 2; o++)
    {
        Faction *f = &g->factions[o];
        f->research = f->researchBuilding = f->center = -1;
        f->rallyX = o ? 73 : 22;
        f->rallyY = o ? 73 : 22;
        f->warmth = 65;
        f->coldWarning = 1;
        f->aiEnabled = o;
        f->nextRaid = 18000;
        for (int r = 0; r < RESOURCE_COUNT; r++)
            f->stock[r] = f->initial[r] = supplies[r];
        for (int r = 0; r < ROLE_COUNT; r++)
        {
            f->areaX[r] = f->areaY[r] = -1;
            f->priority[r] = 1;
        }
        f->center = BuildingsAdd(g, o, HEARTH, o ? 72 : 21, o ? 73 : 20, true);
        BuildingsAdd(g, o, STOCKPILE, o ? 70 : 23, o ? 70 : 23, true);
        const int roles[] = {BUILDER,    FORAGER,        FORAGER, WOODCUTTER,
                             WOODCUTTER, STONE_GATHERER, HUNTER,  HUNTER};
        for (int i = 0; i < 8; i++)
            CitizensAdd(g, o, roles[i]);
    }
    GameEvent(g, "Eight survivors arrive. Build shelter, gather food, and keep the hearth supplied.");
}
bool GameCommandApply(GameState *g, GameCommand cmd, char *r, int n)
{
    if (cmd.owner < 0 || cmd.owner >= FACTION_COUNT || g->factions[cmd.owner].defeated || g->lost)
        return Reject(r, n, "This faction cannot issue commands.");
    Faction *f = &g->factions[cmd.owner];
    bool ok = false;
    switch (cmd.type)
    {
    case CMD_BUILD:
        ok = BuildingsPlace(g, cmd.owner, cmd.a, cmd.b, cmd.c, r, n);
        break;
    case CMD_ASSIGN:
        ok = CitizensAssign(g, cmd.owner, cmd.a, cmd.b, r, n);
        break;
    case CMD_AREA:
        ok = CitizensSetArea(g, cmd.owner, cmd.a, cmd.b, cmd.c, r, n);
        break;
    case CMD_PRIORITY:
        ok = CitizensSetPriority(g, cmd.owner, cmd.a, cmd.b, r, n);
        break;
    case CMD_RESEARCH:
        ok = WorkshopDiscover(g, cmd.owner, cmd.a, r, n);
        break;
    case CMD_UPGRADE:
        ok = BuildingsUpgrade(g, cmd.owner, cmd.a, r, n);
        break;
    case CMD_ADVANCE:
        ok = ProgressionAdvance(g, cmd.owner, r, n);
        break;
    case CMD_ORDER:
    case CMD_STOP:
    case CMD_ROLE:
        ok = OrdersApply(g, cmd, r, n);
        break;
    case CMD_RECRUIT:
    case CMD_TRAIN:
    case CMD_EQUIP:
    case CMD_DEMOBILIZE:
        ok = ProductionCommand(g, cmd, r, n);
        break;
    case CMD_CANCEL:
        if (cmd.a == -1)
        {
            EconomySettle(g, cmd.owner, f->researchReserve, 0);
            f->research = -1;
            f->researchProgress = 0;
            ok = true;
        }
        else if (cmd.a == -2)
        {
            EconomySettle(g, cmd.owner, f->advanceReserve, 0);
            f->advancing = 0;
            f->advanceProgress = 0;
            ok = true;
        }
        else if (cmd.a == -3 && GameBuildingValid(g, cmd.entity) &&
                 g->buildings[cmd.entity.slot].owner == cmd.owner && !g->buildings[cmd.entity.slot].complete)
        {
            Building *b = &g->buildings[cmd.entity.slot];
            EconomySettle(g, cmd.owner, b->reserve, 0);
            if (b->type == SHELTER && b->upgraded)
            {
                b->complete = 1;
                b->upgraded = 0;
                b->progress = BUILDINGS[SHELTER].labor;
            }
            else
                BuildingsDestroy(g, cmd.entity.slot);
            ok = true;
        }
        else
            ok = ProductionCommand(g, cmd, r, n);
        break;
    case CMD_TRUCE:
        if (!cmd.owner)
            ok = RivalsOfferTruce(g, r, n);
        break;
    case CMD_TREATY:
        if (!cmd.owner)
            ok = RivalsOfferTreaty(g, r, n);
        break;
    case CMD_RALLY:
        ok = OrdersRally(g, cmd, r, n);
        break;
    default:
        return Reject(r, n, "Unknown command.");
    }
    if (ok && r && n > 0)
        snprintf(r, (size_t)n, "Done.");
    return ok;
}
void GameTick(GameState *g)
{
    if (g->lost)
        return;
    g->tick++;
    SurvivalUpdate(g);
    CitizensResolveDeaths(g);
    CombatUpdate(g);
    CitizensResolveDeaths(g);
    RivalsUpdate(g);
    FarmingUpdate(g);
    WildlifeUpdate(g);
    JobsUpdate(g);
    ProductionUpdate(g);
    AiUpdate(g);
    ProgressionUpdate(g);
}
void GameAdvance(GameState *g, double seconds, int speed, double *accumulator)
{
    if ((speed != 0 && speed != 1 && speed != 2 && speed != 4) || !(seconds >= 0 && seconds <= 60))
        return;
    *accumulator += seconds * speed;
    while (*accumulator >= .1 - 1e-10)
    {
        GameTick(g);
        *accumulator -= .1;
    }
    if (*accumulator < 0)
        *accumulator = 0;
}
