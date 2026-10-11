#include "test_support.h"
#include "campaign.h"

static int checks, failures;
void TestCheck(bool passed, const char *file, int line, const char *expression)
{
    checks++;
    if (!passed)
    {
        fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expression);
        failures++;
    }
}
int TestReport(void)
{
    printf("%d checks; %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
GameState *TestGame(void)
{
    GameState *g = malloc(sizeof(*g));
    if (!g)
    {
        fputs("Cannot allocate a test village.\n", stderr);
        exit(1);
    }
    GameInit(g);
    return g;
}

void Step(GameState *g, int n)
{
    for (int i = 0; i < n; i++)
        GameTick(g);
}
bool Command(GameState *g, int type, int a, int b, int c)
{
    char reason[256];
    return GameCommandApply(g, (GameCommand){type, a, b, c}, reason, sizeof(reason));
}
void Supply(GameState *g, int r, int n)
{
    g->stock[r] += n;
    g->initial[r] += n;
}

// Reproduce the established paid construction + delivery fixture through commands.
void PrepareCamp(GameState *g)
{
    GameInit(g);
    CHECK(Command(g, CMD_BUILD, SHELTER, 18, 18));
    CHECK(Command(g, CMD_ASSIGN, FORAGER, 3, 0));
    Step(g, 1500);
    CHECK(Command(g, CMD_BUILD, PALISADE, 17, 19));
    Step(g, 500);
}
void CampaignUntil(GameState *g, uint64_t tick)
{
    GameInit(g);
    while (g->tick < tick && !g->lost)
    {
        CampaignOrders(g);
        GameTick(g);
    }
}
