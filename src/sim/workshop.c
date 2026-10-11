#include "workshop.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"

bool WorkshopDiscover(GameState *g, int owner, int discovery, char *reason, int size)
{
    if (discovery < 0 || discovery >= DISCOVERY_COUNT || g->factions[owner].discovery[discovery] ||
        g->factions[owner].research != -1)
        return Reject(reason, size, "Finish the active discovery first, or choose an undiscovered idea.");
    if (!GameBuildingCountFor(g, owner, WORKSHOP, true) || !GameRoleCountFor(g, owner, CRAFTER))
        return Reject(reason, size, "Build a workshop and assign at least one crafter.");
    if (discovery == FARMING && g->tick < WINTER_END)
        return Reject(reason, size, "Farming can be discovered after the thaw.");
    if (!GameCanAffordFor(g, owner, DISCOVERIES[discovery].cost))
        return Reject(reason, size, "Not enough materials for this discovery.");
    if (discovery >= METALLURGY && g->factions[owner].age < FARMING_AGE)
        return Reject(reason, size, "Reach the Farming Village age first.");
    if (discovery == STONEWORK && g->factions[owner].age < BRONZE_AGE)
        return Reject(reason, size, "Stonework requires Bronze Age tools.");
    for (int i = 0; i < g->buildingCount; i++)
        if (g->buildings[i].alive && g->buildings[i].owner == owner && g->buildings[i].type == WORKSHOP &&
            g->buildings[i].complete)
        {
            g->factions[owner].researchBuilding = i;
            break;
        }
    EconomyReserve(g, owner, DISCOVERIES[discovery].cost, g->factions[owner].researchReserve);
    g->factions[owner].research = discovery;
    g->factions[owner].researchProgress = 0;
    return true;
}

void WorkshopResearchWork(GameState *g, Citizen *c)
{
    int owner = c->owner;
    if (c->target != g->factions[owner].researchBuilding)
    {
        JobsReset(c);
        return;
    }
    if (g->factions[owner].research < 0)
    {
        JobsReset(c);
        return;
    }
    g->factions[owner].researchProgress++;
    if (g->factions[owner].researchProgress >= DISCOVERIES[g->factions[owner].research].labor)
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "Discovery: %s.", DISCOVERIES[g->factions[owner].research].name);
        if (!owner)
            GameEvent(g, msg);
        g->factions[owner].discovery[g->factions[owner].research] = 1;
        EconomySettle(g, owner, g->factions[owner].researchReserve, 1);
        g->factions[owner].research = -1;
        g->factions[owner].researchProgress = 0;
    }
}

void WorkshopCraftWork(GameState *g, Citizen *c)
{
    const RecipeDef *recipe = c->job == JOB_SMELT ? &BRONZE_RECIPE : &TOOL_RECIPE;
    if (c->work >= recipe->labor)
    {
        EconomySettle(g, c->owner, c->recipe, 1);
        EconomyProduceBatch(g, c, recipe->resource, recipe->quantity);
        c->job = JOB_DELIVER;
        c->target = -1;
        c->work = 0;
    }
}

bool WorkshopHasWork(const GameState *g, int owner)
{
    return g->factions[owner].research >= 0 ||
           (g->factions[owner].stock[TOOLS] < 30 && GameCanAffordFor(g, owner, TOOL_RECIPE.cost));
}

void WorkshopBeginCraft(GameState *g, Citizen *c)
{
    bool smelt = g->buildings[c->target].type == SMELTER;
    if (EconomyReserve(g, c->owner, smelt ? BRONZE_RECIPE.cost : TOOL_RECIPE.cost, c->recipe))
        c->job = smelt ? JOB_SMELT : JOB_CRAFT;
    else
        JobsReset(c);
}
