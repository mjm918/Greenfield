#include "test_support.h"
#include "campaign.h"

void TestSurvival(void)
{
    GameState *g = TestGame();
    char reason[256];
    GameInit(g);
    for (int i = 0; i < g->citizenCount; i++)
        g->citizens[i].role = IDLE;
    g->spent[FOOD] += g->stock[FOOD];
    g->stock[FOOD] = 0;
    g->spent[WOOD] += g->stock[WOOD];
    g->stock[WOOD] = 0;
    Step(g, 2200);
    CHECK(g->foodWarning && g->coldWarning);
    CHECK(g->citizens[0].hp < 1000);
    int hp = g->citizens[0].hp;
    Supply(g, FOOD, 300);
    Supply(g, WOOD, 300);
    CHECK(Command(g, CMD_BUILD, SHELTER, 18, 18));
    CHECK(Command(g, CMD_BUILD, SHELTER, 18, 21));
    CHECK(Command(g, CMD_ASSIGN, BUILDER, 2, 0));
    Step(g, 5000);
    CHECK(g->citizens[0].hp > hp);
    CHECK(g->shortageRecoveries > 0);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    GameInit(g);
    for (int i = 0; i < g->citizenCount; i++)
        g->citizens[i].hp = 0;
    GameTick(g);
    CHECK(g->lost);
    GameInit(g);
    // Arrivals never resurrect a tribe that died on the same tick.
    g->nextArrival = 1;
    for (int i = 0; i < g->citizenCount; i++)
        g->citizens[i].hp = 0;
    GameTick(g);
    CHECK(g->lost && GamePopulation(g) == 0);
    // A lethal attack is resolved immediately, before work, arrivals and objectives.
    GameInit(g);
    g->nextRaid = 10000;
    g->raidActive = 1;
    g->citizens[0].role = GUARD;
    g->citizens[0].hp = 40;
    g->raiders[0] = (Raider){1, g->citizens[0].x, g->citizens[0].y, 650, 0, 0, 0};
    GameTick(g);
    CHECK(!g->citizens[0].alive);
    CHECK(GamePopulation(g) == 7);
    CHECK(g->deaths == 1);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    free(g);
}
