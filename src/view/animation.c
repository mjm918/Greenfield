#include "animation.h"
#include "raymath.h"
#include "world.h"

SpritePose ViewCitizenPose(const ViewContext *ui, int id)
{
    const Citizen *c = &ui->g->citizens[id];
    bool braids = id % 2 == 0, north = c->facing >= FACING_NE;
    bool moving = c->pathIndex < c->pathLength;
    bool attacking = c->job == JOB_FIGHT && c->attackCooldown > 0;
    bool working = !moving && !c->retry && (c->work > 0 || c->job == JOB_REPAIR);
    int row = 0, special = -1;
    if (c->job == JOB_DELIVER)
        row = 2;
    else if (attacking || (c->job == JOB_GUARD && !moving))
        row = 3;
    else if (working)
    {
        if (c->job == JOB_GATHER && c->target >= 0 && c->target < TILE_COUNT)
        {
            int node = ui->g->tiles[c->target].node;
            if (node == NODE_BERRIES)
                special = 0;
            else if (node == NODE_STONE || node == NODE_ORE)
                special = 1;
            else if (node == NODE_TREE)
                row = 1;
            else if (node == NODE_GAME)
                row = 3;
        }
        else if (c->job == JOB_BUILD || c->job == JOB_REPAIR || c->job == JOB_CRAFT ||
                 c->job == JOB_RESEARCH || c->job == JOB_SMELT)
            special = 2;
        else if (c->job == JOB_PLANT || c->job == JOB_HARVEST)
            special = 3;
    }
    int phase = (int)(ui->g->tick / 3 + (unsigned)id);
    if (!moving && !working && !attacking)
        phase = 0;
    SpritePose pose = {.flip = (c->facing & 1) != 0};
    if (special >= 0)
    {
        pose.atlas = braids ? &ui->braidsJobs : &ui->peopleJobs;
        pose.frame = special * 4 + (north ? 2 : 0) + phase % 2;
    }
    else
    {
        pose.atlas =
            braids ? (north ? &ui->braidsNorth : &ui->braids) : (north ? &ui->peopleNorth : &ui->people);
        pose.frame = row * 4 + phase % 4;
    }
    return pose;
}
SpritePose ViewAnimalPose(const ViewContext *ui, int id)
{
    const Wildlife *a = &ui->g->wildlife[id];
    int row = (a->facing >= FACING_NE ? 2 : 0) + (a->state == ANIMAL_RUN);
    int frame = a->state == ANIMAL_IDLE ? 0 : (int)(ui->g->tick / (a->state == ANIMAL_RUN ? 2 : 5) + id) % 4;
    return (SpritePose){&ui->animals, row * 4 + frame, (a->facing & 1) != 0};
}
Vector2 ViewAnimalPosition(const ViewContext *ui, int id)
{
    const Wildlife *a = &ui->g->wildlife[id];
    float alpha = !ui->speed || ui->menu || ui->help ? 1 : Clamp(ui->accumulator * TICKS_SECOND, 0, 1);
    return (Vector2){(a->x - a->stepX * (1 - alpha)) / 256.f + .5f,
                     (a->y - a->stepY * (1 - alpha)) / 256.f + .5f};
}
Rectangle ViewPoseBounds(SpritePose pose, Vector2 foot, float cellHeight)
{
    const Atlas *a = pose.atlas;
    Rectangle src = a->rect[pose.frame];
    float scale = a->cellHeight > 0 ? cellHeight / a->cellHeight : 1;
    Vector2 pivot = a->pivot[pose.frame];
    if (pose.flip)
        pivot.x = src.width - pivot.x;
    return (Rectangle){foot.x - pivot.x * scale, foot.y - pivot.y * scale, src.width * scale,
                       src.height * scale};
}
void ViewPoseDraw(SpritePose pose, Vector2 foot, float cellHeight, Color tint)
{
    Rectangle src = pose.atlas->rect[pose.frame];
    if (pose.flip)
        src.width = -src.width;
    DrawTexturePro(pose.atlas->texture, src, ViewPoseBounds(pose, foot, cellHeight), (Vector2){0}, 0, tint);
}
int ViewAnimalAt(const ViewContext *ui)
{
    Vector2 mouse = GetScreenToWorld2D(ui->mouse, ui->camera);
    int best = -1;
    float depth = -1;
    for (int i = 0; i < ui->g->wildlifeCount; i++)
    {
        const Wildlife *a = &ui->g->wildlife[i];
        if (!GameWildlifeForTile(ui->g, a->home))
            continue;
        Vector2 p = ViewAnimalPosition(ui, i), foot = ViewIso(p.x, p.y);
        foot.y += 5;
        if (p.x + p.y >= depth &&
            CheckCollisionPointRec(mouse, ViewPoseBounds(ViewAnimalPose(ui, i), foot, 52)))
        {
            best = a->home;
            depth = p.x + p.y;
        }
    }
    return best;
}
