#include "hud.h"
#include "actions.h"
#include "assets.h"
#include "layout.h"
#include "raymath.h"
#include "widgets.h"
#include "world.h"
#include <math.h>

bool ViewHasWarning(const ViewContext *ui)
{
    const GameState *g = ui->g;
    return g->factions[0].foodWarning || g->factions[0].coldWarning || g->factions[1].raidWarning ||
           g->factions[1].raidActive || GameHousing(g) < GamePopulation(g) || ui->autosaveBlocked;
}
bool ViewWorldPointHit(const ViewContext *ui, Vector2 point)
{
    return !ViewPanelOpen(ui) && CheckCollisionPointRec(point, ViewWorldRect()) &&
           !CheckCollisionPointRec(point, ViewObjectiveRect()) &&
           !CheckCollisionPointRec(point, ViewSelectionRect(ui)) &&
           !CheckCollisionPointRec(point, ViewMapRect()) &&
           !CheckCollisionPointRec(point, ViewProductionRect(ui)) &&
           !(ui->orderMode && CheckCollisionPointRec(point, ViewOrderHintRect())) &&
           !(ViewHasWarning(ui) && CheckCollisionPointRec(point, ViewWarningRect()));
}
bool ViewWorldHit(ViewContext *ui)
{
    return ViewWorldPointHit(ui, ui->mouse);
}

static void ResourceIcon(ViewContext *ui, int r, Vector2 foot)
{
    const int icons[] = {14, 2, 13, 4, 5, 13, 5};
    ViewSprite(ui->objects, icons[r], foot, 23,
               r == ORE      ? (Color){211, 143, 89, 255}
               : r == BRONZE ? ACCENT
                             : WHITE,
               false);
}
void ViewHeader(ViewContext *ui)
{
    GameState *g = ui->g;
    Faction *f = &g->factions[0];
    DrawRectangle(0, 0, SCREEN_W, WORLD_TOP, (Color){28, 39, 33, 255});
    DrawLine(0, WORLD_TOP - 1, SCREEN_W, WORLD_TOP - 1, Fade(ACCENT, .35f));
    for (int r = 0; r < RESOURCE_COUNT; r++)
    {
        float x = 14 + r * 78;
        ResourceIcon(ui, r, (Vector2){x + 12, 36});
        ViewTitle(ui,
                  f->stock[r] >= 1000000 ? TextFormat("%.1fm", f->stock[r] / 1000000.f)
                  : f->stock[r] >= 10000 ? TextFormat("%.0fk", f->stock[r] / 1000.f)
                                         : TextFormat("%d", f->stock[r]),
                  x + 30, 11, 20, r == FOOD && f->foodWarning ? ACCENT : PAPER);
    }
    DrawLine(567, 10, 567, 38, Fade(MUTED, .25f));
    ViewText(ui, TextFormat("%d/%d people", GamePopulation(g), GameHousing(g)), 582, 14, 18, PAPER);
    if (ViewButton(ui, (Rectangle){704, 8, 74, 32}, TextFormat("Idle %d", GameIdleCountFor(g, 0)), false,
                   true))
        ViewSelectIdle(ui);
    if (ViewButton(ui, (Rectangle){788, 8, 136, 32}, AGES[f->age].name, false, true))
        ViewTogglePanel(ui, 0);
    ViewText(ui,
             TextFormat("Day %llu  /  %s", (unsigned long long)g->tick / DAY_TICKS + 1,
                        g->tick < WINTER_END ? "Winter"
                        : ViewSnow(ui) > .1f ? "Thaw"
                                             : "Growing"),
             938, 8, 16, PAPER);
    ViewText(ui,
             TextFormat("%02llu:%02llu / %d%% warm", (unsigned long long)g->tick / 600,
                        (unsigned long long)g->tick / 10 % 60, f->warmth),
             938, 27, 14, MUTED);
    if (ViewButton(ui, (Rectangle){1082, 8, 65, 32}, ui->speed ? "Pause" : "Play", !ui->speed, true))
    {
        if (ui->speed)
        {
            ui->lastSpeed = ui->speed;
            ui->speed = 0;
        }
        else
            ui->speed = ui->lastSpeed;
    }
    const int speeds[] = {1, 2, 4};
    for (int i = 0; i < 3; i++)
        if (ViewButton(ui, (Rectangle){1153 + i * 43, 8, 38, 32}, TextFormat("%dx", speeds[i]),
                       ui->speed == speeds[i], true))
            ui->speed = ui->lastSpeed = speeds[i];
    if (ViewButton(ui, (Rectangle){1291, 8, 54, 32}, "Help", false, true))
        ui->help = true;
    if (ViewButton(ui, (Rectangle){1353, 8, 71, 32}, "Menu", false, true))
        ui->menu = true;
}

void ViewBuildPalette(ViewContext *ui)
{
    if (!ui->buildMenu || !ViewSelectedWorkers(ui))
        return;
    const char *names[] = {"Hearth", "Shelter",    "Stockpile", "Forage", "Hunting",  "Workshop", "Granary",
                           "Farm",   "Watch post", "Palisade",  "Gate",   "Training", "Smelter"};
    for (int i = 0; i < BUILDING_COUNT; i++)
    {
        Rectangle r = ViewBuildRect(ui, i);
        bool hover = ViewHit(ui, r);
        ViewRounded(r, ui->build == i ? (Color){75, 94, 62, 255}
                       : hover        ? (Color){56, 71, 53, 255}
                                      : (Color){41, 55, 43, 255});
        ViewBorder(r, ui->build == i ? ACCENT : Fade(MUTED, .3f));
        Atlas atlas = i >= TRAINING_GROUND ? ui->bronze : ui->objects;
        int index = i >= TRAINING_GROUND ? i - TRAINING_GROUND : i;
        Rectangle src = atlas.rect[index];
        float width = src.height > 0 ? ViewMinF(32, 32 * src.width / src.height) : 32;
        ViewSprite(atlas, index, (Vector2){r.x + 37, r.y + 34}, width, WHITE, false);
        float w = MeasureTextEx(ui->font, names[i], 14, .5f).x;
        ViewText(ui, names[i], r.x + (r.width - w) / 2, r.y + 37, 14, PAPER);
        if (ViewClick(ui, r))
        {
            ui->build = ui->build == i ? -1 : i;
            ui->orderMode = 0;
        }
        if (hover && ui->allowInput)
        {
            Rectangle h = ViewCommandHintRect(ui);
            ViewRounded(h, INK);
            ViewBorder(h, Fade(ACCENT, .35f));
            ViewTitle(ui, BUILDINGS[i].name, h.x + 12, h.y + 10, 19, ACCENT);
            ViewWrapText(ui, BUILDINGS[i].description, h.x + 12, h.y + 37, h.width - 24, 16, PAPER);
            ViewCost(ui, BUILDINGS[i].cost, h.x + 12, h.y + 87, h.width - 24);
        }
    }
    if (ViewButton(ui, ViewBuildRect(ui, BUILDING_COUNT), "Back", false, true))
        ViewBuildToggle(ui);
}

static void MiniMap(ViewContext *ui)
{
    Rectangle map = ViewMapRect();
    ViewRounded(map, (Color){30, 42, 34, 255});
    ViewBorder(map, Fade(ACCENT, .35f));
    const float cx = map.x + map.width / 2, cy = map.y + 10, sx = .99f, sy = .59f;
    for (int y = 0; y < MAP_SIZE; y++)
        for (int x = 0; x < MAP_SIZE; x++)
        {
            Tile *t = &ui->g->tiles[GameTile(x, y)];
            Color c = t->terrain == WATER     ? (Color){92, 160, 168, 255}
                      : t->node == NODE_TREE  ? (Color){86, 116, 74, 255}
                      : t->terrain == FERTILE ? (Color){169, 167, 94, 255}
                                              : (Color){150, 167, 129, 255};
            if (t->occupied >= 0)
                c = ui->g->buildings[t->occupied].owner ? RUST : ACCENT;
            DrawRectangle(cx + (x - y) * sx, cy + (x + y) * sy, 2, 2, c);
        }
    for (int i = 0; i < ui->g->citizenCount; i++)
    {
        Citizen *c = &ui->g->citizens[i];
        if (c->alive)
            DrawRectangle(cx + (c->x - c->y) / 256.f * sx, cy + (c->x + c->y) / 256.f * sy, 2, 2,
                          c->owner ? RUST : PAPER);
    }
    Rectangle world = ViewWorldRect();
    Vector2 corners[] = {{world.x, world.y},
                         {world.x + world.width, world.y},
                         {world.x + world.width, world.y + world.height},
                         {world.x, world.y + world.height}};
    Vector2 points[4];
    for (int i = 0; i < 4; i++)
    {
        Vector2 t = ViewTileAt(ui, corners[i]);
        points[i] = (Vector2){cx + (t.x - t.y) * sx, cy + (t.x + t.y) * sy};
    }
    BeginScissorMode(map.x * RENDER_SCALE, map.y * RENDER_SCALE, map.width * RENDER_SCALE,
                     map.height * RENDER_SCALE);
    for (int i = 0; i < 4; i++)
        DrawLineV(points[i], points[(i + 1) % 4], PAPER);
    EndScissorMode();
    bool clicked = ViewClick(ui, map);
    if (clicked)
        ui->mapDragging = true;
    if (!ui->allowInput || !IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        ui->mapDragging = false;
    if (clicked || ui->mapDragging)
    {
        float mx = (ui->mouse.x - cx) / sx, my = (ui->mouse.y - cy) / sy;
        ui->camera.target =
            ViewIso(Clamp((mx + my) / 2, 0, MAP_SIZE - 1), Clamp((my - mx) / 2, 0, MAP_SIZE - 1));
    }
}

void ViewWorldHUD(ViewContext *ui)
{
    GameState *g = ui->g;
    Faction *f = &g->factions[0];
    const char *goal = !g->winterSurvived ? "Survive winter"
                       : !f->village      ? "Found your village"
                       : !g->won          ? "Establish your country"
                                          : "Country founded";
    if (ViewButton(ui, ViewObjectiveRect(), TextFormat("%s  /  Objectives O", goal), false, true))
        ViewTogglePanel(ui, 0);
    if (ViewHasWarning(ui))
    {
        const char *warning = ui->autosaveBlocked          ? "Autosave paused after load error / F5 to save"
                              : g->factions[1].raidActive  ? "Raid underway / rally defenders"
                              : g->factions[1].raidWarning ? "Raid approaching / prepare or negotiate"
                              : f->foodWarning             ? "Food running low / assign food workers"
                              : f->coldWarning ? "Cold threatens health / build beds and gather wood"
                                               : "Housing full / build shelters to recruit";
        Rectangle r = ViewWarningRect();
        ViewRounded(r, (Color){101, 57, 36, 245});
        ViewText(ui, warning, r.x + 12, r.y + 8, 16, PAPER);
        if (ViewClick(ui, r))
            ViewTogglePanel(ui, g->factions[1].raidActive || g->factions[1].raidWarning ? 3 : 1);
    }
    if (ui->dragging && !ui->panCandidate)
    {
        Rectangle r = {fminf(ui->dragStart.x, ui->mouse.x), fminf(ui->dragStart.y, ui->mouse.y),
                       fabsf(ui->mouse.x - ui->dragStart.x), fabsf(ui->mouse.y - ui->dragStart.y)};
        DrawRectangleRec(r, Fade(ACCENT, .12f));
        DrawRectangleLinesEx(r, 1, ACCENT);
    }
    MiniMap(ui);
    if (ui->orderMode)
    {
        Rectangle r = ViewOrderHintRect();
        ViewRounded(r, INK);
        ViewText(ui,
                 ui->orderMode == VIEW_ORDER_RALLY ? "Choose a rally point / Esc cancels"
                                                   : "Choose a target / Shift queues / Esc cancels",
                 r.x + 12, r.y + 7, 16, PAPER);
    }
    if (ui->toastTime > 0)
    {
        ViewRounded((Rectangle){528, 100, 600, 57}, INK);
        ViewWrapText(ui, ui->toast, 543, 111, 570, 16, PAPER);
    }
    // Rates and reserve details stay available without occupying the main HUD.
    for (int r = 0; r < RESOURCE_COUNT; r++)
        if (ui->allowInput && ViewHit(ui, (Rectangle){14 + r * 78, 0, 78, WORLD_TOP}))
        {
            double minutes = g->tick ? g->tick / 600.0 : 1;
            ViewRounded((Rectangle){16, 54, 660, 94}, INK);
            ViewTitle(ui, TextFormat("%s  /  %d available", RESOURCE_NAMES[r], f->stock[r]), 30, 63, 19,
                      ACCENT);
            ViewText(ui,
                     TextFormat("Produced %.1f/min   Spent %.1f/min   Reserved %d  (campaign averages)",
                                f->produced[r] / minutes, f->spent[r] / minutes, f->reserved[r]),
                     30, 91, 16, PAPER);
            ViewText(ui,
                     r == FOOD ? TextFormat("Upkeep %d/min. Food reserve %.1f days.", GamePopulation(g),
                                            GameFoodDaysTenths(g) / 10.f)
                               : "Workers must deliver carried goods before they can be spent.",
                     30, 119, 16, MUTED);
        }
}
