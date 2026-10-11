#include "test_support.h"
#include "campaign.h"

void TestFarming(void)
{
    GameState *g = TestGame();
    char reason[256];
    // Actual farm work: seed -> growth -> harvest cargo -> storage. No passive food award.
    GameInit(g);
    g->tick = WINTER_END;
    g->winterSurvived = 1;
    g->discovery[FARMING] = 1;
    Supply(g, FOOD, 100);
    CHECK(Command(g, CMD_BUILD, FARM, 28, 18));
    CHECK(Command(g, CMD_ASSIGN, FARMER, 1, 0));
    Step(g, 1200);
    CHECK(g->buildings[2].crop == 1);
    CHECK(!g->harvestDelivered);
    Step(g, 2800);
    CHECK(g->harvestDelivered);
    CHECK(g->produced[FOOD] >= 24);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    // Arrivals require BOTH spare housing and food, including the retry path.
    GameInit(g);
    g->nextArrival = 1;
    GameTick(g);
    CHECK(GamePopulation(g) == 8);
    CHECK(g->nextArrival == 601);
    g->nextArrival = 100000;
    CHECK(Command(g, CMD_BUILD, SHELTER, 18, 18));
    CHECK(Command(g, CMD_BUILD, SHELTER, 18, 21));
    CHECK(Command(g, CMD_BUILD, SHELTER, 18, 24));
    Step(g, 2200);
    Supply(g, FOOD, 100);
    CHECK(Command(g, CMD_BUILD, SHELTER, 20, 24));
    g->nextArrival = (int)g->tick + 1;
    GameTick(g);
    CHECK(GamePopulation(g) == 10);
    CHECK(GameWalkable(g, g->citizens[8].x / 256, g->citizens[8].y / 256));
    free(g);
}
