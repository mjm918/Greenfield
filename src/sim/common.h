#ifndef GREENFIELD_SIM_COMMON_H
#define GREENFIELD_SIM_COMMON_H
#include "game.h"
#include <stdio.h>

// Private scalar helpers. Random state belongs exclusively to GameState.
static inline int Min(int a, int b)
{
    return a < b ? a : b;
}
static inline int Max(int a, int b)
{
    return a > b ? a : b;
}
static inline int Abs(int a)
{
    return a < 0 ? -a : a;
}
static inline int Dist(int x, int y, int a, int b)
{
    return Abs(x - a) + Abs(y - b);
}
static inline bool Reject(char *r, int n, const char *s)
{
    if (r && n > 0)
        snprintf(r, (size_t)n, "%s", s);
    return false;
}
uint32_t SimRandom(GameState *g);
#endif
