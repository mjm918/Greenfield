#include "world.h"
#include "assets.h"
#include "context.h"
#include "raymath.h"
#include "widgets.h"
#include <math.h>
#include <stdlib.h>

typedef struct
{
    int kind, id;
    float depth;
} DrawItem;

Vector2 ViewIso(float x, float y) { return (Vector2){(x - y) * TILE_W / 2, (x + y) * TILE_H / 2}; }

Vector2 ViewTileAt(ViewContext *ui, Vector2 screen)
{
    Vector2 p = GetScreenToWorld2D(screen, ui->camera);
    return (Vector2){p.x / TILE_W + p.y / TILE_H, p.y / TILE_H - p.x / TILE_W};
}

static void Diamond(float x, float y, float w, float h, Color color)
{
    Vector2 top = ViewIso(x, y), right = ViewIso(x + w, y), bottom = ViewIso(x + w, y + h), left = ViewIso(x, y + h);
    DrawTriangle(top, left, bottom, color);
    DrawTriangle(top, bottom, right, color);
}

static void DiamondLine(float x, float y, float w, float h, Color color, float thick)
{
    Vector2 p[] = {ViewIso(x, y), ViewIso(x + w, y), ViewIso(x + w, y + h), ViewIso(x, y + h)};
    for (int i = 0; i < 4; i++)
        DrawLineEx(p[i], p[(i + 1) % 4], thick, color);
}

float ViewSnow(ViewContext *ui) { return Clamp(1 - (float)((int64_t)ui->g->tick - (WINTER_END - 1200)) / 2400, 0, 1); }

static unsigned TileNoise(int x, int y)
{
    unsigned n = (unsigned)(x * 3719 + y * 16699 + 19);
    n ^= n << 13;
    n ^= n >> 17;
    return n;
}

static void TerrainDraw(ViewContext *ui)
{
    GameState *g = ui->g;
    float snow = ViewSnow(ui);
    // Earth rim gives the board a coherent footprint without a background painting.
    Vector2 a = ViewIso(0, 48), b = ViewIso(48, 48), c = ViewIso(48, 0);
    Vector2 aa = {a.x, a.y + 18}, bb = {b.x, b.y + 18}, cc = {c.x, c.y + 18};
    DrawTriangle(a, aa, bb, (Color){80, 77, 55, 255});
    DrawTriangle(a, bb, b, (Color){80, 77, 55, 255});
    DrawTriangle(b, bb, cc, (Color){65, 70, 52, 255});
    DrawTriangle(b, cc, c, (Color){65, 70, 52, 255});
    for (int y = 0; y < MAP_SIZE; y++)
        for (int x = 0; x < MAP_SIZE; x++)
        {
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
                    DrawLineEx((Vector2){p.x - 8, p.y}, (Vector2){p.x + 8 + sin(ui->time * 1.5f + n) * 4, p.y},
                               1, Fade((Color){210, 239, 224, 255}, 0.5f));
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

Vector2 ViewBuildingVisual(const Building *b, int *index, float *width)
{
    const BuildingDef *d = &BUILDINGS[b->type];
    Vector2 foot = ViewIso(b->x + d->w * .5f, b->y + d->h * .5f);
    foot.y += d->h * 5;
    *index = !b->complete ? 2 : b->upgraded ? 11 : b->type;
    *width = !b->complete            ? 48
             : b->type == WORKSHOP   ? 126
             : b->type == HEARTH     ? 64
             : b->type == PALISADE   ? 58
             : b->type == GATE       ? 62
             : b->type == WATCH_POST ? 59
                                     : 102;
    return foot;
}

static void BuildingDraw(ViewContext *ui, int id)
{
    GameState *g = ui->g;
    Building *b = &g->buildings[id];
    const BuildingDef *d = &BUILDINGS[b->type];
    int index;
    float width;
    Vector2 foot = ViewBuildingVisual(b, &index, &width);
    if (id == ui->selected)
        DiamondLine(b->x, b->y, d->w, d->h, ACCENT, 2.5f);
    if (!b->complete)
    {
        DiamondLine(b->x, b->y, d->w, d->h, Fade(ACCENT, 0.8f), 2);
        Vector2 start = ViewIso(b->x + 0.2f, b->y + 0.2f), end = ViewIso(b->x + d->w - 0.2f, b->y + d->h - 0.2f);
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
                    ViewSprite(ui->objects, 7, p, 38 * growth, b->crop == 2 ? WHITE : (Color){163, 200, 114, 255},
                           false);
                }
        }
        return;
    }
    DrawEllipse((int)foot.x, (int)(foot.y - 3), width * .35f, width * .12f,
                Fade((Color){28, 45, 35, 255}, .15f));
    ViewSprite(ui->objects, index, foot, width, WHITE, false);
    if (b->type == HEARTH && g->warmth > 20)
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
    int x, y, job, hp, frame;
    bool attacking = false, working = false;
    Atlas person = !raider && id % 2 == 0 ? ui->braids : ui->people;
    bool moving, flip = false;
    if (raider)
    {
        Raider *r = &g->raiders[id];
        x = r->x;
        y = r->y;
        job = JOB_FIGHT;
        hp = r->hp;
        moving = r->cooldown == 0;
        attacking = r->cooldown > 0;
    }
    else
    {
        Citizen *c = &g->citizens[id];
        x = c->x;
        y = c->y;
        job = c->job;
        hp = c->hp;
        moving = c->pathIndex < c->pathLength;
        attacking = c->job == JOB_FIGHT && c->attackCooldown > 0;
        working = c->work > 0;
        if (attacking)
            moving = false;
        if (moving)
        {
            int dest = c->path[c->pathIndex];
            flip = dest % MAP_SIZE * 256 < x || dest / MAP_SIZE * 256 > y;
        }
    }
    Vector2 foot = ViewIso(x / 256.0f + .5f, y / 256.0f + .5f);
    foot.y += 5;
    int row = 0;
    if (job == JOB_DELIVER)
        row = 2;
    else if (attacking || (job == JOB_GUARD && !moving))
        row = 3;
    else if (!moving && working && job != JOB_IDLE && job != JOB_FIGHT)
        row = 1;
    frame = (int)(g->tick / 3 + (unsigned)id) % 4;
    if (!moving && (job == JOB_IDLE || job == JOB_GUARD || job == JOB_DELIVER))
        frame = 0;
    Rectangle src = person.rect[row * 4 + frame];
    float height = 34, ratio = height / (person.texture.height / 4 * .8f), width = src.width * ratio;
    float h = src.height * ratio;
    if (flip)
        src.width = -src.width;
    DrawEllipse((int)foot.x, (int)(foot.y - 1), 10, 4, Fade(INK, .20f));
    if (!raider && id == ui->selectedCitizen)
        DrawEllipseLines((int)foot.x, (int)foot.y, 13, 5, ACCENT);
    DrawTexturePro(person.texture, src, (Rectangle){foot.x - width / 2, foot.y - h, width, h},
                   (Vector2){0, 0}, 0, raider ? (Color){245, 158, 137, 255} : WHITE);
    if (hp < 900 || raider)
        ViewBar(foot.x - 13, foot.y - h - 7, 26, (float)hp / (raider ? 650 : 1000), raider ? RUST : SAGE);
    if (!raider && job == JOB_IDLE && g->citizens[id].role == IDLE)
    {
        DrawCircle((int)foot.x, (int)(foot.y - h - 5), 2, ACCENT);
    }
}

void ViewWorldDraw(ViewContext *ui)
{
    // Keep the input camera in logical coordinates; rasterize at Retina resolution.
    Camera2D renderCamera = ui->camera;
    renderCamera.offset = Vector2Scale(renderCamera.offset, RENDER_SCALE);
    renderCamera.zoom *= RENDER_SCALE;
    BeginScissorMode(0, 86 * RENDER_SCALE, PANEL_X * RENDER_SCALE,
                     (SCREEN_H - 250) * RENDER_SCALE);
    BeginMode2D(renderCamera);
    TerrainDraw(ui);
    GameState *g = ui->g;
    DrawItem items[TILE_COUNT + MAX_BUILDINGS + MAX_CITIZENS + MAX_RAIDERS + 4];
    int count = 0;
    for (int t = 0; t < TILE_COUNT; t++)
        if (g->tiles[t].node)
            items[count++] = (DrawItem){0, t, (float)(t % MAP_SIZE + t / MAP_SIZE) + 1};
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *b = &g->buildings[i];
        items[count++] =
            (DrawItem){1, i, (float)(b->x + b->y) + (BUILDINGS[b->type].w + BUILDINGS[b->type].h) * .5f};
    }
    for (int i = 0; i < g->citizenCount; i++)
        if (g->citizens[i].alive)
            items[count++] = (DrawItem){2, i, (g->citizens[i].x + g->citizens[i].y) / 256.0f + 1};
    for (int i = 0; i < MAX_RAIDERS; i++)
        if (g->raiders[i].active)
            items[count++] = (DrawItem){3, i, (g->raiders[i].x + g->raiders[i].y) / 256.0f + 1};
    items[count++] = (DrawItem){4, 0, 49};
    qsort(items, (size_t)count, sizeof(items[0]), DepthCompare);
    float snow = ViewSnow(ui);
    for (int i = 0; i < count; i++)
    {
        DrawItem item = items[i];
        if (item.kind == 0)
        {
            int x = item.id % MAP_SIZE, y = item.id / MAP_SIZE;
            Tile *t = &g->tiles[item.id];
            Vector2 foot = ViewIso(x + .5f, y + .5f);
            foot.y += 8;
            int sprite = t->node == NODE_TREE      ? 12
                         : t->node == NODE_STONE   ? 13
                         : t->node == NODE_BERRIES ? 14
                                                   : 15;
            float width = t->node == NODE_TREE ? 64 : t->node == NODE_GAME ? 42 : 50;
            Color tint = t->node == NODE_TREE ? ViewMix(WHITE, (Color){194, 218, 207, 255}, snow * .7f) : WHITE;
            DrawEllipse((int)foot.x, (int)foot.y, width * .28f, width * .10f, Fade(INK, .15f));
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
            PersonDraw(ui, item.id, true);
        else
        {
            Vector2 foot = ViewIso(40.5f, 8.5f);
            ViewSprite(ui->objects, 1, foot, 110, (Color){218, 177, 158, 255}, false);
            ViewSprite(ui->objects, 4, ViewIso(42, 9), 64, WHITE, false);
            ViewSprite(ui->objects, 0, ViewIso(40, 10), 50, WHITE, false);
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
        int index = ui->build == WATCH_POST ? 8 : ui->build == PALISADE ? 9 : ui->build == GATE ? 10 : ui->build;
        ViewSprite(ui->objects, index, ViewIso(x + d->w * .5f, y + d->h * .5f), ui->build == WORKSHOP ? 126 : 90,
               Fade(WHITE, .65f), false);
    }
    if (ui->panel == 1)
    {
        Vector2 p = ViewIso(g->rallyX + .5f, g->rallyY + .5f);
        DrawEllipseLines((int)p.x, (int)p.y, 19, 9, ACCENT);
        DrawLineEx(p, (Vector2){p.x, p.y - 27}, 2, ACCENT);
        DrawTriangle((Vector2){p.x, p.y - 27}, (Vector2){p.x, p.y - 17}, (Vector2){p.x + 14, p.y - 22},
                     ACCENT);
    }
    EndMode2D();
    EndScissorMode();
}
