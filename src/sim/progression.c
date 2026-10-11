#include "progression.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"
bool GameCanAdvance(const GameState *g, int owner, char *r, int n)
{
    if (owner < 0 || owner >= FACTION_COUNT)
        return Reject(r, n, "Unknown faction.");
    const Faction *f = &g->factions[owner];
    if (f->age >= BRONZE_AGE || f->advancing)
        return Reject(r, n, "No advancement available.");
    if (!g->winterSurvived)
        return Reject(r, n, "Survive the opening winter first.");
    int pop = GamePopulationFor(g, owner);
    if (pop < AGES[f->age + 1].population || GameHousingFor(g, owner) < pop)
        return Reject(r, n, "More citizens and housing are required.");
    if (f->center < 0 || !g->buildings[f->center].alive || !g->buildings[f->center].complete)
        return Reject(r, n, "Rebuild your communal center first.");
    if (f->age == SETTLED_AGE && (!f->discovery[FARMING] || !f->discovery[FOOD_STORAGE]))
        return Reject(r, n, "Discover cultivation and food storage first.");
    if (f->age == FARMING_AGE && (!f->bronzeDelivered || !GameBuildingCountFor(g, owner, SMELTER, true)))
        return Reject(r, n, "Operate a smelter and deliver bronze first.");
    if (!GameCanAffordFor(g, owner, AGES[f->age + 1].cost))
        return Reject(r, n, "Save the materials for the next age.");
    if (r && n > 0)
        snprintf(r, (size_t)n, "Ready to advance.");
    return true;
}
bool ProgressionAdvance(GameState *g, int owner, char *r, int n)
{
    if (!GameCanAdvance(g, owner, r, n))
        return false;
    Faction *f = &g->factions[owner];
    EconomyReserve(g, owner, AGES[f->age + 1].cost, f->advanceReserve);
    f->advancing = 1;
    f->advanceProgress = 0;
    return true;
}
void ProgressionWork(GameState *g, Citizen *c)
{
    Faction *f = &g->factions[c->owner];
    if (!f->advancing)
    {
        JobsReset(c);
        return;
    }
    if (++f->advanceProgress >= AGES[f->age + 1].labor)
    {
        EconomySettle(g, c->owner, f->advanceReserve, 1);
        f->age++;
        f->advancing = 0;
        f->advanceProgress = 0;
        g->buildings[f->center].upgraded = f->age;
        if (!c->owner)
            GameEvent(g, AGES[f->age].name);
        JobsReset(c);
    }
}
int GameSiteControl(const GameState *g, int owner)
{
    const int sites[2][2] = {{38, 38}, {56, 56}};
    int count = 0;
    for (int s = 0; s < 2; s++)
    {
        bool watch = false, enemy = false;
        for (int b = 0; b < g->buildingCount; b++)
        {
            const Building *p = &g->buildings[b];
            if (p->alive && p->complete && p->owner == owner && p->type == WATCH_POST &&
                Dist(p->x, p->y, sites[s][0], sites[s][1]) <= 5)
                watch = true;
        }
        for (int i = 0; i < g->citizenCount; i++)
        {
            const Citizen *c = &g->citizens[i];
            if (c->alive && c->owner != owner && c->unitType &&
                Dist(c->x / 256, c->y / 256, sites[s][0], sites[s][1]) <= 8)
                enemy = true;
        }
        count += watch && !enemy;
    }
    return count;
}
void ProgressionUpdate(GameState *g)
{
    for (int o = 0; o < 2; o++)
    {
        Faction *f = &g->factions[o];
        int pop = GamePopulationFor(g, o);
        if (!f->village && pop >= 20 && GameHousingFor(g, o) >= pop && g->winterSurvived &&
            f->harvestDelivered && GameFoodDaysTenthsFor(g, o) >= 30 && f->raidResolved)
        {
            f->village = 1;
            f->villageTick = (int)g->tick;
            if (!o)
                GameEvent(g, "A village is born. Continue toward a Bronze Age country.");
        }
        if (o && !pop)
            f->defeated = 1;
    }
    if (!GamePopulation(g))
        g->lost = 1;
    int center = g->factions[0].center;
    if (!g->won && center >= 0 && g->buildings[center].complete &&
        g->buildings[center].upgraded == BRONZE_AGE && g->factions[0].age == BRONZE_AGE &&
        GamePopulation(g) >= 50 && GameHousing(g) >= GamePopulation(g) && GameFoodDaysTenths(g) >= 30 &&
        GameSiteControl(g, 0) == 2 && (g->treaty || g->factions[1].defeated))
    {
        g->won = 1;
        GameEvent(g, "A country is founded. The valley is yours to develop.");
    }
}
