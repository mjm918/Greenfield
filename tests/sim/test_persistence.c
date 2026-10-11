#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "test_support.h"
#include "campaign.h"

void TestPersistence(void)
{
    GameState *g = TestGame();
    GameState *b = TestGame();
    char reason[256];
    PrepareCamp(g);
    CHECK(GameSave(g, "persistence-save.gfv1", reason, sizeof(reason)));
    GameInit(b);
    CHECK(GameLoad(b, "persistence-save.gfv1", reason, sizeof(reason)));
    CHECK(GameHash(g) == GameHash(b));
    Step(g, 300);
    Step(b, 300);
    CHECK(GameHash(g) == GameHash(b));
    uint64_t before = GameHash(b);
    FILE *f = fopen("persistence-bad.gfv1", "wb");
    fputs("broken", f);
    fclose(f);
    CHECK(!GameLoad(b, "persistence-bad.gfv1", reason, sizeof(reason)));
    CHECK(GameHash(b) == before);
    f = fopen("persistence-save.gfv1.tmp", "wb");
    fputs("interrupted replacement", f);
    fclose(f);
    CHECK(GameLoad(b, "persistence-save.gfv1", reason, sizeof(reason)));
    int original = b->stock[FOOD];
    g->stock[FOOD] = -1;
    CHECK(!GameSave(g, "persistence-save.gfv1", reason, sizeof(reason)));
    CHECK(GameLoad(g, "persistence-save.gfv1", reason, sizeof(reason)));
    CHECK(g->stock[FOOD] == original);
    // A complete file with an altered version or payload must also be rejected.
    before = GameHash(b);
    const int corruptOffsets[] = {8, 100};
    for (unsigned i = 0; i < sizeof(corruptOffsets) / sizeof(corruptOffsets[0]); i++)
    {
        CHECK(GameSave(g, "persistence-bad.gfv1", reason, sizeof(reason)));
        f = fopen("persistence-bad.gfv1", "r+b");
        CHECK(f != NULL);
        if (f)
        {
            fseek(f, corruptOffsets[i], SEEK_SET);
            int byte = fgetc(f) ^ 0x40;
            fseek(f, corruptOffsets[i], SEEK_SET);
            fputc(byte, f);
            fclose(f);
            CHECK(!GameLoad(b, "persistence-bad.gfv1", reason, sizeof(reason)));
            CHECK(GameHash(b) == before);
        }
    }
    // A filesystem failure opening the replacement must leave the last usable slot intact.
    remove("persistence-save.gfv1.tmp");
    CHECK(mkdir("persistence-save.gfv1.tmp", 0700) == 0);
    CHECK(!GameSave(g, "persistence-save.gfv1", reason, sizeof(reason)));
    CHECK(GameLoad(b, "persistence-save.gfv1", reason, sizeof(reason)));
    CHECK(GameHash(g) == GameHash(b));
    rmdir("persistence-save.gfv1.tmp");
    // Save at each milestone with active jobs and discoveries; continuation must match.
    GameInit(g);
    for (int i = 0; i < 18000; i++)
    {
        CampaignOrders(g);
        GameTick(g);
        if (g->tick % 3000 == 0)
        {
            CHECK(GameSave(g, "persistence-save.gfv1", reason, sizeof(reason)));
            CHECK(GameLoad(b, "persistence-save.gfv1", reason, sizeof(reason)));
            CHECK(GameHash(g) == GameHash(b));
        }
    }
    Step(g, 1000);
    // Kill a real saving process. The slot must always be a complete old or new state.
    CHECK(GameSave(g, "persistence-save.gfv1", reason, sizeof(reason)));
    pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0)
    {
        for (int j = 0; j < 1000; j++)
        {
            g->stock[WOOD]++;
            g->initial[WOOD]++;
            GameSave(g, "persistence-save.gfv1", reason, sizeof(reason));
        }
        _exit(0);
    }
    if (child > 0)
    {
        struct timespec nap = {0, 25000000};
        nanosleep(&nap, NULL);
        kill(child, SIGTERM);
        waitpid(child, NULL, 0);
        CHECK(GameLoad(b, "persistence-save.gfv1", reason, sizeof(reason)));
        CHECK(GameValidate(b, reason, sizeof(reason)));
        CHECK(b->stock[WOOD] >= g->stock[WOOD] && b->stock[WOOD] <= g->stock[WOOD] + 1000);
    }
    remove("persistence-save.gfv1");
    remove("persistence-save.gfv1.tmp");
    remove("persistence-bad.gfv1");
    free(g);
    free(b);
}
