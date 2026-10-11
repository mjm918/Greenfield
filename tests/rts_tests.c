#include "game.h"
#include "sim/ai.h"
#include "sim/buildings.h"
#include "sim/citizens.h"
#include "sim/combat.h"
#include "sim/economy.h"
#include "sim/jobs.h"
#include "sim/orders.h"
#include "sim/production.h"
#include "sim/progression.h"
#include "sim/rivals.h"
#include "sim/world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int checks, failures;
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        checks++;                                                                                            \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                                     \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
static GameState *New(void)
{
    GameState *g = calloc(1, sizeof(*g));
    GameInit(g);
    g->factions[1].aiEnabled = 0;
    return g;
}
static void Quiet(GameState *g)
{
    for (int i = 0; i < g->citizenCount; i++)
    {
        g->citizens[i].role = IDLE;
        JobsReset(&g->citizens[i]);
    }
}
static void Stock(GameState *g, int o, int resource, int amount)
{
    g->factions[o].stock[resource] += amount;
    g->factions[o].initial[resource] += amount;
}
static bool Do(GameState *g, GameCommand c)
{
    return GameCommandApply(g, c, NULL, 0);
}
static void Run(GameState *g, int ticks)
{
    while (ticks--)
        GameTick(g);
}
static void Valid(GameState *g)
{
    char why[256];
    bool ok = GameValidate(g, why, sizeof(why));
    if (!ok)
        fprintf(stderr, "Validation: %s\n", why);
    CHECK(ok);
}
static void Foundation(void)
{
    GameState *g = New();
    CHECK(MAP_SIZE == 96);
    CHECK(GamePopulation(g) == 8);
    CHECK(GamePopulationFor(g, 1) == 8);
    Valid(g);
    int old = g->factions[1].stock[WOOD];
    CHECK(Do(g, (GameCommand){.type = CMD_BUILD, .a = SHELTER, .b = 18, .c = 18}));
    int b = g->tiles[GameTile(18, 18)].occupied;
    CHECK(g->factions[0].reserved[WOOD] == 18);
    CHECK(g->factions[1].stock[WOOD] == old);
    CHECK(GameWalkable(g, 18, 18));
    CHECK(!Do(g, (GameCommand){.type = CMD_BUILD, .a = SHELTER, .b = 18, .c = 18}));
    Run(g, 1000);
    CHECK(g->buildings[b].complete);
    CHECK(g->factions[0].reserved[WOOD] == 0);
    CHECK(!GameWalkable(g, 18, 18));
    CHECK(GameHousing(g) == 4);
    CHECK(GameHousingFor(g, 1) == 0);
    Valid(g);
    CHECK(!Do(g, (GameCommand){.type = CMD_BUILD, .a = FARM, .b = 28, .c = 18}));
    CHECK(!Do(g, (GameCommand){.type = CMD_BUILD, .a = SHELTER, .b = 95, .c = 95}));
    CHECK(!Do(g, (GameCommand){.type = CMD_BUILD, .a = WORKSHOP, .b = 24, .c = 18}));
    free(g);
}
static void Orders(void)
{
    GameState *g = New();
    Quiet(g);
    EntityRef ref = GameCitizenRef(g, 0);
    UnitOrder move = {.type = ORDER_MOVE, .x = 20, .y = 30};
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER, .entity = ref, .order = move}));
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = ref,
                              .append = 1,
                              .order = {.type = ORDER_MOVE, .x = 25, .y = 30}}));
    CHECK(g->citizens[0].orderCount == 2);
    Run(g, 700);
    CHECK(g->citizens[0].x / 256 == 25 && g->citizens[0].y / 256 == 30);
    CHECK(g->citizens[0].orderCount == 0);
    CHECK(!Do(g, (GameCommand){.type = CMD_ORDER, .owner = 1, .entity = ref, .order = move}));
    g->citizens[0].hp = 0;
    CitizensResolveDeaths(g);
    int id = CitizensAdd(g, 0, IDLE);
    CHECK(id == ref.slot);
    CHECK(!GameCitizenValid(g, ref));
    CHECK(!Do(g, (GameCommand){.type = CMD_ORDER, .entity = ref, .order = move}));
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = GameCitizenRef(g, id),
                              .order = {.type = ORDER_GATHER, .x = 14, .y = 22}}));
    Run(g, 900);
    CHECK(g->factions[0].produced[WOOD] > 0);
    Valid(g);
    for (int i = 0; i < MAX_ORDERS; i++)
        Do(g, (GameCommand){.type = CMD_ORDER, .entity = GameCitizenRef(g, id), .append = 1, .order = move});
    CHECK(!Do(g,
              (GameCommand){.type = CMD_ORDER, .entity = GameCitizenRef(g, id), .append = 1, .order = move}));
    free(g);
}
static void Queues(void)
{
    GameState *g = New();
    Quiet(g);
    int center = g->factions[0].center;
    BuildingsAdd(g, 0, SHELTER, 18, 18, true);
    BuildingsAdd(g, 0, SHELTER, 18, 22, true);
    BuildingsAdd(g, 0, SHELTER, 18, 26, true);
    GameCommand recruit = {.type = CMD_RECRUIT, .entity = GameBuildingRef(g, center)};
    int food = g->factions[0].stock[FOOD];
    CHECK(Do(g, recruit));
    CHECK(g->factions[0].stock[FOOD] == food - 12);
    CHECK(Do(g, (GameCommand){.type = CMD_CANCEL, .entity = recruit.entity, .a = 0}));
    CHECK(g->factions[0].stock[FOOD] == food);
    CHECK(g->factions[0].reserved[FOOD] == 0);
    CHECK(Do(g, recruit));
    Stock(g, 0, FOOD, 100);
    Run(g, 2400);
    CHECK(GamePopulation(g) == 9);
    CHECK(g->factions[0].arrivals == 1);
    CHECK(g->buildings[center].queueCount == 0);
    g->factions[0].age = SETTLED_AGE;
    int train = BuildingsAdd(g, 0, TRAINING_GROUND, 24, 16, true);
    GameCommand cmd = {.type = CMD_TRAIN,
                       .a = SPEARMAN,
                       .entity = GameCitizenRef(g, 0),
                       .order = {.target = GameBuildingRef(g, train)}};
    CHECK(Do(g, cmd));
    CHECK(!Do(g, cmd));
    CHECK(!Do(g, (GameCommand){.type = CMD_STOP, .entity = cmd.entity}));
    // Regression: the north-east corner used to route successfully but never
    // satisfy training arrival, permanently blocking this building's queue.
    g->citizens[0].x = (g->buildings[train].x + 3) * 256;
    g->citizens[0].y = (g->buildings[train].y - 1) * 256;
    Run(g, 301);
    CHECK(g->citizens[0].unitType == SPEARMAN);
    CHECK(GamePopulation(g) == 9);
    CHECK(Do(g, (GameCommand){.type = CMD_DEMOBILIZE, .entity = cmd.entity}));
    CHECK(g->citizens[0].unitType == CIVILIAN);
    CHECK(GamePopulation(g) == 9);
    CHECK(Do(g, cmd));
    int reserved = g->factions[0].reserved[WOOD];
    BuildingsDestroy(g, train);
    CHECK(g->citizens[0].training == -1);
    CHECK(g->factions[0].reserved[WOOD] == 0);
    CHECK(g->factions[0].lostGoods[WOOD] >= reserved);
    Valid(g);
    free(g);
}
static void Boundaries(void)
{
    GameState *g = New();
    Quiet(g);
    EntityRef worker = GameCitizenRef(g, 0);
    CHECK(!GameCanPlaceFor(g, -1, FARM, 20, 20, NULL, 0));
    CHECK(!GameCanAdvance(g, 2, NULL, 0));
    CHECK(!Do(g, (GameCommand){.type = CMD_RALLY, .order = {.type = ORDER_WORK, .targetKind = TARGET_TILE}}));
    CHECK(!Do(g, (GameCommand){.type = CMD_ORDER,
                               .entity = worker,
                               .order = {.type = ORDER_WORK,
                                         .targetKind = TARGET_BUILDING,
                                         .target = GameBuildingRef(g, 0)}}));
    g->factions[0].age = SETTLED_AGE;
    int school = BuildingsAdd(g, 0, TRAINING_GROUND, 24, 16, true);
    for (int i = 0; i < 2; i++)
        CHECK(Do(g, (GameCommand){.type = CMD_TRAIN,
                                  .a = SPEARMAN,
                                  .entity = GameCitizenRef(g, i),
                                  .order = {.target = GameBuildingRef(g, school)}}));
    int wood = g->factions[0].reserved[WOOD];
    g->citizens[1].hp = 0;
    CitizensResolveDeaths(g);
    CHECK(g->buildings[school].queueCount == 1);
    CHECK(g->factions[0].reserved[WOOD] == wood - UNITS[SPEARMAN].cost[WOOD]);
    CHECK(g->factions[0].lostGoods[WOOD] == UNITS[SPEARMAN].cost[WOOD]);
    CHECK(CitizensAdd(g, 0, IDLE) == 1);
    CHECK(g->buildings[school].queue[0].citizen.slot == 0);
    g->buildings[school].queue[0].citizen = GameCitizenRef(g, 8);
    CHECK(!GameValidate(g, NULL, 0));
    g->buildings[school].queue[0].citizen = worker;
    ProductionCancel(g, &g->buildings[school], 0, 0);
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = worker,
                              .order = {.type = ORDER_ATTACK_MOVE, .x = 22, .y = 30}}));
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = worker,
                              .append = 1,
                              .order = {.type = ORDER_MOVE, .x = 24, .y = 30}}));
    Run(g, 400);
    CHECK(!g->citizens[0].orderCount && g->citizens[0].x / 256 == 24);
    g->citizens[0].orders[0] =
        (UnitOrder){.type = ORDER_REPAIR, .targetKind = TARGET_TILE, .target = {-1, 0}};
    g->citizens[0].orderCount = 1;
    CHECK(!GameValidate(g, NULL, 0));
    OrdersClear(g, &g->citizens[0]);
    // Own gates are passable, hostile gates block actual unit routes.
    for (int y = 0; y < MAP_SIZE; y++)
        BuildingsAdd(g, 0, y == 24 ? GATE : PALISADE, 43, y, true);
    g->citizens[0].x = 42 * 256;
    g->citizens[0].y = 24 * 256;
    g->citizens[8].x = 44 * 256;
    g->citizens[8].y = 24 * 256;
    CHECK(WorldRoute(g, &g->citizens[0], GameTile(44, 24)));
    CHECK(!WorldRoute(g, &g->citizens[8], GameTile(42, 24)));
    g->factions[1].raidActive = 1;
    g->factions[1].raidSize = 1;
    g->tick = 2400;
    g->citizens[8].unitType = SPEARMAN;
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .owner = 1,
                              .entity = GameCitizenRef(g, 8),
                              .order = {.type = ORDER_PLUNDER,
                                        .targetKind = TARGET_BUILDING,
                                        .target = GameBuildingRef(g, 0)}}));
    RivalsUpdate(g);
    CHECK(!g->factions[1].raidActive && g->factions[0].raidResolved);
    CHECK(g->citizens[8].orders[0].type == ORDER_RETREAT);
    Stock(g, 0, FOOD, 1000);
    CHECK(Do(g, (GameCommand){.type = CMD_TRUCE}));
    g->tick = g->truceUntil;
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .owner = 1,
                              .entity = GameCitizenRef(g, 8),
                              .order = {.type = ORDER_ATTACK,
                                        .targetKind = TARGET_BUILDING,
                                        .target = GameBuildingRef(g, 0)}}));
    Valid(g);
    free(g);
}
static void MetalsAndProjects(void)
{
    GameState *g = New();
    Quiet(g);
    g->tick = WINTER_END;
    g->winterSurvived = 1;
    g->factions[0].age = FARMING_AGE;
    g->factions[0].discovery[METALLURGY] = 1;
    for (int r = 0; r < RESOURCE_COUNT; r++)
        Stock(g, 0, r, 1000);
    for (int x = 16; x <= 28; x += 4)
        for (int y = 16; y <= 28; y += 4)
            if (GameCanPlace(g, SHELTER, x, y, NULL, 0))
                BuildingsAdd(g, 0, SHELTER, x, y, true);
    while (GamePopulation(g) < 32)
        CitizensAdd(g, 0, IDLE);
    CHECK(!Do(g, (GameCommand){.type = CMD_ADVANCE}));
    int smelter = BuildingsAdd(g, 0, SMELTER, 32, 16, true);
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = GameCitizenRef(g, 0),
                              .order = {.type = ORDER_WORK,
                                        .targetKind = TARGET_BUILDING,
                                        .target = GameBuildingRef(g, smelter)}}));
    Run(g, 1600);
    CHECK(g->factions[0].bronzeDelivered >= 2);
    CHECK(g->factions[0].spent[ORE] >= 4);
    CHECK(Do(g, (GameCommand){.type = CMD_ADVANCE}));
    int paid = g->factions[0].stock[BRONZE];
    CHECK(!Do(g, (GameCommand){.type = CMD_ADVANCE}));
    CHECK(g->factions[0].stock[BRONZE] == paid);
    CHECK(Do(g, (GameCommand){.type = CMD_CANCEL, .a = -2}));
    CHECK(g->factions[0].stock[BRONZE] == paid + AGES[BRONZE_AGE].cost[BRONZE]);
    CHECK(Do(g, (GameCommand){.type = CMD_ADVANCE}));
    CHECK(Do(g, (GameCommand){.type = CMD_ROLE, .entity = GameCitizenRef(g, 1), .a = BUILDER}));
    Run(g, 4000);
    CHECK(g->factions[0].age == BRONZE_AGE);
    CHECK(g->buildings[g->factions[0].center].upgraded == BRONZE_AGE);
    g->factions[0].discovery[STONEWORK] = 1;
    CHECK(GameBuildingMaxHealth(g, &g->buildings[smelter]) == 4500);
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = GameCitizenRef(g, 1),
                              .order = {.type = ORDER_REPAIR,
                                        .targetKind = TARGET_BUILDING,
                                        .target = GameBuildingRef(g, smelter)}}));
    Run(g, 800);
    CHECK(g->buildings[smelter].hp == 4500);
    Valid(g);
    free(g);
}
static void Survival(void)
{
    GameState *g = New();
    Quiet(g);
    EconomySpend(g, 0, FOOD, g->factions[0].stock[FOOD]);
    EconomySpend(g, 0, WOOD, g->factions[0].stock[WOOD]);
    Run(g, 2400);
    CHECK(g->factions[0].foodWarning && g->factions[0].coldWarning);
    CHECK(g->citizens[0].hp < 1000);
    int hp = g->citizens[0].hp;
    Stock(g, 0, FOOD, 300);
    Stock(g, 0, WOOD, 300);
    for (int x = 16; x <= 24; x += 4)
        BuildingsAdd(g, 0, SHELTER, x, 16, true);
    Run(g, 1800);
    CHECK(g->citizens[0].hp > hp);
    CHECK(g->factions[0].shortageRecoveries > 0);
    Valid(g);
    for (int i = 0; i < g->citizenCount; i++)
        if (!g->citizens[i].owner)
            g->citizens[i].hp = 0;
    Run(g, 1);
    CHECK(g->lost);
    free(g);
}
static void Progress(void)
{
    GameState *g = New();
    Quiet(g);
    CHECK(!Do(g, (GameCommand){.type = CMD_ADVANCE}));
    g->tick = WINTER_END;
    g->winterSurvived = 1;
    for (int x = 16; x <= 24; x += 4)
        BuildingsAdd(g, 0, SHELTER, x, 16, true);
    Stock(g, 0, FOOD, 1000);
    Stock(g, 0, WOOD, 1000);
    Stock(g, 0, STONE, 1000);
    Stock(g, 0, TOOLS, 100);
    CHECK(Do(g, (GameCommand){.type = CMD_ADVANCE}));
    CHECK(!Do(g, (GameCommand){.type = CMD_ADVANCE}));
    g->citizens[0].role = BUILDER;
    Run(g, 2000);
    CHECK(g->factions[0].age == SETTLED_AGE);
    int workshop = BuildingsAdd(g, 0, WORKSHOP, 16, 24, true);
    g->citizens[1].role = CRAFTER;
    CHECK(Do(g, (GameCommand){.type = CMD_RESEARCH, .a = FARMING}));
    Run(g, 2700);
    CHECK(g->factions[0].discovery[FARMING]);
    CHECK(!GameCanPlace(g, FARM, 28, 20, NULL, 0));
    g->factions[0].age = FARMING_AGE;
    CHECK(Do(g, (GameCommand){.type = CMD_BUILD, .a = FARM, .b = 28, .c = 20}));
    g->citizens[2].role = FARMER;
    Run(g, 3400);
    CHECK(g->factions[0].harvestDelivered);
    CHECK(g->factions[0].produced[FOOD] >= 24);
    CHECK(Do(g, (GameCommand){.type = CMD_RESEARCH, .a = METALLURGY}));
    BuildingsDestroy(g, workshop);
    CHECK(g->factions[0].research == -1);
    CHECK(g->factions[0].researchReserve[TOOLS] == 0);
    Valid(g);
    free(g);
}
static int CounterDamage(int attacker, int defender)
{
    GameState *g = New();
    Quiet(g);
    Citizen *a = &g->citizens[0], *b = &g->citizens[8];
    a->x = 87 * 256;
    a->y = 88 * 256;
    b->x = 88 * 256;
    b->y = 88 * 256;
    a->unitType = attacker;
    b->unitType = defender;
    CombatUpdate(g);
    int result = 1000 - b->hp;
    free(g);
    return result;
}
static void Combat(void)
{
    CHECK(CounterDamage(SPEARMAN, SLINGER) > CounterDamage(SPEARMAN, AXEMAN));
    CHECK(CounterDamage(AXEMAN, SPEARMAN) > CounterDamage(AXEMAN, SLINGER));
    CHECK(CounterDamage(SLINGER, AXEMAN) > CounterDamage(SLINGER, SPEARMAN));
    GameState *g = New();
    Quiet(g);
    Citizen *c = &g->citizens[0];
    c->unitType = AXEMAN;
    c->role = GUARD;
    c->x = 69 * 256;
    c->y = 70 * 256;
    int depot = 3;
    int food = g->factions[1].stock[FOOD];
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = GameCitizenRef(g, 0),
                              .order = {.type = ORDER_PLUNDER,
                                        .targetKind = TARGET_BUILDING,
                                        .target = GameBuildingRef(g, depot),
                                        .x = 70,
                                        .y = 70}}));
    CombatUpdate(g);
    CHECK(EconomyCargo(c) == 18);
    CHECK(g->factions[1].stock[FOOD] == food - 18);
    CHECK(g->factions[0].imported[FOOD] == 18);
    int stored = g->factions[0].stock[FOOD];
    CHECK(stored == 60);
    EconomyDeliver(g, c);
    CHECK(g->factions[0].stock[FOOD] == 78);
    Valid(g);
    Stock(g, 0, FOOD, 1000);
    CHECK(Do(g, (GameCommand){.type = CMD_TRUCE}));
    CHECK(!Do(g, (GameCommand){.type = CMD_ORDER,
                               .entity = GameCitizenRef(g, 0),
                               .order = {.type = ORDER_ATTACK,
                                         .targetKind = TARGET_BUILDING,
                                         .target = GameBuildingRef(g, depot),
                                         .x = 70,
                                         .y = 70}}));
    CHECK(!Do(g, (GameCommand){.type = CMD_TREATY}));
    g->factions[0].age = BRONZE_AGE;
    Stock(g, 0, BRONZE, 40);
    g->tick = 6000;
    CHECK(Do(g, (GameCommand){.type = CMD_TREATY}));
    CHECK(g->treaty);
    Valid(g);
    free(g);
}
static void Paths(void)
{
    GameState *g = New();
    Quiet(g);
    int ids[MAP_SIZE];
    for (int y = 0; y < MAP_SIZE; y++)
        ids[y] = BuildingsAdd(g, 0, PALISADE, 43, y, true);
    int path[MAX_PATH];
    CHECK(GameFindPath(g, GameTile(20, 24), GameTile(60, 24), path, MAX_PATH) < 0);
    BuildingsDestroy(g, ids[24]);
    CHECK(GameFindPath(g, GameTile(20, 24), GameTile(60, 24), path, MAX_PATH) > 0);
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = GameCitizenRef(g, 0),
                              .order = {.type = ORDER_MOVE, .x = 60, .y = 24}}));
    Run(g, 1000);
    CHECK(g->citizens[0].x / 256 == 60);
    Valid(g);
    free(g);
}
static void Population(void)
{
    GameState *g = New();
    Quiet(g);
    for (int o = 0; o < 2; o++)
        while (GamePopulationFor(g, o) < POPULATION_LIMIT)
            CHECK(CitizensAdd(g, o, IDLE) >= 0);
    CHECK(CitizensAdd(g, 0, IDLE) == -1);
    CHECK(g->citizenCount == MAX_CITIZENS);
    EntityRef old = GameCitizenRef(g, 0);
    g->citizens[0].hp = 0;
    CitizensResolveDeaths(g);
    CHECK(CitizensAdd(g, 0, IDLE) == 0);
    CHECK(!GameCitizenValid(g, old));
    Valid(g);
    free(g);
}
static void Persistence(void)
{
    GameState *g = New(), *h = New();
    char why[256];
    CHECK(Do(g, (GameCommand){.type = CMD_BUILD, .a = SHELTER, .b = 18, .c = 18}));
    Run(g, 100);
    uint64_t hash = GameHash(g);
    CHECK(GameSave(g, "rts-test.gfv2", why, sizeof(why)));
    CHECK(GameLoad(h, "rts-test.gfv2", why, sizeof(why)));
    CHECK(GameHash(h) == hash);
    CHECK(!GameLoad(h, OLD_SAVE_FIXTURE, why, sizeof(why)));
    CHECK(GameHash(h) == hash);
    FILE *f = fopen("rts-test.gfv2.tmp", "wb");
    fputs("interrupted", f);
    fclose(f);
    CHECK(GameLoad(h, "rts-test.gfv2", why, sizeof(why)));
    g->factions[0].stock[FOOD]++;
    CHECK(!GameSave(g, "rts-test.gfv2", why, sizeof(why)));
    CHECK(GameLoad(h, "rts-test.gfv2", why, sizeof(why)));
    CHECK(GameHash(h) == hash);
    g->factions[0].stock[FOOD]--;
    f = fopen("rts-invalid.gfv2", "wb");
    fputs("invalid", f);
    fclose(f);
    CHECK(!GameLoad(h, "rts-invalid.gfv2", why, sizeof(why)));
    CHECK(GameHash(h) == hash);
    Run(g, 500);
    Run(h, 500);
    CHECK(GameHash(g) == GameHash(h));
    Valid(g);
    remove("rts-test.gfv2");
    remove("rts-test.gfv2.tmp");
    remove("rts-invalid.gfv2");
    free(g);
    free(h);
}
static void ComplexPersistence(void)
{
    GameState *g = New(), *h = New();
    Quiet(g);
    g->tick = 9000;
    g->winterSurvived = 1;
    g->factions[0].age = SETTLED_AGE;
    for (int r = 0; r < RESOURCE_COUNT; r++)
        Stock(g, 0, r, 1000);
    BuildingsAdd(g, 0, SHELTER, 16, 16, true);
    BuildingsAdd(g, 0, SHELTER, 16, 20, true);
    BuildingsAdd(g, 0, SHELTER, 16, 24, true);
    int school = BuildingsAdd(g, 0, TRAINING_GROUND, 24, 16, true);
    BuildingsAdd(g, 0, WORKSHOP, 24, 28, true);
    CHECK(Do(g, (GameCommand){.type = CMD_ROLE, .a = CRAFTER, .entity = GameCitizenRef(g, 1)}));
    CHECK(Do(g, (GameCommand){.type = CMD_RESEARCH, .a = FOOD_STORAGE}));
    CHECK(Do(g, (GameCommand){.type = CMD_RECRUIT, .entity = GameBuildingRef(g, 0)}));
    CHECK(Do(g, (GameCommand){.type = CMD_TRAIN,
                              .a = SPEARMAN,
                              .entity = GameCitizenRef(g, 0),
                              .order = {.target = GameBuildingRef(g, school)}}));
    CHECK(Do(g, (GameCommand){.type = CMD_BUILD, .a = SHELTER, .b = 20, .c = 28}));
    g->factions[1].raidWarning = 1;
    g->factions[1].nextRaid = 9600;
    CHECK(Do(g, (GameCommand){.type = CMD_ORDER,
                              .entity = GameCitizenRef(g, 2),
                              .order = {.type = ORDER_GATHER, .x = 14, .y = 22}}));
    Run(g, 100);
    char why[256];
    CHECK(GameSave(g, "complex.gfv2", why, sizeof(why)));
    CHECK(GameLoad(h, "complex.gfv2", why, sizeof(why)));
    CHECK(GameHash(g) == GameHash(h));
    Run(g, 1000);
    Run(h, 1000);
    CHECK(GameHash(g) == GameHash(h));
    // A write failure at the temporary-file boundary preserves the usable slot.
    uint64_t savedHash = 0;
    CHECK(GameLoad(h, "complex.gfv2", why, sizeof(why)));
    savedHash = GameHash(h);
    CHECK(mkdir("complex.gfv2.tmp", 0700) == 0);
    CHECK(!GameSave(g, "complex.gfv2", why, sizeof(why)));
    CHECK(GameLoad(h, "complex.gfv2", why, sizeof(why)) && GameHash(h) == savedHash);
    CHECK(rmdir("complex.gfv2.tmp") == 0);
    remove("complex.gfv2");
    Valid(g);
    free(g);
    free(h);
}
static void RenderStream(GameState *g, int frames, double seconds, int speed)
{
    double accumulator = 0;
    uint64_t previous = UINT64_MAX;
    g->factions[1].aiEnabled = 1;
    for (int i = 0; i < frames; i++)
    {
        if (g->tick != previous)
        {
            if (g->tick == 0)
                Do(g, (GameCommand){.type = CMD_BUILD, .a = SHELTER, .b = 18, .c = 18});
            if (g->tick == 100)
                Do(g, (GameCommand){.type = CMD_ORDER,
                                    .entity = GameCitizenRef(g, 1),
                                    .order = {.type = ORDER_GATHER, .x = 14, .y = 22}});
            if (g->tick == 250)
                Do(g, (GameCommand){.type = CMD_ROLE, .entity = GameCitizenRef(g, 3), .a = FORAGER});
            previous = g->tick;
        }
        GameAdvance(g, seconds, speed, &accumulator);
    }
}
static void Determinism(void)
{
    GameState *a = New(), *b = New(), *c = New(), *d = New();
    RenderStream(a, 5400, 1. / 60, 1);
    RenderStream(b, 2700, 1. / 30, 1);
    RenderStream(c, 1350, 1. / 60, 4);
    RenderStream(d, 2700, 1. / 60, 2);
    CHECK(a->tick == 900);
    CHECK(GameHash(a) == GameHash(b));
    CHECK(GameHash(a) == GameHash(c));
    CHECK(GameHash(a) == GameHash(d));
    uint64_t hash = GameHash(a);
    double aa = 0;
    GameAdvance(a, 1, 0, &aa);
    CHECK(GameHash(a) == hash);
    free(a);
    free(b);
    free(c);
    free(d);
}
int main(void)
{
    Foundation();
    Orders();
    Queues();
    Boundaries();
    MetalsAndProjects();
    Survival();
    Progress();
    Combat();
    Paths();
    Population();
    Persistence();
    ComplexPersistence();
    Determinism();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
