#ifndef GREENFIELD_GAME_H
#define GREENFIELD_GAME_H
#include <stdbool.h>
#include <stdint.h>
#define MAP_SIZE 96
#define TILE_COUNT (MAP_SIZE * MAP_SIZE)
#define FACTION_COUNT 2
#define POPULATION_LIMIT 96
#define MAX_CITIZENS (FACTION_COUNT * POPULATION_LIMIT)
#define MAX_BUILDINGS 512
#define MAX_PATH 512
#define MAX_ORDERS 16
#define MAX_QUEUE 8
#define TICKS_SECOND 10
#define DAY_TICKS 1800
#define WINTER_END 7200
#define SAVE_VERSION 2

typedef enum
{
    FOOD,
    WOOD,
    STONE,
    HIDES,
    TOOLS,
    ORE,
    BRONZE,
    RESOURCE_COUNT
} Resource;
typedef enum
{
    BUILDER,
    FORAGER,
    WOODCUTTER,
    STONE_GATHERER,
    HUNTER,
    CRAFTER,
    FARMER,
    GUARD,
    MINER,
    ROLE_COUNT,
    IDLE = ROLE_COUNT
} Role;
typedef enum
{
    GRASS,
    FERTILE,
    WATER,
    SHALLOWS,
    ROCK_GROUND
} Terrain;
typedef enum
{
    NODE_NONE,
    NODE_TREE,
    NODE_STONE,
    NODE_BERRIES,
    NODE_GAME,
    NODE_ORE
} Node;
typedef enum
{
    HEARTH,
    SHELTER,
    STOCKPILE,
    FORAGING_POST,
    HUNTING_CAMP,
    WORKSHOP,
    GRANARY,
    FARM,
    WATCH_POST,
    PALISADE,
    GATE,
    TRAINING_GROUND,
    SMELTER,
    BUILDING_COUNT
} BuildingType;
typedef enum
{
    IMPROVED_TOOLS,
    INSULATION,
    FOOD_STORAGE,
    FARMING,
    METALLURGY,
    CROP_YIELD,
    STONEWORK,
    DISCOVERY_COUNT
} Discovery;
typedef enum
{
    ICE_AGE,
    SETTLED_AGE,
    FARMING_AGE,
    BRONZE_AGE,
    AGE_COUNT
} Age;
typedef enum
{
    CIVILIAN,
    SPEARMAN,
    SLINGER,
    AXEMAN,
    UNIT_COUNT
} UnitType;
typedef enum
{
    JOB_IDLE,
    JOB_GATHER,
    JOB_BUILD,
    JOB_DELIVER,
    JOB_RESEARCH,
    JOB_CRAFT,
    JOB_PLANT,
    JOB_HARVEST,
    JOB_GUARD,
    JOB_FIGHT,
    JOB_MOVE,
    JOB_REPAIR,
    JOB_SMELT,
    JOB_COUNT
} Job;
typedef enum
{
    ORDER_AUTO,
    ORDER_MOVE,
    ORDER_GATHER,
    ORDER_BUILD,
    ORDER_REPAIR,
    ORDER_DELIVER,
    ORDER_ATTACK,
    ORDER_ATTACK_MOVE,
    ORDER_HOLD,
    ORDER_DEFEND,
    ORDER_RETREAT,
    ORDER_WORK,
    ORDER_PLUNDER,
    ORDER_COUNT
} OrderType;
typedef enum
{
    TARGET_TILE,
    TARGET_CITIZEN,
    TARGET_BUILDING
} TargetKind;
typedef enum
{
    QUEUE_RECRUIT,
    QUEUE_TRAIN,
    QUEUE_EQUIP
} QueueType;
typedef enum
{
    CMD_BUILD,
    CMD_ASSIGN,
    CMD_AREA,
    CMD_PRIORITY,
    CMD_RESEARCH,
    CMD_UPGRADE,
    CMD_TRUCE,
    CMD_RALLY,
    CMD_ORDER,
    CMD_RECRUIT,
    CMD_TRAIN,
    CMD_DEMOBILIZE,
    CMD_ADVANCE,
    CMD_CANCEL,
    CMD_TREATY,
    CMD_EQUIP,
    CMD_STOP,
    CMD_ROLE
} CommandType;
typedef struct
{
    int slot, generation;
} EntityRef;
typedef struct
{
    int type, targetKind, x, y;
    EntityRef target;
} UnitOrder;
typedef struct
{
    int terrain, node, amount, occupied, natural, regrowth;
} Tile;
typedef struct
{
    int type, unitType, progress, duration, cost[RESOURCE_COUNT];
    EntityRef citizen;
} Production;
typedef struct
{
    int alive, owner, generation, type, x, y, hp, progress, complete, upgraded, crop, cropProgress, priority;
    int started, reserve[RESOURCE_COUNT], queueCount;
    Production queue[MAX_QUEUE];
} Building;
typedef struct
{
    int alive, owner, generation, role, unitType, equipped, x, y, hp, hunger, cold, job, target, work;
    int carry[RESOURCE_COUNT], harvestCargo, path[MAX_PATH], pathLength, pathIndex, pathRevision, retry,
        attackCooldown;
    int orderCount, training, recipe[RESOURCE_COUNT];
    UnitOrder orders[MAX_ORDERS];
} Citizen;
typedef struct
{
    int stock[RESOURCE_COUNT], initial[RESOURCE_COUNT], produced[RESOURCE_COUNT], spent[RESOURCE_COUNT];
    int imported[RESOURCE_COUNT], exported[RESOURCE_COUNT], reserved[RESOURCE_COUNT],
        lostGoods[RESOURCE_COUNT];
    int areaX[ROLE_COUNT], areaY[ROLE_COUNT], priority[ROLE_COUNT], rallyX, rallyY;
    UnitOrder rallyOrder;
    int discovery[DISCOVERY_COUNT], research, researchProgress, researchBuilding,
        researchReserve[RESOURCE_COUNT];
    int age, advancing, advanceProgress, advanceReserve[RESOURCE_COUNT], center;
    int harvestDelivered, bronzeDelivered, village, villageTick, arrivals, deaths;
    int warmth, foodWarning, coldWarning, shortageTicks, shortageRecoveries, hadShortage;
    int aiEnabled, nextDecision, nextRaid, raidWarning, raidActive, raidCount, raidResolved, defeated;
    int raidStarted, raidSize;
} Faction;
typedef struct
{
    uint64_t tick;
    uint32_t random;
    Tile tiles[TILE_COUNT];
    Citizen citizens[MAX_CITIZENS];
    Building buildings[MAX_BUILDINGS];
    Faction factions[FACTION_COUNT];
    int citizenCount, buildingCount, mapRevision, winterSurvived, won, lost;
    int truceUntil, treaty, lastHostileTick;
    char events[8][128];
    int eventCount;
} GameState;
// The owner is explicit even for AI commands. Entity handles survive slot reuse.
typedef struct
{
    int type, a, b, c, owner;
    EntityRef entity;
    UnitOrder order;
    int append;
} GameCommand;
typedef struct
{
    const char *name, *description;
    int w, h, cost[RESOURCE_COUNT], labor;
} BuildingDef;
typedef struct
{
    const char *name, *description;
    int cost[RESOURCE_COUNT], labor;
} DiscoveryDef;
typedef struct
{
    const char *name;
    int cost[RESOURCE_COUNT], labor, population;
} AgeDef;
typedef struct
{
    int cost[RESOURCE_COUNT], labor, damage, buildingDamage, range, cooldown, moveStep, armor;
} UnitDef;
typedef struct
{
    int cost[RESOURCE_COUNT], resource, quantity, labor;
} RecipeDef;
extern const RecipeDef TOOL_RECIPE, BRONZE_RECIPE;
extern const BuildingDef BUILDINGS[BUILDING_COUNT];
extern const DiscoveryDef DISCOVERIES[DISCOVERY_COUNT];
extern const AgeDef AGES[AGE_COUNT];
extern const UnitDef UNITS[UNIT_COUNT];
extern const int RECRUIT_COST[RESOURCE_COUNT], EQUIPMENT_COST[RESOURCE_COUNT];
extern const int RECRUIT_TICKS[AGE_COUNT];
extern const char *RESOURCE_NAMES[RESOURCE_COUNT], *ROLE_NAMES[ROLE_COUNT], *JOB_NAMES[JOB_COUNT],
    *UNIT_NAMES[UNIT_COUNT];
void GameInit(GameState *g);
void GameTick(GameState *g);
void GameAdvance(GameState *g, double seconds, int speed, double *accumulator);
bool GameCommandApply(GameState *g, GameCommand command, char *reason, int size);
bool GameCanPlaceFor(const GameState *g, int owner, int type, int x, int y, char *reason, int size);
bool GameCanAffordFor(const GameState *g, int owner, const int cost[RESOURCE_COUNT]);
int GamePopulationFor(const GameState *g, int owner);
int GameHousingFor(const GameState *g, int owner);
int GameRoleCountFor(const GameState *g, int owner, int role);
int GameBuildingCountFor(const GameState *g, int owner, int type, bool completeOnly);
int GameFoodDaysTenthsFor(const GameState *g, int owner);
int GameSiteControl(const GameState *g, int owner);
bool GameCanAdvance(const GameState *g, int owner, char *reason, int size);
bool GameCanTreaty(const GameState *g, char *reason, int size);
int GameTruceCost(const GameState *g);
int GameBuildingMaxHealth(const GameState *g, const Building *b);
EntityRef GameCitizenRef(const GameState *g, int id);
EntityRef GameBuildingRef(const GameState *g, int id);
bool GameCitizenValid(const GameState *g, EntityRef ref);
bool GameBuildingValid(const GameState *g, EntityRef ref);
int GameTile(int x, int y);
bool GameWalkable(const GameState *g, int x, int y);
int GameFindPath(const GameState *g, int start, int target, int *path, int capacity);
void GameEvent(GameState *g, const char *text);
bool GameValidate(const GameState *g, char *reason, int size);
bool GameSave(const GameState *g, const char *path, char *reason, int size);
bool GameLoad(GameState *g, const char *path, char *reason, int size);
uint64_t GameHash(const GameState *g);
// Presentation convenience queries always address the human faction.
static inline int GamePopulation(const GameState *g)
{
    return GamePopulationFor(g, 0);
}
static inline int GameHousing(const GameState *g)
{
    return GameHousingFor(g, 0);
}
static inline int GameRoleCount(const GameState *g, int r)
{
    return GameRoleCountFor(g, 0, r);
}
static inline int GameBuildingCount(const GameState *g, int t, bool c)
{
    return GameBuildingCountFor(g, 0, t, c);
}
static inline int GameFoodDaysTenths(const GameState *g)
{
    return GameFoodDaysTenthsFor(g, 0);
}
static inline int GameStage(const GameState *g)
{
    return g->factions[0].age;
}
static inline bool GameCanAfford(const GameState *g, const int *c)
{
    return GameCanAffordFor(g, 0, c);
}
static inline bool GameCanPlace(const GameState *g, int t, int x, int y, char *r, int n)
{
    return GameCanPlaceFor(g, 0, t, x, y, r, n);
}
#endif
