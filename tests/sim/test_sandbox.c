#include "test_support.h"
#include "campaign.h"

void TestSandbox(void)
{
    GameState *g = TestGame();
    GameState *b = TestGame();
    char reason[256];
    // Victory unlocks continued play: new orders, production and saves still work.
    GameInit(g);
    while (!g->won && !g->lost && g->tick < 36000)
    {
        CampaignOrders(g);
        GameTick(g);
    }
    CHECK(g->won && GamePopulation(g) >= 20);
    uint64_t victoryTick = g->tick;
    int foodProduced = g->produced[FOOD], nextBuilding = g->buildingCount;
    CHECK(Command(g, CMD_BUILD, WATCH_POST, 22, 29));
    Step(g, 1800);
    CHECK(g->tick == victoryTick + 1800 && g->won && !g->lost);
    CHECK(g->buildings[nextBuilding].complete);
    CHECK(g->produced[FOOD] > foodProduced);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    CHECK(GameSave(g, "sandbox-save.gfv1", reason, sizeof(reason)));
    CHECK(GameLoad(b, "sandbox-save.gfv1", reason, sizeof(reason)));
    CHECK(GameHash(g) == GameHash(b));
    remove("sandbox-save.gfv1");
    remove("sandbox-save.gfv1.tmp");
    free(g);
    free(b);
}
