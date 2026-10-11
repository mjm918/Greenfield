#ifndef GREENFIELD_SIM_ECONOMY_H
#define GREENFIELD_SIM_ECONOMY_H
#include "game.h"

// Callers validate eligibility and costs; these functions keep stock/cargo and
// the existing conservation ledger together. No new inventory is maintained.
void EconomySpend(GameState *g, int resource, int count);
void EconomyPay(GameState *g, const int cost[RESOURCE_COUNT]);
int EconomyCargo(const Citizen *c);
int EconomyDeliveryTile(GameState *g, Citizen *c);
void EconomyDeliver(GameState *g, Citizen *c);
void EconomyGatherWork(GameState *g, Citizen *c);
void EconomyProduceBatch(GameState *g, Citizen *c, int resource, int count);
void EconomyDiscardCargo(GameState *g, Citizen *c);
void EconomySteal(GameState *g, int resource, int count);
#endif
