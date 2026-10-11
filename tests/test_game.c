#define _POSIX_C_SOURCE 200809L
#include "campaign.h"
#include "game.h"
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static int failures = 0, checks = 0;
#define CHECK(test)                                                                                          \
    do                                                                                                       \
    {                                                                                                        \
        checks++;                                                                                            \
        if (!(test))                                                                                         \
        {                                                                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #test);                                  \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static void Step(GameState *g, int n)
{
    for (int i = 0; i < n; i++)
        GameTick(g);
}
static bool Command(GameState *g, int type, int a, int b, int c)
{
    char reason[256];
    return GameCommandApply(g, (GameCommand){type, a, b, c}, reason, sizeof(reason));
}
static void Supply(GameState *g, int r, int n)
{
    g->stock[r] += n;
    g->initial[r] += n;
}
static void Tests(void)
{
    GameState *g = malloc(sizeof(*g)), *b = malloc(sizeof(*b));
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
    CHECK(GameSave(g, "test-save.gfv1", reason, sizeof(reason)));
    GameInit(b);
    CHECK(GameLoad(b, "test-save.gfv1", reason, sizeof(reason)));
    CHECK(GameHash(g) == GameHash(b));
    Step(g, 300);
    Step(b, 300);
    CHECK(GameHash(g) == GameHash(b));
    uint64_t before = GameHash(b);
    FILE *f = fopen("bad-save.gfv1", "wb");
    fputs("broken", f);
    fclose(f);
    CHECK(!GameLoad(b, "bad-save.gfv1", reason, sizeof(reason)));
    CHECK(GameHash(b) == before);
    f = fopen("test-save.gfv1.tmp", "wb");
    fputs("interrupted replacement", f);
    fclose(f);
    CHECK(GameLoad(b, "test-save.gfv1", reason, sizeof(reason)));
    int original = b->stock[FOOD];
    g->stock[FOOD] = -1;
    CHECK(!GameSave(g, "test-save.gfv1", reason, sizeof(reason)));
    CHECK(GameLoad(g, "test-save.gfv1", reason, sizeof(reason)));
    CHECK(g->stock[FOOD] == original);
    // A complete file with an altered version or payload must also be rejected.
    before = GameHash(b);
    const int corruptOffsets[] = {8, 100};
    for (unsigned i = 0; i < sizeof(corruptOffsets) / sizeof(corruptOffsets[0]); i++)
    {
        CHECK(GameSave(g, "bad-save.gfv1", reason, sizeof(reason)));
        f = fopen("bad-save.gfv1", "r+b");
        CHECK(f != NULL);
        if (f)
        {
            fseek(f, corruptOffsets[i], SEEK_SET);
            int byte = fgetc(f) ^ 0x40;
            fseek(f, corruptOffsets[i], SEEK_SET);
            fputc(byte, f);
            fclose(f);
            CHECK(!GameLoad(b, "bad-save.gfv1", reason, sizeof(reason)));
            CHECK(GameHash(b) == before);
        }
    }
    // A filesystem failure opening the replacement must leave the last usable slot intact.
    remove("test-save.gfv1.tmp");
    CHECK(mkdir("test-save.gfv1.tmp", 0700) == 0);
    CHECK(!GameSave(g, "test-save.gfv1", reason, sizeof(reason)));
    CHECK(GameLoad(b, "test-save.gfv1", reason, sizeof(reason)));
    CHECK(GameHash(g) == GameHash(b));
    rmdir("test-save.gfv1.tmp");
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
    CHECK(GameSave(g, "test-save.gfv1", reason, sizeof(reason)));
    CHECK(GameLoad(b, "test-save.gfv1", reason, sizeof(reason)));
    Step(g, 500);
    Step(b, 500);
    CHECK(GameHash(g) == GameHash(b));

    // Zero tools must remain recoverable through the existing workshop crafting path.
    GameInit(g);
    g->spent[TOOLS] += g->stock[TOOLS];
    g->stock[TOOLS] = 0;
    CHECK(Command(g, CMD_BUILD, WORKSHOP, 18, 18));
    CHECK(Command(g, CMD_ASSIGN, CRAFTER, 1, 0));
    Step(g, 1600);
    CHECK(g->produced[TOOLS] > 0);
    CHECK(GameValidate(g, reason, sizeof(reason)));
    // A raider enclosed after arrival cannot hold the rival state open forever.
    GameInit(g);
    g->tick = 3700;
    g->nextRaid = 100;
    g->raidActive = 1;
    g->raiders[0] = (Raider){1, 10 * 256, 20 * 256, 650, 1, 0, 0};
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

    // Extreme coordinates are rejected before any footprint arithmetic or charge.
    GameInit(g);
    before = GameHash(g);
    CHECK(!Command(g, CMD_BUILD, SHELTER, INT_MAX, INT_MAX));
    CHECK(!Command(g, CMD_BUILD, SHELTER, INT_MIN, INT_MIN));
    CHECK(GameHash(g) == before);
    g->nextRaid = INT_MAX;
    CHECK(!GameValidate(g, reason, sizeof(reason)));
    GameInit(g);
    g->tiles[0].amount = INT_MAX;
    CHECK(!GameValidate(g, reason, sizeof(reason)));
    GameInit(g);
    g->citizens[0].attackCooldown = -1;
    CHECK(!GameValidate(g, reason, sizeof(reason)));
    GameInit(g);
    a = 0;
    GameAdvance(g, NAN, 1, &a);
    CHECK(a == 0 && g->tick == 0);
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
    // Save at each milestone with active jobs and discoveries; continuation must match.
    GameInit(g);
    for (int i = 0; i < 18000; i++)
    {
        CampaignOrders(g);
        GameTick(g);
        if (g->tick % 3000 == 0)
        {
            CHECK(GameSave(g, "test-save.gfv1", reason, sizeof(reason)));
            CHECK(GameLoad(b, "test-save.gfv1", reason, sizeof(reason)));
            CHECK(GameHash(g) == GameHash(b));
        }
    }
    *b = *g;
    a = 0;
    z = 0;
    for (int i = 0; i < 14400; i++)
        GameAdvance(g, 1.0 / 144.0, 1, &a);
    for (int i = 0; i < 1200; i++)
        GameAdvance(b, 1.0 / 24.0, 2, &z);
    CHECK(GameHash(g) == GameHash(b));
    before = GameHash(g);
    GameAdvance(g, 30, 0, &a);
    CHECK(GameHash(g) == before);
    // Kill a real saving process. The slot must always be a complete old or new state.
    CHECK(GameSave(g, "test-save.gfv1", reason, sizeof(reason)));
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0)
    {
        for (int j = 0; j < 1000; j++)
        {
            g->stock[WOOD]++;
            g->initial[WOOD]++;
            GameSave(g, "test-save.gfv1", reason, sizeof(reason));
        }
        _exit(0);
    }
    if (child > 0)
    {
        struct timespec nap = {0, 25000000};
        nanosleep(&nap, NULL);
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
        CHECK(GameLoad(b, "test-save.gfv1", reason, sizeof(reason)));
        CHECK(GameValidate(b, reason, sizeof(reason)));
        CHECK(b->stock[WOOD] >= g->stock[WOOD] && b->stock[WOOD] <= g->stock[WOOD] + 1000);
    }

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
    CHECK(GameSave(g, "test-save.gfv1", reason, sizeof(reason)));
    CHECK(GameLoad(b, "test-save.gfv1", reason, sizeof(reason)));
    CHECK(GameHash(g) == GameHash(b));

    remove("test-save.gfv1");
    remove("test-save.gfv1.tmp");
    remove("bad-save.gfv1");
    free(g);
    free(b);
    printf("%d checks; %d failures\n", checks, failures);
}
static double Now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
int main(int argc, char **argv)
{
    if (argc > 1 && (!strcmp(argv[1], "--campaign") || !strcmp(argv[1], "--realtime-campaign")))
    {
        bool real = !strcmp(argv[1], "--realtime-campaign");
        GameState *g = malloc(sizeof(*g));
        GameInit(g);
        double start = Now();
        char reason[256];
        for (int i = 0; i < 36000 && !g->lost && !g->won; i++)
        {
            CampaignOrders(g);
            GameTick(g);
            if (g->tick % 600 == 0)
            {
                printf("minute=%llu population=%d beds=%d food=%d wood=%d tools=%d research=%d harvest=%d "
                       "raids=%d resolved=%d hp=%d\n",
                       (unsigned long long)(g->tick / 600), GamePopulation(g), GameHousing(g), g->stock[FOOD],
                       g->stock[WOOD], g->stock[TOOLS], g->research, g->harvestDelivered, g->raidCount,
                       g->raidResolved, g->citizens[0].hp);
                fflush(stdout);
                if (!GameValidate(g, reason, sizeof(reason)))
                {
                    fprintf(stderr, "Invalid campaign: %s\n", reason);
                    return 1;
                }
            }
            if (real)
            {
                double until = start + g->tick / 10.0 - Now();
                if (until > 0)
                {
                    struct timespec nap = {(time_t)until, (long)((until - (time_t)until) * 1e9)};
                    nanosleep(&nap, NULL);
                }
            }
        }
        printf("CAMPAIGN %s at %.1f minutes; wall %.1fs; arrivals=%d deaths=%d food-days=%.1f "
               "shortage-recoveries=%d food-stolen=%d raids=%d\n",
               g->won ? "WON" : "INCOMPLETE", g->tick / 600.0, Now() - start, g->arrivals, g->deaths,
               GameFoodDaysTenths(g) / 10.0, g->shortageRecoveries, g->stolen[FOOD], g->raidCount);
        GameSave(g, real ? "evidence/realtime-campaign.gfv1" : "campaign.gfv1", reason, sizeof(reason));
        printf("state-hash=%016llx\n", (unsigned long long)GameHash(g));
        bool passed = g->won && g->tick >= 27000 && g->tick <= 36000 && g->shortageRecoveries > 0 &&
                      g->raidCount > 0 && g->stolen[FOOD] > 0;
        free(g);
        return passed ? 0 : 1;
    }
    Tests();
    return failures ? 1 : 0;
}
