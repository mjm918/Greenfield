#include <limits.h>
#include "test_support.h"
#include "campaign.h"

void TestValidation(void)
{
    GameState *g = TestGame();
    char reason[256];
    g->nextRaid = INT_MAX;
    CHECK(!GameValidate(g, reason, sizeof(reason)));
    GameInit(g);
    g->tiles[0].amount = INT_MAX;
    CHECK(!GameValidate(g, reason, sizeof(reason)));
    GameInit(g);
    g->citizens[0].attackCooldown = -1;
    CHECK(!GameValidate(g, reason, sizeof(reason)));
    free(g);
}
