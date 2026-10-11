#include "game.h"
#include "sim/buildings.h"
#include "sim/jobs.h"
#include "sim/production.h"
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
            fprintf(stderr, "FAIL buildings:%d %s\n", __LINE__, #x);                                     \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)

static GameState *New(void)
{
    GameState *g = calloc(1, sizeof(*g));
    GameInit(g);
    g->factions[1].aiEnabled = 0;
    for (int i = 0; i < TILE_COUNT; i++)
    {
        g->tiles[i].terrain = GRASS;
        g->tiles[i].node = g->tiles[i].natural = NODE_NONE;
        g->tiles[i].amount = 0;
    }
    for (int i = 0; i < g->citizenCount; i++)
    {
        g->citizens[i].role = IDLE;
        JobsReset(&g->citizens[i]);
    }
    for (int o = 0; o < FACTION_COUNT; o++)
    {
        g->factions[o].age = SETTLED_AGE;
        for (int r = 0; r < RESOURCE_COUNT; r++)
        {
            g->factions[o].stock[r] += 1000;
            g->factions[o].initial[r] += 1000;
        }
    }
    return g;
}

static bool Apply(GameState *g, GameCommand cmd)
{
    return GameCommandApply(g, cmd, NULL, 0);
}

static void Valid(const GameState *g)
{
    char why[256];
    bool ok = GameValidate(g, why, sizeof(why));
    if (!ok)
        fprintf(stderr, "%s\n", why);
    CHECK(ok);
}

static void PlacementAndMovement(void)
{
    GameState *g = New(), *copy = calloc(1, sizeof(*copy));
    for (int owner = 0; owner < FACTION_COUNT; owner++)
    {
        int x = 35 + owner * 15, y = 35;
        CHECK(Apply(g, (GameCommand){.type = CMD_BUILD, .owner = owner, .a = WORKSHOP,
                                    .b = x, .c = y, .rotation = 1}));
        int id = g->tiles[GameTile(x, y)].occupied;
        Building *b = &g->buildings[id];
        EntityRef ref = GameBuildingRef(g, id);
        CHECK(GameBuildingWidth(b) == 2 && GameBuildingHeight(b) == 3);
        CHECK(g->tiles[GameTile(x + 1, y + 2)].occupied == id);
        CHECK(g->tiles[GameTile(x + 2, y)].occupied == -1);
        int stored = g->factions[owner].stock[WOOD], reserved = g->factions[owner].reserved[WOOD];
        Citizen *c = &g->citizens[owner * 8];
        CHECK(Apply(g, (GameCommand){.type = CMD_ORDER, .owner = owner, .entity = GameCitizenRef(g, owner * 8),
                                    .order = {.type = ORDER_BUILD, .targetKind = TARGET_BUILDING,
                                              .target = ref, .x = x, .y = y}}));
        for (int tick = 0; tick < 1500 && b->progress < 30; tick++)
            GameTick(g);
        CHECK(b->progress >= 30 && !b->complete);
        int progress = b->progress, hp = b->hp;
        CHECK(Apply(g, (GameCommand){.type = CMD_MOVE_BUILDING, .owner = owner, .entity = ref,
                                    .a = x + 5, .b = y + 5, .rotation = 0}));
        CHECK(GameBuildingValid(g, ref) && b->progress == progress && b->hp == hp);
        CHECK(b->rotation == 0 && GameBuildingWidth(b) == 3 && GameBuildingHeight(b) == 2);
        CHECK(g->tiles[GameTile(x, y)].occupied == -1);
        CHECK(g->tiles[GameTile(x + 7, y + 6)].occupied == id);
        CHECK(g->factions[owner].stock[WOOD] <= stored); // Only winter upkeep may spend wood.
        CHECK(g->factions[owner].reserved[WOOD] == reserved);
        CHECK(c->orders[0].target.generation == ref.generation && c->orders[0].x == b->x);
        GameTick(g);
        CHECK(b->progress == progress); // No labor from the old site.
        for (int tick = 0; tick < 2000 && !b->complete; tick++)
            GameTick(g);
        CHECK(b->complete && g->factions[owner].reserved[WOOD] == 0);
        stored = g->factions[owner].stock[WOOD];
        CHECK(Apply(g, (GameCommand){.type = CMD_ROTATE_BUILDING, .owner = owner, .entity = ref}));
        CHECK(b->rotation == 1 && g->factions[owner].stock[WOOD] == stored);
        Valid(g);
    }
    char why[256];
    CHECK(GameSave(g, "building-edits.save", why, sizeof(why)));
    CHECK(GameLoad(copy, "building-edits.save", why, sizeof(why)));
    CHECK(GameHash(g) == GameHash(copy));
    uint64_t saved = GameHash(copy);
    g->buildings[0].rotation = 2;
    CHECK(!GameSave(g, "building-edits.save", why, sizeof(why)));
    CHECK(GameLoad(copy, "building-edits.save", why, sizeof(why)) && GameHash(copy) == saved);
    g->buildings[0].rotation = 0;
    for (int i = 0; i < 100; i++)
    {
        GameTick(g);
        GameTick(copy);
    }
    CHECK(GameHash(g) == GameHash(copy));
    remove("building-edits.save");
    free(copy);
    free(g);
}

static void RejectionAndDemolition(void)
{
    GameState *g = New();
    int id = BuildingsAdd(g, 0, WORKSHOP, 35, 35, true);
    EntityRef ref = GameBuildingRef(g, id);
    GameCommand move = {.type = CMD_MOVE_BUILDING, .entity = ref, .a = 40, .b = 35, .rotation = 1};
    uint64_t before = GameHash(g);
    move.owner = 1;
    CHECK(!Apply(g, move) && GameHash(g) == before);
    move.owner = 0;
    move.rotation = 2;
    CHECK(!Apply(g, move) && GameHash(g) == before);
    move.rotation = 1;
    move.a = 95;
    CHECK(!Apply(g, move) && GameHash(g) == before);
    move.a = 40;
    const int terrain[] = {WATER, SHALLOWS};
    for (int i = 0; i < 2; i++)
    {
        g->tiles[GameTile(40, 35)].terrain = terrain[i];
        before = GameHash(g);
        CHECK(!Apply(g, move) && GameHash(g) == before);
    }
    g->tiles[GameTile(40, 35)].terrain = GRASS;
    g->tiles[GameTile(40, 35)].node = NODE_TREE;
    before = GameHash(g);
    CHECK(!Apply(g, move) && GameHash(g) == before);
    g->tiles[GameTile(40, 35)].node = NODE_NONE;
    g->citizens[0].x = 40 * 256;
    g->citizens[0].y = 35 * 256;
    before = GameHash(g);
    CHECK(!Apply(g, move) && GameHash(g) == before);
    g->citizens[0].x = 20 * 256;
    g->citizens[0].y = 20 * 256;
    int blocker = BuildingsAdd(g, 0, PALISADE, 36, 37, true);
    before = GameHash(g);
    CHECK(!Apply(g, (GameCommand){.type = CMD_ROTATE_BUILDING, .entity = ref}) && GameHash(g) == before);
    BuildingsDestroy(g, blocker);
    CHECK(Apply(g, (GameCommand){.type = CMD_ROTATE_BUILDING, .entity = ref}));
    // Rotated rectangular placement at the right edge must not write a default 3x2 footprint first.
    CHECK(Apply(g, (GameCommand){.type = CMD_BUILD, .a = WORKSHOP, .b = 94, .c = 90, .rotation = 1}));
    int edge = g->tiles[GameTile(95, 92)].occupied;
    CHECK(edge >= 0 && g->buildings[edge].rotation == 1);
    int lost = g->factions[0].lostGoods[WOOD];
    CHECK(Apply(g, (GameCommand){.type = CMD_DEMOLISH, .entity = GameBuildingRef(g, edge)}));
    CHECK(g->factions[0].lostGoods[WOOD] == lost + BUILDINGS[WORKSHOP].cost[WOOD]);
    CHECK(g->tiles[GameTile(95, 92)].occupied == -1);
    CHECK(Apply(g, (GameCommand){.type = CMD_DEMOLISH, .entity = ref}));
    int replacement = BuildingsAdd(g, 0, SHELTER, 35, 35, true);
    CHECK(replacement == id);
    before = GameHash(g);
    CHECK(!Apply(g, move) && GameHash(g) == before);
    CHECK(!Apply(g, (GameCommand){.type = CMD_ROTATE_BUILDING, .entity = ref}) && GameHash(g) == before);
    CHECK(!Apply(g, (GameCommand){.type = CMD_DEMOLISH, .entity = ref}) && GameHash(g) == before);
    Valid(g);
    free(g);
}

static void QueuesAndGates(void)
{
    GameState *g = New();
    BuildingsAdd(g, 0, SHELTER, 30, 30, true);
    BuildingsAdd(g, 0, SHELTER, 32, 30, true);
    BuildingsAdd(g, 0, SHELTER, 34, 30, true);
    EntityRef center = GameBuildingRef(g, g->factions[0].center);
    CHECK(Apply(g, (GameCommand){.type = CMD_RECRUIT, .entity = center}));
    Building *b = &g->buildings[center.slot];
    b->queue[0].progress = 50;
    int reserved = g->factions[0].reserved[FOOD], stock = g->factions[0].stock[FOOD];
    CHECK(Apply(g, (GameCommand){.type = CMD_MOVE_BUILDING, .entity = center, .a = 35, .b = 35}));
    CHECK(b->queueCount == 1 && b->queue[0].progress == 50);
    CHECK(g->factions[0].reserved[FOOD] == reserved && g->factions[0].stock[FOOD] == stock);
    int lost = g->factions[0].lostGoods[FOOD];
    CHECK(Apply(g, (GameCommand){.type = CMD_DEMOLISH, .entity = center}));
    CHECK(b->queueCount == 0 && g->factions[0].reserved[FOOD] == 0);
    CHECK(g->factions[0].lostGoods[FOOD] == lost + reserved && g->factions[0].center == -1);
    Valid(g);
    free(g);
    for (int rotation = 0; rotation < 2; rotation++)
    {
        g = New();
        int gate = -1;
        for (int tile = 0; tile < MAP_SIZE; tile++)
        {
            int id = BuildingsAdd(g, 0, tile == 40 ? GATE : PALISADE,
                                  rotation ? 40 : tile, rotation ? tile : 40, true);
            g->buildings[id].rotation = rotation;
            if (tile == 40)
                gate = id;
        }
        Citizen *friendly = &g->citizens[0], *enemy = &g->citizens[8];
        friendly->x = enemy->x = (rotation ? 38 : 40) * 256;
        friendly->y = enemy->y = (rotation ? 40 : 38) * 256;
        int destination = GameTile(rotation ? 42 : 40, rotation ? 40 : 42);
        CHECK(WorldRoute(g, friendly, destination));
        CHECK(!WorldRoute(g, enemy, destination));
        CHECK(Apply(g, (GameCommand){.type = CMD_DEMOLISH, .entity = GameBuildingRef(g, gate)}));
        CHECK(WorldRoute(g, enemy, destination));
        Valid(g);
        free(g);
    }
}

static void ActiveWork(void)
{
    GameState *g = New();
    int workshop = BuildingsAdd(g, 0, WORKSHOP, 35, 35, true);
    Citizen *crafter = &g->citizens[0];
    crafter->role = CRAFTER;
    crafter->x = 34 * 256;
    crafter->y = 35 * 256;
    CHECK(Apply(g, (GameCommand){.type = CMD_RESEARCH, .a = IMPROVED_TOOLS}));
    JobsUpdate(g);
    CHECK(g->factions[0].researchProgress == 1);
    EntityRef ref = GameBuildingRef(g, workshop);
    int materials = g->factions[0].reserved[WOOD];
    CHECK(Apply(g, (GameCommand){.type = CMD_MOVE_BUILDING, .entity = ref,
                                .a = 50, .b = 50, .rotation = 1}));
    JobsUpdate(g);
    CHECK(g->factions[0].researchProgress == 1 && g->factions[0].reserved[WOOD] == materials);
    for (int tick = 0; tick < 1200 && g->factions[0].researchProgress == 1; tick++)
        GameTick(g);
    CHECK(g->factions[0].researchProgress > 1 && crafter->target == workshop);
    int school = BuildingsAdd(g, 0, TRAINING_GROUND, 35, 35, true);
    Citizen *trainee = &g->citizens[1];
    trainee->x = 34 * 256;
    trainee->y = 35 * 256;
    CHECK(Apply(g, (GameCommand){.type = CMD_TRAIN, .a = SPEARMAN, .entity = GameCitizenRef(g, 1),
                                .order = {.target = GameBuildingRef(g, school)}}));
    ProductionUpdate(g);
    CHECK(g->buildings[school].queue[0].progress == 1);
    CHECK(Apply(g, (GameCommand){.type = CMD_MOVE_BUILDING, .entity = GameBuildingRef(g, school),
                                .a = 60, .b = 50, .rotation = 1}));
    ProductionUpdate(g);
    CHECK(g->buildings[school].queue[0].progress == 1 && trainee->training == school);
    for (int tick = 0; tick < 2000 && trainee->training >= 0; tick++)
        GameTick(g);
    CHECK(trainee->unitType == SPEARMAN && trainee->training == -1);
    CHECK(GamePopulation(g) == 8);
    int lost = g->factions[0].lostGoods[WOOD];
    int researchWood = g->factions[0].researchReserve[WOOD];
    CHECK(Apply(g, (GameCommand){.type = CMD_DEMOLISH, .entity = ref}));
    CHECK(g->factions[0].research == -1 && g->factions[0].researchReserve[WOOD] == 0);
    CHECK(g->factions[0].lostGoods[WOOD] == lost + researchWood);
    CHECK(crafter->job == JOB_IDLE && crafter->target == -1);
    Valid(g);
    free(g);
}

int main(void)
{
    PlacementAndMovement();
    RejectionAndDemolition();
    QueuesAndGates();
    ActiveWork();
    printf("%d building checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
