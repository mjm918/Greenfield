#include "game.h"
#include "sim/jobs.h"
#include "sim/survival.h"
#include <stdio.h>
#include <stdlib.h>

static int checks, failures;
#define CHECK(x) do { checks++; if (!(x)) { \
    fprintf(stderr, "FAIL resources:%d %s\n", __LINE__, #x); failures++; } } while (0)

static GameState *New(void)
{
    GameState *g = calloc(1, sizeof(*g));
    GameInit(g);
    g->factions[1].aiEnabled = 0;
    for (int i = 0; i < g->citizenCount; i++)
    {
        JobsReset(&g->citizens[i]);
        g->citizens[i].role = IDLE;
    }
    return g;
}

static void Valid(const GameState *g)
{
    char why[256];
    bool ok = GameValidate(g, why, sizeof(why));
    if (!ok)
        fprintf(stderr, "%s\n", why);
    CHECK(ok);
}

static void Depletion(void)
{
    const int kinds[] = {NODE_TREE, NODE_BERRIES, NODE_GAME};
    for (int owner = 0; owner < FACTION_COUNT; owner++)
        for (int k = 0; k < 3; k++)
        {
            GameState *g = New();
            int id = owner * 8, x = owner ? 81 : 14, y = owner ? 73 : 22;
            Tile *t = &g->tiles[GameTile(x, y)];
            t->node = t->natural = kinds[k];
            t->amount = 2; // Final load is smaller than any normal work batch.
            Citizen *c = &g->citizens[id];
            c->x = (x + (owner ? -1 : 1)) * 256;
            c->y = y * 256;
            int resource = k ? FOOD : WOOD;
            int stock = g->factions[owner].stock[resource];
            GameCommand command = {.type = CMD_ORDER, .owner = owner, .entity = GameCitizenRef(g, id),
                                   .order = {.type = ORDER_GATHER, .x = x, .y = y}};
            CHECK(GameCommandApply(g, command, NULL, 0));
            // Exercise ordinary routing/work/delivery without unrelated consumption.
            for (int n = 0; n < 1600 && (c->orderCount || c->job != JOB_IDLE); n++)
            {
                g->tick++;
                JobsUpdate(g);
            }
            CHECK(t->amount == 0);
            CHECK(t->node == (k ? kinds[k] : NODE_NONE));
            CHECK(k || (t->natural == NODE_TREE && t->regrowth == 1));
            CHECK(g->factions[owner].produced[resource] == 2);
            CHECK(g->factions[owner].stock[resource] == stock + 2 && c->carry[resource] == 0);
            CHECK(!c->orderCount && c->job == JOB_IDLE);
            CHECK(!GameCommandApply(g, command, NULL, 0));
            Valid(g);
            free(g);
        }
}

static void RenewalAndPersistence(void)
{
    GameState *g = New(), *copy = New();
    Tile *berries = &g->tiles[GameTile(25, 30)];
    Tile *deer = &g->tiles[GameTile(11, 29)];
    Tile *tree = &g->tiles[GameTile(14, 22)];
    Tile *occupiedTree = &g->tiles[GameTile(g->buildings[0].x, g->buildings[0].y)];
    berries->amount = deer->amount = 0;
    tree->node = NODE_NONE;
    tree->amount = 0;
    tree->regrowth = 1;
    occupiedTree->natural = NODE_TREE;
    occupiedTree->regrowth = 1;
    g->tick = DAY_TICKS - 1;
    char why[256];
    CHECK(GameSave(g, "resource-test.gfv2", why, sizeof(why)));
    CHECK(GameLoad(copy, "resource-test.gfv2", why, sizeof(why)));
    CHECK(GameHash(g) == GameHash(copy));
    GameTick(g);
    GameTick(copy);
    CHECK(GameHash(g) == GameHash(copy));
    CHECK(NATURAL_FOOD_PER_DAY == 3 && berries->amount == 3 && deer->amount == 3);
    CHECK(tree->node == NODE_NONE && tree->regrowth == 2);
    CHECK(occupiedTree->node == NODE_NONE && occupiedTree->regrowth == 1);
    for (int day = 2; day <= 8; day++)
    {
        g->tick = day * DAY_TICKS;
        SurvivalUpdate(g);
        CHECK(tree->node == NODE_NONE);
    }
    CHECK(berries->amount == 24 && deer->amount == 24);
    g->tick = DAY_TICKS * 9;
    SurvivalUpdate(g);
    CHECK(tree->node == NODE_TREE && tree->amount == 100 && tree->regrowth == 0);
    CHECK(occupiedTree->node == NODE_NONE && occupiedTree->regrowth == 1);
    berries->amount = NATURAL_FOOD_CAPACITY - 1;
    deer->amount = NATURAL_FOOD_CAPACITY;
    g->tick += DAY_TICKS;
    SurvivalUpdate(g);
    CHECK(berries->amount == NATURAL_FOOD_CAPACITY && deer->amount == NATURAL_FOOD_CAPACITY);
    Valid(g);
    Valid(copy);
    remove("resource-test.gfv2");
    free(g);
    free(copy);
}

static void RenewableDeterminism(void)
{
    GameState *g[4];
    const int fps[] = {30, 60, 30, 60}, speeds[] = {1, 1, 2, 4};
    for (int i = 0; i < 4; i++)
    {
        g[i] = New();
        g[i]->tiles[GameTile(25, 30)].amount = 0;
        double accumulator = 0;
        for (int f = 0; f < 180 * fps[i] / speeds[i]; f++)
            GameAdvance(g[i], 1.0 / fps[i], speeds[i], &accumulator);
        CHECK(g[i]->tick == DAY_TICKS);
        CHECK(GameHash(g[i]) == GameHash(g[0]));
    }
    for (int i = 0; i < 4; i++)
        free(g[i]);
}

int main(void)
{
    Depletion();
    RenewalAndPersistence();
    RenewableDeterminism();
    printf("%d resource checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
