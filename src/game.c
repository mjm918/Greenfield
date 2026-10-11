#include "game.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *RESOURCE_NAMES[] = {"Food", "Wood", "Stone", "Hides", "Tools"};
const char *ROLE_NAMES[] = {"Builders", "Foragers", "Woodcutters", "Stone gatherers",
                            "Hunters",  "Crafters", "Farmers",     "Guards"};
const char *JOB_NAMES[] = {"Finding work", "Gathering",  "Building", "Delivering", "Discovering", "Crafting",
                           "Planting",     "Harvesting", "On watch", "Defending",  "Resting"};
const BuildingDef BUILDINGS[] = {
    {"Hearth", "Burns wood to keep the whole camp warm.", 2, 2, {0, 12, 8, 0, 0}, 400},
    {"Shelter", "Four beds. Upgrade after discovering insulation.", 2, 2, {0, 18, 2, 4, 0}, 650},
    {"Stockpile", "A delivery point for every resource.", 2, 2, {0, 12, 0, 0, 0}, 350},
    {"Foraging post", "Nearby foragers bring back extra food.", 2, 2, {0, 14, 0, 0, 0}, 450},
    {"Hunting camp", "Nearby hunters bring back extra meat and hides.", 2, 2, {0, 20, 4, 0, 0}, 550},
    {"Workshop", "Staff with crafters to discover and make tools.", 3, 2, {0, 30, 12, 0, 0}, 850},
    {"Granary", "A food delivery point. Keeps stored food fresh.", 2, 2, {0, 24, 8, 0, 0}, 600},
    {"Farm plot", "Fertile soil only. Plant, grow and harvest grain.", 2, 2, {0, 8, 0, 0, 1}, 250},
    {"Watch post", "Guards rally here and intercept nearby raiders.", 1, 1, {0, 18, 5, 0, 0}, 500},
    {"Palisade", "Blocks movement. Leave a gate for deliveries.", 1, 1, {0, 4, 0, 0, 0}, 160},
    {"Gate", "A passable entrance through your palisade.", 1, 1, {0, 8, 2, 0, 0}, 220}};
const DiscoveryDef DISCOVERIES[] = {
    {"Better tools", "Workers bring back more supplies per trip.", {0, 16, 10, 0, 3}, 1800},
    {"Insulation", "Unlock warm huts: upgrade shelters for wood and hides.", {0, 12, 0, 6, 0}, 1800},
    {"Food storage", "Unlock granaries; reduce daily food spoilage.", {0, 18, 8, 0, 1}, 1800},
    {"Farming", "After the thaw, cultivate fertile river soil.", {10, 20, 10, 0, 2}, 2400}};
static int Min(int a, int b) { return a < b ? a : b; }
static int Max(int a, int b) { return a > b ? a : b; }
static int Abs(int a) { return a < 0 ? -a : a; }
static int Dist(int x, int y, int a, int b) { return Abs(x - a) + Abs(y - b); }
static bool Reject(char *r, int n, const char *s)
{
    if (r && n > 0)
        snprintf(r, (size_t)n, "%s", s);
    return false;
}
static uint32_t Random(GameState *g)
{
    g->random ^= g->random << 13;
    g->random ^= g->random >> 17;
    g->random ^= g->random << 5;
    return g->random;
}
int GameTile(int x, int y)
{
    return x < 0 || y < 0 || x >= MAP_SIZE || y >= MAP_SIZE ? -1 : y * MAP_SIZE + x;
}
void GameEvent(GameState *g, const char *text)
{
    for (int i = 7; i > 0; i--)
        memcpy(g->events[i], g->events[i - 1], 128);
    snprintf(g->events[0], 128, "%s", text);
    g->eventCount = Min(8, g->eventCount + 1);
}
int GamePopulation(const GameState *g)
{
    int n = 0;
    for (int i = 0; i < g->citizenCount; i++)
        n += g->citizens[i].alive;
    return n;
}
int GameHousing(const GameState *g)
{
    int n = 0;
    for (int i = 0; i < g->buildingCount; i++)
        if ((g->buildings[i].complete || g->buildings[i].upgraded) && g->buildings[i].type == SHELTER)
            n += 4;
    return n;
}
int GameRoleCount(const GameState *g, int role)
{
    int n = 0;
    for (int i = 0; i < g->citizenCount; i++)
        n += g->citizens[i].alive && g->citizens[i].role == role;
    return n;
}
int GameBuildingCount(const GameState *g, int type, bool completeOnly)
{
    int n = 0;
    for (int i = 0; i < g->buildingCount; i++)
        n += g->buildings[i].type == type && (!completeOnly || g->buildings[i].complete);
    return n;
}
int GameFoodDaysTenths(const GameState *g)
{
    int n = GamePopulation(g);
    return n ? g->stock[FOOD] * 10 / (n * 3) : 0;
}
int GameStage(const GameState *g) { return g->discovery[FARMING] ? 2 : g->winterSurvived ? 1 : 0; }
bool GameCanAfford(const GameState *g, const int cost[RESOURCE_COUNT])
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
        if (g->stock[r] < cost[r])
            return false;
    return true;
}
static void Spend(GameState *g, int resource, int count)
{
    g->stock[resource] -= count;
    g->spent[resource] += count;
}
static void Pay(GameState *g, const int cost[RESOURCE_COUNT])
{
    for (int r = 0; r < RESOURCE_COUNT; r++)
        Spend(g, r, cost[r]);
}
bool GameWalkable(const GameState *g, int x, int y)
{
    int t = GameTile(x, y);
    if (t < 0 || g->tiles[t].terrain == WATER)
        return false;
    int b = g->tiles[t].occupied;
    if (b >= 0)
    {
        int k = g->buildings[b].type;
        return k == GATE || k == FARM || k == STOCKPILE || k == HEARTH || k == WATCH_POST;
    }
    return true;
}
int GameFindPath(const GameState *g, int start, int target, int *path, int capacity)
{
    if (start < 0 || target < 0 || start >= TILE_COUNT || target >= TILE_COUNT)
        return -1;
    int prev[TILE_COUNT], queue[TILE_COUNT];
    for (int i = 0; i < TILE_COUNT; i++)
        prev[i] = -2;
    int head = 0, tail = 0;
    queue[tail++] = start;
    prev[start] = -1;
    while (head < tail)
    {
        int p = queue[head++];
        if (p == target)
            break;
        int x = p % MAP_SIZE, y = p / MAP_SIZE;
        const int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
        for (int d = 0; d < 4; d++)
        {
            int xx = x + dx[d], yy = y + dy[d], t = GameTile(xx, yy);
            if (t >= 0 && prev[t] == -2 && GameWalkable(g, xx, yy))
            {
                prev[t] = p;
                queue[tail++] = t;
            }
        }
    }
    if (prev[target] == -2)
        return -1;
    int reverse[MAX_PATH], n = 0;
    for (int t = target; t != start; t = prev[t])
    {
        if (n >= capacity || n >= MAX_PATH)
            return -1;
        reverse[n++] = t;
    }
    for (int i = 0; i < n; i++)
        path[i] = reverse[n - i - 1];
    return n;
}
static int Adjacent(const GameState *g, int x, int y, int w, int h, int from)
{
    int best = -1, score = INT_MAX, path[MAX_PATH];
    for (int yy = y - 1; yy <= y + h; yy++)
        for (int xx = x - 1; xx <= x + w; xx++)
        {
            if (xx >= x && xx < x + w && yy >= y && yy < y + h && !GameWalkable(g, xx, yy))
                continue;
            if (!GameWalkable(g, xx, yy))
                continue;
            int t = GameTile(xx, yy), d = Dist(xx, yy, from % MAP_SIZE, from / MAP_SIZE);
            if (d >= score)
                continue;
            int n = GameFindPath(g, from, t, path, MAX_PATH);
            if (n >= 0)
            {
                best = t;
                score = d;
            }
        }
    return best;
}
static void ResetJob(Citizen *c)
{
    c->job = JOB_IDLE;
    c->target = -1;
    c->work = 0;
    c->pathLength = 0;
    c->pathIndex = 0;
    c->retry = 0;
}
static void AddCitizen(GameState *g, int role)
{
    if (g->citizenCount >= MAX_CITIZENS)
        return;
    Citizen *c = &g->citizens[g->citizenCount++];
    memset(c, 0, sizeof(*c));
    c->alive = 1;
    c->hp = 1000;
    c->role = role;
    c->x = (20 + (g->citizenCount % 3)) * 256;
    c->y = (23 + (g->citizenCount % 2)) * 256;
    if (!GameWalkable(g, c->x / 256, c->y / 256))
    {
        int distance = INT_MAX, tile = -1;
        for (int t = 0; t < TILE_COUNT; t++)
        {
            int d = Dist(t % MAP_SIZE, t / MAP_SIZE, c->x / 256, c->y / 256);
            if (d < distance && GameWalkable(g, t % MAP_SIZE, t / MAP_SIZE))
            {
                tile = t;
                distance = d;
            }
        }
        if (tile >= 0)
        {
            c->x = tile % MAP_SIZE * 256;
            c->y = tile / MAP_SIZE * 256;
        }
    }
    c->hunger = (int)(Random(g) % 300);
    ResetJob(c);
}
static int AddBuilding(GameState *g, int type, int x, int y, bool done)
{
    int id = g->buildingCount++;
    Building *b = &g->buildings[id];
    *b = (Building){.type = type,
                    .x = x,
                    .y = y,
                    .progress = done ? BUILDINGS[type].labor : 0,
                    .complete = done,
                    .priority = 1};
    for (int yy = y; yy < y + BUILDINGS[type].h; yy++)
        for (int xx = x; xx < x + BUILDINGS[type].w; xx++)
            g->tiles[GameTile(xx, yy)].occupied = id;
    g->mapRevision++;
    return id;
}
void GameInit(GameState *g)
{
    memset(g, 0, sizeof(*g));
    g->random = 0x47f1e1d;
    g->research = -1;
    g->nextArrival = ARRIVAL_FIRST;
    g->nextRaid = 11400;
    g->rallyX = 26;
    g->rallyY = 20;
    g->warmth = 65;
    g->coldWarning = 1;
    const int supplies[] = {60, 100, 40, 24, 15};
    for (int r = 0; r < RESOURCE_COUNT; r++)
        g->stock[r] = g->initial[r] = supplies[r];
    for (int r = 0; r < ROLE_COUNT; r++)
    {
        g->areaX[r] = -1;
        g->areaY[r] = -1;
        g->priority[r] = 1;
    }
    for (int y = 0; y < MAP_SIZE; y++)
        for (int x = 0; x < MAP_SIZE; x++)
        {
            Tile *t = &g->tiles[GameTile(x, y)];
            t->occupied = -1;
            t->terrain = GRASS;
            int river = 32 + (y > 30 ? 2 : y > 18 ? 0 : -2);
            if (x >= river && x <= river + 2)
                t->terrain = WATER;
            if (y >= 22 && y <= 25 && x >= river && x <= river + 2)
                t->terrain = SHALLOWS;
            if (x >= 26 && x < river && y > 12 && y < 38)
                t->terrain = FERTILE;
            unsigned v = Random(g);
            bool camp = x >= 16 && x <= 28 && y >= 16 && y <= 29;
            if (!camp && t->terrain == GRASS)
            {
                if ((x < 13 || y < 10 || y > 37) && v % 4 != 0)
                {
                    t->node = NODE_TREE;
                    t->amount = 100;
                }
                else if ((x > 37 && y > 27) || (x > 13 && x < 22 && y < 15))
                {
                    if (v % 3 == 0)
                    {
                        t->node = NODE_STONE;
                        t->amount = 160;
                        t->terrain = ROCK_GROUND;
                    }
                }
                else if (v % 13 == 0)
                {
                    t->node = NODE_BERRIES;
                    t->amount = 60;
                }
            }
        }
    const int nodes[][3] = {{14, 22, NODE_TREE},    {14, 24, NODE_TREE},    {15, 20, NODE_TREE},
                            {15, 18, NODE_TREE},    {21, 14, NODE_STONE},   {23, 14, NODE_STONE},
                            {16, 27, NODE_BERRIES}, {14, 28, NODE_BERRIES}, {25, 30, NODE_BERRIES},
                            {11, 29, NODE_GAME},    {13, 33, NODE_GAME},    {22, 34, NODE_GAME}};
    for (unsigned i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++)
    {
        Tile *t = &g->tiles[GameTile(nodes[i][0], nodes[i][1])];
        t->node = nodes[i][2];
        t->amount = 200;
    }
    for (int y = 5; y < 11; y++)
        for (int x = 38; x < 44; x++)
        {
            Tile *t = &g->tiles[GameTile(x, y)];
            t->node = NODE_NONE;
            t->terrain = GRASS;
        }
    AddBuilding(g, HEARTH, 21, 20, true);
    AddBuilding(g, STOCKPILE, 23, 23, true);
    int roles[] = {BUILDER, BUILDER, FORAGER, FORAGER, WOODCUTTER, STONE_GATHERER, HUNTER, IDLE};
    for (int i = 0; i < 8; i++)
        AddCitizen(g, roles[i]);
    GameEvent(g, "Eight survivors reach the valley. Build two shelters before the cold deepens.");
}
bool GameCanPlace(const GameState *g, int type, int x, int y, char *reason, int size)
{
    if (type < 0 || type >= BUILDING_COUNT)
        return Reject(reason, size, "Unknown building.");
    if (g->buildingCount >= MAX_BUILDINGS)
        return Reject(reason, size, "The valley's building limit is reached.");
    if (type == FARM && (!g->discovery[FARMING] || g->tick < WINTER_END))
        return Reject(reason, size, "Wait for the thaw and discover Farming at a staffed workshop.");
    if (type == GRANARY && !g->discovery[FOOD_STORAGE])
        return Reject(reason, size, "Discover Food storage at a staffed workshop first.");
    const BuildingDef *d = &BUILDINGS[type];
    if (x < 0 || y < 0 || x > MAP_SIZE - d->w || y > MAP_SIZE - d->h)
        return Reject(reason, size, "Keep the whole footprint inside the valley.");
    for (int yy = y; yy < y + d->h; yy++)
        for (int xx = x; xx < x + d->w; xx++)
        {
            int t = GameTile(xx, yy);
            if (t < 0)
                return Reject(reason, size, "Keep the whole footprint inside the valley.");
            if (g->tiles[t].terrain == WATER || g->tiles[t].terrain == SHALLOWS)
                return Reject(reason, size, "Water and river crossings must stay clear.");
            if (xx >= 37 && yy < 13)
                return Reject(reason, size, "This land belongs to the rival camp.");
            if (g->tiles[t].occupied >= 0)
                return Reject(reason, size, "Another building occupies this footprint.");
            if (g->tiles[t].node != NODE_NONE)
                return Reject(reason, size, "Gather the natural resources on this tile first.");
            if (type == FARM && g->tiles[t].terrain != FERTILE)
                return Reject(reason, size, "Farm plots need the fertile soil beside the river.");
            for (int i = 0; i < g->citizenCount; i++)
                if (g->citizens[i].alive && g->citizens[i].x / 256 == xx && g->citizens[i].y / 256 == yy)
                    return Reject(reason, size, "A citizen is standing here. Wait until they move.");
            for (int i = 0; i < MAX_RAIDERS; i++)
                if (g->raiders[i].active && g->raiders[i].x / 256 == xx && g->raiders[i].y / 256 == yy)
                    return Reject(reason, size, "A raider occupies this tile.");
        }
    if (!GameCanAfford(g, d->cost))
        return Reject(reason, size, "Not enough stored materials. Carried goods must be delivered first.");
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Click to place %s", d->name);
    return true;
}
bool GameCommandApply(GameState *g, GameCommand cmd, char *reason, int size)
{
    if (g->lost)
        return Reject(reason, size, "The tribe is lost. Start again or load a saved game.");
    switch (cmd.type)
    {
    case CMD_BUILD:
        if (!GameCanPlace(g, cmd.a, cmd.b, cmd.c, reason, size))
            return false;
        Pay(g, BUILDINGS[cmd.a].cost);
        AddBuilding(g, cmd.a, cmd.b, cmd.c, false);
        break;
    case CMD_ASSIGN:
    {
        if (cmd.a < 0 || cmd.a >= ROLE_COUNT || cmd.b < 0 || cmd.b > GamePopulation(g))
            return Reject(reason, size, "Invalid worker assignment.");
        int current = GameRoleCount(g, cmd.a), change = cmd.b - current;
        if (change > GameRoleCount(g, IDLE))
            return Reject(reason, size, "No unassigned citizens. Reduce another role first.");
        for (int i = 0; i < g->citizenCount && change; i++)
        {
            Citizen *c = &g->citizens[i];
            if (!c->alive)
                continue;
            if (change > 0 && c->role == IDLE)
            {
                c->role = cmd.a;
                change--;
            }
            else if (change < 0 && c->role == cmd.a)
            {
                c->role = IDLE;
                change++;
            }
            else
                continue;
            int cargo = 0;
            for (int r = 0; r < RESOURCE_COUNT; r++)
                cargo += c->carry[r];
            ResetJob(c);
            if (cargo)
                c->job = JOB_DELIVER;
        }
        break;
    }
    case CMD_AREA:
        if (cmd.a < 0 || cmd.a >= ROLE_COUNT || ((cmd.b != -1 || cmd.c != -1) && GameTile(cmd.b, cmd.c) < 0))
            return Reject(reason, size, "Choose a tile in the valley.");
        g->areaX[cmd.a] = cmd.b;
        g->areaY[cmd.a] = cmd.c;
        for (int i = 0; i < g->citizenCount; i++)
            if (g->citizens[i].role == cmd.a && g->citizens[i].job != JOB_DELIVER)
                ResetJob(&g->citizens[i]);
        break;
    case CMD_PRIORITY:
        if (cmd.a < 0 || cmd.a >= ROLE_COUNT || cmd.b < 0 || cmd.b > 2)
            return Reject(reason, size, "Priority must be low, normal or high.");
        g->priority[cmd.a] = cmd.b;
        break;
    case CMD_RESEARCH:
        if (cmd.a < 0 || cmd.a >= DISCOVERY_COUNT || g->discovery[cmd.a] || g->research != -1)
            return Reject(reason, size, "Finish the active discovery first, or choose an undiscovered idea.");
        if (!GameBuildingCount(g, WORKSHOP, true) || !GameRoleCount(g, CRAFTER))
            return Reject(reason, size, "Build a workshop and assign at least one crafter.");
        if (cmd.a == FARMING && g->tick < WINTER_END)
            return Reject(reason, size, "Farming can be discovered after the thaw.");
        if (!GameCanAfford(g, DISCOVERIES[cmd.a].cost))
            return Reject(reason, size, "Not enough materials for this discovery.");
        Pay(g, DISCOVERIES[cmd.a].cost);
        g->research = cmd.a;
        g->researchProgress = 0;
        break;
    case CMD_UPGRADE:
    {
        if (cmd.a < 0 || cmd.a >= g->buildingCount)
            return Reject(reason, size, "Select a completed shelter.");
        Building *b = &g->buildings[cmd.a];
        int cost[] = {0, 10, 0, 3, 0};
        if (b->type != SHELTER || !b->complete || b->upgraded || !g->discovery[INSULATION])
            return Reject(reason, size, "Discover Insulation and select a completed shelter.");
        if (!GameCanAfford(g, cost))
            return Reject(reason, size, "A hut upgrade needs 10 wood and 3 hides.");
        Pay(g, cost);
        b->upgraded = 1;
        b->complete = 0;
        b->progress = 0;
        g->mapRevision++;
        break;
    }
    case CMD_TRUCE:
        if (g->truceUntil > (int)g->tick)
            return Reject(reason, size, "The truce is already active.");
        if (g->stock[FOOD] < 40)
            return Reject(reason, size, "A truce costs 40 stored food.");
        Spend(g, FOOD, 40);
        g->truceUntil = (int)g->tick + 7200;
        g->raidResolved = 1;
        g->raidWarning = 0;
        // During an active raid this is its start deadline, used by the retreat bound.
        // Schedule the next threat once the last current raider has left.
        if (!g->raidActive)
            g->nextRaid = g->truceUntil + 1200;
        for (int i = 0; i < MAX_RAIDERS; i++)
            if (g->raiders[i].active)
                g->raiders[i].retreat = 1;
        GameEvent(g, "The rival camp accepts 40 food. A twelve-minute truce begins.");
        break;
    case CMD_RALLY:
        if (!GameWalkable(g, cmd.a, cmd.b))
            return Reject(reason, size, "Choose a walkable rally tile.");
        g->rallyX = cmd.a;
        g->rallyY = cmd.b;
        break;
    default:
        return Reject(reason, size, "Unknown command.");
    }
    if (reason && size > 0)
        snprintf(reason, (size_t)size, "Done.");
    return true;
}
static bool Route(GameState *g, Citizen *c, int destination)
{
    if (destination < 0)
        return false;
    int n = GameFindPath(g, GameTile(c->x / 256, c->y / 256), destination, c->path, MAX_PATH);
    if (n < 0)
        return false;
    c->pathLength = n;
    c->pathIndex = 0;
    c->pathRevision = g->mapRevision;
    return true;
}
static int Cargo(const Citizen *c)
{
    int n = 0;
    for (int r = 0; r < RESOURCE_COUNT; r++)
        n += c->carry[r];
    return n;
}
static bool NearBuilding(const GameState *g, int type, int x, int y, int range)
{
    for (int i = 0; i < g->buildingCount; i++)
    {
        const Building *b = &g->buildings[i];
        if (b->type == type && b->complete && Dist(b->x, b->y, x, y) <= range)
            return true;
    }
    return false;
}
static int Delivery(GameState *g, Citizen *c)
{
    int best = -1, d = INT_MAX, from = GameTile(c->x / 256, c->y / 256);
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *b = &g->buildings[i];
        if (!b->complete || (b->type != STOCKPILE && !(b->type == GRANARY && c->carry[FOOD] == Cargo(c))))
            continue;
        int t = Adjacent(g, b->x, b->y, BUILDINGS[b->type].w, BUILDINGS[b->type].h, from);
        if (t >= 0)
        {
            int dd = Dist(t % MAP_SIZE, t / MAP_SIZE, c->x / 256, c->y / 256);
            if (dd < d)
            {
                best = t;
                d = dd;
            }
        }
    }
    return best;
}
static int Destination(GameState *g, Citizen *c)
{
    int from = GameTile(c->x / 256, c->y / 256);
    if (c->job == JOB_DELIVER)
        return Delivery(g, c);
    if (c->job == JOB_GATHER && c->target >= 0 && c->target < TILE_COUNT)
        return Adjacent(g, c->target % MAP_SIZE, c->target / MAP_SIZE, 1, 1, from);
    if (c->job == JOB_GUARD)
        return GameTile(g->rallyX, g->rallyY);
    if (c->target >= 0 && c->target < g->buildingCount)
    {
        Building *b = &g->buildings[c->target];
        return Adjacent(g, b->x, b->y, BUILDINGS[b->type].w, BUILDINGS[b->type].h, from);
    }
    return -1;
}
static void ChooseJob(GameState *g, Citizen *c)
{
    if (Cargo(c))
    {
        c->job = JOB_DELIVER;
        c->target = -1;
        return;
    }
    if (c->role == IDLE)
        return;
    int from = GameTile(c->x / 256, c->y / 256), best = -1, score = INT_MAX;
    if (c->role == GUARD)
    {
        c->job = JOB_GUARD;
        c->target = -1;
        return;
    }
    if (c->role == BUILDER || c->role == CRAFTER || c->role == FARMER)
    {
        for (int i = 0; i < g->buildingCount; i++)
        {
            Building *b = &g->buildings[i];
            bool match = c->role == BUILDER ? !b->complete
                         : c->role == CRAFTER
                             ? (b->type == WORKSHOP && b->complete)
                             : (b->type == FARM && b->complete && (b->crop == 0 || b->crop == 2));
            if (!match)
                continue;
            if (g->areaX[c->role] >= 0 && Dist(b->x, b->y, g->areaX[c->role], g->areaY[c->role]) > 9)
                continue;
            if (c->role == CRAFTER && g->research < 0 &&
                (g->stock[TOOLS] >= 30 || g->stock[WOOD] < 2 || g->stock[STONE] < 1))
                continue;
            int d = Dist(c->x / 256, c->y / 256, b->x, b->y);
            if (d >= score || Adjacent(g, b->x, b->y, BUILDINGS[b->type].w, BUILDINGS[b->type].h, from) < 0)
                continue;
            best = i;
            score = d;
        }
        if (best < 0)
            return;
        c->target = best;
        c->work = 0;
        if (c->role == BUILDER)
            c->job = JOB_BUILD;
        else if (c->role == FARMER)
            c->job = g->buildings[best].crop == 2 ? JOB_HARVEST : JOB_PLANT;
        else if (g->research >= 0)
            c->job = JOB_RESEARCH;
        else
        {
            Spend(g, WOOD, 2);
            Spend(g, STONE, 1);
            c->job = JOB_CRAFT;
        }
        return;
    }
    int wanted = c->role == FORAGER          ? NODE_BERRIES
                 : c->role == WOODCUTTER     ? NODE_TREE
                 : c->role == STONE_GATHERER ? NODE_STONE
                                             : NODE_GAME;
    for (int t = 0; t < TILE_COUNT; t++)
    {
        Tile *tile = &g->tiles[t];
        if (tile->node != wanted || tile->amount <= 0)
            continue;
        int x = t % MAP_SIZE, y = t / MAP_SIZE;
        if (g->areaX[c->role] >= 0 && Dist(x, y, g->areaX[c->role], g->areaY[c->role]) > 9)
            continue;
        int d = Dist(c->x / 256, c->y / 256, x, y);
        if (d >= score)
            continue;
        if (Adjacent(g, x, y, 1, 1, from) < 0)
            continue;
        score = d;
        best = t;
    }
    if (best >= 0)
    {
        c->job = JOB_GATHER;
        c->target = best;
        c->work = 0;
    }
}
static void MoveCitizen(GameState *g, Citizen *c)
{
    if (c->pathIndex >= c->pathLength)
        return;
    int tile = c->path[c->pathIndex];
    if (!GameWalkable(g, tile % MAP_SIZE, tile / MAP_SIZE) || c->pathRevision != g->mapRevision)
    {
        c->pathLength = 0;
        c->pathIndex = 0;
        return;
    }
    int xx = (tile % MAP_SIZE) * 256, yy = (tile / MAP_SIZE) * 256, step = 40;
    if (c->x != xx)
        c->x += c->x < xx ? Min(step, xx - c->x) : -Min(step, c->x - xx);
    else if (c->y != yy)
        c->y += c->y < yy ? Min(step, yy - c->y) : -Min(step, c->y - yy);
    if (c->x == xx && c->y == yy)
        c->pathIndex++;
}
static void CitizenWork(GameState *g, Citizen *c)
{
    if (c->job == JOB_IDLE)
    {
        ChooseJob(g, c);
        if (c->job == JOB_IDLE)
        {
            c->retry = 20;
            return;
        }
    }
    if (c->job == JOB_GUARD && c->target >= 0)
        c->target = -1;
    if (c->pathIndex < c->pathLength)
    {
        MoveCitizen(g, c);
        return;
    }
    int dest = Destination(g, c);
    if (dest < 0)
    {
        if (c->job != JOB_DELIVER)
            ResetJob(c);
        c->retry = 30;
        return;
    }
    if (c->x / 256 != dest % MAP_SIZE || c->y / 256 != dest / MAP_SIZE)
    {
        if (!Route(g, c, dest))
        {
            if (c->job != JOB_DELIVER)
                ResetJob(c);
            c->retry = 30;
        }
        return;
    }
    if (c->job == JOB_DELIVER)
    {
        for (int r = 0; r < RESOURCE_COUNT; r++)
        {
            g->stock[r] += c->carry[r];
            c->carry[r] = 0;
        }
        if (c->harvestCargo)
        {
            g->harvestDelivered = 1;
            c->harvestCargo = 0;
            GameEvent(g, "The first grain reaches storage. Farming can sustain a village.");
        }
        ResetJob(c);
        return;
    }
    if (c->job == JOB_GUARD)
        return;
    c->work++;
    if (c->job == JOB_BUILD)
    {
        Building *b = &g->buildings[c->target];
        if (b->complete)
        {
            ResetJob(c);
            return;
        }
        b->progress++;
        if (b->progress >= BUILDINGS[b->type].labor)
        {
            b->complete = 1;
            if (b->type == WATCH_POST)
            {
                g->rallyX = b->x;
                g->rallyY = b->y;
            }
            char msg[128];
            snprintf(msg, sizeof(msg), "%s completed.",
                     b->upgraded ? "Insulated hut" : BUILDINGS[b->type].name);
            GameEvent(g, msg);
            g->mapRevision++;
        }
        return;
    }
    if (c->job == JOB_RESEARCH)
    {
        if (g->research < 0)
        {
            ResetJob(c);
            return;
        }
        g->researchProgress++;
        if (g->researchProgress >= DISCOVERIES[g->research].labor)
        {
            char msg[128];
            snprintf(msg, sizeof(msg), "Discovery: %s.", DISCOVERIES[g->research].name);
            GameEvent(g, msg);
            g->discovery[g->research] = 1;
            g->research = -1;
            g->researchProgress = 0;
        }
        return;
    }
    if (c->job == JOB_CRAFT)
    {
        if (c->work >= 300)
        {
            c->carry[TOOLS] = 3;
            g->produced[TOOLS] += 3;
            c->job = JOB_DELIVER;
            c->target = -1;
            c->work = 0;
        }
        return;
    }
    if (c->job == JOB_PLANT || c->job == JOB_HARVEST)
    {
        Building *b = &g->buildings[c->target];
        if ((c->job == JOB_PLANT && b->crop != 0) || (c->job == JOB_HARVEST && b->crop != 2))
        {
            ResetJob(c);
            return;
        }
        if (c->work >= 180)
        {
            if (c->job == JOB_PLANT)
            {
                b->crop = 1;
                b->cropProgress = 0;
                ResetJob(c);
            }
            else
            {
                c->carry[FOOD] = 24;
                g->produced[FOOD] += 24;
                c->harvestCargo = 1;
                b->crop = 0;
                b->cropProgress = 0;
                c->job = JOB_DELIVER;
                c->target = -1;
                c->work = 0;
            }
        }
        return;
    }
    if (c->job == JOB_GATHER)
    {
        Tile *t = &g->tiles[c->target];
        if (t->amount <= 0)
        {
            ResetJob(c);
            return;
        }
        int duration = c->role == HUNTER           ? 450
                       : c->role == FORAGER        ? 300
                       : c->role == STONE_GATHERER ? 180
                                                   : 140;
        if (g->stock[TOOLS] <= 0)
            duration *= 2;
        if (c->work < duration)
            return;
        int resource = c->role == WOODCUTTER ? WOOD : c->role == STONE_GATHERER ? STONE : FOOD;
        int quantity = c->role == HUNTER ? 6 : c->role == FORAGER ? 3 : 5;
        quantity += g->discovery[IMPROVED_TOOLS] ? (resource == FOOD ? 1 : 2) : 0;
        if (c->role == FORAGER && NearBuilding(g, FORAGING_POST, c->x / 256, c->y / 256, 10))
            quantity += 1;
        if (c->role == HUNTER && NearBuilding(g, HUNTING_CAMP, c->x / 256, c->y / 256, 12))
            quantity += 2;
        quantity = Min(quantity, t->amount);
        t->amount -= quantity;
        c->carry[resource] += quantity;
        g->produced[resource] += quantity;
        if (c->role == HUNTER)
        {
            c->carry[HIDES] += 2;
            g->produced[HIDES] += 2;
        }
        if (g->stock[TOOLS] > 0 && Random(g) % 20 == 0)
            Spend(g, TOOLS, 1);
        if (t->amount == 0 && (t->node == NODE_TREE || t->node == NODE_STONE))
        {
            t->node = NODE_NONE;
            g->mapRevision++;
        }
        c->job = JOB_DELIVER;
        c->target = -1;
        c->work = 0;
    }
}
static void Survival(GameState *g)
{
    int pop = GamePopulation(g), beds = GameHousing(g), huts = 0;
    for (int i = 0; i < g->buildingCount; i++)
        huts += g->buildings[i].type == SHELTER && g->buildings[i].complete && g->buildings[i].upgraded;
    bool winter = g->tick < WINTER_END;
    bool fire = GameBuildingCount(g, HEARTH, true) > 0 && g->stock[WOOD] > 0;
    g->warmth = winter ? Min(100, 20 + (fire ? 45 : 0) + (beds >= pop ? 25 : 0) + Min(10, huts * 5)) : 100;
    if (winter && g->tick % 200 == 0 && fire)
        Spend(g, WOOD, 1);
    g->foodWarning = GameFoodDaysTenths(g) < 10;
    g->coldWarning = winter && g->warmth < 80;
    bool shortage = g->stock[FOOD] == 0 || (winter && g->warmth < 50);
    if (shortage)
    {
        g->shortageTicks++;
        g->hadShortage = 1;
    }
    else if (g->hadShortage)
    {
        g->shortageRecoveries++;
        g->hadShortage = 0;
        GameEvent(g, "Supplies and warmth are restored. Citizens can recover.");
    }
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive)
            continue;
        c->hunger++;
        if (c->hunger >= 600 && g->stock[FOOD] > 0)
        {
            Spend(g, FOOD, 1);
            c->hunger = 0;
        }
        if (winter && g->warmth < 80)
            c->cold++;
        else
            c->cold = Max(0, c->cold - 3);
        if (g->tick % 10 == 0)
        {
            if (c->hunger > 1500)
                c->hp -= 3;
            if (c->cold > 1800)
                c->hp -= g->warmth < 50 ? 3 : 1;
            if (c->hunger < 600 && c->cold < 600)
                c->hp = Min(1000, c->hp + 2);
        }
    }
    if (g->tick == WINTER_END)
    {
        g->winterSurvived = 1;
        GameEvent(g, "The thaw has arrived. Discover farming and settle the fertile riverbank.");
    }
    if (g->tick % DAY_TICKS == 0)
    {
        int spoiled = g->stock[FOOD] / (GameBuildingCount(g, GRANARY, true) ? 50
                                        : g->discovery[FOOD_STORAGE]        ? 33
                                                                            : 20);
        Spend(g, FOOD, spoiled);
        for (int t = 0; t < TILE_COUNT; t++)
            if (g->tiles[t].node == NODE_BERRIES || g->tiles[t].node == NODE_GAME)
                g->tiles[t].amount = Min(200, g->tiles[t].amount + 30);
    }
}
static void ResolveDeaths(GameState *g)
{
    // Both survival and combat use this one inventory/death transition. Run it
    // before arrivals and victory so zero-health citizens cannot win or revive.
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive || c->hp > 0)
            continue;
        for (int r = 0; r < RESOURCE_COUNT; r++)
        {
            g->spent[r] += c->carry[r];
            c->carry[r] = 0;
        }
        c->alive = 0;
        g->deaths++;
        GameEvent(g, "A citizen has died. Restore food and warmth to protect the tribe.");
    }
}
static void Population(GameState *g)
{
    if ((int)g->tick < g->nextArrival)
        return;
    int pop = GamePopulation(g);
    if (!pop)
        return;
    if (pop <= MAX_CITIZENS - 2 && g->citizenCount <= MAX_CITIZENS - 2 && GameHousing(g) >= pop + 2 &&
        g->stock[FOOD] >= pop * 3)
    {
        AddCitizen(g, IDLE);
        AddCitizen(g, IDLE);
        g->arrivals += 2;
        g->nextArrival = (int)g->tick + ARRIVAL_INTERVAL;
        GameEvent(g, "Two travelers join the tribe. Assign their work in the workforce panel.");
    }
    else
        g->nextArrival = (int)g->tick + 600;
}
static void MoveRaider(GameState *g, Raider *r, int tx, int ty)
{
    int from = GameTile(r->x / 256, r->y / 256), path[MAX_PATH],
        n = GameFindPath(g, from, GameTile(tx, ty), path, MAX_PATH);
    if (n <= 0)
        return;
    int xx = path[0] % MAP_SIZE * 256, yy = path[0] / MAP_SIZE * 256;
    // Align to the current tile before changing axis, avoiding corner cutting.
    int step = 32;
    if (r->x != xx)
        r->x += r->x < xx ? Min(step, xx - r->x) : -Min(step, r->x - xx);
    else if (r->y != yy)
        r->y += r->y < yy ? Min(step, yy - r->y) : -Min(step, r->y - yy);
}
static void Rivals(GameState *g)
{
    if (g->truceUntil > 0 && g->tick == (uint64_t)g->truceUntil)
        GameEvent(g, "The truce has expired. Another raid may approach in two minutes.");
    if (!g->raidActive && g->truceUntil <= (int)g->tick)
    {
        if ((int)g->tick >= g->nextRaid - 900 && !g->raidWarning)
        {
            g->raidWarning = 1;
            GameEvent(g, "Scouts warn of a raid in 90 seconds. Rally guards or offer a truce.");
        }
        if ((int)g->tick >= g->nextRaid)
        {
            g->raidActive = 1;
            g->raidLosses = 0;
            g->raidLoot = 0;
            g->raidWarning = 0;
            g->raidCount++;
            int count = Min(5, 2 + g->raidCount);
            for (int i = 0; i < count; i++)
                g->raiders[i] = (Raider){1, (40 + i % 2) * 256, (8 + i) * 256, 650, 0, 0, 0};
            GameEvent(g, "Raiders enter the valley. They seek food, then retreat.");
        }
    }
    int active = 0;
    for (int i = 0; i < MAX_RAIDERS; i++)
    {
        Raider *r = &g->raiders[i];
        if (!r->active)
            continue;
        active++;
        // A raid is bounded even if later construction encloses a retreating raider.
        // At six minutes, anyone unable to leave surrenders rather than blocking
        // future rival events forever.
        if ((int)g->tick > g->nextRaid + 3600)
        {
            r->active = 0;
            if (r->hp >= 220)
                g->raidLosses++;
            continue;
        }
        if (r->cooldown > 0)
            r->cooldown--;
        if (r->hp <= 0)
        {
            r->active = 0;
            g->raidLosses++;
            continue;
        }
        if (r->hp < 220 && !r->retreat)
        {
            r->retreat = 1;
            g->raidLosses++;
        }
        if (g->raidLosses >= 2 || g->raidLoot >= 18 || g->truceUntil > (int)g->tick)
            r->retreat = 1;
        if (r->retreat)
        {
            MoveRaider(g, r, 40, 8);
            if (Dist(r->x / 256, r->y / 256, 40, 8) < 2)
                r->active = 0;
            continue;
        }
        int foe = -1, d = INT_MAX;
        for (int j = 0; j < g->citizenCount; j++)
        {
            Citizen *c = &g->citizens[j];
            if (!c->alive || c->role != GUARD)
                continue;
            int dd = Dist(r->x, r->y, c->x, c->y);
            if (dd < d)
            {
                d = dd;
                foe = j;
            }
        }
        if (foe >= 0 && d < 400)
        {
            if (!r->cooldown)
            {
                g->citizens[foe].hp -= 45;
                r->cooldown = 16;
            }
            continue;
        }
        int destination = -1;
        for (int b = 0; b < g->buildingCount; b++)
            if (g->buildings[b].type == STOCKPILE && g->buildings[b].complete)
            {
                destination = b;
                break;
            }
        if (destination >= 0)
        {
            Building *b = &g->buildings[destination];
            if (Dist(r->x / 256, r->y / 256, b->x, b->y) <= 2)
            {
                int n = Min(6, Min(g->stock[FOOD], 18 - g->raidLoot));
                g->stock[FOOD] -= n;
                g->stolen[FOOD] += n;
                g->raidLoot += n;
                r->stolen += n;
                r->retreat = 1;
            }
            else
                MoveRaider(g, r, b->x, b->y);
        }
        if ((int)g->tick > g->nextRaid + 2400)
            r->retreat = 1;
    }
    for (int j = 0; j < g->citizenCount; j++)
    {
        Citizen *c = &g->citizens[j];
        if (!c->alive || c->role != GUARD)
            continue;
        if (c->attackCooldown)
            c->attackCooldown--;
        int foe = -1, d = INT_MAX;
        for (int i = 0; i < MAX_RAIDERS; i++)
        {
            Raider *r = &g->raiders[i];
            if (!r->active || r->retreat)
                continue;
            int dd = Dist(c->x, c->y, r->x, r->y);
            if (dd < d && Dist(g->rallyX, g->rallyY, r->x / 256, r->y / 256) < 16)
            {
                d = dd;
                foe = i;
            }
        }
        if (foe >= 0 && c->hp > 250)
        {
            c->job = JOB_FIGHT;
            Raider *r = &g->raiders[foe];
            if (d < 450)
            {
                if (!c->attackCooldown)
                {
                    r->hp -= 95;
                    c->attackCooldown = 12;
                }
            }
            else
            {
                if (c->pathIndex >= c->pathLength || g->tick % 10 == 0)
                    Route(g, c, GameTile(r->x / 256, r->y / 256));
                MoveCitizen(g, c);
            }
        }
        else
        {
            if (c->job == JOB_FIGHT)
                ResetJob(c);
            if (c->hp <= 250)
            {
                int home = GameTile(22, 23);
                if (c->pathIndex >= c->pathLength)
                    Route(g, c, home);
                MoveCitizen(g, c);
            }
        }
    }
    if (g->raidActive && !active)
    {
        g->raidActive = 0;
        if (g->raidLosses >= 2 || g->raidLoot == 0)
        {
            g->raidResolved = 1;
            GameEvent(g, "The raid is over and your supplies are protected. The tribe stands together.");
        }
        else
            GameEvent(g, "The raiders retreat. Supplies lost are shown in the chronicle.");
        g->nextRaid = g->truceUntil > (int)g->tick ? g->truceUntil + 1200 : (int)g->tick + 6600;
    }
}
void GameTick(GameState *g)
{
    if (g->lost)
        return;
    g->tick++;
    Survival(g);
    ResolveDeaths(g);
    Rivals(g);
    ResolveDeaths(g);
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *b = &g->buildings[i];
        if (b->type == FARM && b->complete && b->crop == 1 && g->tick >= WINTER_END)
        {
            if (++b->cropProgress >= 2400)
                b->crop = 2;
        }
    }
    for (int priority = 2; priority >= 0; priority--)
        for (int i = 0; i < g->citizenCount; i++)
        {
            Citizen *c = &g->citizens[i];
            if ((c->role == IDLE ? 1 : g->priority[c->role]) != priority || !c->alive ||
                c->job == JOB_FIGHT || (c->role == GUARD && c->hp <= 250))
                continue;
            if (c->retry > 0)
            {
                c->retry--;
                continue;
            }
            CitizenWork(g, c);
        }
    Population(g);
    int pop = GamePopulation(g);
    if (!pop)
    {
        g->lost = 1;
        GameEvent(g, "The last hearth has gone quiet. Load your save or begin again.");
    }
    if (!g->won && pop >= 20 && GameHousing(g) >= pop && g->winterSurvived && g->harvestDelivered &&
        GameFoodDaysTenths(g) >= 30 && g->raidResolved)
    {
        g->won = 1;
        GameEvent(g, "A village is born. Your people have a home, a harvest and a future.");
    }
}
void GameAdvance(GameState *g, double seconds, int speed, double *accumulator)
{
    if (speed != 0 && speed != 1 && speed != 2 && speed != 4)
        return;
    if (!(seconds >= 0 && seconds <= 60))
        return;
    *accumulator += seconds * speed;
    while (*accumulator >= 0.1 - 1e-10)
    {
        GameTick(g);
        *accumulator -= 0.1;
    }
    if (*accumulator < 0)
        *accumulator = 0;
}

bool GameValidate(const GameState *g, char *r, int n)
{
    if (g->tick > INT_MAX - 100000 || g->citizenCount < 0 || g->citizenCount > MAX_CITIZENS ||
        g->buildingCount < 2 || g->buildingCount > MAX_BUILDINGS || g->research < -1 ||
        g->research >= DISCOVERY_COUNT || g->eventCount < 0 || g->eventCount > 8)
        return Reject(r, n, "Invalid simulation header.");
    if (g->warmth < 0 || g->warmth > 100 || g->researchProgress < 0 || g->nextArrival < 0 ||
        g->nextRaid < 0 || g->truceUntil < 0 || g->raidLoot < 0 || g->raidLoot > 18 || g->raidLosses < 0 ||
        g->raidLosses > MAX_RAIDERS || g->researchProgress > 2400 || g->arrivals < 0 ||
        g->arrivals > MAX_CITIZENS || g->deaths < 0 || g->deaths > MAX_CITIZENS)
        return Reject(r, n, "Invalid progression state.");
    const int counters[] = {g->nextArrival, g->nextRaid,      g->truceUntil,        g->mapRevision,
                            g->raidCount,   g->shortageTicks, g->shortageRecoveries};
    for (unsigned i = 0; i < sizeof(counters) / sizeof(counters[0]); i++)
        if (counters[i] < 0 || counters[i] > INT_MAX - 100000)
            return Reject(r, n, "Invalid simulation counter.");
    const int flags[] = {g->winterSurvived, g->harvestDelivered, g->raidResolved, g->won,
                         g->lost,           g->raidWarning,      g->raidActive,   g->foodWarning,
                         g->coldWarning,    g->hadShortage};
    for (unsigned i = 0; i < sizeof(flags) / sizeof(flags[0]); i++)
        if (flags[i] < 0 || flags[i] > 1)
            return Reject(r, n, "Invalid simulation flag.");
    for (int i = 0; i < 8; i++)
        if (!memchr(g->events[i], 0, sizeof(g->events[i])))
            return Reject(r, n, "Invalid chronicle entry.");
    for (int k = 0; k < DISCOVERY_COUNT; k++)
        if (g->discovery[k] < 0 || g->discovery[k] > 1)
            return Reject(r, n, "Invalid discovery flag.");
    for (int k = 0; k < RESOURCE_COUNT; k++)
    {
        int64_t carried = 0;
        for (int i = 0; i < g->citizenCount; i++)
            carried += g->citizens[i].carry[k];
        if (g->stock[k] < 0 || g->stock[k] > 10000000 || g->initial[k] < 0 || g->initial[k] > 1000000000 ||
            g->produced[k] > 1000000000 || g->spent[k] > 1000000000 || g->stolen[k] > 1000000000 ||
            g->produced[k] < 0 || g->spent[k] < 0 || g->stolen[k] < 0 ||
            (int64_t)g->initial[k] + g->produced[k] - g->spent[k] - g->stolen[k] !=
                (int64_t)g->stock[k] + carried)
            return Reject(r, n, "Resource conservation failed.");
    }
    for (int i = 0; i < TILE_COUNT; i++)
    {
        const Tile *t = &g->tiles[i];
        if (t->terrain < 0 || t->terrain > ROCK_GROUND || t->node < 0 || t->node > NODE_GAME ||
            t->amount < 0 || t->amount > 1000000 || t->occupied < -1 || t->occupied >= g->buildingCount)
            return Reject(r, n, "Invalid map tile.");
        if (t->occupied >= 0)
        {
            const Building *b = &g->buildings[t->occupied];
            if (b->type < 0 || b->type >= BUILDING_COUNT || b->x < 0 || b->y < 0 || b->x >= MAP_SIZE ||
                b->y >= MAP_SIZE || i % MAP_SIZE < b->x || i % MAP_SIZE >= b->x + BUILDINGS[b->type].w ||
                i / MAP_SIZE < b->y || i / MAP_SIZE >= b->y + BUILDINGS[b->type].h)
                return Reject(r, n, "Unexpected building occupancy.");
        }
    }
    for (int i = 0; i < g->buildingCount; i++)
    {
        const Building *b = &g->buildings[i];
        if (b->type < 0 || b->type >= BUILDING_COUNT || b->x < 0 || b->y < 0 || b->x >= MAP_SIZE ||
            b->y >= MAP_SIZE || b->x + BUILDINGS[b->type].w > MAP_SIZE ||
            b->y + BUILDINGS[b->type].h > MAP_SIZE || b->progress < 0 ||
            b->progress > BUILDINGS[b->type].labor || b->crop < 0 || b->crop > 2 || b->cropProgress < 0 ||
            b->cropProgress > 2400 || b->complete < 0 || b->complete > 1 || b->upgraded < 0 ||
            b->upgraded > 1 || b->priority < 0 || b->priority > 2)
            return Reject(r, n, "Invalid building.");
        for (int y = b->y; y < b->y + BUILDINGS[b->type].h; y++)
            for (int x = b->x; x < b->x + BUILDINGS[b->type].w; x++)
                if (g->tiles[GameTile(x, y)].occupied != i)
                    return Reject(r, n, "Building occupancy disagrees with the map.");
    }
    for (int i = 0; i < g->citizenCount; i++)
    {
        const Citizen *c = &g->citizens[i];
        if (c->alive < 0 || c->alive > 1 || c->hunger < 0 || c->cold < 0 || c->work < 0 || c->role < 0 ||
            c->role > IDLE || c->x < 0 || c->y < 0 || c->x >= MAP_SIZE * 256 || c->y >= MAP_SIZE * 256 ||
            c->job < 0 || c->job > JOB_FIGHT || c->pathLength < 0 || c->pathLength > MAX_PATH ||
            c->pathIndex < 0 || c->pathIndex > c->pathLength || c->hp > 1000 || c->hp < -1000 ||
            c->hunger > 1000000 || c->cold > 1000000 || c->work > 1000000 || c->retry < 0 || c->retry > 30 ||
            c->attackCooldown < 0 || c->attackCooldown > 12 || c->pathRevision < 0 ||
            c->pathRevision > INT_MAX - 100000 || c->harvestCargo < 0 || c->harvestCargo > 1 ||
            c->target < -1 || c->target >= TILE_COUNT)
            return Reject(r, n, "Invalid citizen or job.");
        if (c->job != JOB_GATHER && c->job != JOB_DELIVER && c->job != JOB_GUARD && c->job != JOB_FIGHT &&
            c->job != JOB_IDLE && (c->target < 0 || c->target >= g->buildingCount))
            return Reject(r, n, "Job references a missing building.");
        for (int p = 0; p < c->pathLength; p++)
            if (c->path[p] < 0 || c->path[p] >= TILE_COUNT)
                return Reject(r, n, "Invalid route.");
        for (int k = 0; k < RESOURCE_COUNT; k++)
            if (c->carry[k] < 0 || c->carry[k] > 1000)
                return Reject(r, n, "Invalid carried goods.");
    }
    for (int i = 0; i < ROLE_COUNT; i++)
        if (g->areaX[i] < -1 || g->areaX[i] >= MAP_SIZE || g->areaY[i] < -1 || g->areaY[i] >= MAP_SIZE ||
            g->priority[i] < 0 || g->priority[i] > 2)
            return Reject(r, n, "Invalid work area.");
    for (int i = 0; i < MAX_RAIDERS; i++)
    {
        const Raider *a = &g->raiders[i];
        if (a->x < 0 || a->y < 0 || a->x >= MAP_SIZE * 256 || a->y >= MAP_SIZE * 256 || a->hp > 650 ||
            a->hp < -1000 || a->stolen < 0 || a->stolen > 18 || a->active < 0 || a->active > 1 ||
            a->retreat < 0 || a->retreat > 1 || a->cooldown < 0 || a->cooldown > 16)
            return Reject(r, n, "Invalid raider.");
    }
    if (GameTile(g->rallyX, g->rallyY) < 0)
        return Reject(r, n, "Invalid rally point.");
    return true;
}
