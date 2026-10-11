#include "ai.h"
#include "common.h"
static bool Command(GameState *g, GameCommand c)
{
    return GameCommandApply(g, c, NULL, 0);
}
static int Find(const GameState *g, int o, int type, bool complete)
{
    for (int i = 0; i < g->buildingCount; i++)
    {
        const Building *b = &g->buildings[i];
        if (b->alive && b->owner == o && b->type == type && (!complete || b->complete))
            return i;
    }
    return -1;
}
static bool Build(GameState *g, int o, int type)
{
    Faction *f = &g->factions[o];
    int cx = o ? 73 : 22, cy = o ? 73 : 22;
    if (f->center >= 0)
    {
        cx = g->buildings[f->center].x;
        cy = g->buildings[f->center].y;
    }
    for (int radius = 2; radius < 28; radius++)
        for (int y = cy - radius; y <= cy + radius; y++)
            for (int x = cx - radius; x <= cx + radius; x++)
            {
                if (x % 4 || y % 4)
                    continue;
                if (Abs(x - cx) != radius && Abs(y - cy) != radius)
                    continue;
                if (GameCanPlaceFor(g, o, type, x, y, NULL, 0))
                    return Command(g,
                                   (GameCommand){.type = CMD_BUILD, .owner = o, .a = type, .b = x, .c = y});
            }
    return false;
}
void AiEconomy(GameState *g, int o)
{
    Faction *f = &g->factions[o];
    int pop = GamePopulationFor(g, o);
    if (!pop || f->defeated)
        return;
    int center = f->center;
    if (center < 0)
    {
        Build(g, o, HEARTH);
        return;
    }
    int farms = GameBuildingCountFor(g, o, FARM, false);
    int desired[ROLE_COUNT] = {0};
    desired[BUILDER] = pop > 24 ? 2 : 1;
    desired[HUNTER] = pop < 16 ? 2 : 3;
    desired[FORAGER] = 1;
    desired[WOODCUTTER] = pop > 20 ? 3 : 2;
    desired[STONE_GATHERER] = 1;
    desired[CRAFTER] = Find(g, o, WORKSHOP, true) >= 0 ? (Find(g, o, SMELTER, true) >= 0 ? 3 : 1) : 0;
    desired[FARMER] = Min(12, (farms + 1) / 2);
    desired[MINER] = f->discovery[METALLURGY] ? 2 : 0;
    int used[ROLE_COUNT] = {0};
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        if (!c->alive || c->owner != o || c->unitType || c->training >= 0)
            continue;
        int role = FORAGER;
        for (int r = 0; r < ROLE_COUNT; r++)
            if (used[r] < desired[r])
            {
                role = r;
                break;
            }
        used[role]++;
        if (c->role != role)
            Command(g,
                    (GameCommand){.type = CMD_ROLE, .owner = o, .a = role, .entity = GameCitizenRef(g, i)});
        if (role == CRAFTER)
        {
            int station = Find(g, o, used[CRAFTER] == 1 ? WORKSHOP : SMELTER, true);
            if (station >= 0 && (!c->orderCount || c->orders[0].target.slot != station))
            {
                Building *b = &g->buildings[station];
                Command(g, (GameCommand){.type = CMD_ORDER,
                                         .owner = o,
                                         .entity = GameCitizenRef(g, i),
                                         .order = {.type = ORDER_WORK,
                                                   .targetKind = TARGET_BUILDING,
                                                   .target = GameBuildingRef(g, station),
                                                   .x = b->x,
                                                   .y = b->y}});
            }
        }
    }
    int beds = GameHousingFor(g, o);
    if (beds < pop + 4 && GameBuildingCountFor(g, o, SHELTER, false) * 4 < pop + 8)
        Build(g, o, SHELTER);
    if (beds > pop && g->buildings[center].queueCount == 0)
        Command(g, (GameCommand){.type = CMD_RECRUIT, .owner = o, .entity = GameBuildingRef(g, center)});
    if (!GameBuildingCountFor(g, o, HUNTING_CAMP, false) && beds >= 8)
        Build(g, o, HUNTING_CAMP);
    if (!GameBuildingCountFor(g, o, FORAGING_POST, false) && beds >= 8)
        Build(g, o, FORAGING_POST);
    if (f->age >= SETTLED_AGE)
    {
        if (!GameBuildingCountFor(g, o, WORKSHOP, false))
            Build(g, o, WORKSHOP);
        if (pop >= 14 && !GameBuildingCountFor(g, o, TRAINING_GROUND, false))
            Build(g, o, TRAINING_GROUND);
        const int ideas[] = {FOOD_STORAGE, FARMING,    IMPROVED_TOOLS, INSULATION,
                             METALLURGY,   CROP_YIELD, STONEWORK};
        if (f->research < 0)
            for (int k = 0; k < DISCOVERY_COUNT; k++)
                if (!f->discovery[ideas[k]])
                {
                    Command(g, (GameCommand){.type = CMD_RESEARCH, .owner = o, .a = ideas[k]});
                    break;
                }
    }
    if (f->age >= FARMING_AGE)
    {
        if (farms < Max(6, pop / 3))
            Build(g, o, FARM);
        if (Find(g, o, GRANARY, false) < 0)
            Build(g, o, GRANARY);
        if (f->discovery[METALLURGY] && Find(g, o, SMELTER, false) < 0)
            Build(g, o, SMELTER);
    }
    if (!f->advancing)
        Command(g, (GameCommand){.type = CMD_ADVANCE, .owner = o});
    int training = Find(g, o, TRAINING_GROUND, true), army = 0;
    for (int i = 0; i < g->citizenCount; i++)
        army += g->citizens[i].alive && g->citizens[i].owner == o &&
                (g->citizens[i].unitType || g->citizens[i].training >= 0);
    if (training >= 0 && army < Min(8, Max(0, (pop - 10) / 3)))
        for (int i = g->citizenCount - 1; i >= 0; i--)
        {
            Citizen *c = &g->citizens[i];
            if (c->alive && c->owner == o && !c->unitType && c->training < 0 && c->role != BUILDER)
            {
                Command(g, (GameCommand){.type = CMD_TRAIN,
                                         .owner = o,
                                         .a = army % 2 ? SLINGER : SPEARMAN,
                                         .entity = GameCitizenRef(g, i),
                                         .order = {.target = GameBuildingRef(g, training)}});
                break;
            }
        }
}
void AiUpdate(GameState *g)
{
    for (int o = 0; o < 2; o++)
    {
        Faction *f = &g->factions[o];
        if (!f->aiEnabled || f->defeated || (int)g->tick < f->nextDecision)
            continue;
        f->nextDecision = (int)g->tick + 100;
        AiEconomy(g, o);
        if (g->treaty || g->truceUntil > (int)g->tick || f->raidActive || g->factions[1 - o].defeated)
            continue;
        int army = 0;
        for (int i = 0; i < g->citizenCount; i++)
            army += g->citizens[i].alive && g->citizens[i].owner == o && g->citizens[i].unitType &&
                    g->citizens[i].hp > 700;
        if (army < 2 || g->tick < 18000)
            continue;
        if (!f->raidWarning && (int)g->tick >= f->nextRaid)
        {
            f->raidWarning = 1;
            f->nextRaid = (int)g->tick + 900;
            if (o)
                GameEvent(g, "The rival is mustering a raid. Ninety seconds to prepare.");
        }
        else if (f->raidWarning && (int)g->tick >= f->nextRaid)
        {
            int target = Find(g, 1 - o, STOCKPILE, true);
            if (target < 0)
                target = g->factions[1 - o].center;
            if (target < 0)
                continue;
            Building *b = &g->buildings[target];
            int sent = 0;
            for (int i = 0; i < g->citizenCount && sent < Max(2, army / 2); i++)
            {
                Citizen *c = &g->citizens[i];
                if (!c->alive || c->owner != o || !c->unitType || c->hp < 700)
                    continue;
                if (Command(g, (GameCommand){.type = CMD_ORDER,
                                             .owner = o,
                                             .entity = GameCitizenRef(g, i),
                                             .order = {.type = ORDER_PLUNDER,
                                                       .targetKind = TARGET_BUILDING,
                                                       .target = GameBuildingRef(g, target),
                                                       .x = b->x,
                                                       .y = b->y}}))
                    sent++;
            }
            f->raidWarning = 0;
            f->raidActive = sent > 0;
            f->raidStarted = (int)g->tick;
            f->raidSize = sent;
            f->raidCount++;
            f->nextRaid = (int)g->tick + 7200;
        }
    }
}
