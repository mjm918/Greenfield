#include "game.h"
#include "sim/ai.h"
#include "sim/buildings.h"
#include "sim/citizens.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// Explicit stress fixture, separate from the command-only campaign evidence.
// Populate the actual state/owners to exercise routing, survival and rendering
// at the supported population cap without waiting for recruitment.
static bool Add(GameState *g, int owner, int type)
{
    for (int y = 8; y <= 40; y += 4)
        for (int x = 16; x <= 40; x += 4)
        {
            int xx = owner ? 94 - x : x, yy = owner ? 94 - y : y;
            if (GameCanPlaceFor(g, owner, type, xx, yy, NULL, 0))
                return BuildingsAdd(g, owner, type, xx, yy, true) >= 0;
        }
    return false;
}
int main(void)
{
    GameState *g = calloc(1, sizeof(*g));
    GameInit(g);
    g->tick = 90000;
    g->winterSurvived = g->treaty = 1;
    for (int o = 0; o < FACTION_COUNT; o++)
    {
        Faction *f = &g->factions[o];
        f->age = BRONZE_AGE;
        g->buildings[f->center].upgraded = BRONZE_AGE;
        for (int d = 0; d < DISCOVERY_COUNT; d++)
            f->discovery[d] = 1;
        for (int r = 0; r < RESOURCE_COUNT; r++)
            f->stock[r] = f->initial[r] = 10000;
        for (int b = 0; b < 24; b++)
            if (!Add(g, o, SHELTER))
                return 1;
        if (!Add(g, o, WORKSHOP) || !Add(g, o, SMELTER) || !Add(g, o, TRAINING_GROUND))
            return 1;
        for (int b = 0; b < 6; b++)
            Add(g, o, FARM);
        while (GamePopulationFor(g, o) < POPULATION_LIMIT)
            if (CitizensAdd(g, o, IDLE) < 0)
                return 1;
        AiEconomy(g, o);
    }
    clock_t start = clock();
    for (int i = 0; i < 600; i++)
        GameTick(g);
    double ms = 1000.0 * (clock() - start) / CLOCKS_PER_SEC;
    char reason[256];
    bool ok = GameValidate(g, reason, sizeof(reason)) && GamePopulationFor(g, 0) == 96 &&
              GamePopulationFor(g, 1) == 96;
    if (ok)
        ok = GameSave(g, "evidence/capacity.gfv2", reason, sizeof(reason));
    printf("192-person simulation: 600 ticks, %.3f ms/tick; %s\n", ms / 600, ok ? "valid" : reason);
    free(g);
    return ok ? 0 : 1;
}
