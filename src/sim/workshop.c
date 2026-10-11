#include "workshop.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"

bool WorkshopDiscover(GameState *g, int discovery, char *reason, int size)
{
    if (discovery < 0 || discovery >= DISCOVERY_COUNT || g->discovery[discovery] || g->research != -1)
        return Reject(reason, size, "Finish the active discovery first, or choose an undiscovered idea.");
    if (!GameBuildingCount(g, WORKSHOP, true) || !GameRoleCount(g, CRAFTER))
        return Reject(reason, size, "Build a workshop and assign at least one crafter.");
    if (discovery == FARMING && g->tick < WINTER_END)
        return Reject(reason, size, "Farming can be discovered after the thaw.");
    if (!GameCanAfford(g, DISCOVERIES[discovery].cost))
        return Reject(reason, size, "Not enough materials for this discovery.");
    EconomyPay(g, DISCOVERIES[discovery].cost);
    g->research = discovery;
    g->researchProgress = 0;
    return true;
}

void WorkshopResearchWork(GameState *g, Citizen *c)
{
    if (g->research < 0)
    {
        JobsReset(c);
        return;
    }
    g->researchProgress++;
    if (g->researchProgress >= DISCOVERIES[g->research].labor)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Discovery: %s.", DISCOVERIES[g->research].name);
        GameEvent(g, msg);
        g->discovery[g->research] = 1;
        g->research = -1;
        g->researchProgress = 0;
    }
}

void WorkshopCraftWork(GameState *g, Citizen *c)
{
    if (c->work >= 300)
    {
        EconomyProduceBatch(g, c, TOOLS, 3);
        c->job = JOB_DELIVER;
        c->target = -1;
        c->work = 0;
    }
}

bool WorkshopHasWork(const GameState *g)
{
    return g->research >= 0 || (g->stock[TOOLS] < 30 && g->stock[WOOD] >= 2 && g->stock[STONE] >= 1);
}

void WorkshopBeginCraft(GameState *g, Citizen *c)
{
    // Materials are reserved when the job starts, as in v1, before walking.
    EconomySpend(g, WOOD, 2);
    EconomySpend(g, STONE, 1);
    c->job = JOB_CRAFT;
}
