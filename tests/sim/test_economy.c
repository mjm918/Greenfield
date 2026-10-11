#include "test_support.h"
#include "campaign.h"

void TestEconomy(void)
{
    GameState *g = TestGame();
    char reason[256];
    // Roles retain carried goods through reassignment; construction is already charged.
    GameInit(g);
    Citizen *carrier = &g->citizens[2];
    carrier->carry[FOOD] = 5;
    g->produced[FOOD] += 5;
    carrier->job = JOB_DELIVER;
    CHECK(Command(g, CMD_ASSIGN, FORAGER, 1, 0));
    CHECK(carrier->role == IDLE && carrier->job == JOB_DELIVER);
    Step(g, 200);
    CHECK(carrier->carry[FOOD] == 0);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    // An unreachable carrier retries, then recovers after an obstacle changes.
    GameInit(g);
    carrier = &g->citizens[7];
    carrier->x = 10 * 256;
    carrier->y = 20 * 256;
    carrier->carry[WOOD] = 7;
    g->produced[WOOD] += 7;
    carrier->job = JOB_DELIVER;
    int closed[] = {GameTile(10, 19), GameTile(11, 20), GameTile(10, 21), GameTile(9, 20)};
    for (int i = 0; i < 4; i++)
        g->tiles[closed[i]].terrain = WATER;
    g->mapRevision++;
    Step(g, 80);
    CHECK(carrier->carry[WOOD] == 7);
    g->tiles[closed[1]].terrain = GRASS;
    g->mapRevision++;
    Step(g, 400);
    CHECK(carrier->carry[WOOD] == 0);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    // Zero tools must remain recoverable through the existing workshop crafting path.
    GameInit(g);
    g->spent[TOOLS] += g->stock[TOOLS];
    g->stock[TOOLS] = 0;
    CHECK(Command(g, CMD_BUILD, WORKSHOP, 18, 18));
    CHECK(Command(g, CMD_ASSIGN, CRAFTER, 1, 0));
    Step(g, 1600);
    CHECK(g->produced[TOOLS] > 0);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    free(g);
}
