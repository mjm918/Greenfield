#ifndef GREENFIELD_ORDERS_H
#define GREENFIELD_ORDERS_H
#include "game.h"
bool OrdersApply(GameState *g, GameCommand cmd, char *reason, int size);
bool OrdersRally(GameState *g, GameCommand cmd, char *reason, int size);
void OrdersClear(GameState *g, Citizen *c);
void OrdersPop(GameState *g, Citizen *c);
bool OrdersChoose(GameState *g, Citizen *c);
#endif
