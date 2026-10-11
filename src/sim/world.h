#ifndef GREENFIELD_SIM_WORLD_H
#define GREENFIELD_SIM_WORLD_H
#include "game.h"

void WorldInit(GameState *g);
int WorldAdjacent(const GameState *g, int owner, int x, int y, int w, int h, int from);
bool WorldRoute(GameState *g, Citizen *c, int destination);
void WorldMoveCitizen(GameState *g, Citizen *c);
#endif
