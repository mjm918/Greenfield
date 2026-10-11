#include "game.h"
#include "sim/buildings.h"
#include "sim/combat.h"
#include "sim/jobs.h"
#include "sim/wildlife.h"
#include "sim/world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int checks, failures;
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        checks++;                                                                                            \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "FAIL wildlife:%d %s\n", __LINE__, #x);                                          \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static GameState *New(void)
{
    GameState *g = calloc(1, sizeof(*g));
    GameInit(g);
    g->factions[1].aiEnabled = 0;
    for (int i = 0; i < g->citizenCount; i++)
    {
        g->citizens[i].role = IDLE;
        JobsReset(&g->citizens[i]);
    }
    return g;
}
static void FacingTests(void)
{
    GameState *g = New();
    Citizen *c = &g->citizens[0];
    int t = GameTile(34, 30);
    g->tiles[t].node = NODE_BERRIES;
    g->tiles[t].amount = 400;
    const int dx[] = {-1, 1, 0, 0}, dy[] = {0, 0, -1, 1};
    const int expected[] = {FACING_SE, FACING_NW, FACING_SW, FACING_NE};
    for (int i = 0; i < 4; i++)
    {
        JobsReset(c);
        c->x = (34 + dx[i]) * 256;
        c->y = (30 + dy[i]) * 256;
        c->role = FORAGER;
        c->job = JOB_GATHER;
        c->target = t;
        JobsUpdate(g);
        CHECK(c->work == 1 && c->facing == expected[i]);
        JobsReset(c);
        CHECK(c->facing == expected[i]);
    }
    CHECK(GameFacing(0, 0, FACING_NE) == FACING_NE);
    int b = BuildingsAdd(g, 0, SHELTER, 34, 30, true);
    g->buildings[b].hp -= 100;
    c->x = 34 * 256;
    c->y = 32 * 256;
    c->job = JOB_REPAIR;
    c->target = b;
    c->role = BUILDER;
    JobsUpdate(g);
    CHECK(c->facing == FACING_NE);
    JobsReset(c);
    c->x = c->y = 40 * 256;
    CHECK(WorldRoute(g, c, GameTile(40, 39)));
    WorldMoveCitizen(g, c);
    CHECK(c->facing == FACING_NE && c->y < 40 * 256);
    JobsReset(c);
    c->x = c->y = 40 * 256;
    c->unitType = SPEARMAN;
    c->role = GUARD;
    g->citizens[8].x = 40 * 256;
    g->citizens[8].y = 39 * 256;
    CombatUpdate(g);
    CHECK(c->job == JOB_FIGHT && c->facing == FACING_NE);
    free(g);
}
static void MovementTests(void)
{
    GameState *g = New(), *h = New();
    CHECK(g->wildlifeCount == 6);
    uint32_t random = g->random;
    int moved = 0;
    bool safe = true;
    for (int tick = 0; tick < 1000; tick++)
    {
        g->tick++;
        WildlifeUpdate(g);
        for (int i = 0; i < g->wildlifeCount; i++)
        {
            Wildlife *a = &g->wildlife[i];
            if (a->stepX || a->stepY)
                moved |= 1 << i;
            safe &= GameWalkable(g, a->x / 256, a->y / 256);
            safe &= abs(a->x - a->home % MAP_SIZE * 256) + abs(a->y - a->home / MAP_SIZE * 256) <= 512;
        }
    }
    CHECK(moved == (1 << g->wildlifeCount) - 1);
    CHECK(safe && g->random == random);
    for (int i = 0; i < g->wildlifeCount; i++)
        CHECK(g->tiles[g->wildlife[i].home].amount == 400);
    // Compare the same open hunting ground with and without a nearby citizen.
    Wildlife *a = &g->wildlife[2];
    a->x = a->goalX = a->home % MAP_SIZE * 256;
    a->y = a->goalY = a->home / MAP_SIZE * 256;
    a->wait = 0;
    *h = *g;
    h->citizens[0].x = a->x - 256;
    h->citizens[0].y = a->y;
    WildlifeUpdate(g);
    WildlifeUpdate(h);
    CHECK(g->wildlife[2].state == ANIMAL_WALK);
    CHECK(h->wildlife[2].state == ANIMAL_RUN);
    CHECK(abs(h->wildlife[2].stepX) + abs(h->wildlife[2].stepY) > abs(a->stepX) + abs(a->stepY));
    // A person approaching mid-walk causes an immediate reversal, not a charge.
    Wildlife *fleeing = &h->wildlife[2];
    fleeing->x = fleeing->home % MAP_SIZE * 256 + 128;
    fleeing->y = fleeing->goalY = fleeing->home / MAP_SIZE * 256;
    fleeing->goalX = fleeing->x + 128;
    h->citizens[0].x = fleeing->goalX + 256;
    h->citizens[0].y = fleeing->y;
    int distance = h->citizens[0].x - fleeing->x;
    WildlifeUpdate(h);
    CHECK(fleeing->state == ANIMAL_RUN && h->citizens[0].x - fleeing->x > distance);
    // A blocked destination interrupts motion; reopening it allows recovery.
    a->x = a->goalX = a->home % MAP_SIZE * 256;
    a->y = a->goalY = a->home / MAP_SIZE * 256;
    a->goalX += 256;
    int blocked = GameTile(a->goalX / 256, a->goalY / 256);
    int terrain = g->tiles[blocked].terrain;
    g->tiles[blocked].terrain = WATER;
    int beforeX = a->x, beforeY = a->y;
    WildlifeUpdate(g);
    CHECK(a->state == ANIMAL_IDLE && a->x == beforeX && a->y == beforeY);
    g->tiles[blocked].terrain = terrain;
    a->goalX = a->x + 256;
    WildlifeUpdate(g);
    CHECK(a->x > beforeX);
    g->tiles[a->home].amount = 0;
    beforeX = a->x;
    WildlifeUpdate(g);
    CHECK(!GameWildlifeForTile(g, a->home) && a->state == ANIMAL_IDLE && a->x == beforeX);
    g->tiles[a->home].amount = 60;
    WildlifeUpdate(g);
    CHECK(GameWildlifeForTile(g, a->home) != NULL);
    double elapsed = 0;
    uint64_t hash = GameHash(g);
    GameAdvance(g, 10, 0, &elapsed);
    CHECK(hash == GameHash(g));
    free(h);
    free(g);
}
static void Put32(unsigned char *p, uint32_t v)
{
    for (int i = 0; i < 4; i++)
        p[i] = (unsigned char)(v >> (i * 8));
}
static void PersistenceTests(void)
{
    GameState *g = New(), *h = New();
    for (int i = 0; i < 400; i++)
        GameTick(g);
    g->citizens[0].facing = FACING_NW;
    char why[256];
    CHECK(GameSave(g, "wildlife-test.save", why, sizeof(why)));
    CHECK(GameLoad(h, "wildlife-test.save", why, sizeof(why)));
    CHECK(GameHash(g) == GameHash(h));
    uint64_t savedHash = GameHash(h);
    for (int i = 0; i < 200; i++)
    {
        GameTick(g);
        GameTick(h);
    }
    CHECK(GameHash(g) == GameHash(h));
    g->wildlife[0].facing = 9;
    CHECK(!GameSave(g, "wildlife-test.save", why, sizeof(why)));
    g->wildlife[0].facing = h->wildlife[0].facing;
    CHECK(GameLoad(h, "wildlife-test.save", why, sizeof(why)));
    CHECK(GameHash(h) == savedHash);
    // Make an authentic v2 wire payload from the unchanged prefix of the codec.
    FILE *f = fopen("wildlife-test.save", "rb");
    fseek(f, 0, SEEK_END);
    size_t n = (size_t)ftell(f);
    rewind(f);
    unsigned char *bytes = malloc(n);
    CHECK(fread(bytes, 1, n, f) == n);
    fclose(f);
    n -= 4 * (MAX_CITIZENS + 1 + MAX_WILDLIFE * 11);
    Put32(bytes + 8, 2);
    Put32(bytes + 12, (uint32_t)n - 24);
    uint64_t digest = UINT64_C(14695981039346656037);
    for (size_t i = 24; i < n; i++)
    {
        digest ^= bytes[i];
        digest *= UINT64_C(1099511628211);
    }
    Put32(bytes + 16, (uint32_t)digest);
    Put32(bytes + 20, (uint32_t)(digest >> 32));
    f = fopen("wildlife-v2.save", "wb");
    CHECK(fwrite(bytes, 1, n, f) == n);
    fclose(f);
    free(bytes);
    CHECK(GameLoad(g, "wildlife-v2.save", why, sizeof(why)));
    CHECK(g->tick == h->tick && !memcmp(g->tiles, h->tiles, sizeof(g->tiles)));
    CHECK(!memcmp(g->factions, h->factions, sizeof(g->factions)));
    CHECK(g->wildlifeCount == 6 && g->citizens[0].facing == FACING_SE);
    CHECK(GameValidate(g, why, sizeof(why)));
    CHECK(GameSave(g, "wildlife-test.save", why, sizeof(why)));
    CHECK(GameLoad(h, "wildlife-test.save", why, sizeof(why)) && GameHash(g) == GameHash(h));
    remove("wildlife-test.save");
    remove("wildlife-v2.save");
    free(g);
    free(h);
}
int main(void)
{
    FacingTests();
    MovementTests();
    PersistenceTests();
    printf("%d wildlife checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
