#ifndef GREENFIELD_SIM_WORKSHOP_H
#define GREENFIELD_SIM_WORKSHOP_H
#include "game.h"

bool WorkshopDiscover(GameState *g, int owner, int discovery, char *reason, int size);
void WorkshopResearchWork(GameState *g, Citizen *c);
void WorkshopCraftWork(GameState *g, Citizen *c);
bool WorkshopHasWork(const GameState *g, int owner);
void WorkshopBeginCraft(GameState *g, Citizen *c);
#endif
