#include "test_support.h"
#include "campaign.h"

void TestRivals(void)
{
    GameState *g = TestGame();
    GameState *b = TestGame();
    char reason[256];
    GameInit(g);
    CHECK(Command(g, CMD_TRUCE, 0, 0, 0));
    CHECK(g->stock[FOOD] == 20 && g->raidResolved);
    CHECK(!Command(g, CMD_TRUCE, 0, 0, 0));
    CHECK(g->truceUntil == 7200);
    GameInit(g);
    g->nextRaid = 1000;
    Step(g, 101);
    CHECK(g->raidWarning);
    Step(g, 900);
    CHECK(g->raidActive);
    Step(g, 3000);
    CHECK(g->stolen[FOOD] <= 18);
    CHECK(!g->raidActive);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    // Guards intercept and rout a raid without a truce payment.
    GameInit(g);
    CHECK(Command(g, CMD_ASSIGN, BUILDER, 0, 0));
    CHECK(Command(g, CMD_ASSIGN, GUARD, 3, 0));
    CHECK(Command(g, CMD_RALLY, 26, 23, 0));
    g->nextRaid = 100;
    Step(g, 2200);
    CHECK(g->raidResolved);
    CHECK(g->raidLosses >= 2);
    CHECK(g->stolen[FOOD] <= 18);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    // Truce expiry returns to a warned threat, and survives a mid-raid save.
    GameInit(g);
    CHECK(Command(g, CMD_TRUCE, 0, 0, 0));
    g->tick = 7199;
    GameTick(g);
    CHECK(g->truceUntil <= (int)g->tick);
    Step(g, 301);
    CHECK(g->raidWarning);
    Step(g, 900);
    CHECK(g->raidActive);
    CHECK(GameSave(g, "rivals-save.gfv1", reason, sizeof(reason)));
    CHECK(GameLoad(b, "rivals-save.gfv1", reason, sizeof(reason)));
    Step(g, 500);
    Step(b, 500);
    CHECK(GameHash(g) == GameHash(b));
    // A raider enclosed after arrival cannot hold the rival state open forever.
    GameInit(g);
    g->tick = 3700;
    g->nextRaid = 100;
    g->raidActive = 1;
    g->raiders[0] = (Raider){1, 10 * 256, 20 * 256, 650, 1, 0, 0};
    int closed[] = {GameTile(10, 19), GameTile(11, 20), GameTile(10, 21), GameTile(9, 20)};
    for (int j = 0; j < 4; j++)
        g->tiles[closed[j]].terrain = WATER;
    *b = *g;
    CHECK(Command(b, CMD_TRUCE, 0, 0, 0));
    Step(b, 2);
    CHECK(!b->raidActive);
    CHECK(b->nextRaid == b->truceUntil + 1200);
    Step(g, 2);
    CHECK(!g->raidActive);
    CHECK(g->raidResolved);
    remove("rivals-save.gfv1");
    remove("rivals-save.gfv1.tmp");
    free(g);
    free(b);
}
