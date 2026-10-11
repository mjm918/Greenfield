#include <math.h>
#include "test_support.h"
#include "campaign.h"

void TestDeterminism(void)
{
    GameState *g = TestGame();
    GameState *b = TestGame();
    GameInit(g);
    GameInit(b);
    double a = 0, z = 0;
    for (int i = 0; i < 18000; i++)
        GameAdvance(g, 1.0 / 60.0, 1, &a);
    for (int i = 0; i < 2250; i++)
        GameAdvance(b, 1.0 / 30.0, 4, &z);
    CHECK(g->tick == 3000);
    CHECK(GameHash(g) == GameHash(b));
    GameInit(b);
    z = 0;
    for (int i = 0; i < 300; i++)
        GameAdvance(b, 1.0, 1, &z);
    CHECK(GameHash(g) == GameHash(b));
    GameInit(g);
    a = 0;
    GameAdvance(g, NAN, 1, &a);
    CHECK(a == 0 && g->tick == 0);
    CampaignUntil(g, 18000);
    *b = *g;
    a = 0;
    z = 0;
    for (int i = 0; i < 14400; i++)
        GameAdvance(g, 1.0 / 144.0, 1, &a);
    for (int i = 0; i < 1200; i++)
        GameAdvance(b, 1.0 / 24.0, 2, &z);
    CHECK(GameHash(g) == GameHash(b));
    uint64_t before = GameHash(g);
    GameAdvance(g, 30, 0, &a);
    CHECK(GameHash(g) == before);
    free(g);
    free(b);
}
