#include "survival.h"
#include "common.h"
#include "economy.h"

void SurvivalUpdate(GameState *g)
{
    for (int owner = 0; owner < FACTION_COUNT; owner++)
    {
        Faction *f = &g->factions[owner];
        int pop = GamePopulationFor(g, owner), beds = GameHousingFor(g, owner), huts = 0;
        for (int i = 0; i < g->buildingCount; i++)
            huts += g->buildings[i].alive && g->buildings[i].owner == owner &&
                    g->buildings[i].type == SHELTER && g->buildings[i].complete && g->buildings[i].upgraded;
        bool winter = g->tick < WINTER_END;
        bool fire = GameBuildingCountFor(g, owner, HEARTH, true) > 0 && f->stock[WOOD] > 0;
        f->warmth =
            winter ? Min(100, 20 + (fire ? 45 : 0) + (beds >= pop ? 25 : 0) + Min(10, huts * 5)) : 100;
        if (winter && g->tick % 200 == 0 && fire)
            EconomySpend(g, owner, WOOD, 1);
        f->foodWarning = GameFoodDaysTenthsFor(g, owner) < 10;
        f->coldWarning = winter && f->warmth < 80;
        bool shortage = f->stock[FOOD] == 0 || (winter && f->warmth < 50);
        if (shortage)
        {
            f->shortageTicks++;
            f->hadShortage = 1;
        }
        else if (f->hadShortage)
        {
            f->shortageRecoveries++;
            f->hadShortage = 0;
            if (!owner)
                GameEvent(g, "Supplies and warmth are restored. Citizens can recover.");
        }
        for (int i = 0; i < g->citizenCount; i++)
        {
            Citizen *c = &g->citizens[i];
            if (!c->alive || c->owner != owner)
                continue;
            c->hunger++;
            if (c->hunger >= 600 && f->stock[FOOD] > 0)
            {
                EconomySpend(g, owner, FOOD, 1);
                c->hunger = 0;
            }
            if (winter && f->warmth < 80)
                c->cold++;
            else
                c->cold = Max(0, c->cold - 3);
            if (g->tick % 10 == 0)
            {
                if (c->hunger > 1500)
                    c->hp -= 3;
                if (c->cold > 1800)
                    c->hp -= f->warmth < 50 ? 3 : 1;
                if (c->hunger < 600 && c->cold < 600)
                    c->hp = Min(1000, c->hp + 2);
            }
        }
        if (g->tick == WINTER_END && !owner)
        {
            g->winterSurvived = 1;
            GameEvent(g, "The thaw has arrived. Discover farming and settle the fertile riverbank.");
        }
        if (g->tick % DAY_TICKS == 0)
        {
            int spoiled = f->stock[FOOD] / (GameBuildingCountFor(g, owner, GRANARY, true) ? 50
                                            : f->discovery[FOOD_STORAGE]                  ? 33
                                                                                          : 20);
            EconomySpend(g, owner, FOOD, spoiled);
        }
    }
    if (g->tick % DAY_TICKS == 0)
        for (int t = 0; t < TILE_COUNT; t++)
        {
            Tile *tile = &g->tiles[t];
            if (tile->node == NODE_BERRIES || tile->node == NODE_GAME)
                tile->amount = Min(NATURAL_FOOD_CAPACITY, tile->amount + NATURAL_FOOD_PER_DAY);
            if (tile->node == NODE_NONE && tile->natural && tile->occupied < 0)
            {
                if (tile->natural == NODE_STONE || tile->natural == NODE_ORE)
                {
                    tile->node = tile->natural;
                    tile->amount = 10;
                }
                else if (tile->natural == NODE_TREE && ++tile->regrowth >= 10)
                {
                    tile->node = NODE_TREE;
                    tile->amount = 100;
                    tile->regrowth = 0;
                }
            }
        }
}
