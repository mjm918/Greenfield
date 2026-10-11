#ifndef GREENFIELD_PROGRESSION_H
#define GREENFIELD_PROGRESSION_H
#include "game.h"
bool ProgressionAdvance(GameState *g, int owner, char *reason, int size);
void ProgressionWork(GameState *g, Citizen *c);
void ProgressionUpdate(GameState *g);
#endif
