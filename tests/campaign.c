#include "game.h"
#include "sim/ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct
{
    uint64_t tick, treaty, military;
} Checkpoint;
static const Checkpoint checkpoints[] = {
#include "fixtures/rts-checkpoints.inc"
};
static bool Baseline(const GameState *g, bool military, bool record)
{
    uint64_t hash = GameHash(g);
    if (record)
    {
        printf("baseline %llu %016llx\n", (unsigned long long)g->tick, (unsigned long long)hash);
        return true;
    }
    for (unsigned i = 0; i < sizeof(checkpoints) / sizeof(checkpoints[0]); i++)
        if (checkpoints[i].tick == g->tick)
        {
            uint64_t expected = military ? checkpoints[i].military : checkpoints[i].treaty;
            if (hash == expected)
                return true;
            fprintf(stderr, "Campaign baseline differs at tick %llu: got %016llx, expected %016llx\n",
                    (unsigned long long)g->tick, (unsigned long long)hash, (unsigned long long)expected);
            return false;
        }
    return true;
}
// Test-only player: every intervention is a normal validated command. No stock,
// population, health, discovery, or clock shortcuts are used in this campaign.
static bool Command(GameState *g, GameCommand c)
{
    return GameCommandApply(g, c, NULL, 0);
}
static void Player(GameState *g, bool military)
{
    if (g->tick % 100)
        return;
    AiEconomy(g, 0);
    if (g->tick < 6000)
        for (int i = 0; i < g->citizenCount; i++)
        {
            Citizen *c = &g->citizens[i];
            if (c->alive && !c->owner && (c->role == HUNTER || c->role == FORAGER))
                Command(g, (GameCommand){.type = CMD_ROLE, .a = WOODCUTTER, .entity = GameCitizenRef(g, i)});
        }
    // Allow one raid to steal supplies before negotiating; keep subsequent
    // development peaceful until the offensive campaign deliberately resumes.
    bool raided = g->factions[0].exported[FOOD] > 0 || g->factions[0].deaths > 0;
    if (raided && (!military || g->tick < 60000) && g->truceUntil <= (int)g->tick && !g->treaty)
        Command(g, (GameCommand){.type = CMD_TRUCE});
    if (g->factions[0].age == BRONZE_AGE)
    {
        Command(g, (GameCommand){.type = CMD_BUILD, .a = WATCH_POST, .b = 38, .c = 38});
        Command(g, (GameCommand){.type = CMD_BUILD, .a = WATCH_POST, .b = 56, .c = 56});
        if (!military)
            Command(g, (GameCommand){.type = CMD_TREATY});
        else
        {
            int school = -1, army = 0;
            for (int b = 0; b < g->buildingCount; b++)
                if (g->buildings[b].alive && !g->buildings[b].owner &&
                    g->buildings[b].type == TRAINING_GROUND && g->buildings[b].complete)
                    school = b;
            for (int i = 0; i < g->citizenCount; i++)
                army += g->citizens[i].alive && !g->citizens[i].owner &&
                        (g->citizens[i].unitType || g->citizens[i].training >= 0);
            if (school >= 0 && army < 20)
                for (int i = g->citizenCount - 1; i >= 0; i--)
                {
                    Citizen *c = &g->citizens[i];
                    if (c->alive && !c->owner && !c->unitType && c->training < 0 && c->role != BUILDER)
                    {
                        Command(g, (GameCommand){.type = CMD_TRAIN,
                                                 .a = AXEMAN,
                                                 .entity = GameCitizenRef(g, i),
                                                 .order = {.target = GameBuildingRef(g, school)}});
                        break;
                    }
                }
            int target = g->factions[1].center;
            if (school >= 0 && target >= 0)
                for (int i = 0; i < g->citizenCount; i++)
                {
                    Citizen *c = &g->citizens[i];
                    if (!c->alive || c->owner || !c->unitType || c->training >= 0)
                        continue;
                    if (!c->equipped && g->tick < 72000)
                        Command(g, (GameCommand){.type = CMD_EQUIP,
                                                 .entity = GameCitizenRef(g, i),
                                                 .order = {.target = GameBuildingRef(g, school)}});
                    if (g->tick >= 72000 && g->truceUntil <= (int)g->tick && c->hp > 700 &&
                        (!c->orderCount || c->orders[0].type != ORDER_ATTACK))
                        Command(g, (GameCommand){.type = CMD_ORDER,
                                                 .entity = GameCitizenRef(g, i),
                                                 .order = {.type = ORDER_ATTACK,
                                                           .targetKind = TARGET_BUILDING,
                                                           .target = GameBuildingRef(g, target),
                                                           .x = g->buildings[target].x,
                                                           .y = g->buildings[target].y}});
                }
        }
    }
}
int main(int argc, char **argv)
{
    bool military = argc > 1 && !strcmp(argv[1], "military");
    bool record = argc > 2 && !strcmp(argv[2], "--record-baseline");
    GameState *g = calloc(1, sizeof(*g));
    GameInit(g);
    bool baselineOK = Baseline(g, military, record);
    double accumulator = 0;
    char why[256], file[128];
    snprintf(file, sizeof(file), "evidence/%s-campaign.gfv2", military ? "military" : "treaty");
    int lowestHP = 1000;
    uint64_t midHash = 0;
    for (int frame = 0; frame < 108000 && !g->lost && !g->won; frame++)
    {
        Player(g, military);
        GameAdvance(g, .1, 1, &accumulator);
        for (int i = 0; i < g->citizenCount; i++)
            if (g->citizens[i].alive && !g->citizens[i].owner && g->citizens[i].hp < lowestHP)
                lowestHP = g->citizens[i].hp;
        if (g->tick == 45000)
        {
            if (!GameSave(g, file, why, sizeof(why)))
            {
                puts(why);
                return 1;
            }
            midHash = GameHash(g);
            if (!GameLoad(g, file, why, sizeof(why)) || GameHash(g) != midHash)
            {
                puts("Mid-campaign continuation failed");
                return 1;
            }
        }
        if (g->tick % 6000 == 0)
        {
            baselineOK = Baseline(g, military, record) && baselineOK;
            printf("%s min=%llu pop=%d age=%d village=%d sites=%d raids=%d food=%d bronze=%d lost=%d\n",
                   military ? "military" : "treaty", g->tick / 600, GamePopulation(g), GameStage(g),
                   g->factions[0].village, GameSiteControl(g, 0), g->factions[1].raidCount,
                   g->factions[0].stock[FOOD], g->factions[0].stock[BRONZE], g->lost);
            fflush(stdout);
            if (!GameValidate(g, why, sizeof(why)))
            {
                puts(why);
                return 1;
            }
        }
    }
    Faction *f = &g->factions[0];
    printf("result won=%d lost=%d time=%.2f village=%.2f population=%d shortage recoveries=%d lowestHP=%d "
           "raids=%d exports=%d hash=%016llx\n",
           g->won, g->lost, g->tick / 600., f->villageTick / 600., GamePopulation(g), f->shortageRecoveries,
           lowestHP, g->factions[1].raidCount, f->exported[FOOD], (unsigned long long)GameHash(g));
    bool valid = GameValidate(g, why, sizeof(why));
    if (!valid)
        puts(why);
    bool ok = baselineOK && valid && g->won && !g->lost && g->tick >= 72000 && g->tick <= 108000 &&
              f->villageTick >= 27000 && f->villageTick <= 36000 && f->shortageRecoveries > 0 &&
              lowestHP < 1000 && g->factions[1].raidCount > 0 && f->exported[FOOD] > 0 &&
              (military ? g->factions[1].defeated : g->treaty);
    if (!GameSave(g, file, why, sizeof(why)))
    {
        puts(why);
        ok = false;
    }
    free(g);
    return ok ? 0 : 1;
}
