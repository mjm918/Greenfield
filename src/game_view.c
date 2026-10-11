#include "game_view.h"
#include "raylib.h"
#include "raymath.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SCREEN_W 1440
#define SCREEN_H 900
#define RENDER_SCALE 2
#define PANEL_X 1130
#define TILE_W 64.0f
#define TILE_H 32.0f
static const Color INK = {37, 48, 42, 255}, PAPER = {240, 234, 213, 255}, MUTED = {161, 174, 151, 255},
                   ACCENT = {221, 180, 93, 255}, SAGE = {136, 177, 116, 255}, RUST = {192, 104, 74, 255};
typedef struct
{
    Texture2D texture;
    Rectangle rect[16];
    float baseline[16];
} Atlas;
typedef struct
{
    Atlas objects, people, braids;
    Font font, title;
    RenderTexture2D canvas;
    Camera2D camera;
    Vector2 mouse;
    float time;
    int panel, build, selected, selectedCitizen, areaRole, speed, lastSpeed;
    bool menu, help, confirmNew, winSeen, allowInput, grid, quit, reference, autosaveBlocked;
    char toast[256], savePath[4096];
    float toastTime;
    double accumulator;
    uint64_t lastAutosave;
    int hoverTile;
    GameState *g;
} UI;
static UI ui;
static const char *CITIZEN_NAMES[] = {"Asha", "Tova",  "Oren", "Nara", "Koa",  "Eira", "Bram", "Suri",
                                      "Luma", "Tarin", "Ari",  "Mira", "Ivo",  "Una",  "Ravi", "Nima",
                                      "Arlo", "Yara",  "Finn", "Sena", "Mako", "Eden", "Runa", "Tala"};
static float MinF(float a, float b) { return a < b ? a : b; }
static Color Mix(Color a, Color b, float f)
{
    return (Color){(unsigned char)(a.r + (b.r - a.r) * f), (unsigned char)(a.g + (b.g - a.g) * f),
                   (unsigned char)(a.b + (b.b - a.b) * f), 255};
}
static void Text(const char *text, float x, float y, float size, Color color)
{
    DrawTextEx(ui.font, text, (Vector2){roundf(x), roundf(y)}, size, 0.5f, color);
}
static void Title(const char *text, float x, float y, float size, Color color)
{
    DrawTextEx(ui.title, text, (Vector2){roundf(x), roundf(y)}, size, 0.5f, color);
}
static void Label(const char *text, float x, float y) { Text(text, x, y, 14, MUTED); }
static bool Hit(Rectangle r) { return CheckCollisionPointRec(ui.mouse, r); }
static bool Click(Rectangle r) { return ui.allowInput && Hit(r) && IsMouseButtonPressed(MOUSE_BUTTON_LEFT); }
static void Rounded(Rectangle r, Color color)
{
    DrawRectangle((int)r.x + 2, (int)r.y, (int)r.width - 4, (int)r.height, color);
    DrawRectangle((int)r.x, (int)r.y + 2, (int)r.width, (int)r.height - 4, color);
}
static void Border(Rectangle r, Color color)
{
    DrawLine((int)r.x + 2, (int)r.y, (int)(r.x + r.width) - 3, (int)r.y, color);
    DrawLine((int)r.x + 2, (int)(r.y + r.height) - 1, (int)(r.x + r.width) - 3, (int)(r.y + r.height) - 1,
             color);
    DrawLine((int)r.x, (int)r.y + 2, (int)r.x, (int)(r.y + r.height) - 3, color);
    DrawLine((int)(r.x + r.width) - 1, (int)r.y + 2, (int)(r.x + r.width) - 1, (int)(r.y + r.height) - 3,
             color);
}
static void WrapText(const char *text, float x, float y, float width, float size, Color color)
{
    char word[512], line[512] = {0};
    size_t pos = 0;
    float yy = y;
    while (text[pos])
    {
        int n = 0;
        while (text[pos] && text[pos] != ' ' && text[pos] != '\n' && n < 250)
            word[n++] = text[pos++];
        word[n] = 0;
        bool newline = text[pos] == '\n';
        if (text[pos])
            pos++;
        char candidate[512];
        snprintf(candidate, sizeof(candidate), "%s%s%s", line, line[0] ? " " : "", word);
        if (MeasureTextEx(ui.font, candidate, size, 0.5f).x > width && line[0])
        {
            Text(line, x, yy, size, color);
            yy += size + 5;
            snprintf(line, sizeof(line), "%s", word);
        }
        else
            snprintf(line, sizeof(line), "%s", candidate);
        if (newline)
        {
            Text(line, x, yy, size, color);
            yy += size + 5;
            line[0] = 0;
        }
    }
    if (line[0])
        Text(line, x, yy, size, color);
}
static bool Button(Rectangle r, const char *text, bool selected, bool enabled)
{
    bool hover = Hit(r) && ui.allowInput;
    Color base = selected ? (Color){92, 111, 72, 255} : (Color){53, 66, 54, 255};
    if (hover && enabled)
        base = Mix(base, ACCENT, 0.16f);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hover && enabled)
        base = Mix(base, BLACK, 0.15f);
    Rounded(r, base);
    Border(r, selected ? ACCENT : Fade(MUTED, enabled ? 0.4f : 0.15f));
    float width = MeasureTextEx(ui.font, text, 16, 0.5f).x;
    Text(text, r.x + (r.width - width) / 2, r.y + (r.height - 16) / 2 - 1, 16,
         enabled ? PAPER : Fade(MUTED, 0.6f));
    return enabled && Click(r);
}
static void Toast(const char *text)
{
    snprintf(ui.toast, sizeof(ui.toast), "%s", text);
    ui.toastTime = 5;
}
static bool Command(int type, int a, int b, int c)
{
    char reason[256];
    bool ok = GameCommandApply(ui.g, (GameCommand){type, a, b, c}, reason, sizeof(reason));
    if (!ok)
        Toast(reason);
    return ok;
}
static void AssetName(char *path, size_t n, const char *name)
{
    snprintf(path, n, "%sassets/%s", GetApplicationDirectory(), name);
}
static Atlas LoadAtlas(const char *name)
{
    Atlas a = {0};
    char path[4096];
    AssetName(path, sizeof(path), name);
    Image img = LoadImage(path);
    if (!img.data)
        return a;
    Color *pixels = LoadImageColors(img);
    int cw = img.width / 4, ch = img.height / 4;
    for (int i = 0; i < 16; i++)
    {
        int sx = i % 4 * cw, sy = i / 4 * ch, minx = cw, miny = ch, maxx = 0, maxy = 0;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                if (pixels[(sy + y) * img.width + sx + x].a > 100)
                {
                    if (x < minx)
                        minx = x;
                    if (x > maxx)
                        maxx = x;
                    if (y < miny)
                        miny = y;
                    if (y > maxy)
                        maxy = y;
                }
        a.rect[i] = (Rectangle){sx + minx, sy + miny, maxx - minx + 1, maxy - miny + 1};
        a.baseline[i] = maxy;
    }
    UnloadImageColors(pixels);
    a.texture = LoadTextureFromImage(img);
    GenTextureMipmaps(&a.texture);
    SetTextureFilter(a.texture, TEXTURE_FILTER_TRILINEAR);
    UnloadImage(img);
    return a;
}
static void Sprite(Atlas a, int index, Vector2 foot, float width, Color tint, bool flip)
{
    Rectangle src = a.rect[index];
    float h = width * src.height / src.width;
    if (flip)
        src.width = -src.width;
    DrawTexturePro(a.texture, src, (Rectangle){foot.x - width / 2, foot.y - h, width, h}, (Vector2){0, 0}, 0,
                   tint);
}
static Vector2 Iso(float x, float y) { return (Vector2){(x - y) * TILE_W / 2, (x + y) * TILE_H / 2}; }
static Vector2 TileAt(Vector2 screen)
{
    Vector2 p = GetScreenToWorld2D(screen, ui.camera);
    return (Vector2){p.x / TILE_W + p.y / TILE_H, p.y / TILE_H - p.x / TILE_W};
}
static void Diamond(float x, float y, float w, float h, Color color)
{
    Vector2 top = Iso(x, y), right = Iso(x + w, y), bottom = Iso(x + w, y + h), left = Iso(x, y + h);
    DrawTriangle(top, left, bottom, color);
    DrawTriangle(top, bottom, right, color);
}
static void DiamondLine(float x, float y, float w, float h, Color color, float thick)
{
    Vector2 p[] = {Iso(x, y), Iso(x + w, y), Iso(x + w, y + h), Iso(x, y + h)};
    for (int i = 0; i < 4; i++)
        DrawLineEx(p[i], p[(i + 1) % 4], thick, color);
}
static float Snow(void) { return Clamp(1 - (float)((int64_t)ui.g->tick - (WINTER_END - 1200)) / 2400, 0, 1); }
static unsigned TileNoise(int x, int y)
{
    unsigned n = (unsigned)(x * 3719 + y * 16699 + 19);
    n ^= n << 13;
    n ^= n >> 17;
    return n;
}
static void TerrainDraw(void)
{
    GameState *g = ui.g;
    float snow = Snow();
    // Earth rim gives the board a coherent footprint without a background painting.
    Vector2 a = Iso(0, 48), b = Iso(48, 48), c = Iso(48, 0);
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
            Color grass = Mix((Color){123, 146, 83, 255}, (Color){162, 177, 104, 255}, vary * 4);
            if (t->terrain == FERTILE)
                grass = (Color){132 + (n % 8), 139 + (n % 8), 78, 255};
            if (t->terrain == ROCK_GROUND)
                grass = (Color){142, 146, 129, 255};
            Color ground =
                Mix(grass, (Color){211 + (n % 8), 222 + (n % 8), 210 + (n % 8), 255}, snow * 0.94f);
            if (t->terrain == WATER)
            {
                ground = Mix((Color){69, 137, 150, 255}, (Color){136, 180, 183, 255}, snow * 0.55f);
                Diamond(x, y, 1, 1, ground);
                Vector2 p = Iso(x + 0.4f, y + 0.6f);
                if (n % 3 == 0)
                    DrawLineEx((Vector2){p.x - 8, p.y}, (Vector2){p.x + 8 + sin(ui.time * 1.5f + n) * 4, p.y},
                               1, Fade((Color){210, 239, 224, 255}, 0.5f));
            }
            else
            {
                Diamond(x, y, 1, 1, ground);
                Vector2 p = Iso(x + 0.35f, y + 0.45f);
                if (n % 5 == 0)
                    DrawLineEx(p, (Vector2){p.x + 7, p.y + 2}, 1,
                               Fade(snow > .6f ? WHITE : (Color){185, 190, 122, 255}, 0.45f));
                if (t->terrain == SHALLOWS)
                {
                    Diamond(x, y, 1, 1, Mix((Color){159, 169, 138, 255}, ground, 0.4f));
                    DrawEllipse((int)p.x, (int)p.y, 9, 4, (Color){177, 177, 153, 255});
                }
            }
            if (ui.grid)
                DiamondLine(x, y, 1, 1, Fade(INK, 0.10f), 0.7f);
        }
    // Narrow footpaths lead to the ford and across the founding camp.
    for (int y = 19; y <= 26; y++)
        for (int x = 19; x <= 33; x++)
            if ((y == 24 || x == 22) && g->tiles[GameTile(x, y)].terrain != WATER &&
                g->tiles[GameTile(x, y)].occupied < 0)
                Diamond(x, y, 1, 1,
                        Mix((Color){179, 158, 111, 255}, (Color){203, 207, 187, 255}, snow * 0.6f));
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *building = &g->buildings[i];
        const BuildingDef *d = &BUILDINGS[building->type];
        if (building->type == FARM)
        {
            Diamond(building->x, building->y, 2, 2, (Color){104, 90, 63, 255});
            for (int row = 0; row < 6; row++)
            {
                Vector2 start = Iso(building->x + 0.2f, building->y + row * 0.3f + 0.2f),
                        end = Iso(building->x + 1.8f, building->y + row * 0.3f + 0.2f);
                DrawLineEx(start, end, 3, (Color){76, 66, 45, 255});
            }
        }
        else
            Diamond(building->x, building->y, d->w, d->h,
                    Mix((Color){163, 143, 101, 255}, (Color){199, 203, 184, 255}, snow * 0.45f));
    }
}
typedef struct
{
    int kind, id;
    float depth;
} DrawItem;
static int DepthCompare(const void *a, const void *b)
{
    const DrawItem *x = a, *y = b;
    return x->depth < y->depth ? -1 : x->depth > y->depth ? 1 : x->kind - y->kind;
}
static void Bar(float x, float y, float width, float value, Color color)
{
    Rounded((Rectangle){x, y, width, 5}, Fade(INK, 0.7f));
    Rounded((Rectangle){x, y, width * Clamp(value, 0, 1), 5}, color);
}
static Vector2 BuildingVisual(const Building *b, int *index, float *width)
{
    const BuildingDef *d = &BUILDINGS[b->type];
    Vector2 foot = Iso(b->x + d->w * .5f, b->y + d->h * .5f);
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
static void BuildingDraw(int id)
{
    GameState *g = ui.g;
    Building *b = &g->buildings[id];
    const BuildingDef *d = &BUILDINGS[b->type];
    int index;
    float width;
    Vector2 foot = BuildingVisual(b, &index, &width);
    if (id == ui.selected)
        DiamondLine(b->x, b->y, d->w, d->h, ACCENT, 2.5f);
    if (!b->complete)
    {
        DiamondLine(b->x, b->y, d->w, d->h, Fade(ACCENT, 0.8f), 2);
        Vector2 start = Iso(b->x + 0.2f, b->y + 0.2f), end = Iso(b->x + d->w - 0.2f, b->y + d->h - 0.2f);
        DrawLineEx(start, (Vector2){start.x, start.y - 30}, 4, (Color){115, 80, 45, 255});
        DrawLineEx(end, (Vector2){end.x, end.y - 30}, 4, (Color){115, 80, 45, 255});
        DrawLineEx((Vector2){start.x, start.y - 30}, (Vector2){end.x, end.y - 30}, 3,
                   (Color){145, 110, 61, 255});
        Sprite(ui.objects, 2, foot, 48, Fade(WHITE, 0.9f), false);
        Bar(foot.x - 28, foot.y + 10, 56, (float)b->progress / d->labor, ACCENT);
        return;
    }
    if (b->type == FARM)
    {
        if (b->crop)
        {
            for (int yy = 0; yy < 2; yy++)
                for (int xx = 0; xx < 2; xx++)
                {
                    Vector2 p = Iso(b->x + xx + .55f, b->y + yy + .55f);
                    p.y += 6;
                    float growth = b->crop == 2 ? 1 : .25f + .7f * b->cropProgress / 2400.0f;
                    Sprite(ui.objects, 7, p, 38 * growth, b->crop == 2 ? WHITE : (Color){163, 200, 114, 255},
                           false);
                }
        }
        return;
    }
    DrawEllipse((int)foot.x, (int)(foot.y - 3), width * .35f, width * .12f,
                Fade((Color){28, 45, 35, 255}, .15f));
    Sprite(ui.objects, index, foot, width, WHITE, false);
    if (b->type == HEARTH && g->warmth > 20)
    {
        for (int i = 0; i < 4; i++)
        {
            float age = fmodf(ui.time * .25f + i * .24f, 1);
            DrawCircleV((Vector2){foot.x + sinf(age * 4 + i) * 6, foot.y - 40 - age * 55}, 4 + age * 7,
                        Fade((Color){227, 222, 195, 255}, (1 - age) * .35f));
        }
        DrawCircleGradient((Vector2){foot.x, foot.y - 15}, 32, Fade(ACCENT, .12f), BLANK);
    }
}
static void PersonDraw(int id, bool raider)
{
    GameState *g = ui.g;
    int x, y, job, hp, frame;
    bool attacking = false, working = false;
    Atlas person = !raider && id % 2 == 0 ? ui.braids : ui.people;
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
    Vector2 foot = Iso(x / 256.0f + .5f, y / 256.0f + .5f);
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
    if (!raider && id == ui.selectedCitizen)
        DrawEllipseLines((int)foot.x, (int)foot.y, 13, 5, ACCENT);
    DrawTexturePro(person.texture, src, (Rectangle){foot.x - width / 2, foot.y - h, width, h},
                   (Vector2){0, 0}, 0, raider ? (Color){245, 158, 137, 255} : WHITE);
    if (hp < 900 || raider)
        Bar(foot.x - 13, foot.y - h - 7, 26, (float)hp / (raider ? 650 : 1000), raider ? RUST : SAGE);
    if (!raider && job == JOB_IDLE && g->citizens[id].role == IDLE)
    {
        DrawCircle((int)foot.x, (int)(foot.y - h - 5), 2, ACCENT);
    }
}
static void WorldDraw(void)
{
    // Keep the input camera in logical coordinates; rasterize at Retina resolution.
    Camera2D renderCamera = ui.camera;
    renderCamera.offset = Vector2Scale(renderCamera.offset, RENDER_SCALE);
    renderCamera.zoom *= RENDER_SCALE;
    BeginScissorMode(0, 86 * RENDER_SCALE, PANEL_X * RENDER_SCALE,
                     (SCREEN_H - 250) * RENDER_SCALE);
    BeginMode2D(renderCamera);
    TerrainDraw();
    GameState *g = ui.g;
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
    float snow = Snow();
    for (int i = 0; i < count; i++)
    {
        DrawItem item = items[i];
        if (item.kind == 0)
        {
            int x = item.id % MAP_SIZE, y = item.id / MAP_SIZE;
            Tile *t = &g->tiles[item.id];
            Vector2 foot = Iso(x + .5f, y + .5f);
            foot.y += 8;
            int sprite = t->node == NODE_TREE      ? 12
                         : t->node == NODE_STONE   ? 13
                         : t->node == NODE_BERRIES ? 14
                                                   : 15;
            float width = t->node == NODE_TREE ? 64 : t->node == NODE_GAME ? 42 : 50;
            Color tint = t->node == NODE_TREE ? Mix(WHITE, (Color){194, 218, 207, 255}, snow * .7f) : WHITE;
            DrawEllipse((int)foot.x, (int)foot.y, width * .28f, width * .10f, Fade(INK, .15f));
            Sprite(ui.objects, sprite, foot, width, tint, false);
            if (snow > .1f && t->node == NODE_STONE)
                DrawEllipse((int)(foot.x - 4), (int)(foot.y - 18), 13, 4,
                            Fade((Color){236, 240, 223, 255}, snow * .9f));
        }
        else if (item.kind == 1)
            BuildingDraw(item.id);
        else if (item.kind == 2)
            PersonDraw(item.id, false);
        else if (item.kind == 3)
            PersonDraw(item.id, true);
        else
        {
            Vector2 foot = Iso(40.5f, 8.5f);
            Sprite(ui.objects, 1, foot, 110, (Color){218, 177, 158, 255}, false);
            Sprite(ui.objects, 4, Iso(42, 9), 64, WHITE, false);
            Sprite(ui.objects, 0, Iso(40, 10), 50, WHITE, false);
        }
    }
    if (ui.areaRole >= 0 && ui.hoverTile >= 0)
    {
        int x = ui.hoverTile % MAP_SIZE, y = ui.hoverTile / MAP_SIZE;
        for (int yy = y - 9; yy <= y + 9; yy++)
            for (int xx = x - 9; xx <= x + 9; xx++)
                if (GameTile(xx, yy) >= 0 && abs(xx - x) + abs(yy - y) <= 9)
                    DiamondLine(xx, yy, 1, 1, Fade(ACCENT, .45f), 1);
    }
    if (ui.build >= 0 && ui.hoverTile >= 0)
    {
        int x = ui.hoverTile % MAP_SIZE, y = ui.hoverTile / MAP_SIZE;
        char why[256];
        bool valid = GameCanPlace(g, ui.build, x, y, why, sizeof(why));
        const BuildingDef *d = &BUILDINGS[ui.build];
        Diamond(x, y, d->w, d->h, Fade(valid ? SAGE : RUST, .42f));
        DiamondLine(x, y, d->w, d->h, valid ? SAGE : RUST, 2.5f);
        int index = ui.build == WATCH_POST ? 8 : ui.build == PALISADE ? 9 : ui.build == GATE ? 10 : ui.build;
        Sprite(ui.objects, index, Iso(x + d->w * .5f, y + d->h * .5f), ui.build == WORKSHOP ? 126 : 90,
               Fade(WHITE, .65f), false);
    }
    if (ui.panel == 1)
    {
        Vector2 p = Iso(g->rallyX + .5f, g->rallyY + .5f);
        DrawEllipseLines((int)p.x, (int)p.y, 19, 9, ACCENT);
        DrawLineEx(p, (Vector2){p.x, p.y - 27}, 2, ACCENT);
        DrawTriangle((Vector2){p.x, p.y - 27}, (Vector2){p.x, p.y - 17}, (Vector2){p.x + 14, p.y - 22},
                     ACCENT);
    }
    EndMode2D();
    EndScissorMode();
}
static void Cost(const int *cost, float x, float y, float width)
{
    char text[200] = {0};
    size_t used = 0;
    for (int r = 0; r < RESOURCE_COUNT; r++)
        if (cost[r])
            used += (size_t)snprintf(text + used, sizeof(text) - used, "%s%d %s", used ? "  " : "", cost[r],
                                     RESOURCE_NAMES[r]);
    WrapText(text, x, y, width, 14, ACCENT);
}
static void Header(void)
{
    GameState *g = ui.g;
    DrawRectangle(0, 0, SCREEN_W, 86, (Color){31, 43, 35, 255});
    DrawLine(0, 85, SCREEN_W, 85, (Color){78, 89, 66, 255});
    Title("GREENFIELD", 24, 12, 30, PAPER);
    Label("FOUNDING A TRIBE", 27, 48);
    const int icons[] = {14, 2, 13, 4, 5};
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        float x = 234 + r * 133;
        Sprite(ui.objects, icons[r], (Vector2){x + 18, 57}, 30, WHITE, false);
        Text(TextFormat("%d", g->stock[r]), x + 40, 21, 22, PAPER);
        Label(RESOURCE_NAMES[r], x + 41, 49);
        if (r == FOOD && g->foodWarning)
            DrawCircle((int)(x + 105), 28, 3, RUST);
    }
    DrawLine(918, 18, 918, 65, Fade(MUTED, .25f));
    Text(TextFormat("%d / %d", GamePopulation(g), GameHousing(g)), 943, 20, 22, PAPER);
    Label("PEOPLE / BEDS", 944, 49);
    Text(TextFormat("Day %llu", (unsigned long long)(g->tick / DAY_TICKS + 1)), 1088, 18, 20, PAPER);
    Text(g->tick < WINTER_END ? "Deep winter"
         : Snow() > .1f       ? "The thaw"
                              : "Growing season",
         1088, 45, 14, g->tick < WINTER_END ? (Color){169, 204, 210, 255} : SAGE);
    if (Button((Rectangle){1307, 23, 109, 38}, "Menu  Esc", false, true))
        ui.menu = true;
}
static void PanelBase(const char *eyebrow, const char *heading)
{
    DrawRectangle(PANEL_X, 86, SCREEN_W - PANEL_X, SCREEN_H - 86, (Color){35, 48, 39, 255});
    DrawLine(PANEL_X, 86, PANEL_X, SCREEN_H, (Color){95, 105, 78, 255});
    Label(eyebrow, PANEL_X + 23, 111);
    Title(heading, PANEL_X + 23, 134, 28, PAPER);
}
static void Objectives(void)
{
    GameState *g = ui.g;
    PanelBase("THE FIRST CHAPTER", "A place to belong");
    WrapText("From a circle around a fire to a village that can endure.", 1153, 178, 260, 16, MUTED);
    const char *names[] = {"Survive the opening winter", "Welcome 20 people",
                           "Give everyone a home",       "Bring in the first harvest",
                           "Store three days of food",   "Defend a raid or make a truce"};
    bool done[] = {g->winterSurvived,   GamePopulation(g) >= 20,     GameHousing(g) >= GamePopulation(g),
                   g->harvestDelivered, GameFoodDaysTenths(g) >= 30, g->raidResolved};
    for (int i = 0; i < 6; i++)
    {
        float y = 241 + i * 51;
        DrawCircleLines(1161, (int)y + 7, 7, done[i] ? SAGE : Fade(MUTED, .6f));
        if (done[i])
            DrawCircle(1161, (int)y + 7, 3, SAGE);
        Text(names[i], 1179, y - 1, 16, done[i] ? SAGE : PAPER);
        if (i == 0 && !done[i])
            Label(TextFormat("%d:%02d until thaw", (WINTER_END - (int)g->tick) / 600,
                             ((WINTER_END - (int)g->tick) / 10) % 60),
                  1179, y + 19);
        if (i == 1)
            Label(TextFormat("%d of 20 citizens", GamePopulation(g)), 1179, y + 19);
        if (i == 4)
            Label(TextFormat("%.1f days stored", GameFoodDaysTenths(g) / 10.0), 1179, y + 19);
    }
    DrawLine(1153, 560, 1416, 560, Fade(MUTED, .25f));
    Label("NEXT ARRIVALS", 1153, 581);
    Text(TextFormat("In %d:%02d", (g->nextArrival - (int)g->tick) / 600,
                    ((g->nextArrival - (int)g->tick) / 10) % 60),
         1153, 602, 19, PAPER);
    WrapText(
        TextFormat("Need 2 spare beds and %d food. Two travelers join at a time.", GamePopulation(g) * 3),
        1153, 633, 260, 16, MUTED);
    Label("TRIBE CHRONICLE", 1153, 700);
    for (int i = 0; i < 3 && i < g->eventCount; i++)
    {
        WrapText(g->events[i], 1153, 723 + i * 49, 260, 14, i ? MUTED : PAPER);
    }
}
static void Workforce(void)
{
    GameState *g = ui.g;
    PanelBase("EVERY PAIR OF HANDS", "Workforce");
    Text(TextFormat("%d %s available for work", GameRoleCount(g, IDLE),
                    GameRoleCount(g, IDLE) == 1 ? "person" : "people"),
         1153, 176, 15, ACCENT);
    for (int r = 0; r < ROLE_COUNT; r++)
    {
        float y = 216 + r * 63;
        Text(ROLE_NAMES[r], 1153, y, 16, PAPER);
        Label(g->areaX[r] < 0 ? "Whole valley" : "Assigned work area", 1153, y + 22);
        if (Button((Rectangle){1300, y - 4, 26, 27}, "-", false, GameRoleCount(g, r) > 0))
            Command(CMD_ASSIGN, r, GameRoleCount(g, r) - 1, 0);
        Text(TextFormat("%d", GameRoleCount(g, r)), 1335, y, 17, ACCENT);
        if (Button((Rectangle){1360, y - 4, 26, 27}, "+", false, GameRoleCount(g, IDLE) > 0))
            Command(CMD_ASSIGN, r, GameRoleCount(g, r) + 1, 0);
        if (Button((Rectangle){1298, y + 28, 57, 22}, "Area", ui.areaRole == r, true))
            ui.areaRole = ui.areaRole == r ? -1 : r;
        const char *p[] = {"Low", "Norm", "High"};
        if (Button((Rectangle){1362, y + 28, 54, 22}, p[g->priority[r]], false, true))
            Command(CMD_PRIORITY, r, (g->priority[r] + 1) % 3, 0);
    }
    WrapText("High-priority roles take shared work first. Area limits work to nine tiles around your choice.",
             1153, 748, 260, 16, MUTED);
    if (Button((Rectangle){1153, 815, 126, 33}, "Rally guards", ui.areaRole == GUARD, true))
        ui.areaRole = GUARD;
    if (Button((Rectangle){1288, 815, 126, 33}, "Clear areas", false, true))
        for (int r = 0; r < ROLE_COUNT; r++)
            Command(CMD_AREA, r, -1, -1);
}
static void Discoveries(void)
{
    GameState *g = ui.g;
    PanelBase("KNOWLEDGE TAKES ROOT", "Discoveries");
    WrapText("A completed workshop and a crafter turn materials into new possibilities.", 1153, 180, 260, 16,
             MUTED);
    for (int d = 0; d < DISCOVERY_COUNT; d++)
    {
        float y = 245 + d * 144;
        Rounded((Rectangle){1153, y, 263, 132}, (Color){45, 59, 46, 255});
        Text(DISCOVERIES[d].name, 1166, y + 12, 17, PAPER);
        WrapText(DISCOVERIES[d].description, 1166, y + 39, 236, 14, MUTED);
        Cost(DISCOVERIES[d].cost, 1166, y + 77, 236);
        if (g->discovery[d])
            Text("Discovered", 1166, y + 105, 16, SAGE);
        else if (g->research == d)
        {
            Bar(1166, y + 106, 232, (float)g->researchProgress / DISCOVERIES[d].labor, ACCENT);
            Text(TextFormat("%d%%", g->researchProgress * 100 / DISCOVERIES[d].labor), 1350, y + 13, 16,
                 ACCENT);
        }
        else if (Button((Rectangle){1300, y + 100, 101, 24}, "Discover", false, g->research < 0))
            Command(CMD_RESEARCH, d, 0, 0);
    }
}
static void RivalPanel(void)
{
    GameState *g = ui.g;
    PanelBase("ACROSS THE RIVER", "The Ashen camp");
    WrapText("A neighboring tribe watches this valley. They raid supplies when the truce runs out.", 1153,
             181, 260, 16, MUTED);
    Sprite(ui.objects, 1, (Vector2){1285, 361}, 145, (Color){226, 201, 174, 255}, false);
    DrawLine(1153, 392, 1416, 392, Fade(MUTED, .3f));
    const char *state = g->raidActive                  ? "Raiders in the valley"
                        : g->truceUntil > (int)g->tick ? "A truce holds"
                        : g->raidWarning               ? "A raid is approaching"
                                                       : "Scouts are watching";
    Text(state, 1153, 417, 18, g->raidWarning ? RUST : ACCENT);
    int seconds = (g->truceUntil > (int)g->tick ? g->truceUntil : g->nextRaid) - (int)g->tick;
    if (seconds < 0)
        seconds = 0;
    Text(TextFormat("%s %d:%02d", g->truceUntil > (int)g->tick ? "Truce remaining" : "Next known raid",
                    seconds / 600, seconds / 10 % 60),
         1153, 451, 16, PAPER);
    if (g->truceUntil > (int)g->tick)
    {
        int next = g->truceUntil + 1200 - (int)g->tick;
        if (next < 0)
            next = 0;
        Text(TextFormat("Next known raid %d:%02d", next / 600, next / 10 % 60), 1153, 476, 14, MUTED);
    }
    WrapText("Offer 40 food for twelve minutes of peace. An accepted truce also completes the diplomacy "
             "objective.",
             1153, 500, 260, 16, MUTED);
    if (Button((Rectangle){1153, 581, 263, 43}, "Offer truce  /  40 food", false,
               g->truceUntil <= (int)g->tick))
        Command(CMD_TRUCE, 0, 0, 0);
    if (Button((Rectangle){1153, 638, 263, 40}, "Assign guards", false, true))
        ui.panel = 1;
    Text(TextFormat("Food taken so far: %d", g->stolen[FOOD]), 1153, 708, 16, PAPER);
    WrapText("Guards defend near the rally flag. Injured guards withdraw. Palisades block passage; gates "
             "keep your routes open.",
             1153, 746, 260, 16, MUTED);
}
static void Selected(void)
{
    GameState *g = ui.g;
    PanelBase("IN YOUR VILLAGE", ui.selectedCitizen >= 0 ? "A member of the tribe"
                                 : ui.selected >= 0      ? "A place with purpose"
                                                         : "The valley");
    if (ui.selectedCitizen >= 0)
    {
        Citizen *c = &g->citizens[ui.selectedCitizen];
        Title(CITIZEN_NAMES[ui.selectedCitizen % 24], 1153, 195, 26, ACCENT);
        Sprite(ui.selectedCitizen % 2 == 0 ? ui.braids : ui.people,
               c->job == JOB_DELIVER ? 8
               : c->job == JOB_FIGHT ? 12
                                     : 0,
               (Vector2){1280, 375}, 88, WHITE, false);
        Text(c->role == IDLE ? "Unassigned" : ROLE_NAMES[c->role], 1153, 415, 18, PAPER);
        Text(JOB_NAMES[c->job], 1153, 449, 15, MUTED);
        Text(TextFormat("Health  %d%%", c->hp / 10), 1153, 499, 16, PAPER);
        Bar(1153, 528, 262, c->hp / 1000.0f, SAGE);
        Label("CARRYING", 1153, 563);
        int y = 588;
        bool any = false;
        for (int r = 0; r < RESOURCE_COUNT; r++)
            if (c->carry[r])
            {
                Text(TextFormat("%d %s", c->carry[r], RESOURCE_NAMES[r]), 1153, (float)y, 15, ACCENT);
                y += 24;
                any = true;
            }
        if (!any)
            Text("Nothing yet", 1153, 588, 16, MUTED);
        WrapText(c->hunger > 900  ? "Hungry. Restore stored food soon to avoid health loss."
                 : c->cold > 1200 ? "Cold. Keep the hearth fueled and build enough shelter."
                                  : "Fed and sheltered citizens slowly regain health.",
                 1153, 668, 260, 16, MUTED);
        if (Button((Rectangle){1153, 774, 262, 40}, "Manage workforce", false, true))
            ui.panel = 1;
        return;
    }
    if (ui.selected >= 0)
    {
        Building *b = &g->buildings[ui.selected];
        const BuildingDef *d = &BUILDINGS[b->type];
        Title(b->upgraded ? "Insulated hut" : d->name, 1153, 197, 25, ACCENT);
        int sprite = b->upgraded ? 11 : b->type;
        Sprite(ui.objects, sprite, (Vector2){1280, 380}, 155, WHITE, false);
        WrapText(d->description, 1153, 418, 260, 15, PAPER);
        Text(b->complete ? "Ready" : TextFormat("Construction  %d%%", b->progress * 100 / d->labor), 1153,
             498, 17, b->complete ? SAGE : ACCENT);
        if (!b->complete)
            Bar(1153, 532, 263, (float)b->progress / d->labor, ACCENT);
        if (b->type == SHELTER)
        {
            Text("4 beds", 1153, 554, 17, PAPER);
            if (!b->upgraded &&
                Button((Rectangle){1153, 600, 263, 43}, "Insulate  /  10 wood, 3 hides", false, b->complete))
                Command(CMD_UPGRADE, ui.selected, 0, 0);
        }
        if (b->type == FARM)
        {
            Text(b->crop == 2   ? "Ready to harvest"
                 : b->crop == 1 ? "Grain is growing"
                                : "Waiting for a farmer",
                 1153, 554, 17, PAPER);
            if (b->crop == 1)
                Bar(1153, 588, 263, b->cropProgress / 2400.0f, SAGE);
        }
        if (b->type == WORKSHOP && Button((Rectangle){1153, 610, 263, 43}, "Open discoveries", false, true))
            ui.panel = 2;
        WrapText("Select another building or citizen directly in the valley. Construction materials are "
                 "charged once when placed.",
                 1153, 715, 260, 16, MUTED);
        return;
    }
    WrapText("A river, a sheltered clearing, and enough land to begin again.", 1153, 190, 260, 15, PAPER);
    Label("GETTING STARTED", 1153, 266);
    WrapText("1. Place two shelters near the hearth.\n2. Keep foragers and woodcutters working.\n3. Build a "
             "workshop and assign a crafter.\n4. Discover farming after the thaw.",
             1153, 297, 260, 15, MUTED);
    if (Button((Rectangle){1153, 521, 263, 42}, "Follow your objectives", false, true))
        ui.panel = 0;
}
static void BuildPalette(void)
{
    DrawRectangle(0, SCREEN_H - 164, PANEL_X, 164, (Color){31, 43, 35, 255});
    DrawLine(0, SCREEN_H - 164, PANEL_X, SCREEN_H - 164, (Color){92, 102, 74, 255});
    Label("BUILD A HOME, ONE PLACE AT A TIME", 24, SCREEN_H - 146);
    const char *shortNames[] = {"Hearth",  "Shelter", "Stockpile",  "Forage post", "Hunting", "Workshop",
                                "Granary", "Farm",    "Watch post", "Palisade",    "Gate"};
    for (int i = 0; i < BUILDING_COUNT; i++)
    {
        Rectangle r = {22 + i * 99, SCREEN_H - 119, 91, 85};
        bool selected = ui.build == i, hover = Hit(r);
        Rounded(r, selected ? (Color){82, 99, 65, 255}
                   : hover  ? (Color){59, 72, 53, 255}
                            : (Color){43, 56, 43, 255});
        Border(r, selected ? ACCENT : Fade(MUTED, .22f));
        Sprite(ui.objects, i, (Vector2){r.x + 45, r.y + 53}, i == WATCH_POST ? 35 : 51, WHITE, false);
        float width = MeasureTextEx(ui.font, shortNames[i], 14, 0.5f).x;
        Text(shortNames[i], r.x + (91 - width) / 2, r.y + 62, 14, PAPER);
        bool locked =
            (i == FARM && !ui.g->discovery[FARMING]) || (i == GRANARY && !ui.g->discovery[FOOD_STORAGE]);
        if (locked)
            Text("*", r.x + 77, r.y + 7, 16, ACCENT);
        if (Click(r))
        {
            ui.build = selected ? -1 : i;
            ui.areaRole = -1;
        }
        if (hover && ui.allowInput && ui.build < 0)
        {
            Rounded((Rectangle){22, SCREEN_H - 237, 670, 63}, INK);
            Text(BUILDINGS[i].description, 38, SCREEN_H - 225, 16, PAPER);
            Cost(BUILDINGS[i].cost, 38, SCREEN_H - 201, 630);
        }
    }
    Text("Click to build  /  Esc cancels     |     Wheel zoom  /  Right drag pan     |     H for field guide",
         24, SCREEN_H - 21, 14, MUTED);
}
static void WorldHUD(void)
{
    GameState *g = ui.g;
    const char *tabs[] = {"Objectives  O", "Workforce  W", "Discoveries  R", "Rivals", "Inspect"};
    for (int i = 0; i < 5; i++)
        if (Button((Rectangle){22 + i * 135, 103, 126, 33}, tabs[i], ui.panel == i, true))
            ui.panel = i;
    Rounded((Rectangle){22, 151, 238, 67}, Fade(INK, .89f));
    Label(g->tick < WINTER_END    ? "01 / THE LONG WINTER"
          : g->discovery[FARMING] ? "03 / ROOTS IN THE EARTH"
                                  : "02 / A PERMANENT CAMP",
          37, 163);
    Title(g->tick < WINTER_END    ? "Keep the fire alive"
          : g->discovery[FARMING] ? "Let this be home"
                                  : "A valley waking up",
          37, 185, 20, PAPER);
    Rounded((Rectangle){812, 103, 296, 33}, Fade(INK, .90f));
    if (Button((Rectangle){816, 106, 56, 27}, ui.speed == 0 ? "Play" : "Pause", ui.speed == 0, true))
    {
        if (ui.speed)
        {
            ui.lastSpeed = ui.speed;
            ui.speed = 0;
        }
        else
            ui.speed = ui.lastSpeed;
    }
    int speeds[] = {1, 2, 4};
    for (int i = 0; i < 3; i++)
        if (Button((Rectangle){878 + i * 48, 106, 43, 27}, TextFormat("%dx", speeds[i]),
                   ui.speed == speeds[i], true))
            ui.speed = speeds[i];
    Text(
        TextFormat("%02llu:%02llu", (unsigned long long)g->tick / 600, (unsigned long long)g->tick / 10 % 60),
        1030, 115, 14, PAPER);
    Rounded((Rectangle){903, 158, 204, 52}, Fade(INK, .85f));
    Label("CAMP WARMTH", 916, 169);
    Text(TextFormat("%d%%", g->warmth), 1050, 169, 16, g->warmth < 80 ? ACCENT : SAGE);
    Bar(916, 191, 176, g->warmth / 100.0f, g->warmth < 50 ? RUST : SAGE);
    int warningY = 232;
    if (g->foodWarning || g->coldWarning || g->raidWarning || g->raidActive)
    {
        const char *warning =
            g->raidActive    ? "Raid underway - rally your guards"
            : g->raidWarning ? "Raid approaching - prepare or offer a truce"
            : g->foodWarning
                ? TextFormat("Food: %.1f minutes left - assign foragers or hunters",
                             (float)g->stock[FOOD] / (float)(GamePopulation(g) ? GamePopulation(g) : 1))
                : "Cold causes harm after 3 min - build beds and gather wood";
        Rectangle r = {22, (float)warningY, 536, 39};
        Rounded(r, (Color){102, 62, 42, 242});
        Text("!", 36, warningY + 10, 18, ACCENT);
        Text(warning, 55, warningY + 12, 16, PAPER);
        if (Click(r))
        {
            ui.panel = g->raidActive || g->raidWarning ? 3 : 1;
        }
        warningY += 46;
    }
    // Every tile is known from the start. This overview also recenters the camera.
    Rectangle map = {928, 586, 179, 127};
    Rounded(map, Fade(INK, .90f));
    Label("THE VALLEY", 941, 597);
    for (int y = 0; y < 48; y++)
        for (int x = 0; x < 48; x++)
        {
            Tile *t = &g->tiles[GameTile(x, y)];
            Color c = t->terrain == WATER     ? (Color){92, 160, 168, 255}
                      : t->node == NODE_TREE  ? (Color){86, 116, 74, 255}
                      : t->terrain == FERTILE ? (Color){169, 167, 94, 255}
                                              : (Color){150, 167, 129, 255};
            if (t->occupied >= 0)
                c = ACCENT;
            DrawRectangle(1017 + (x - y) * 1.6f, 620 + (x + y) * .8f, 2, 2, c);
        }
    Vector2 center = {ui.camera.target.x / TILE_W + ui.camera.target.y / TILE_H,
                      ui.camera.target.y / TILE_H - ui.camera.target.x / TILE_W};
    DrawCircleLines((int)(1017 + (center.x - center.y) * 1.6f), (int)(620 + (center.x + center.y) * .8f), 8,
                    PAPER);
    if (Click(map))
    {
        float mx = (ui.mouse.x - 1017) / 1.6f, my = (ui.mouse.y - 620) / .8f;
        ui.camera.target = Iso(Clamp((mx + my) / 2, 0, 47), Clamp((my - mx) / 2, 0, 47));
    }
    if (ui.build >= 0)
    {
        Rectangle card = {22, 632, 790, 88};
        Rounded(card, INK);
        const BuildingDef *d = &BUILDINGS[ui.build];
        Text(d->name, 38, 643, 19, ACCENT);
        Cost(d->cost, 210, 647, 580);
        char reason[256];
        if (ui.hoverTile >= 0)
            GameCanPlace(g, ui.build, ui.hoverTile % MAP_SIZE, ui.hoverTile / MAP_SIZE, reason,
                         sizeof(reason));
        else
            snprintf(reason, sizeof(reason), "Choose a clear place in the valley.");
        WrapText(reason, 38, 677, 755, 16, PAPER);
    }
    else if (ui.areaRole >= 0)
    {
        Rounded((Rectangle){22, 650, 810, 66}, INK);
        Text(ui.areaRole == GUARD ? "Choose a guard rally point"
                                  : "Choose the center of this role's work area",
             38, 663, 17, ACCENT);
        Text("Click a tile in the valley. Esc cancels. Clear areas restores access to the whole map.", 38,
             690, 16, PAPER);
    }
    else if (ui.toastTime > 0)
    {
        Rounded((Rectangle){22, 647, 810, 68}, INK);
        WrapText(ui.toast, 38, 663, 777, 16, PAPER);
    }
    else if (ui.autosaveBlocked)
    {
        Rounded((Rectangle){22, 647, 810, 68}, INK);
        Text("Autosave paused after a load error.", 38, 661, 16, ACCENT);
        Text("Load a usable save, or press F5 to explicitly save this village and resume autosaving.", 38,
             690, 16, PAPER);
    }
    else if (g->tick < 1200)
    {
        Rounded((Rectangle){22, 632, 790, 86}, Fade(INK, .94f));
        Label("YOUR FIRST DECISION", 38, 645);
        Text("Eight people. One fire. A future to build.", 38, 664, 20, PAPER);
        Text("Choose Shelter below and place two near the hearth. Builders take it from there.", 38, 694, 16,
             MUTED);
    }
}
static void SaveFolder(void)
{
    const char *home = getenv("HOME");
    if (!home)
        home = ".";
#ifdef __APPLE__
    snprintf(ui.savePath, sizeof(ui.savePath), "%s/Library/Application Support/Greenfield/village.gfv1",
             home);
#else
    snprintf(ui.savePath, sizeof(ui.savePath), "%s/.greenfield/village.gfv1", home);
#endif
}
static bool Save(void)
{
    // One attempt per autosave interval, even if the filesystem is unavailable.
    ui.lastAutosave = ui.g->tick;
    char parent[4096];
    snprintf(parent, sizeof(parent), "%s", ui.savePath);
    char *slash = strrchr(parent, '/');
    if (slash)
        *slash = 0;
    if (mkdir(parent, 0700) && errno != EEXIST)
    {
        Toast("Could not create the save folder. Your previous save is untouched.");
        return false;
    }
    char why[256];
    bool ok = GameSave(ui.g, ui.savePath, why, sizeof(why));
    Toast(why);
    if (ok)
        ui.autosaveBlocked = false;
    return ok;
}
static bool Load(const char *path)
{
    char why[256];
    bool ok = GameLoad(ui.g, path, why, sizeof(why));
    Toast(why);
    if (ok)
    {
        ui.selected = ui.selectedCitizen = ui.build = ui.areaRole = -1;
        ui.accumulator = 0;
        ui.lastAutosave = ui.g->tick;
        ui.winSeen = ui.g->won;
        ui.speed = 0;
        ui.autosaveBlocked = false;
    }
    else
    {
        // Preserve the failed slot until a deliberate save, successful load or new game.
        ui.autosaveBlocked = true;
        ui.speed = 0;
        char message[256];
        snprintf(message, sizeof(message), "%.165s Autosave paused. F5 saves the current village.", why);
        Toast(message);
    }
    return ok;
}
static void Menu(void)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade((Color){15, 27, 22, 255}, .83f));
    ui.allowInput = true;
    Rectangle card = {438, 137, 565, 624};
    Rounded(card, (Color){35, 48, 39, 255});
    Border(card, Fade(ACCENT, .45f));
    Label("A GREENFIELD CHRONICLE", 503, 180);
    Title("Founding a Tribe", 501, 215, 42, PAPER);
    WrapText("The ice is retreating. Your story begins with eight people and the warmth of a single fire.",
             503, 279, 429, 17, MUTED);
    if (ui.confirmNew)
    {
        WrapText(
            "Begin a new village? Your saved slot stays intact until you save or the next autosave runs.",
            503, 382, 427, 17, PAPER);
        if (Button((Rectangle){503, 480, 427, 47}, "Begin a new village", true, true))
        {
            GameInit(ui.g);
            ui.menu = false;
            ui.confirmNew = false;
            ui.winSeen = false;
            ui.build = ui.selected = ui.selectedCitizen = ui.areaRole = -1;
            ui.speed = 1;
            ui.accumulator = 0;
            ui.lastAutosave = 0;
            ui.autosaveBlocked = false;
            ui.camera.target = Iso(23, 23);
            ui.camera.zoom = .86f;
            ui.panel = 0;
        }
        if (Button((Rectangle){503, 542, 427, 43}, "Back", false, true))
            ui.confirmNew = false;
        return;
    }
    if (Button((Rectangle){503, 364, 427, 47}, ui.g->tick ? "Resume village" : "Begin your story", true,
               !ui.g->lost))
    {
        ui.menu = false;
        ui.speed = ui.lastSpeed;
    }
    if (Button((Rectangle){503, 425, 207, 41}, "New Game", false, true))
        ui.confirmNew = true;
    if (Button((Rectangle){723, 425, 207, 41}, "Continue / Load", false, FileExists(ui.savePath)) &&
        Load(ui.savePath))
    {
        ui.menu = false;
        ui.speed = 1;
    }
    if (Button((Rectangle){503, 479, 427, 41}, "Save village   F5", false, !ui.g->lost))
        Save();
    if (Button((Rectangle){503, 533, 427, 41}, "Reference Viewer", false, true))
        ui.reference = true;
    if (Button((Rectangle){503, 587, 207, 41}, "Field guide", false, true))
    {
        ui.help = true;
        ui.menu = false;
    }
    if (Button((Rectangle){723, 587, 207, 41}, "Quit", false, true))
        ui.quit = true;
    Label("SINGLE PLAYER    /    OFFLINE    /    ONE LOCAL SAVE", 503, 664);
    WrapText(ui.toastTime > 0 ? ui.toast
             : ui.autosaveBlocked
                 ? "Autosave paused after a load error. Load a usable save or explicitly "
                   "save this village to resume autosaving."
                 : "Autosaves every two minutes of simulation time. New villages reach their "
                   "first chapter in about 45-60 minutes at 1x.",
             503, 692, 426, 14, MUTED);
}
static void Guide(void)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(INK, .89f));
    ui.allowInput = true;
    Rounded((Rectangle){252, 90, 936, 720}, (Color){35, 48, 39, 255});
    Label("A FEW THINGS TO CARRY WITH YOU", 290, 127);
    Title("The field guide", 288, 160, 40, PAPER);
    const char *headings[] = {"01 / Shelter & survival", "02 / Work & deliveries", "03 / Discover & grow",
                              "04 / Share the valley"};
    const char *copy[] = {
        "Build two shelters for your eight founders. The hearth burns one wood every 20 seconds in winter. "
        "Everyone eats one food per minute. Shortages warn you before hunger or cold begins to harm health. "
        "Food and warmth restore health.",
        "Assign roles in Workforce. Reduce a role to free people, then add them elsewhere. Workers walk to "
        "resources, work and carry goods to storage. Construction costs are paid once, when placed. A "
        "blocked route is retried when the path opens.",
        "Build a workshop and assign a crafter. Discover better tools, insulation and storage. After the "
        "twelve-minute winter, discover Farming. Build on fertile river soil and assign farmers. Travelers "
        "arrive when you have spare beds and a food reserve.",
        "Raid warnings give 90 seconds to prepare. Guards defend near the rally flag; a watch post moves the "
        "rally. Pay 40 food for twelve minutes of truce. Secure a harvest, 20 people, beds and three days of "
        "food to found your village."};
    for (int i = 0; i < 4; i++)
    {
        float x = 290 + (i % 2) * 455, y = 240 + (i / 2) * 205;
        Text(headings[i], x, y, 21, ACCENT);
        WrapText(copy[i], x, y + 40, 401, 16, PAPER);
    }
    Text("Space pause   1/2/4 speed   B build   W workers   R discoveries   O objectives", 290, 693, 16,
         MUTED);
    Text("Wheel zoom   Right drag / arrows pan   Home recenter   M whole valley   G grid", 290, 720, 16,
         MUTED);
    Text("F5 save   F9 load   Esc menu or cancel   F fullscreen", 290, 747, 16, MUTED);
    if (Button((Rectangle){974, 740, 173, 41}, "Back to the valley", true, true))
        ui.help = false;
}
static void Ending(void)
{
    GameState *g = ui.g;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(INK, .83f));
    ui.allowInput = true;
    Rounded((Rectangle){414, 241, 612, 399}, (Color){35, 48, 39, 255});
    Label(g->lost ? "THE FIRE HAS GONE OUT" : "CHAPTER ONE COMPLETE", 463, 281);
    Title(g->lost ? "A quiet valley" : "A village is born", 459, 326, 43, PAPER);
    WrapText(g->lost ? "No one remains to tend the hearth. You can return to your last save, or lead a new "
                       "tribe into the valley."
                     : "Your people have survived the cold, raised a harvest and made a home. The story is "
                       "yours to continue.",
             463, 396, 511, 19, MUTED);
    if (!g->lost && Button((Rectangle){463, 527, 513, 49}, "Continue building", true, true))
    {
        ui.winSeen = true;
        ui.speed = 1;
    }
    if (g->lost)
    {
        if (Button((Rectangle){463, 527, 247, 49}, "Load village", true, FileExists(ui.savePath)))
            Load(ui.savePath);
        if (Button((Rectangle){728, 527, 248, 49}, "Restart", false, true))
        {
            GameInit(g);
            ui.speed = 1;
            ui.winSeen = false;
            ui.lastAutosave = 0;
            ui.autosaveBlocked = false;
            ui.selected = ui.selectedCitizen = -1;
        }
    }
}
static bool WorldHit(void)
{
    if (ui.g->foodWarning || ui.g->coldWarning || ui.g->raidWarning || ui.g->raidActive)
        if (Hit((Rectangle){22, 232, 536, 39}))
            return false;
    return ui.mouse.x < PANEL_X && ui.mouse.y > 218 && ui.mouse.y < 630 &&
           !Hit((Rectangle){928, 586, 179, 127});
}
static void Input(float dt)
{
    GameState *g = ui.g;
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (ui.help)
            ui.help = false;
        else if (ui.build >= 0 || ui.areaRole >= 0)
        {
            ui.build = -1;
            ui.areaRole = -1;
        }
        else
            ui.menu = !ui.menu;
    }
    if (IsKeyPressed(KEY_H))
        ui.help = !ui.help;
    if (ui.menu || ui.help || g->lost || (!ui.winSeen && g->won))
        return;
    if (IsKeyPressed(KEY_SPACE))
    {
        if (ui.speed)
        {
            ui.lastSpeed = ui.speed;
            ui.speed = 0;
        }
        else
            ui.speed = ui.lastSpeed;
    }
    if (IsKeyPressed(KEY_ONE))
        ui.speed = 1;
    if (IsKeyPressed(KEY_TWO))
        ui.speed = 2;
    if (IsKeyPressed(KEY_FOUR))
        ui.speed = 4;
    if (IsKeyPressed(KEY_F5))
        Save();
    if (IsKeyPressed(KEY_F9))
        Load(ui.savePath);
    if (IsKeyPressed(KEY_O))
        ui.panel = 0;
    if (IsKeyPressed(KEY_W))
        ui.panel = 1;
    if (IsKeyPressed(KEY_R))
        ui.panel = 2;
    if (IsKeyPressed(KEY_B))
        ui.build = ui.build >= 0 ? -1 : SHELTER;
    if (IsKeyPressed(KEY_G))
        ui.grid = !ui.grid;
    if (IsKeyPressed(KEY_HOME))
    {
        ui.camera.target = Iso(23, 23);
        ui.camera.zoom = .86f;
    }
    if (IsKeyPressed(KEY_M))
    {
        ui.camera.target = Iso(24, 24);
        ui.camera.zoom = .32f;
    }
    if (IsKeyPressed(KEY_F))
        ToggleBorderlessWindowed();
    if (ui.mouse.x < PANEL_X)
    {
        float wheel = GetMouseWheelMove();
        if (wheel)
        {
            Vector2 before = GetScreenToWorld2D(ui.mouse, ui.camera);
            ui.camera.zoom = Clamp(ui.camera.zoom * powf(1.12f, wheel), .3f, 2.3f);
            Vector2 after = GetScreenToWorld2D(ui.mouse, ui.camera);
            ui.camera.target = Vector2Add(ui.camera.target, Vector2Subtract(before, after));
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            Vector2 d = GetMouseDelta();
            float scale = MinF(GetScreenWidth() / (float)SCREEN_W, GetScreenHeight() / (float)SCREEN_H);
            ui.camera.target.x -= d.x / (scale * ui.camera.zoom);
            ui.camera.target.y -= d.y / (scale * ui.camera.zoom);
        }
    }
    ui.camera.target.x += (IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT)) * dt * 550 / ui.camera.zoom;
    ui.camera.target.y += (IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP)) * dt * 550 / ui.camera.zoom;
    ui.camera.target.x = Clamp(ui.camera.target.x, -1450, 1450);
    ui.camera.target.y = Clamp(ui.camera.target.y, 0, 1536);
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !WorldHit() || ui.hoverTile < 0)
        return;
    int x = ui.hoverTile % MAP_SIZE, y = ui.hoverTile / MAP_SIZE;
    if (ui.build >= 0)
    {
        if (Command(CMD_BUILD, ui.build, x, y))
        {
            ui.selected = g->buildingCount - 1;
            Toast("Foundation placed. Builders will bring it to life.");
            if (!IsKeyDown(KEY_LEFT_SHIFT))
                ui.build = -1;
        }
        return;
    }
    if (ui.areaRole >= 0)
    {
        if (ui.areaRole == GUARD)
            Command(CMD_RALLY, x, y, 0);
        else
            Command(CMD_AREA, ui.areaRole, x, y);
        ui.areaRole = -1;
        return;
    }
    int nearest = -1;
    float dist = 30;
    for (int i = 0; i < g->citizenCount; i++)
        if (g->citizens[i].alive)
        {
            Vector2 p = GetWorldToScreen2D(
                Iso(g->citizens[i].x / 256.0f + .5f, g->citizens[i].y / 256.0f + .5f), ui.camera);
            p.y -= 14 * ui.camera.zoom;
            float d = Vector2Distance(ui.mouse, p);
            if (d < dist)
            {
                dist = d;
                nearest = i;
            }
        }
    // Use the rendered foot anchors and depth so roofs are selectable too.
    Vector2 world = GetScreenToWorld2D(ui.mouse, ui.camera);
    int building = g->tiles[ui.hoverTile].occupied;
    float depth = -1;
    if (building >= 0)
    {
        Building *b = &g->buildings[building];
        depth = b->x + b->y + (BUILDINGS[b->type].w + BUILDINGS[b->type].h) * .5f;
    }
    for (int i = 0; i < g->buildingCount; i++)
    {
        Building *b = &g->buildings[i];
        if (b->type == FARM && b->complete)
            continue;
        int index;
        float width;
        Vector2 foot = BuildingVisual(b, &index, &width);
        Rectangle src = ui.objects.rect[index];
        float height = width * src.height / src.width;
        float d = b->x + b->y + (BUILDINGS[b->type].w + BUILDINGS[b->type].h) * .5f;
        if (d >= depth &&
            CheckCollisionPointRec(world, (Rectangle){foot.x - width / 2, foot.y - height, width, height}))
        {
            building = i;
            depth = d;
        }
    }
    if (nearest >= 0 && (g->citizens[nearest].x + g->citizens[nearest].y) / 256.0f + 1 >= depth)
    {
        ui.selectedCitizen = nearest;
        ui.selected = -1;
        ui.panel = 4;
        return;
    }
    if (x >= 37 && y < 13)
    {
        ui.panel = 3;
        return;
    }
    ui.selectedCitizen = -1;
    ui.selected = building;
    ui.panel = 4;
}
static void Draw(void)
{
    BeginTextureMode(ui.canvas);
    ClearBackground(INK);
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    DrawRectangleGradientV(0, 86, PANEL_X, SCREEN_H - 250, (Color){114, 140, 131, 255},
                           (Color){71, 98, 78, 255});
    EndMode2D();
    WorldDraw();
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    ui.allowInput = !ui.menu && !ui.help && !ui.g->lost && (!ui.g->won || ui.winSeen);
    Header();
    WorldHUD();
    switch (ui.panel)
    {
    case 0:
        Objectives();
        break;
    case 1:
        Workforce();
        break;
    case 2:
        Discoveries();
        break;
    case 3:
        RivalPanel();
        break;
    default:
        Selected();
        break;
    }
    BuildPalette();
    if (ui.menu)
        Menu();
    if (ui.help)
        Guide();
    if (!ui.menu && (ui.g->lost || (!ui.winSeen && ui.g->won)))
        Ending();
    EndMode2D();
    EndTextureMode();
    float scale = MinF(GetScreenWidth() / (float)SCREEN_W, GetScreenHeight() / (float)SCREEN_H);
    Rectangle dst = {(GetScreenWidth() - SCREEN_W * scale) / 2, (GetScreenHeight() - SCREEN_H * scale) / 2,
                     SCREEN_W * scale, SCREEN_H * scale};
    BeginDrawing();
    ClearBackground(INK);
    DrawTexturePro(ui.canvas.texture,
                   (Rectangle){0, 0, ui.canvas.texture.width, -ui.canvas.texture.height}, dst,
                   (Vector2){0, 0}, 0, WHITE);
    EndDrawing();
}
int GameViewRun(GameState *game, int argc, char **argv, bool returning)
{
    const char *capture = NULL, *load = NULL;
    bool play = false;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--play"))
            play = true;
        else if (!strcmp(argv[i], "--game-capture") && i + 1 < argc)
            capture = argv[++i];
        else if (!strcmp(argv[i], "--load") && i + 1 < argc)
            load = argv[++i];
        else
        {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return 2 + 1;
        }
    }
    bool autosaveBlocked = returning && ui.autosaveBlocked;
    memset(&ui, 0, sizeof(ui));
    ui.autosaveBlocked = autosaveBlocked;
    ui.g = game;
    ui.selected = ui.selectedCitizen = ui.build = ui.areaRole = ui.hoverTile = -1;
    ui.speed = ui.lastSpeed = 1;
    ui.menu = !play && !capture;
    ui.camera =
        (Camera2D){.offset = {PANEL_X / 2.0f, 421}, .target = Iso(23, 23), .rotation = 0, .zoom = .86f};
    SaveFolder();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI |
                   (capture ? FLAG_WINDOW_HIDDEN : FLAG_VSYNC_HINT));
    InitWindow(1440, 900, "Greenfield - Founding a Tribe");
    if (!IsWindowReady())
        return 1;
    SetExitKey(KEY_NULL);
    SetWindowMinSize(1000, 625);
    SetTargetFPS(60);
    if (!capture)
    {
        int monitor = GetCurrentMonitor();
        int width = GetMonitorWidth(monitor) - 70, height = GetMonitorHeight(monitor) - 110;
        float scale = MinF(1, MinF(width / (float)SCREEN_W, height / (float)SCREEN_H));
        SetWindowSize((int)(SCREEN_W * scale), (int)(SCREEN_H * scale));
    }
    ui.objects = LoadAtlas("settlement-atlas.png");
    ui.people = LoadAtlas("citizen-atlas.png");
    ui.braids = LoadAtlas("citizen-braids-atlas.png");
    ui.canvas = LoadRenderTexture(SCREEN_W * RENDER_SCALE, SCREEN_H * RENDER_SCALE);
    SetTextureFilter(ui.canvas.texture, TEXTURE_FILTER_BILINEAR);
    char fontPath[4096];
    AssetName(fontPath, sizeof(fontPath), "AtkinsonHyperlegible-Regular.ttf");
    ui.font = LoadFontEx(fontPath, 64, NULL, 0);
    AssetName(fontPath, sizeof(fontPath), "AtkinsonHyperlegible-Bold.ttf");
    ui.title = LoadFontEx(fontPath, 96, NULL, 0);
    SetTextureFilter(ui.font.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ui.title.texture, TEXTURE_FILTER_BILINEAR);
    int result = 0;
    if (!ui.objects.texture.id || !ui.people.texture.id || !ui.braids.texture.id || !ui.canvas.id ||
        ui.font.texture.id == GetFontDefault().texture.id ||
        ui.title.texture.id == GetFontDefault().texture.id)
    {
        fprintf(stderr, "The game artwork, fonts, or render target could not be loaded. Keep assets beside the executable.\n");
        result = 1;
        ui.quit = true;
    }
    if (load && !returning)
    {
        if (!Load(load))
        {
            fprintf(stderr, "%s\n", ui.toast);
            result = 1;
            ui.quit = true;
        }
        else
        {
            ui.menu = false;
            ui.speed = 1;
        }
    }
    ui.lastAutosave = game->tick;
    ui.winSeen = game->won;
    if (capture)
        ui.toastTime = 0;
    while (!WindowShouldClose() && !ui.quit && !ui.reference)
    {
        double frameSeconds = GetFrameTime();
        // Limit camera jumps after a stall without dropping simulation time.
        float dt = (float)frameSeconds;
        if (dt > .25f)
            dt = .25f;
        ui.time += dt;
        ui.toastTime = fmaxf(0, ui.toastTime - dt);
        float scale = MinF(GetScreenWidth() / (float)SCREEN_W, GetScreenHeight() / (float)SCREEN_H);
        Vector2 raw = GetMousePosition();
        ui.mouse = (Vector2){(raw.x - (GetScreenWidth() - SCREEN_W * scale) / 2) / scale,
                             (raw.y - (GetScreenHeight() - SCREEN_H * scale) / 2) / scale};
        Vector2 tile = TileAt(ui.mouse);
        ui.hoverTile = GameTile((int)floorf(tile.x), (int)floorf(tile.y));
        Input(dt);
        if (!ui.menu && !ui.help && !game->lost && (!game->won || ui.winSeen) && !capture)
        {
            GameAdvance(game, frameSeconds, ui.speed, &ui.accumulator);
            if (!ui.autosaveBlocked && game->tick - ui.lastAutosave >= 1200)
                Save();
        }
        Draw();
        if (capture)
        {
            Image img = LoadImageFromTexture(ui.canvas.texture);
            ImageFlipVertical(&img);
            result = ExportImage(img, capture) ? 0 : 1;
            UnloadImage(img);
            break;
        }
    }
    if (ui.reference)
        result = 2;
    UnloadTexture(ui.objects.texture);
    UnloadTexture(ui.people.texture);
    UnloadTexture(ui.braids.texture);
    UnloadRenderTexture(ui.canvas);
    if (ui.font.texture.id != GetFontDefault().texture.id)
        UnloadFont(ui.font);
    if (ui.title.texture.id != GetFontDefault().texture.id)
        UnloadFont(ui.title);
    CloseWindow();
    return result;
}
