#include "world.h"
#include "animation.h"
#include "assets.h"
#include "context.h"
#include "hud.h"
#include "layout.h"
#include "raymath.h"
#include "widgets.h"
#include <math.h>
#include <stdlib.h>

typedef struct
{
    int kind, id;
    float depth;
} DrawItem;

Vector2 ViewIso(float x, float y)
{
    return (Vector2){(x - y) * TILE_W / 2, (x + y) * TILE_H / 2};
}

Vector2 ViewTileAt(ViewContext *ui, Vector2 screen)
{
    Vector2 p = GetScreenToWorld2D(screen, ui->camera);
    return (Vector2){p.x / TILE_W + p.y / TILE_H, p.y / TILE_H - p.x / TILE_W};
}

static void Diamond(float x, float y, float w, float h, Color color)
{
    Vector2 top = ViewIso(x, y), right = ViewIso(x + w, y), bottom = ViewIso(x + w, y + h),
            left = ViewIso(x, y + h);
    DrawTriangle(top, left, bottom, color);
    DrawTriangle(top, bottom, right, color);
}

static void DiamondLine(float x, float y, float w, float h, Color color, float thick)
{
    Vector2 p[] = {ViewIso(x, y), ViewIso(x + w, y), ViewIso(x + w, y + h), ViewIso(x, y + h)};
    for (int i = 0; i < 4; i++)
        DrawLineEx(p[i], p[(i + 1) % 4], thick, color);
}

float ViewSnow(ViewContext *ui)
{
    return Clamp(1 - (float)((int64_t)ui->g->tick - (WINTER_END - 1200)) / 2400, 0, 1);
}

static unsigned TileNoise(int x, int y)
{
    unsigned n = (unsigned)(x * 3719 + y * 16699 + 19);
    n ^= n << 13;
    n ^= n >> 17;
    return n;
}

static bool Visible(const ViewContext *ui, float x, float y, float margin)
{
    Vector2 p = ViewIso(x, y);
    p.x = (p.x - ui->camera.target.x) * ui->camera.zoom + ui->camera.offset.x;
    p.y = (p.y - ui->camera.target.y) * ui->camera.zoom + ui->camera.offset.y;
    float pad = margin * ui->camera.zoom;
    return p.x > -pad && p.x < SCREEN_W + pad && p.y > WORLD_TOP - pad && p.y < SCREEN_H + pad;
}

static Vector2 ResourceVisual(int tile, int node, int *sprite, float *width)
{
    Vector2 foot = ViewIso(tile % MAP_SIZE + .5f, tile / MAP_SIZE + .5f);
    foot.y += 8;
    *sprite = node == NODE_TREE ? 12 : (node == NODE_STONE || node == NODE_ORE) ? 13 : 14;
    *width = node == NODE_TREE ? 64 : 50;
    return foot;
}

int ViewResourceAt(const ViewContext *ui)
{
    int animal = ViewAnimalAt(ui);
    if (animal >= 0)
        return animal;
    Vector2 mouse = GetScreenToWorld2D(ui->mouse, ui->camera);
    int best = -1, depth = -1;
    for (int t = 0; t < TILE_COUNT; t++)
    {
        const Tile *tile = &ui->g->tiles[t];
        if (!tile->node || tile->node == NODE_GAME)
            continue;
        int sprite, d = t % MAP_SIZE + t / MAP_SIZE;
        float width;
        Vector2 foot = ResourceVisual(t, tile->node, &sprite, &width);
        Rectangle src = ui->objects.rect[sprite];
        if (src.width <= 0)
            continue;
        float height = width * src.height / src.width;
        if (d >= depth &&
            CheckCollisionPointRec(mouse, (Rectangle){foot.x - width / 2, foot.y - height, width, height}))
        {
            best = t;
            depth = d;
        }
    }
    if (best >= 0)
        return best;
    return ui->hoverTile >= 0 && ui->g->tiles[ui->hoverTile].node ? ui->hoverTile : -1;
}

static void TerrainDraw(ViewContext *ui)
{
    GameState *g = ui->g;
    float snow = ViewSnow(ui);
    // Earth rim gives the board a coherent footprint without a background painting.
    Vector2 a = ViewIso(0, MAP_SIZE), b = ViewIso(MAP_SIZE, MAP_SIZE), c = ViewIso(MAP_SIZE, 0);
    Vector2 aa = {a.x, a.y + 18}, bb = {b.x, b.y + 18}, cc = {c.x, c.y + 18};
    DrawTriangle(a, aa, bb, (Color){80, 77, 55, 255});
    DrawTriangle(a, bb, b, (Color){80, 77, 55, 255});
    DrawTriangle(b, bb, cc, (Color){65, 70, 52, 255});
    DrawTriangle(b, cc, c, (Color){65, 70, 52, 255});
    for (int y = 0; y < MAP_SIZE; y++)
        for (int x = 0; x < MAP_SIZE; x++)
        {
            if (!Visible(ui, x + .5f, y + .5f, 64))
                continue;
            Tile *t = &g->tiles[GameTile(x, y)];
            unsigned n = TileNoise(x, y);
            float vary = (n % 11) / 100.0f;
            Color grass = ViewMix((Color){123, 146, 83, 255}, (Color){162, 177, 104, 255}, vary * 4);
            if (t->terrain == FERTILE)
                grass = (Color){132 + (n % 8), 139 + (n % 8), 78, 255};
            if (t->terrain == ROCK_GROUND)
                grass = (Color){142, 146, 129, 255};
            Color ground =
                ViewMix(grass, (Color){211 + (n % 8), 222 + (n % 8), 210 + (n % 8), 255}, snow * 0.94f);
            if (t->terrain == WATER)
            {
                ground = ViewMix((Color){69, 137, 150, 255}, (Color){136, 180, 183, 255}, snow * 0.55f);
                Diamond(x, y, 1, 1, ground);
                Vector2 p = ViewIso(x + 0.4f, y + 0.6f);
                if (n % 3 == 0)
                    DrawLineEx((Vector2){p.x - 8, p.y},
                               (Vector2){p.x + 8 + sin(ui->time * 1.5f + n) * 4, p.y}, 1,
                               Fade((Color){210, 239, 224, 255}, 0.5f));
            }
            else
            {
                Diamond(x, y, 1, 1, ground);
                Vector2 p = ViewIso(x + 0.35f, y + 0.45f);
                if (n % 5 == 0)
                    DrawLineEx(p, (Vector2){p.x + 7, p.y + 2}, 1,
                               Fade(snow > .6f ? WHITE : (Color){185, 190, 122, 255}, 0.45f));
                if (t->terrain == SHALLOWS)
                {
                    Diamond(x, y, 1, 1, ViewMix((Color){159, 169, 138, 255}, ground, 0.4f));
                    DrawEllipse((int)p.x, (int)p.y, 9, 4, (Color){177, 177, 153, 255});
                }
            }
            if (ui->grid)
                DiamondLine(x, y, 1, 1, Fade(INK, 0.10f), 0.7f);
        }
    // Narrow footpaths lead to the ford and across the founding camp.
    for (int y = 19; y <= 26; y++)
        for (int x = 19; x <= 33; x++)
            if ((y == 24 || x == 22) && g->tiles[GameTile(x, y)].terrain != WATER &&
                g->tiles[GameTile(x, y)].occupied < 0)
                Diamond(x, y, 1, 1,
                        ViewMix((Color){179, 158, 111, 255}, (Color){203, 207, 187, 255}, snow * 0.6f));
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *building = &g->buildings[i];
        if (!building->alive)
            continue;
        const BuildingDef *d = &BUILDINGS[building->type];
        if (building->type == FARM)
        {
            Diamond(building->x, building->y, 2, 2, (Color){104, 90, 63, 255});
            for (int row = 0; row < 6; row++)
            {
                Vector2 start = ViewIso(building->x + 0.2f, building->y + row * 0.3f + 0.2f),
                        end = ViewIso(building->x + 1.8f, building->y + row * 0.3f + 0.2f);
                DrawLineEx(start, end, 3, (Color){76, 66, 45, 255});
            }
        }
        else
            Diamond(building->x, building->y, d->w, d->h,
                    ViewMix((Color){163, 143, 101, 255}, (Color){199, 203, 184, 255}, snow * 0.45f));
    }
}

static int DepthCompare(const void *a, const void *b)
{
    const DrawItem *x = a, *y = b;
    return x->depth < y->depth ? -1 : x->depth > y->depth ? 1 : x->kind - y->kind;
}

Vector2 ViewBuildingVisual(ViewContext *ui, const Building *b, Atlas *atlas, int *index, float *width)
{
    const BuildingDef *d = &BUILDINGS[b->type];
    Vector2 foot = ViewIso(b->x + d->w * .5f, b->y + d->h * .5f);
    foot.y += d->h * 5;
    *index = !b->complete                 ? 2
             : b->type >= TRAINING_GROUND ? 5
             : b->type == HEARTH          ? (b->upgraded ? 11 : 0)
             : b->upgraded                ? 11
                                          : b->type;
    *width = !b->complete            ? 48
             : b->type == WORKSHOP   ? 126
             : b->type == HEARTH     ? 64
             : b->type == PALISADE   ? 58
             : b->type == GATE       ? 62
             : b->type == WATCH_POST ? 59
                                     : 102;
    *atlas = ui->objects;
    if (b->complete && b->type >= TRAINING_GROUND)
    {
        *atlas = ui->bronze;
        *index = b->type - TRAINING_GROUND;
        *width = 112;
    }
    if (b->complete && b->type == HEARTH && b->upgraded >= 2)
    {
        *atlas = ui->bronze;
        *index = b->upgraded == 3 ? 3 : 2;
        *width = 132;
    }
    return foot;
}

static void BuildingDraw(ViewContext *ui, int id)
{
    GameState *g = ui->g;
    Building *b = &g->buildings[id];
    const BuildingDef *d = &BUILDINGS[b->type];
    int index;
    float width;
    Atlas atlas;
    Vector2 foot = ViewBuildingVisual(ui, b, &atlas, &index, &width);
    if (!b->complete)
    {
        DiamondLine(b->x, b->y, d->w, d->h, Fade(ACCENT, 0.8f), 2);
        Vector2 start = ViewIso(b->x + 0.2f, b->y + 0.2f),
                end = ViewIso(b->x + d->w - 0.2f, b->y + d->h - 0.2f);
        DrawLineEx(start, (Vector2){start.x, start.y - 30}, 4, (Color){115, 80, 45, 255});
        DrawLineEx(end, (Vector2){end.x, end.y - 30}, 4, (Color){115, 80, 45, 255});
        DrawLineEx((Vector2){start.x, start.y - 30}, (Vector2){end.x, end.y - 30}, 3,
                   (Color){145, 110, 61, 255});
        ViewSprite(ui->objects, 2, foot, 48, Fade(WHITE, 0.9f), false);
        ViewBar(foot.x - 28, foot.y + 10, 56, (float)b->progress / d->labor, ACCENT);
        return;
    }
    if (b->type == FARM)
    {
        if (b->crop)
        {
            for (int yy = 0; yy < 2; yy++)
                for (int xx = 0; xx < 2; xx++)
                {
                    Vector2 p = ViewIso(b->x + xx + .55f, b->y + yy + .55f);
                    p.y += 6;
                    float growth = b->crop == 2 ? 1 : .25f + .7f * b->cropProgress / 2400.0f;
                    ViewSprite(ui->objects, 7, p, 38 * growth,
                               b->crop == 2 ? WHITE : (Color){163, 200, 114, 255}, false);
                }
        }
        return;
    }
    ViewSprite(atlas, index, foot, width, b->owner ? (Color){238, 194, 180, 255} : WHITE, false);
    if (b->hp < GameBuildingMaxHealth(g, b))
        ViewBar(foot.x - 25, foot.y + 10, 50, (float)b->hp / GameBuildingMaxHealth(g, b),
                b->owner ? RUST : SAGE);
    if (b->type == HEARTH && g->factions[b->owner].warmth > 20)
    {
        for (int i = 0; i < 4; i++)
        {
            float age = fmodf(ui->time * .25f + i * .24f, 1);
            DrawCircleV((Vector2){foot.x + sinf(age * 4 + i) * 6, foot.y - 40 - age * 55}, 4 + age * 7,
                        Fade((Color){227, 222, 195, 255}, (1 - age) * .35f));
        }
        DrawCircleGradient((Vector2){foot.x, foot.y - 15}, 32, Fade(ACCENT, .12f), BLANK);
    }
}

static void PersonDraw(ViewContext *ui, int id, bool raider)
{
    GameState *g = ui->g;
    Citizen *c = &g->citizens[id];
    raider = c->owner != 0;
    int job = c->job, hp = c->hp;
    SpritePose pose = ViewCitizenPose(ui, id);
    Vector2 foot = ViewIso(c->x / 256.f + .5f, c->y / 256.f + .5f);
    foot.y += 5;
    Rectangle bounds = ViewPoseBounds(pose, foot, 42.5f);
    float h = foot.y - bounds.y;
    ViewPoseDraw(pose, foot, 42.5f, raider ? (Color){245, 158, 137, 255} : WHITE);
    if (c->unitType)
    {
        int side = pose.flip ? -1 : 1;
        Color weapon = c->equipped ? (Color){219, 154, 71, 255} : PAPER;
        if (c->unitType == SPEARMAN)
        {
            DrawLineEx((Vector2){foot.x + side * 9, foot.y}, (Vector2){foot.x + side * 9, foot.y - 39}, 2,
                       weapon);
            DrawEllipse((int)foot.x - side * 8, (int)foot.y - 15, 5, 7, raider ? RUST : SAGE);
        }
        else if (c->unitType == AXEMAN)
        {
            DrawLineEx((Vector2){foot.x + side * 8, foot.y - 4}, (Vector2){foot.x + side * 13, foot.y - 27},
                       2, ACCENT);
            DrawRectangle((int)foot.x + side * 10, (int)foot.y - 28, 8, 5, weapon);
        }
        else
            DrawCircleLines((int)foot.x + side * 11, (int)foot.y - 22, 4, weapon);
    }
    if (hp < 900 || raider)
        ViewBar(foot.x - 13, foot.y - h - 7, 26, (float)hp / 1000, raider ? RUST : SAGE);
    if (!raider && job == JOB_IDLE && g->citizens[id].role == IDLE)
    {
        DrawCircle((int)foot.x, (int)(foot.y - h - 5), 2, ACCENT);
    }
}

static void SelectionDraw(ViewContext *ui)
{
    // Draw after sprites so foliage cannot hide selection. Keep strokes and
    // markers legible at overview zoom without changing sprite scale or depth.
    float pixel = 1 / ui->camera.zoom;
    const Building *b = ViewSelectedBuilding(ui);
    if (b && b->generation == ui->selectedGeneration)
    {
        const BuildingDef *d = &BUILDINGS[b->type];
        DiamondLine(b->x, b->y, d->w, d->h, INK, 7 * pixel);
        DiamondLine(b->x, b->y, d->w, d->h, PAPER, 4 * pixel);
        DiamondLine(b->x, b->y, d->w, d->h, ACCENT, 2 * pixel);
    }
    for (int i = 0; i < ui->selectionCount; i++)
    {
        EntityRef ref = ui->selection[i];
        if (!GameCitizenValid(ui->g, ref))
            continue;
        Citizen *c = &ui->g->citizens[ref.slot];
        Vector2 foot = ViewIso(c->x / 256.f + .5f, c->y / 256.f + .5f);
        foot.y += 5;
        float rx = fmaxf(18, 11 * pixel), ry = fmaxf(8, 5 * pixel);
        for (int pass = 0; pass < 2; pass++)
            for (int segment = 0; segment < 32; segment++)
            {
                float a = segment * 2 * PI / 32, z = (segment + 1) * 2 * PI / 32;
                DrawLineEx((Vector2){foot.x + cosf(a) * rx, foot.y + sinf(a) * ry},
                           (Vector2){foot.x + cosf(z) * rx, foot.y + sinf(z) * ry},
                           (pass ? 2.5f : 5.5f) * pixel, pass ? ACCENT : INK);
            }
        Rectangle bounds = ViewPoseBounds(ViewCitizenPose(ui, ref.slot), foot, 42.5f);
        Vector2 tip = {foot.x, bounds.y - 5 * pixel};
        DrawTriangle((Vector2){tip.x - 7 * pixel, tip.y - 10 * pixel}, tip,
                     (Vector2){tip.x + 7 * pixel, tip.y - 10 * pixel}, INK);
        DrawTriangle((Vector2){tip.x - 4 * pixel, tip.y - 8 * pixel}, (Vector2){tip.x, tip.y - 2 * pixel},
                     (Vector2){tip.x + 4 * pixel, tip.y - 8 * pixel}, PAPER);
    }
}

void ViewWorldDraw(ViewContext *ui)
{
    // Keep the input camera in logical coordinates; rasterize at Retina resolution.
    Camera2D renderCamera = ui->camera;
    renderCamera.offset = Vector2Scale(renderCamera.offset, RENDER_SCALE);
    renderCamera.zoom *= RENDER_SCALE;
    BeginScissorMode(0, WORLD_TOP * RENDER_SCALE, SCREEN_W * RENDER_SCALE,
                     (SCREEN_H - WORLD_TOP) * RENDER_SCALE);
    BeginMode2D(renderCamera);
    TerrainDraw(ui);
    GameState *g = ui->g;
    DrawItem items[TILE_COUNT + MAX_BUILDINGS + MAX_CITIZENS + MAX_WILDLIFE + 4];
    int count = 0;
    for (int t = 0; t < TILE_COUNT; t++)
        if (g->tiles[t].node && g->tiles[t].node != NODE_GAME &&
            Visible(ui, t % MAP_SIZE + .5f, t / MAP_SIZE + .5f, 150))
            items[count++] = (DrawItem){0, t, (float)(t % MAP_SIZE + t / MAP_SIZE) + 1};
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *b = &g->buildings[i];
        if (!b->alive || !Visible(ui, b->x + 1, b->y + 1, 200))
            continue;
        items[count++] =
            (DrawItem){1, i, (float)(b->x + b->y) + (BUILDINGS[b->type].w + BUILDINGS[b->type].h) * .5f};
    }
    for (int i = 0; i < g->citizenCount; i++)
        if (g->citizens[i].alive && Visible(ui, g->citizens[i].x / 256.f, g->citizens[i].y / 256.f, 64))
            items[count++] = (DrawItem){2, i, (g->citizens[i].x + g->citizens[i].y) / 256.0f + 1};
    for (int i = 0; i < g->wildlifeCount; i++)
    {
        const Wildlife *a = &g->wildlife[i];
        Vector2 p = ViewAnimalPosition(ui, i);
        if (GameWildlifeForTile(g, a->home) && Visible(ui, p.x, p.y, 90))
            items[count++] = (DrawItem){3, i, p.x + p.y};
    }
    // Draw ground shadows together. Interleaving thousands of primitive shadows
    // and atlas sprites flushed the GPU batch for almost every tree in overview.
    for (int i = 0; i < count; i++)
    {
        DrawItem item = items[i];
        Vector2 foot;
        float width = 10, height = 4;
        if (item.kind == 0)
        {
            foot = ViewIso(item.id % MAP_SIZE + .5f, item.id / MAP_SIZE + .5f);
            foot.y += 8;
            width = g->tiles[item.id].node == NODE_TREE ? 18 : 14;
            height = width / 3;
        }
        else if (item.kind == 1)
        {
            Building *b = &g->buildings[item.id];
            if (!b->complete || b->type == FARM)
                continue;
            Atlas atlas;
            int index;
            float spriteWidth;
            foot = ViewBuildingVisual(ui, b, &atlas, &index, &spriteWidth);
            width = spriteWidth * .35f;
            height = spriteWidth * .12f;
        }
        else if (item.kind == 2)
        {
            Citizen *c = &g->citizens[item.id];
            foot = ViewIso(c->x / 256.f + .5f, c->y / 256.f + .5f);
            foot.y += 4;
        }
        else
        {
            Vector2 p = ViewAnimalPosition(ui, item.id);
            foot = ViewIso(p.x, p.y);
            foot.y += 5;
            width = 14;
            height = 5;
        }
        DrawEllipse((int)foot.x, (int)foot.y, width, height, Fade(INK, .15f));
    }
    qsort(items, (size_t)count, sizeof(items[0]), DepthCompare);
    float snow = ViewSnow(ui);
    for (int i = 0; i < count; i++)
    {
        DrawItem item = items[i];
        if (item.kind == 0)
        {
            Tile *t = &g->tiles[item.id];
            int sprite;
            float width;
            Vector2 foot = ResourceVisual(item.id, t->node, &sprite, &width);
            Color tint = t->node == NODE_ORE    ? (Color){204, 142, 84, 255}
                         : t->node == NODE_TREE ? ViewMix(WHITE, (Color){194, 218, 207, 255}, snow * .7f)
                                                : WHITE;
            ViewSprite(ui->objects, sprite, foot, width, tint, false);
            if (snow > .1f && t->node == NODE_STONE)
                DrawEllipse((int)(foot.x - 4), (int)(foot.y - 18), 13, 4,
                            Fade((Color){236, 240, 223, 255}, snow * .9f));
        }
        else if (item.kind == 1)
            BuildingDraw(ui, item.id);
        else if (item.kind == 2)
            PersonDraw(ui, item.id, false);
        else if (item.kind == 3)
        {
            Vector2 p = ViewAnimalPosition(ui, item.id), foot = ViewIso(p.x, p.y);
            foot.y += 5;
            ViewPoseDraw(ViewAnimalPose(ui, item.id), foot, 52, WHITE);
        }
    }
    const int sites[2][2] = {{38, 38}, {56, 56}};
    for (int i = 0; i < 2; i++)
    {
        Vector2 p = ViewIso(sites[i][0], sites[i][1]);
        DiamondLine(sites[i][0] - 2, sites[i][1] - 2, 5, 5, ACCENT, 2);
        DrawCircleV(p, 5, ACCENT);
    }
    SelectionDraw(ui);
    if (ui->orderMode == ORDER_GATHER && ViewWorldHit(ui))
    {
        int resource = ViewResourceAt(ui);
        if (resource >= 0)
        {
            int x = resource % MAP_SIZE, y = resource / MAP_SIZE;
            DiamondLine(x, y, 1, 1, INK, 7 / ui->camera.zoom);
            DiamondLine(x, y, 1, 1, PAPER, 4 / ui->camera.zoom);
            DiamondLine(x, y, 1, 1, ACCENT, 2 / ui->camera.zoom);
        }
    }
    if (ui->areaRole >= 0 && ui->hoverTile >= 0)
    {
        int x = ui->hoverTile % MAP_SIZE, y = ui->hoverTile / MAP_SIZE;
        for (int yy = y - 9; yy <= y + 9; yy++)
            for (int xx = x - 9; xx <= x + 9; xx++)
                if (GameTile(xx, yy) >= 0 && abs(xx - x) + abs(yy - y) <= 9)
                    DiamondLine(xx, yy, 1, 1, Fade(ACCENT, .45f), 1);
    }
    if (ui->build >= 0 && ui->hoverTile >= 0)
    {
        int x = ui->hoverTile % MAP_SIZE, y = ui->hoverTile / MAP_SIZE;
        char why[256];
        bool valid = GameCanPlace(g, ui->build, x, y, why, sizeof(why));
        const BuildingDef *d = &BUILDINGS[ui->build];
        Diamond(x, y, d->w, d->h, Fade(valid ? SAGE : RUST, .42f));
        DiamondLine(x, y, d->w, d->h, valid ? SAGE : RUST, 2.5f);
        int index = ui->build == WATCH_POST ? 8
                    : ui->build == PALISADE ? 9
                    : ui->build == GATE     ? 10
                                            : ui->build;
        ViewSprite(ui->build >= TRAINING_GROUND ? ui->bronze : ui->objects,
                   ui->build >= TRAINING_GROUND ? ui->build - TRAINING_GROUND : index,
                   ViewIso(x + d->w * .5f, y + d->h * .5f), ui->build == WORKSHOP ? 126 : 90,
                   Fade(WHITE, .65f), false);
    }
    if (ui->panel == 1)
    {
        Vector2 p = ViewIso(g->factions[0].rallyX + .5f, g->factions[0].rallyY + .5f);
        DrawEllipseLines((int)p.x, (int)p.y, 19, 9, ACCENT);
        DrawLineEx(p, (Vector2){p.x, p.y - 27}, 2, ACCENT);
        DrawTriangle((Vector2){p.x, p.y - 27}, (Vector2){p.x, p.y - 17}, (Vector2){p.x + 14, p.y - 22},
                     ACCENT);
    }
    EndMode2D();
    EndScissorMode();
}
