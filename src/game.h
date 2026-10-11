#ifndef GREENFIELD_GAME_H
#define GREENFIELD_GAME_H
#include <stdbool.h>
#include <stdint.h>
#define MAP_SIZE 48
#define TILE_COUNT (MAP_SIZE * MAP_SIZE)
#define MAX_CITIZENS 64
#define MAX_BUILDINGS 160
#define MAX_RAIDERS 8
#define MAX_PATH 256
#define TICKS_SECOND 10
#define DAY_TICKS 1800
#define WINTER_END 7200
#define ARRIVAL_FIRST 10800
#define ARRIVAL_INTERVAL 3600
#define SAVE_VERSION 1

typedef enum
{
    FOOD,
    WOOD,
    STONE,
    HIDES,
    TOOLS,
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
    NODE_GAME
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
    BUILDING_COUNT
} BuildingType;
typedef enum
{
    IMPROVED_TOOLS,
    INSULATION,
    FOOD_STORAGE,
    FARMING,
    DISCOVERY_COUNT
} Discovery;
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
    JOB_FIGHT
} Job;
typedef enum
{
    CMD_BUILD,
    CMD_ASSIGN,
    CMD_AREA,
    CMD_PRIORITY,
    CMD_RESEARCH,
    CMD_UPGRADE,
    CMD_TRUCE,
    CMD_RALLY
} CommandType;
typedef struct
{
    int terrain, node, amount, occupied;
} Tile;
typedef struct
{
    int type, x, y, progress, complete, upgraded, crop, cropProgress, priority;
} Building;
typedef struct
{
    int alive, role, x, y, hp, hunger, cold, job, target, work, carry[RESOURCE_COUNT], harvestCargo;
    int path[MAX_PATH], pathLength, pathIndex, pathRevision, retry, attackCooldown;
} Citizen;
typedef struct
{
    int active, x, y, hp, retreat, cooldown, stolen;
} Raider;
typedef struct
{
    uint64_t tick;
    uint32_t random;
    Tile tiles[TILE_COUNT];
    Citizen citizens[MAX_CITIZENS];
    Building buildings[MAX_BUILDINGS];
    Raider raiders[MAX_RAIDERS];
    int citizenCount, buildingCount, stock[RESOURCE_COUNT], initial[RESOURCE_COUNT], produced[RESOURCE_COUNT],
        spent[RESOURCE_COUNT], stolen[RESOURCE_COUNT];
    int areaX[ROLE_COUNT], areaY[ROLE_COUNT], priority[ROLE_COUNT], rallyX, rallyY, mapRevision;
    int discovery[DISCOVERY_COUNT], research, researchProgress;
    int winterSurvived, harvestDelivered, raidResolved, won, lost;
    int nextArrival, arrivals, deaths, nextRaid, raidWarning, raidActive, raidLosses, raidLoot, truceUntil,
        raidCount;
    int warmth, foodWarning, coldWarning, shortageTicks, shortageRecoveries, hadShortage;
    char events[8][128];
    int eventCount;
} GameState;
typedef struct
{
    int type, a, b, c;
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
extern const BuildingDef BUILDINGS[BUILDING_COUNT];
extern const DiscoveryDef DISCOVERIES[DISCOVERY_COUNT];
extern const char *RESOURCE_NAMES[RESOURCE_COUNT], *ROLE_NAMES[ROLE_COUNT], *JOB_NAMES[11];
void GameInit(GameState *g);
void GameTick(GameState *g);
void GameAdvance(GameState *g, double seconds, int speed, double *accumulator);
bool GameCommandApply(GameState *g, GameCommand command, char *reason, int size);
bool GameCanPlace(const GameState *g, int type, int x, int y, char *reason, int size);
bool GameCanAfford(const GameState *g, const int cost[RESOURCE_COUNT]);
int GamePopulation(const GameState *g);
int GameHousing(const GameState *g);
int GameRoleCount(const GameState *g, int role);
int GameBuildingCount(const GameState *g, int type, bool completeOnly);
int GameFoodDaysTenths(const GameState *g);
int GameStage(const GameState *g);
int GameTile(int x, int y);
bool GameWalkable(const GameState *g, int x, int y);
int GameFindPath(const GameState *g, int start, int target, int *path, int capacity);
void GameEvent(GameState *g, const char *text);
bool GameValidate(const GameState *g, char *reason, int size);
bool GameSave(const GameState *g, const char *path, char *reason, int size);
bool GameLoad(GameState *g, const char *path, char *reason, int size);
uint64_t GameHash(const GameState *g);
#endif
