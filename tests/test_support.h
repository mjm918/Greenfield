#ifndef GREENFIELD_TEST_SUPPORT_H
#define GREENFIELD_TEST_SUPPORT_H
#include "game.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(test) TestCheck((test), __FILE__, __LINE__, #test)
void TestCheck(bool passed, const char *file, int line, const char *expression);
int TestReport(void);
GameState *TestGame(void);
void Step(GameState *g, int n);
bool Command(GameState *g, int type, int a, int b, int c);
void Supply(GameState *g, int resource, int count);
void PrepareCamp(GameState *g);
void CampaignUntil(GameState *g, uint64_t tick);
void TestConstruction(void);
void TestEconomy(void);
void TestSurvival(void);
void TestFarming(void);
void TestRivals(void);
void TestPersistence(void);
void TestDeterminism(void);
void TestValidation(void);
void TestSandbox(void);
#endif
