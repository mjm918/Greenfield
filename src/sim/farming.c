#include "farming.h"
#include "common.h"
#include "economy.h"
#include "jobs.h"

void FarmingWork(GameState *g, Citizen *c)
{
    Building *b = &g->buildings[c->target];
    if ((c->job == JOB_PLANT && b->crop != 0) || (c->job == JOB_HARVEST && b->crop != 2))
    {
        JobsReset(c);
        return;
    }
    if (c->work >= 180)
    {
        if (c->job == JOB_PLANT)
        {
            b->crop = 1;
            b->cropProgress = 0;
            JobsReset(c);
        }
        else
        {
            EconomyProduceBatch(g, c, FOOD, 24);
            c->harvestCargo = 1;
            b->crop = 0;
            b->cropProgress = 0;
            c->job = JOB_DELIVER;
            c->target = -1;
            c->work = 0;
        }
    }
}

void FarmingUpdate(GameState *g)
{
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *b = &g->buildings[i];
        if (b->type == FARM && b->complete && b->crop == 1 && g->tick >= WINTER_END)
        {
            if (++b->cropProgress >= 2400)
                b->crop = 2;
        }
    }
}
