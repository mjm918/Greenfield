#ifndef GREENFIELD_PRODUCTION_H
#define GREENFIELD_PRODUCTION_H
#include "game.h"
bool ProductionCommand(GameState *g, GameCommand cmd, char *reason, int size);
void ProductionCancel(GameState *g, Building *b, int index, int disposition);
void ProductionUpdate(GameState *g);
#endif
