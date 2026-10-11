#include "wildlife.h"
#include "common.h"
#include <limits.h>
#include <string.h>

static uint32_t Random(Wildlife *a)
{
    a->random ^= a->random << 13;
    a->random ^= a->random >> 17;
    a->random ^= a->random << 5;
    return a->random;
}
const Wildlife *GameWildlifeForTile(const GameState *g, int tile)
{
    if (tile < 0 || tile >= TILE_COUNT)
        return NULL;
    for (int i = 0; i < g->wildlifeCount; i++)
        if (g->wildlife[i].home == tile && g->tiles[tile].node == NODE_GAME && g->tiles[tile].amount > 0)
            return &g->wildlife[i];
    return NULL;
}
void WildlifeInit(GameState *g)
{
    g->wildlifeCount = 0;
    memset(g->wildlife, 0, sizeof(g->wildlife));
    for (int t = 0; t < TILE_COUNT && g->wildlifeCount < MAX_WILDLIFE; t++)
        if (g->tiles[t].node == NODE_GAME)
        {
            Wildlife *a = &g->wildlife[g->wildlifeCount++];
            a->home = t;
            a->x = a->goalX = t % MAP_SIZE * 256;
            a->y = a->goalY = t / MAP_SIZE * 256;
            a->random = 0x753ad81u ^ (uint32_t)t * 2654435761u;
            if (!a->random)
                a->random = 1;
            a->wait = 5 + Random(a) % 40;
        }
}
static bool Clear(const GameState *g, const Wildlife *a, int x, int y)
{
    int t = GameTile(x, y);
    if (t < 0 || Dist(x, y, a->home % MAP_SIZE, a->home / MAP_SIZE) > 2 || !GameWalkable(g, x, y))
        return false;
    const Tile *tile = &g->tiles[t];
    if (tile->occupied >= 0 && g->buildings[tile->occupied].started)
        return false;
    // Forest tiles are traversable for citizens too; wildlife can pass beneath
    // their depth-sorted canopies instead of remaining trapped at forest sites.
    return tile->node != NODE_STONE && tile->node != NODE_ORE;
}
void WildlifeUpdate(GameState *g)
{
    for (int i = 0; i < g->wildlifeCount; i++)
    {
        Wildlife *a = &g->wildlife[i];
        a->stepX = a->stepY = 0;
        if (g->tiles[a->home].node != NODE_GAME || g->tiles[a->home].amount <= 0)
        {
            a->state = ANIMAL_IDLE;
            continue;
        }
        const Citizen *threat = NULL;
        int nearest = 3 * 256;
        for (int c = 0; c < g->citizenCount; c++)
        {
            const Citizen *person = &g->citizens[c];
            int d = Dist(person->x, person->y, a->x, a->y);
            if (person->alive && d < nearest)
            {
                threat = person;
                nearest = d;
            }
        }
        bool towardThreat = threat && Dist(a->goalX, a->goalY, threat->x, threat->y) < nearest;
        if ((a->x == a->goalX && a->y == a->goalY) || towardThreat)
        {
            if (!threat && a->wait > 0)
            {
                a->wait--;
                a->state = ANIMAL_IDLE;
                continue;
            }
            int best = -1, score = INT_MIN, start = Random(a) % 4;
            const int dx[] = {1, 0, -1, 0}, dy[] = {0, 1, 0, -1};
            for (int n = 0; n < 4; n++)
            {
                int d = (start + n) % 4;
                int x = a->x / 256 + dx[d], y = a->y / 256 + dy[d];
                // Finish or reverse the current axis before turning a corner.
                // Retargeting mid-step must not cut across an obstructed tile.
                if ((a->x % 256 && y * 256 != a->y) || (a->y % 256 && x * 256 != a->x))
                    continue;
                if (!Clear(g, a, x, y))
                    continue;
                int value = threat ? Dist(x * 256, y * 256, threat->x, threat->y) : 4 - n;
                if (threat && value <= nearest)
                    continue;
                if (value > score)
                {
                    best = d;
                    score = value;
                }
            }
            if (best < 0)
            {
                a->goalX = a->x;
                a->goalY = a->y;
                a->state = ANIMAL_IDLE;
                a->wait = 20;
                continue;
            }
            a->goalX = (a->x / 256 + dx[best]) * 256;
            a->goalY = (a->y / 256 + dy[best]) * 256;
        }
        if (!Clear(g, a, a->goalX / 256, a->goalY / 256))
        {
            // A new wall can close a route while an animal is walking. Stop at
            // its current position; never slide through the new obstruction.
            a->goalX = a->x;
            a->goalY = a->y;
            a->state = ANIMAL_IDLE;
            a->wait = 20;
            continue;
        }
        a->state = threat ? ANIMAL_RUN : ANIMAL_WALK;
        int step = threat ? 18 : 6;
        a->facing = GameFacing(a->goalX - a->x, a->goalY - a->y, a->facing);
        if (a->x != a->goalX)
            a->stepX = a->x < a->goalX ? Min(step, a->goalX - a->x) : -Min(step, a->x - a->goalX);
        else
            a->stepY = a->y < a->goalY ? Min(step, a->goalY - a->y) : -Min(step, a->y - a->goalY);
        a->x += a->stepX;
        a->y += a->stepY;
        if (a->x == a->goalX && a->y == a->goalY)
            a->wait = 30 + Random(a) % 70;
    }
}
