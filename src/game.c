#include "sim/buildings.h"
#include "sim/citizens.h"
#include "sim/common.h"
#include "sim/farming.h"
#include "sim/jobs.h"
#include "sim/rivals.h"
#include "sim/survival.h"
#include "sim/workshop.h"
#include "sim/world.h"
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

int GameStage(const GameState *g) { return g->discovery[FARMING] ? 2 : g->winterSurvived ? 1 : 0; }

void GameInit(GameState *g)
{
    memset(g, 0, sizeof(*g));
    g->random = 0x47f1e1d;
    g->research = -1;
    g->nextArrival = ARRIVAL_FIRST;
    g->nextRaid = 11400;
    g->rallyX = 26;
    g->rallyY = 20;
    g->warmth = 65;
    g->coldWarning = 1;
    const int supplies[] = {60, 100, 40, 24, 15};
    for (int r = 0; r < RESOURCE_COUNT; r++)
        g->stock[r] = g->initial[r] = supplies[r];
    for (int r = 0; r < ROLE_COUNT; r++)
    {
        g->areaX[r] = -1;
        g->areaY[r] = -1;
        g->priority[r] = 1;
    }
    WorldInit(g);
    BuildingsAdd(g, HEARTH, 21, 20, true);
    BuildingsAdd(g, STOCKPILE, 23, 23, true);
    int roles[] = {BUILDER, BUILDER, FORAGER, FORAGER, WOODCUTTER, STONE_GATHERER, HUNTER, IDLE};
    for (int i = 0; i < 8; i++)
        CitizensAdd(g, roles[i]);
    GameEvent(g, "Eight survivors reach the valley. Build two shelters before the cold deepens.");
}

bool GameCommandApply(GameState *g, GameCommand cmd, char *reason, int size)
{
    if (g->lost)
        return Reject(reason, size, "The tribe is lost. Start again or load a saved game.");
    bool ok;
    switch (cmd.type)
    {
    case CMD_BUILD:
        ok = BuildingsPlace(g, cmd.a, cmd.b, cmd.c, reason, size);
        break;
    case CMD_ASSIGN:
        ok = CitizensAssign(g, cmd.a, cmd.b, reason, size);
        break;
    case CMD_AREA:
        ok = CitizensSetArea(g, cmd.a, cmd.b, cmd.c, reason, size);
        break;
    case CMD_PRIORITY:
        ok = CitizensSetPriority(g, cmd.a, cmd.b, reason, size);
        break;
    case CMD_RESEARCH:
        ok = WorkshopDiscover(g, cmd.a, reason, size);
        break;
    case CMD_UPGRADE:
        ok = BuildingsUpgrade(g, cmd.a, reason, size);
        break;
    case CMD_TRUCE:
        ok = RivalsOfferTruce(g, reason, size);
        break;
    case CMD_RALLY:
        ok = RivalsSetRally(g, cmd.a, cmd.b, reason, size);
        break;
    default:
        return Reject(reason, size, "Unknown command.");
    }
    if (!ok)
        return false;
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Done.");
    return true;
}

// Phase order is gameplay: resolve deaths after survival and again after combat,
// before work, arrivals, and objectives. See simulation_baseline before changing it.
void GameTick(GameState *g)
{
    if (g->lost)
        return;
    g->tick++;
    SurvivalUpdate(g);
    CitizensResolveDeaths(g);
    RivalsUpdate(g);
    CitizensResolveDeaths(g);
    FarmingUpdate(g);
    JobsUpdate(g);
    CitizensUpdate(g);
    int pop = GamePopulation(g);
    if (!pop)
    {
        g->lost = 1;
        GameEvent(g, "The last hearth has gone quiet. Load your save or begin again.");
    }
    if (!g->won && pop >= 20 && GameHousing(g) >= pop && g->winterSurvived && g->harvestDelivered &&
        GameFoodDaysTenths(g) >= 30 && g->raidResolved)
    {
        g->won = 1;
        GameEvent(g, "A village is born. Your people have a home, a harvest and a future.");
    }
}
void GameAdvance(GameState *g, double seconds, int speed, double *accumulator)
{
    if (speed != 0 && speed != 1 && speed != 2 && speed != 4)
        return;
    if (!(seconds >= 0 && seconds <= 60))
        return;
    *accumulator += seconds * speed;
    while (*accumulator >= 0.1 - 1e-10)
    {
        GameTick(g);
        *accumulator -= 0.1;
    }
    if (*accumulator < 0)
        *accumulator = 0;
}
