#include "hud.h"
#include "actions.h"
#include "assets.h"
#include "layout.h"
#include "raymath.h"
#include "widgets.h"
#include "world.h"
#include <math.h>
#include <stdio.h>

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
           !CheckCollisionPointRec(point, ViewBuildInfoRect(ui)) &&
           !CheckCollisionPointRec(point, ViewMapRect()) &&
           !CheckCollisionPointRec(point, ViewProductionRect(ui)) &&
           !((ui->orderMode || ui->movingBuilding) && CheckCollisionPointRec(point, ViewOrderHintRect())) &&
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
        bool hover = ui->allowInput && ViewHit(ui, r);
        bool unlocked = GameBuildingUnlockedFor(ui->g, 0, i, NULL, 0);
        bool affordable = GameCanAfford(ui->g, BUILDINGS[i].cost);
        if (ViewClick(ui, r))
        {
            ui->build = unlocked && affordable && ui->build != i ? i : -1;
            ui->buildInspect = i + 1;
            ui->buildRotation = 0;
            ui->orderMode = 0;
        }
        bool selected = ui->build == i || (ui->build < 0 && ui->buildInspect == i + 1);
        Color base = selected ? (Color){75, 94, 62, 255} : (Color){41, 55, 43, 255};
        if (!unlocked)
            base = ViewMix(base, INK, .6f);
        if (hover)
            base = ViewMix(base, PAPER, IsMouseButtonDown(MOUSE_BUTTON_LEFT) ? .05f : .12f);
        ViewRounded(r, base);
        ViewBorder(r, Fade(MUTED, .4f));
        if (selected)
            ViewSelectedBorder(r);
        Atlas atlas = i >= TRAINING_GROUND ? ui->bronze : ui->objects;
        int index = i >= TRAINING_GROUND ? i - TRAINING_GROUND : i;
        Rectangle src = atlas.rect[index];
        float width = src.height > 0 ? ViewMinF(24, 24 * src.width / src.height) : 24;
        ViewSprite(atlas, index, (Vector2){r.x + 37, r.y + 25}, width, unlocked ? WHITE : MUTED, false);
        float w = MeasureTextEx(ui->font, names[i], 14, .5f).x;
        ViewText(ui, names[i], r.x + (r.width - w) / 2, r.y + 26, 14, PAPER);
        const char *status = !unlocked ? "Locked" : !affordable ? "Missing" : "Ready";
        Color statusColor = !unlocked ? MUTED : !affordable ? ACCENT : SAGE;
        w = MeasureTextEx(ui->font, status, 14, .5f).x;
        ViewText(ui, status, r.x + (r.width - w) / 2, r.y + 41, 14, statusColor);
    }
    if (ViewButton(ui, ViewBuildRect(ui, BUILDING_COUNT), "Back", false, true))
    {
        ViewBuildToggle(ui);
        return;
    }
    float x = ViewSelectionRect(ui).x + 12, y = ViewSelectionRect(ui).y;
    int inspect = ui->build >= 0 ? ui->build : ui->buildInspect - 1;
    ViewTitle(ui, inspect >= 0 ? BUILDINGS[inspect].name : "Construction", x, y + 12, 22, PAPER);
    char why[256] = "Choose a building. Locked and missing-material buildings show their requirements.";
    if (inspect >= 0)
    {
        if (GameBuildingUnlockedFor(ui->g, 0, inspect, why, sizeof(why)))
        {
            if (!GameCanAfford(ui->g, BUILDINGS[inspect].cost))
                snprintf(why, sizeof(why), "Missing stored materials. Hover this building for amounts.");
            else if (ui->build >= 0)
            {
                snprintf(why, sizeof(why), "Choose a tile. Shift repeats.");
                if (ViewWorldHit(ui) && ui->hoverTile >= 0)
                {
                    Building preview;
                    ViewBuildingPreview(ui, &preview, why, sizeof(why));
                }
            }
            else
                snprintf(why, sizeof(why), "Materials ready. Click the building to place it.");
        }
    }
    ViewWrapText(ui, why, x, y + 43, 252, 16, PAPER);
    if (ui->build >= 0)
    {
        if (ViewButton(ui, (Rectangle){x, y + 105, 130, 28}, "Rotate  Q", false, true))
            ViewRotateBuilding(ui);
        ViewText(ui, "Esc cancels", x + 142, y + 113, 14, MUTED);
    }
    else
        ViewText(ui, "Hover for details / Esc cancels", x, y + 119, 14, MUTED);

    int detail = ViewBuildInfoType(ui);
    if (detail < 0)
        return;
    Rectangle h = ViewBuildInfoRect(ui);
    const BuildingDef *d = &BUILDINGS[detail];
    bool unlocked = GameBuildingUnlockedFor(ui->g, 0, detail, why, sizeof(why));
    bool affordable = GameCanAfford(ui->g, d->cost);
    ViewRounded(h, INK);
    ViewBorder(h, PAPER);
    ViewTitle(ui, d->name, h.x + 12, h.y + 10, 21, PAPER);
    ViewText(ui,
             !unlocked     ? "LOCKED"
             : !affordable ? "MISSING MATERIALS"
                           : "READY TO BUILD",
             h.x + 350, h.y + 13, 16, unlocked && affordable ? SAGE : ACCENT);
    ViewText(ui, d->description, h.x + 12, h.y + 39, 16, MUTED);
    ViewWrapText(ui, unlocked ? "Unlocked. Materials must be delivered before building." : why, h.x + 12,
                 h.y + 64, h.width - 24, 17, unlocked ? PAPER : ACCENT);
    ViewText(ui, "Resource", h.x + 12, h.y + 109, 15, MUTED);
    ViewText(ui, "Cost", h.x + 244, h.y + 109, 15, MUTED);
    ViewText(ui, "Stored", h.x + 330, h.y + 109, 15, MUTED);
    ViewText(ui, "Missing", h.x + 434, h.y + 109, 15, MUTED);
    int row = 0;
    for (int resource = 0; resource < RESOURCE_COUNT; resource++)
        if (d->cost[resource])
        {
            int stock = ui->g->factions[0].stock[resource];
            int missing = d->cost[resource] - stock;
            float yy = h.y + 130 + row++ * 22;
            ViewText(ui, RESOURCE_NAMES[resource], h.x + 12, yy, 17, PAPER);
            ViewText(ui, TextFormat("%d", d->cost[resource]), h.x + 244, yy, 17, PAPER);
            ViewText(ui, TextFormat("%d", stock), h.x + 330, yy, 17, missing > 0 ? ACCENT : PAPER);
            ViewTitle(ui, missing > 0 ? TextFormat("%d more", missing) : "Enough", h.x + 434, yy, 17,
                      missing > 0 ? ACCENT : SAGE);
        }
    ViewText(ui, "Stored excludes goods being carried or reserved for other projects.", h.x + 12,
             h.y + h.height - 24, 14, MUTED);
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
    if (ui->orderMode || ui->movingBuilding)
    {
        Rectangle r = ViewOrderHintRect();
        ViewRounded(r, INK);
        ViewBorder(r, ACCENT);
        ViewText(ui,
                 ui->movingBuilding ? "Move: click a position / Q rotates / Esc cancels"
                 : ui->orderMode == VIEW_ORDER_RALLY ? "Choose a rally point / Esc cancels"
                 : ui->orderMode == ORDER_GATHER
                     ? "Gather: left-click a resource / Shift queues / Esc cancels"
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
