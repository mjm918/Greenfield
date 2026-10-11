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
            t->terrain = GRASS;
            int river = 32 + (y > 30 ? 2 : y > 18 ? 0 : -2);
            if (x >= river && x <= river + 2)
                t->terrain = WATER;
            if (y >= 22 && y <= 25 && x >= river && x <= river + 2)
                t->terrain = SHALLOWS;
            if (x >= 26 && x < river && y > 12 && y < 38)
                t->terrain = FERTILE;
            unsigned v = SimRandom(g);
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

int WorldAdjacent(const GameState *g, int x, int y, int w, int h, int from)
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

bool WorldRoute(GameState *g, Citizen *c, int destination)
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

void WorldMoveCitizen(GameState *g, Citizen *c)
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
