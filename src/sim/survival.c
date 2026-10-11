#include "survival.h"
#include "common.h"
#include "economy.h"

void SurvivalUpdate(GameState *g)
{
    int pop = GamePopulation(g), beds = GameHousing(g), huts = 0;
    for (int i = 0; i < g->buildingCount; i++)
        huts += g->buildings[i].type == SHELTER && g->buildings[i].complete && g->buildings[i].upgraded;
    bool winter = g->tick < WINTER_END;
    bool fire = GameBuildingCount(g, HEARTH, true) > 0 && g->stock[WOOD] > 0;
    g->warmth = winter ? Min(100, 20 + (fire ? 45 : 0) + (beds >= pop ? 25 : 0) + Min(10, huts * 5)) : 100;
    if (winter && g->tick % 200 == 0 && fire)
        EconomySpend(g, WOOD, 1);
    g->foodWarning = GameFoodDaysTenths(g) < 10;
    g->coldWarning = winter && g->warmth < 80;
    bool shortage = g->stock[FOOD] == 0 || (winter && g->warmth < 50);
    if (shortage)
    {
        g->shortageTicks++;
        g->hadShortage = 1;
    }
    else if (g->hadShortage)
    {
        g->shortageRecoveries++;
        g->hadShortage = 0;
        GameEvent(g, "Supplies and warmth are restored. Citizens can recover.");
    }
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive)
            continue;
        c->hunger++;
        if (c->hunger >= 600 && g->stock[FOOD] > 0)
        {
            EconomySpend(g, FOOD, 1);
            c->hunger = 0;
        }
        if (winter && g->warmth < 80)
            c->cold++;
        else
            c->cold = Max(0, c->cold - 3);
        if (g->tick % 10 == 0)
        {
            if (c->hunger > 1500)
                c->hp -= 3;
            if (c->cold > 1800)
                c->hp -= g->warmth < 50 ? 3 : 1;
            if (c->hunger < 600 && c->cold < 600)
                c->hp = Min(1000, c->hp + 2);
        }
    }
    if (g->tick == WINTER_END)
    {
        g->winterSurvived = 1;
        GameEvent(g, "The thaw has arrived. Discover farming and settle the fertile riverbank.");
    }
    if (g->tick % DAY_TICKS == 0)
    {
        int spoiled = g->stock[FOOD] / (GameBuildingCount(g, GRANARY, true) ? 50
                                        : g->discovery[FOOD_STORAGE]        ? 33
                                                                            : 20);
        EconomySpend(g, FOOD, spoiled);
        for (int t = 0; t < TILE_COUNT; t++)
            if (g->tiles[t].node == NODE_BERRIES || g->tiles[t].node == NODE_GAME)
                g->tiles[t].amount = Min(200, g->tiles[t].amount + 30);
    }
}
