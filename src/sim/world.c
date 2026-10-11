#include "world.h"
#include "common.h"
#include <limits.h>

void WorldInit(GameState *g)
{
    for (int y = 0; y < MAP_SIZE; y++)
        for (int x = 0; x < MAP_SIZE; x++)
        {
            Tile *t = &g->tiles[GameTile(x, y)];
            t->occupied = -1;
            unsigned v = SimRandom(g);
            int river = 47 + (y > 60 ? 2 : y > 32 ? 0 : -2);
            t->terrain = (x >= river && x <= river + 2) ? WATER : GRASS;
            if ((y >= 22 && y <= 26) || (y >= 67 && y <= 71))
                if (t->terrain == WATER)
                    t->terrain = SHALLOWS;
            if ((x >= 26 && x <= 40 && y >= 12 && y <= 40) || (x >= 55 && x <= 69 && y >= 55 && y <= 83))
                t->terrain = FERTILE;
            bool camp =
                (x >= 16 && x <= 25 && y >= 16 && y <= 31) || (x >= 70 && x <= 79 && y >= 64 && y <= 79);
            if (!camp && t->terrain == GRASS)
            {
                if ((x < 14 || y < 10 || y > 85 || x > 82) && v % 4 != 0)
                {
                    t->node = NODE_TREE;
                    t->amount = 100;
                }
                else if (v % 29 == 0)
                {
                    t->node = NODE_STONE;
                    t->amount = 160;
                }
                else if (v % 23 == 0)
                {
                    t->node = NODE_BERRIES;
                    t->amount = 200;
                }
            }
        }
    const int nodes[][3] = {
        {14, 22, NODE_TREE},    {14, 24, NODE_TREE},  {15, 20, NODE_TREE},    {15, 18, NODE_TREE},
        {21, 14, NODE_STONE},   {23, 14, NODE_STONE}, {16, 27, NODE_BERRIES}, {14, 28, NODE_BERRIES},
        {25, 30, NODE_BERRIES}, {11, 29, NODE_GAME},  {13, 33, NODE_GAME},    {22, 34, NODE_GAME},
        {39, 43, NODE_ORE},     {40, 43, NODE_ORE},   {39, 44, NODE_ORE},     {40, 44, NODE_ORE}};
    for (int o = 0; o < 2; o++)
        for (unsigned i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++)
        {
            int x = o ? 95 - nodes[i][0] : nodes[i][0], y = o ? 95 - nodes[i][1] : nodes[i][1];
            Tile *t = &g->tiles[GameTile(x, y)];
            t->terrain = GRASS;
            t->node = nodes[i][2];
            t->amount = 400;
        }
    for (int i = 0; i < TILE_COUNT; i++)
        g->tiles[i].natural = g->tiles[i].node;
}

int GameTile(int x, int y)
{
    return x < 0 || y < 0 || x >= MAP_SIZE || y >= MAP_SIZE ? -1 : y * MAP_SIZE + x;
}

bool GameWalkable(const GameState *g, int x, int y)
{
    int t = GameTile(x, y);
    if (t < 0 || g->tiles[t].terrain == WATER)
        return false;
    int b = g->tiles[t].occupied;
    if (b >= 0 && g->buildings[b].alive && g->buildings[b].started)
    {
        int k = g->buildings[b].type;
        return k == GATE || k == FARM || k == STOCKPILE || k == HEARTH || k == WATCH_POST;
    }
    return true;
}

// Gates admit their own faction. Neutral geometry queries retain the original
// API; every actual route and movement step checks ownership here.
static bool WalkableFor(const GameState *g, int owner, int x, int y)
{
    if (!GameWalkable(g, x, y))
        return false;
    int id = g->tiles[GameTile(x, y)].occupied;
    return id < 0 || owner < 0 || !g->buildings[id].started || g->buildings[id].type != GATE ||
           g->buildings[id].owner == owner;
}

static int FindPath(const GameState *g, int owner, int start, int target, int *path, int capacity,
                    bool people)
{
    if (start < 0 || target < 0 || start >= TILE_COUNT || target >= TILE_COUNT)
        return -1;
    int prev[TILE_COUNT], queue[TILE_COUNT];
    bool occupied[TILE_COUNT] = {0};
    if (people)
        for (int i = 0; i < g->citizenCount; i++)
        {
            const Citizen *c = &g->citizens[i];
            if (c->alive && c->pathIndex >= c->pathLength)
                occupied[GameTile(c->x / 256, c->y / 256)] = true;
        }
    occupied[start] = false;

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
            if (t >= 0 && prev[t] == -2 && !occupied[t] && WalkableFor(g, owner, xx, yy))
            {
                prev[t] = p;
                queue[tail++] = t;
            }
        }
    }
    if (prev[target] == -2)
        return -1;
    int reverse[TILE_COUNT], n = 0;
    for (int t = target; t != start; t = prev[t])
    {
        if (n >= TILE_COUNT)
            return -1;
        reverse[n++] = t;
    }
    int copied = Min(n, capacity);
    for (int i = 0; i < copied; i++)
        path[i] = reverse[n - i - 1];
    return copied;
}

int GameFindPath(const GameState *g, int start, int target, int *path, int capacity)
{
    return FindPath(g, -1, start, target, path, capacity, false);
}

int WorldAdjacent(const GameState *g, int owner, int x, int y, int w, int h, int from)
{
    int best = -1, score = INT_MAX, path[MAX_PATH];
    for (int yy = y - 1; yy <= y + h; yy++)
        for (int xx = x - 1; xx <= x + w; xx++)
        {
            if (xx >= x && xx < x + w && yy >= y && yy < y + h && !WalkableFor(g, owner, xx, yy))
                continue;
            if (!WalkableFor(g, owner, xx, yy))
                continue;
            int t = GameTile(xx, yy), d = Dist(xx, yy, from % MAP_SIZE, from / MAP_SIZE);
            bool busy = false;
            for (int i = 0; i < g->citizenCount; i++)
            {
                const Citizen *c = &g->citizens[i];
                if (t != from && c->alive && c->pathIndex >= c->pathLength && c->x / 256 == xx &&
                    c->y / 256 == yy)
                    busy = true;
            }
            if (busy)
                continue;

            if (d >= score)
                continue;
            int n = FindPath(g, owner, from, t, path, MAX_PATH, false);
            if (n >= 0)
            {
                best = t;
                score = d;
            }
        }
    return best;
}

bool WorldRoute(GameState *g, Citizen *c, int destination)
{
    if (destination < 0)
        return false;
    int n = FindPath(g, c->owner, GameTile(c->x / 256, c->y / 256), destination, c->path, MAX_PATH, true);
    if (n < 0)
        n = FindPath(g, c->owner, GameTile(c->x / 256, c->y / 256), destination, c->path, MAX_PATH, false);
    if (n < 0)
        return false;
    c->pathLength = n;
    c->pathIndex = 0;
    c->pathRevision = g->mapRevision;
    return true;
}

// Friendly idle citizens step aside at bottlenecks. Hold-position orders are
// respected. The bounded search and neighbor order keep group movement replayable.
static bool Yield(GameState *g, Citizen *c, int depth, bool *visited)
{
    int id = (int)(c - g->citizens);
    if (depth == 0 || visited[id] || c->training >= 0 || c->orderCount ||
        (c->job != JOB_IDLE && c->job != JOB_GUARD))
        return false;
    visited[id] = true;
    const int dx[] = {0, 1, 0, -1}, dy[] = {-1, 0, 1, 0};
    for (int pass = 0; pass < 2; pass++)
        for (int d = 0; d < 4; d++)
        {
            int x = c->x / 256 + dx[d], y = c->y / 256 + dy[d];
            if (!WalkableFor(g, c->owner, x, y))
                continue;
            Citizen *occupant = NULL;
            bool reserved = false;
            for (int i = 0; i < g->citizenCount; i++)
            {
                Citizen *other = &g->citizens[i];
                if (!other->alive || other == c)
                    continue;
                if (other->x / 256 == x && other->y / 256 == y)
                    occupant = other;
                if (other->pathIndex < other->pathLength && other->path[other->pathIndex] == GameTile(x, y))
                    reserved = true;
            }
            if (reserved)
                continue;
            if (occupant && (!pass || occupant->owner != c->owner || !Yield(g, occupant, depth - 1, visited)))
                continue;
            c->path[0] = GameTile(x, y);
            c->pathIndex = 0;
            c->pathLength = 1;
            c->pathRevision = g->mapRevision;
            c->job = JOB_MOVE;
            c->target = GameTile(x, y);
            c->retry = 0;
            return true;
        }
    return false;
}

void WorldMoveCitizen(GameState *g, Citizen *c)
{
    if (c->pathIndex >= c->pathLength)
        return;
    int tile = c->path[c->pathIndex];
    if (!WalkableFor(g, c->owner, tile % MAP_SIZE, tile / MAP_SIZE) || c->pathRevision != g->mapRevision)
    {
        c->pathLength = 0;
        c->pathIndex = 0;
        return;
    }
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *other = &g->citizens[i];
        if (other != c && other->alive && other->pathIndex >= other->pathLength &&
            other->x / 256 == tile % MAP_SIZE && other->y / 256 == tile / MAP_SIZE)
        {
            bool visited[MAX_CITIZENS] = {0};
            visited[c - g->citizens] = true;
            if (other->owner == c->owner && Yield(g, other, 4, visited))
                return;
            c->pathLength = c->pathIndex = 0;
            c->retry = 10;
            return;
        }
    }
    int xx = (tile % MAP_SIZE) * 256, yy = (tile / MAP_SIZE) * 256, step = UNITS[c->unitType].moveStep;
    if (c->x != xx)
        c->x += c->x < xx ? Min(step, xx - c->x) : -Min(step, c->x - xx);
    else if (c->y != yy)
        c->y += c->y < yy ? Min(step, yy - c->y) : -Min(step, c->y - yy);
    if (c->x == xx && c->y == yy)
        c->pathIndex++;
}
