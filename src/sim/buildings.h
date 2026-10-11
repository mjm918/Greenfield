#ifndef GREENFIELD_SIM_BUILDINGS_H
#define GREENFIELD_SIM_BUILDINGS_H
#include "game.h"

int BuildingsAdd(GameState *g, int type, int x, int y, bool done);
bool BuildingsNearby(const GameState *g, int type, int x, int y, int range);
bool BuildingsPlace(GameState *g, int type, int x, int y, char *reason, int size);
bool BuildingsUpgrade(GameState *g, int id, char *reason, int size);
void BuildingsWork(GameState *g, Citizen *c);
#endif
