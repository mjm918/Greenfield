#include "hud.h"
#include "assets.h"
#include "context.h"
#include "raymath.h"
#include "widgets.h"
#include "world.h"
#include <stdio.h>

void ViewHeader(ViewContext *ui)
{
    GameState *g = ui->g;
    DrawRectangle(0, 0, SCREEN_W, 86, (Color){31, 43, 35, 255});
    DrawLine(0, 85, SCREEN_W, 85, (Color){78, 89, 66, 255});
    ViewTitle(ui, "GREENFIELD", 24, 12, 30, PAPER);
    ViewLabel(ui, "FOUNDING A TRIBE", 27, 48);
    const int icons[] = {14, 2, 13, 4, 5};
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        float x = 234 + r * 133;
        ViewSprite(ui->objects, icons[r], (Vector2){x + 18, 57}, 30, WHITE, false);
        ViewText(ui, TextFormat("%d", g->stock[r]), x + 40, 21, 22, PAPER);
        ViewLabel(ui, RESOURCE_NAMES[r], x + 41, 49);
        if (r == FOOD && g->foodWarning)
            DrawCircle((int)(x + 105), 28, 3, RUST);
    }
    DrawLine(918, 18, 918, 65, Fade(MUTED, .25f));
    ViewText(ui, TextFormat("%d / %d", GamePopulation(g), GameHousing(g)), 943, 20, 22, PAPER);
    ViewLabel(ui, "PEOPLE / BEDS", 944, 49);
    ViewText(ui, TextFormat("Day %llu", (unsigned long long)(g->tick / DAY_TICKS + 1)), 1088, 18, 20, PAPER);
    ViewText(ui, g->tick < WINTER_END ? "Deep winter"
         : ViewSnow(ui) > .1f       ? "The thaw"
                              : "Growing season",
         1088, 45, 14, g->tick < WINTER_END ? (Color){169, 204, 210, 255} : SAGE);
    if (ViewButton(ui, (Rectangle){1307, 23, 109, 38}, "Menu  Esc", false, true))
        ui->menu = true;
}

void ViewBuildPalette(ViewContext *ui)
{
    DrawRectangle(0, SCREEN_H - 164, PANEL_X, 164, (Color){31, 43, 35, 255});
    DrawLine(0, SCREEN_H - 164, PANEL_X, SCREEN_H - 164, (Color){92, 102, 74, 255});
    ViewLabel(ui, "BUILD A HOME, ONE PLACE AT A TIME", 24, SCREEN_H - 146);
    const char *shortNames[] = {"Hearth",  "Shelter", "Stockpile",  "Forage post", "Hunting", "Workshop",
                                "Granary", "Farm",    "Watch post", "Palisade",    "Gate"};
    for (int i = 0; i < BUILDING_COUNT; i++)
    {
        Rectangle r = {22 + i * 99, SCREEN_H - 119, 91, 85};
        bool selected = ui->build == i, hover = ViewHit(ui, r);
        ViewRounded(r, selected ? (Color){82, 99, 65, 255}
                   : hover  ? (Color){59, 72, 53, 255}
                            : (Color){43, 56, 43, 255});
        ViewBorder(r, selected ? ACCENT : Fade(MUTED, .22f));
        ViewSprite(ui->objects, i, (Vector2){r.x + 45, r.y + 53}, i == WATCH_POST ? 35 : 51, WHITE, false);
        float width = MeasureTextEx(ui->font, shortNames[i], 14, 0.5f).x;
        ViewText(ui, shortNames[i], r.x + (91 - width) / 2, r.y + 62, 14, PAPER);
        bool locked =
            (i == FARM && !ui->g->discovery[FARMING]) || (i == GRANARY && !ui->g->discovery[FOOD_STORAGE]);
        if (locked)
            ViewText(ui, "*", r.x + 77, r.y + 7, 16, ACCENT);
        if (ViewClick(ui, r))
        {
            ui->build = selected ? -1 : i;
            ui->areaRole = -1;
        }
        if (hover && ui->allowInput && ui->build < 0)
        {
            ViewRounded((Rectangle){22, SCREEN_H - 237, 670, 63}, INK);
            ViewText(ui, BUILDINGS[i].description, 38, SCREEN_H - 225, 16, PAPER);
            ViewCost(ui, BUILDINGS[i].cost, 38, SCREEN_H - 201, 630);
        }
    }
    ViewText(ui, "Click to build  /  Esc cancels     |     Wheel zoom  /  Right drag pan     |     H for field guide",
         24, SCREEN_H - 21, 14, MUTED);
}

void ViewWorldHUD(ViewContext *ui)
{
    GameState *g = ui->g;
    const char *tabs[] = {"Objectives  O", "Workforce  W", "Discoveries  R", "Rivals", "Inspect"};
    for (int i = 0; i < 5; i++)
        if (ViewButton(ui, (Rectangle){22 + i * 135, 103, 126, 33}, tabs[i], ui->panel == i, true))
            ui->panel = i;
    ViewRounded((Rectangle){22, 151, 238, 67}, Fade(INK, .89f));
    ViewLabel(ui, g->tick < WINTER_END    ? "01 / THE LONG WINTER"
          : g->discovery[FARMING] ? "03 / ROOTS IN THE EARTH"
                                  : "02 / A PERMANENT CAMP",
          37, 163);
    ViewTitle(ui, g->tick < WINTER_END    ? "Keep the fire alive"
          : g->discovery[FARMING] ? "Let this be home"
                                  : "A valley waking up",
          37, 185, 20, PAPER);
    ViewRounded((Rectangle){812, 103, 296, 33}, Fade(INK, .90f));
    if (ViewButton(ui, (Rectangle){816, 106, 56, 27}, ui->speed == 0 ? "Play" : "Pause", ui->speed == 0, true))
    {
        if (ui->speed)
        {
            ui->lastSpeed = ui->speed;
            ui->speed = 0;
        }
        else
            ui->speed = ui->lastSpeed;
    }
    int speeds[] = {1, 2, 4};
    for (int i = 0; i < 3; i++)
        if (ViewButton(ui, (Rectangle){878 + i * 48, 106, 43, 27}, TextFormat("%dx", speeds[i]),
                   ui->speed == speeds[i], true))
            ui->speed = speeds[i];
    ViewText(ui,
        TextFormat("%02llu:%02llu", (unsigned long long)g->tick / 600, (unsigned long long)g->tick / 10 % 60),
        1030, 115, 14, PAPER);
    ViewRounded((Rectangle){903, 158, 204, 52}, Fade(INK, .85f));
    ViewLabel(ui, "CAMP WARMTH", 916, 169);
    ViewText(ui, TextFormat("%d%%", g->warmth), 1050, 169, 16, g->warmth < 80 ? ACCENT : SAGE);
    ViewBar(916, 191, 176, g->warmth / 100.0f, g->warmth < 50 ? RUST : SAGE);
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
        ViewRounded(r, (Color){102, 62, 42, 242});
        ViewText(ui, "!", 36, warningY + 10, 18, ACCENT);
        ViewText(ui, warning, 55, warningY + 12, 16, PAPER);
        if (ViewClick(ui, r))
        {
            ui->panel = g->raidActive || g->raidWarning ? 3 : 1;
        }
        warningY += 46;
    }
    // Every tile is known from the start. This overview also recenters the camera.
    Rectangle map = {928, 586, 179, 127};
    ViewRounded(map, Fade(INK, .90f));
    ViewLabel(ui, "THE VALLEY", 941, 597);
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
    Vector2 center = {ui->camera.target.x / TILE_W + ui->camera.target.y / TILE_H,
                      ui->camera.target.y / TILE_H - ui->camera.target.x / TILE_W};
    DrawCircleLines((int)(1017 + (center.x - center.y) * 1.6f), (int)(620 + (center.x + center.y) * .8f), 8,
                    PAPER);
    if (ViewClick(ui, map))
    {
        float mx = (ui->mouse.x - 1017) / 1.6f, my = (ui->mouse.y - 620) / .8f;
        ui->camera.target = ViewIso(Clamp((mx + my) / 2, 0, 47), Clamp((my - mx) / 2, 0, 47));
    }
    if (ui->build >= 0)
    {
        Rectangle card = {22, 632, 790, 88};
        ViewRounded(card, INK);
        const BuildingDef *d = &BUILDINGS[ui->build];
        ViewText(ui, d->name, 38, 643, 19, ACCENT);
        ViewCost(ui, d->cost, 210, 647, 580);
        char reason[256];
        if (ui->hoverTile >= 0)
            GameCanPlace(g, ui->build, ui->hoverTile % MAP_SIZE, ui->hoverTile / MAP_SIZE, reason,
                         sizeof(reason));
        else
            snprintf(reason, sizeof(reason), "Choose a clear place in the valley.");
        ViewWrapText(ui, reason, 38, 677, 755, 16, PAPER);
    }
    else if (ui->areaRole >= 0)
    {
        ViewRounded((Rectangle){22, 650, 810, 66}, INK);
        ViewText(ui, ui->areaRole == GUARD ? "Choose a guard rally point"
                                  : "Choose the center of this role's work area",
             38, 663, 17, ACCENT);
        ViewText(ui, "Click a tile in the valley. Esc cancels. Clear areas restores access to the whole map.", 38,
             690, 16, PAPER);
    }
    else if (ui->toastTime > 0)
    {
        ViewRounded((Rectangle){22, 647, 810, 68}, INK);
        ViewWrapText(ui, ui->toast, 38, 663, 777, 16, PAPER);
    }
    else if (ui->autosaveBlocked)
    {
        ViewRounded((Rectangle){22, 647, 810, 68}, INK);
        ViewText(ui, "Autosave paused after a load error.", 38, 661, 16, ACCENT);
        ViewText(ui, "Load a usable save, or press F5 to explicitly save this village and resume autosaving.", 38,
             690, 16, PAPER);
    }
    else if (g->tick < 1200)
    {
        ViewRounded((Rectangle){22, 632, 790, 86}, Fade(INK, .94f));
        ViewLabel(ui, "YOUR FIRST DECISION", 38, 645);
        ViewText(ui, "Eight people. One fire. A future to build.", 38, 664, 20, PAPER);
        ViewText(ui, "Choose Shelter below and place two near the hearth. Builders take it from there.", 38, 694, 16,
             MUTED);
    }
}
