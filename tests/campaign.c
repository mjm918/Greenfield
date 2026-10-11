#include "campaign.h"
static void Assign(GameState *g, int role, int n)
{
    char why[128];
    GameCommandApply(g, (GameCommand){CMD_ASSIGN, role, n, 0}, why, sizeof(why));
}
static void Build(GameState *g, int type, int x, int y)
{
    char why[128];
    GameCommandApply(g, (GameCommand){CMD_BUILD, type, x, y}, why, sizeof(why));
}
void CampaignOrders(GameState *g)
{
    if (g->tick % 100)
        return;
    if (g->tick == 0)
    {
        char why[128];
        GameCommandApply(g, (GameCommand){CMD_TRUCE, 0, 0, 0}, why, sizeof(why));
        Assign(g, FORAGER, 0);
        Assign(g, HUNTER, 0);
    }
    if (g->tick == 2000)
    {
        Assign(g, FORAGER, 2);
        Assign(g, HUNTER, 1);
    }
    int shelters = GameBuildingCount(g, SHELTER, false);
    const int sites[][2] = {{18, 18}, {18, 21}, {18, 24}, {21, 26}, {24, 26}, {21, 17}, {24, 17}, {16, 16},
                            {16, 23}, {16, 25}, {20, 29}, {23, 29}, {16, 30}, {20, 31}, {23, 31}, {25, 15}};
    if (shelters < 6)
        Build(g, SHELTER, sites[shelters][0], sites[shelters][1]);
    if (!GameBuildingCount(g, WORKSHOP, false) && shelters >= 2)
        Build(g, WORKSHOP, 24, 19);
    if (!GameBuildingCount(g, FORAGING_POST, false) && shelters >= 2)
        Build(g, FORAGING_POST, 16, 29);
    if (!GameBuildingCount(g, HUNTING_CAMP, false) && shelters >= 3)
        Build(g, HUNTING_CAMP, 18, 32);
    if (g->tick > 2100 && !GameRoleCount(g, CRAFTER) && GameRoleCount(g, IDLE))
        Assign(g, CRAFTER, 1);
    if (g->tick > 2500 && GameRoleCount(g, BUILDER) > 1)
        Assign(g, BUILDER, 1);
    if (g->tick > 2600 && GameRoleCount(g, WOODCUTTER) < 2 && GameRoleCount(g, IDLE))
        Assign(g, WOODCUTTER, 2);
    if (g->research < 0 && GameBuildingCount(g, WORKSHOP, true) && GameRoleCount(g, CRAFTER))
    {
        int idea = !g->discovery[IMPROVED_TOOLS]                     ? IMPROVED_TOOLS
                   : !g->discovery[INSULATION]                       ? INSULATION
                   : !g->discovery[FOOD_STORAGE]                     ? FOOD_STORAGE
                   : !g->discovery[FARMING] && g->tick >= WINTER_END ? FARMING
                                                                     : -1;
        char why[128];
        if (idea >= 0)
            GameCommandApply(g, (GameCommand){CMD_RESEARCH, idea, 0, 0}, why, sizeof(why));
    }
    if (g->discovery[INSULATION])
        for (int i = 0; i < g->buildingCount; i++)
            if (g->buildings[i].type == SHELTER && g->buildings[i].complete && !g->buildings[i].upgraded)
            {
                char why[128];
                GameCommandApply(g, (GameCommand){CMD_UPGRADE, i, 0, 0}, why, sizeof(why));
                break;
            }
    if (g->discovery[FOOD_STORAGE] && !GameBuildingCount(g, GRANARY, false))
        Build(g, GRANARY, 25, 22);
    if (g->discovery[FARMING])
    {
        int farms = GameBuildingCount(g, FARM, false);
        if (farms < 4)
            Build(g, FARM, 28, 17 + farms * 3);
    }
    if (g->tick > ARRIVAL_FIRST && GameRoleCount(g, IDLE))
    {
        if (g->discovery[FARMING] && GameRoleCount(g, FARMER) < 2)
            Assign(g, FARMER, GameRoleCount(g, FARMER) + 1);
        else if (GameRoleCount(g, GUARD) < 4)
            Assign(g, GUARD, GameRoleCount(g, GUARD) + 1);
        else
            Assign(g, FORAGER, GameRoleCount(g, FORAGER) + 1);
    }
    if (g->raidWarning && !g->raidResolved)
    {
        char why[128];
        GameCommandApply(g, (GameCommand){CMD_TRUCE, 0, 0, 0}, why, sizeof(why));
    }
}
