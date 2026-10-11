#include "campaign.h"
#include <stdio.h>
#include <stdlib.h>

// Captured from the monolithic simulation at b1fbbc9. Never regenerate these
// merely to make a refactor pass: a mismatch means gameplay or the wire changed.
static const struct { uint64_t tick, hash; } checkpoints[] = {
#include "fixtures/campaign-checkpoints.inc"
};

static int Replay(GameState *g)
{
    char reason[256];
    for (unsigned i = 0; i < sizeof(checkpoints) / sizeof(checkpoints[0]); i++)
    {
        if (checkpoints[i].tick < g->tick)
            continue;
        while (g->tick < checkpoints[i].tick)
        {
            uint64_t previous = g->tick;
            CampaignOrders(g);
            GameTick(g);
            if (g->tick == previous)
            {
                fprintf(stderr, "Baseline replay stopped at tick %llu\n", (unsigned long long)g->tick);
                return 1;
            }
        }
        if (!GameValidate(g, reason, sizeof(reason)))
        {
            fprintf(stderr, "Baseline state invalid: %s\n", reason);
            return 1;
        }
        uint64_t actual = GameHash(g);
        if (actual != checkpoints[i].hash)
        {
            fprintf(stderr, "Tick %llu: expected %016llx, got %016llx\n",
                    (unsigned long long)g->tick, (unsigned long long)checkpoints[i].hash,
                    (unsigned long long)actual);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    GameState *g = malloc(sizeof(*g));
    if (!g)
        return 1;
    GameInit(g);
    int failures = Replay(g);
    const char *saves[] = {FIXTURE_DIR "/campaign-5000.gfv1", FIXTURE_DIR "/campaign-8600.gfv1"};
    for (unsigned i = 0; i < sizeof(saves) / sizeof(saves[0]); i++)
    {
        char reason[256];
        if (!GameLoad(g, saves[i], reason, sizeof(reason)))
        {
            fprintf(stderr, "Cannot load baseline fixture: %s\n", reason);
            failures++;
        }
        else
            failures += Replay(g);
    }
    free(g);
    if (!failures)
        puts("Baseline campaign and both version-1 saves match through tick 30600.");
    return failures ? 1 : 0;
}
