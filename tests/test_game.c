#define _POSIX_C_SOURCE 200809L
#include "test_support.h"
#include "campaign.h"
#include <string.h>
#include <time.h>

static const struct { const char *name; void (*run)(void); } suites[] = {
    {"construction", TestConstruction},
    {"economy", TestEconomy},
    {"survival", TestSurvival},
    {"farming", TestFarming},
    {"rivals", TestRivals},
    {"persistence", TestPersistence},
    {"determinism", TestDeterminism},
    {"validation", TestValidation},
    {"sandbox", TestSandbox}
};

static double Now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
int main(int argc, char **argv)
{
    if (argc > 1 && (!strcmp(argv[1], "--campaign") || !strcmp(argv[1], "--realtime-campaign")))
    {
        bool real = !strcmp(argv[1], "--realtime-campaign");
        GameState *g = malloc(sizeof(*g));
        GameInit(g);
        double start = Now();
        char reason[256];
        for (int i = 0; i < 36000 && !g->lost && !g->won; i++)
        {
            CampaignOrders(g);
            GameTick(g);
            if (g->tick % 600 == 0)
            {
                printf("minute=%llu population=%d beds=%d food=%d wood=%d tools=%d research=%d harvest=%d "
                       "raids=%d resolved=%d hp=%d\n",
                       (unsigned long long)(g->tick / 600), GamePopulation(g), GameHousing(g), g->stock[FOOD],
                       g->stock[WOOD], g->stock[TOOLS], g->research, g->harvestDelivered, g->raidCount,
                       g->raidResolved, g->citizens[0].hp);
                fflush(stdout);
                if (!GameValidate(g, reason, sizeof(reason)))
                {
                    fprintf(stderr, "Invalid campaign: %s\n", reason);
                    return 1;
                }
            }
            if (real)
            {
                double until = start + g->tick / 10.0 - Now();
                if (until > 0)
                {
                    struct timespec nap = {(time_t)until, (long)((until - (time_t)until) * 1e9)};
                    nanosleep(&nap, NULL);
                }
            }
        }
        printf("CAMPAIGN %s at %.1f minutes; wall %.1fs; arrivals=%d deaths=%d food-days=%.1f "
               "shortage-recoveries=%d food-stolen=%d raids=%d\n",
               g->won ? "WON" : "INCOMPLETE", g->tick / 600.0, Now() - start, g->arrivals, g->deaths,
               GameFoodDaysTenths(g) / 10.0, g->shortageRecoveries, g->stolen[FOOD], g->raidCount);
        GameSave(g, real ? "evidence/realtime-campaign.gfv1" : "campaign.gfv1", reason, sizeof(reason));
        printf("state-hash=%016llx\n", (unsigned long long)GameHash(g));
        bool passed = g->won && g->tick >= 27000 && g->tick <= 36000 && g->shortageRecoveries > 0 &&
                      g->raidCount > 0 && g->stolen[FOOD] > 0;
        free(g);
        return passed ? 0 : 1;
    }
    if (argc == 3 && !strcmp(argv[1], "--suite"))
    {
        for (unsigned i = 0; i < sizeof(suites) / sizeof(suites[0]); i++)
            if (!strcmp(argv[2], suites[i].name))
            {
                suites[i].run();
                return TestReport();
            }
        fprintf(stderr, "Unknown simulation suite: %s\n", argv[2]);
        return 2;
    }
    for (unsigned i = 0; i < sizeof(suites) / sizeof(suites[0]); i++)
        suites[i].run();
    return TestReport();
}
