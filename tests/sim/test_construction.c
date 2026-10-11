#include <limits.h>
#include "test_support.h"
#include "campaign.h"

void TestConstruction(void)
{
    GameState *g = TestGame();
    char reason[256];
    GameInit(g);
    CHECK(GamePopulation(g) == 8);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    int wood = g->stock[WOOD];
    CHECK(Command(g, CMD_BUILD, SHELTER, 18, 18));
    CHECK(g->stock[WOOD] == wood - 18);
    CHECK(!Command(g, CMD_BUILD, SHELTER, 18, 18));
    CHECK(g->stock[WOOD] == wood - 18);
    CHECK(!Command(g, CMD_BUILD, SHELTER, 31, 18));
    CHECK(!Command(g, CMD_BUILD, FARM, 28, 18));
    CHECK(!Command(g, CMD_ASSIGN, FORAGER, 20, 0));
    CHECK(Command(g, CMD_ASSIGN, FORAGER, 3, 0));
    CHECK(GameRoleCount(g, FORAGER) == 3);
    CHECK(!Command(g, CMD_ASSIGN, FORAGER, 4, 0));
    Step(g, 1500);
    CHECK(g->buildings[2].complete);
    CHECK(g->produced[FOOD] > 0);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    int p[MAX_PATH];
    CHECK(GameFindPath(g, GameTile(18, 17), GameTile(18, 20), p, MAX_PATH) > 0);
    CHECK(Command(g, CMD_BUILD, PALISADE, 17, 19));
    Step(g, 500);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    // Extreme coordinates are rejected before any footprint arithmetic or charge.
    GameInit(g);
    uint64_t before = GameHash(g);
    CHECK(!Command(g, CMD_BUILD, SHELTER, INT_MAX, INT_MAX));
    CHECK(!Command(g, CMD_BUILD, SHELTER, INT_MIN, INT_MIN));
    CHECK(GameHash(g) == before);
    free(g);
}
