#ifndef GREENFIELD_SIM_RIVALS_H
#define GREENFIELD_SIM_RIVALS_H
#include "game.h"
void RivalsUpdate(GameState *g);
bool RivalsOfferTruce(GameState *g, char *reason, int size);
bool RivalsOfferTreaty(GameState *g, char *reason, int size);
#endif
