#ifndef GREENFIELD_SIM_CITIZENS_H
#define GREENFIELD_SIM_CITIZENS_H
#include "game.h"

int CitizensAdd(GameState *g, int owner, int role);
void CitizensResolveDeaths(GameState *g);
bool CitizensAssign(GameState *g, int owner, int role, int count, char *reason, int size);
bool CitizensSetArea(GameState *g, int owner, int role, int x, int y, char *reason, int size);
bool CitizensSetPriority(GameState *g, int owner, int role, int priority, char *reason, int size);
#endif
