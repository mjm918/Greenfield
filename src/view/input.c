#include "input.h"
#include "actions.h"
#include "context.h"
#include "hud.h"
#include "layout.h"
#include "raymath.h"
#include "widgets.h"
#include "world.h"
#include <math.h>
#include <string.h>
static int PersonAt(ViewContext *ui)
{
    int best = -1;
    float distance = 24;
    for (int i = 0; i < ui->g->citizenCount; i++)
    {
        Citizen *c = &ui->g->citizens[i];
        if (!c->alive)
            continue;
        Vector2 p = GetWorldToScreen2D(ViewIso(c->x / 256.f + .5f, c->y / 256.f + .5f), ui->camera);
        p.y -= 14 * ui->camera.zoom;
        float d = Vector2Distance(p, ui->mouse);
        if (d < distance)
        {
            distance = d;
            best = i;
        }
    }
    return best;
}
static int BuildingAt(ViewContext *ui)
{
    int best = ui->hoverTile < 0 ? -1 : ui->g->tiles[ui->hoverTile].occupied;
    float depth = -1;
    Vector2 mouse = GetScreenToWorld2D(ui->mouse, ui->camera);
    for (int i = 0; i < ui->g->buildingCount; i++)
    {
        Building *b = &ui->g->buildings[i];
        if (!b->alive || b->type == FARM)
            continue;
        int index;
        float width;
        Atlas atlas;
        Vector2 foot = ViewBuildingVisual(ui, b, &atlas, &index, &width);
        Rectangle src = atlas.rect[index];
        if (src.width <= 0)
            continue;
        float height = width * src.height / src.width;
        float d = b->x + b->y;
        if (d >= depth &&
            CheckCollisionPointRec(mouse, (Rectangle){foot.x - width / 2, foot.y - height, width, height}))
        {
            best = i;
            depth = d;
        }
    }
    return best;
}
static void Select(ViewContext *ui, int id, bool toggle)
{
    if (id < 0)
        return;
    Citizen *c = &ui->g->citizens[id];
    if (c->owner || !c->alive)
        return;
    for (int i = 0; i < ui->selectionCount; i++)
        if (ui->selection[i].slot == id)
        {
            if (toggle)
            {
                ui->selection[i] = ui->selection[--ui->selectionCount];
            }
            return;
        }
    if (ui->selectionCount < MAX_CITIZENS)
        ui->selection[ui->selectionCount++] = GameCitizenRef(ui->g, id);
}
static UnitOrder ContextOrder(ViewContext *ui)
{
    GameState *g = ui->g;
    int x = ui->hoverTile % MAP_SIZE, y = ui->hoverTile / MAP_SIZE;
    UnitOrder o = {.type = ORDER_MOVE, .x = x, .y = y};
    int person = PersonAt(ui), building = BuildingAt(ui);
    if (person >= 0 && g->citizens[person].owner)
    {
        o.type = ORDER_ATTACK;
        o.targetKind = TARGET_CITIZEN;
        o.target = GameCitizenRef(g, person);
    }
    else if (building >= 0)
    {
        Building *b = &g->buildings[building];
        o.targetKind = TARGET_BUILDING;
        o.target = GameBuildingRef(g, building);
        o.x = b->x;
        o.y = b->y;
        o.type = b->owner ? ORDER_ATTACK
                 : (!b->complete || (g->factions[0].advancing && g->factions[0].center == building))
                     ? ORDER_BUILD
                 : b->hp < GameBuildingMaxHealth(g, b)                            ? ORDER_REPAIR
                 : (b->type == FARM || b->type == WORKSHOP || b->type == SMELTER) ? ORDER_WORK
                                                                                  : ORDER_DELIVER;
    }
    else if (g->tiles[ui->hoverTile].node)
        o.type = ORDER_GATHER;
    return o;
}
void ViewInput(ViewContext *ui, float dt)
{
    GameState *g = ui->g;
    for (int i = 0; i < ui->selectionCount;)
        if (!GameCitizenValid(g, ui->selection[i]))
            ui->selection[i] = ui->selection[--ui->selectionCount];
        else
            i++;
    if (ui->selected >= 0 && (!g->buildings[ui->selected].alive ||
                              g->buildings[ui->selected].generation != ui->selectedGeneration))
        ui->selected = -1;
    ui->selectedCitizen = ui->selectionCount ? ui->selection[0].slot : -1;
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (ui->help)
            ui->help = false;
        else if (ui->menu)
        {
            ui->menu = false;
            ui->confirmNew = false;
        }
        else if (ui->build >= 0 || ui->orderMode)
        {
            ui->build = -1;
            ui->orderMode = 0;
        }
        else if (ui->buildMenu)
            ui->buildMenu = false;
        else if (ui->commandPage)
            ui->commandPage = 0;
        else if (ViewPanelOpen(ui))
            ui->panel = -1;
        else
            ui->menu = !ui->menu;
    }
    if (IsKeyPressed(KEY_H))
        ui->help = !ui->help;
    if (ui->menu || ui->help || g->lost || (!ui->winSeen && g->won))
    {
        ui->dragging = false;
        return;
    }
    if (IsKeyPressed(KEY_SPACE))
    {
        if (ui->speed)
        {
            ui->lastSpeed = ui->speed;
            ui->speed = 0;
        }
        else
            ui->speed = ui->lastSpeed;
    }
    if (IsKeyPressed(KEY_F1))
        ui->speed = 1;
    if (IsKeyPressed(KEY_F2))
        ui->speed = 2;
    if (IsKeyPressed(KEY_F3))
        ui->speed = 4;
    if (IsKeyPressed(KEY_F5))
        ViewSave(ui);
    if (IsKeyPressed(KEY_F9))
        ViewLoad(ui, ui->savePath);
    if (IsKeyPressed(KEY_O))
        ViewTogglePanel(ui, 0);
    if (IsKeyPressed(KEY_W))
        ViewTogglePanel(ui, 1);
    if (IsKeyPressed(KEY_R))
        ViewTogglePanel(ui, 2);
    if (IsKeyPressed(KEY_F))
        ToggleBorderlessWindowed();
    if (ViewPanelOpen(ui))
    {
        ui->dragging = false;
        return;
    }
    if (!ViewSelectedWorkers(ui))
    {
        ui->buildMenu = false;
        ui->build = -1;
    }
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT),
         control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    for (int k = 0; k < 10; k++)
        if (IsKeyPressed(KEY_ZERO + k))
        {
            if (control)
            {
                ui->groupCount[k] = ui->selectionCount;
                memcpy(ui->groups[k], ui->selection, (size_t)ui->selectionCount * sizeof(EntityRef));
            }
            else
            {
                ui->selectionCount = 0;
                for (int i = 0; i < ui->groupCount[k]; i++)
                    if (GameCitizenValid(g, ui->groups[k][i]))
                        ui->selection[ui->selectionCount++] = ui->groups[k][i];
                ui->selected = -1;
                ui->panel = -1;
                ui->buildMenu = false;
                ui->build = -1;
                ui->orderMode = 0;
                ui->commandPage = 0;
            }
        }
    if (IsKeyPressed(KEY_B))
        ViewBuildToggle(ui);
    if (IsKeyPressed(KEY_G))
        ui->grid = !ui->grid;
    if (IsKeyPressed(KEY_A) && ui->selectionCount)
        ui->orderMode = ORDER_ATTACK_MOVE;
    if (IsKeyPressed(KEY_D) && ui->selectionCount)
        ui->orderMode = ORDER_DEFEND;
    if (IsKeyPressed(KEY_L) && ui->selectionCount)
        ui->orderMode = ORDER_PLUNDER;
    if (ui->orderMode)
    {
        ui->build = -1;
        ui->buildMenu = false;
    }
    if (IsKeyPressed(KEY_S))
        ViewIssueSelected(ui, CMD_STOP, 0, (UnitOrder){0}, false);
    if (IsKeyPressed(KEY_X))
        ViewHoldSelected(ui);
    if (IsKeyPressed(KEY_T))
        ViewIssueSelected(
            ui, CMD_ORDER, 0,
            (UnitOrder){.type = ORDER_RETREAT, .x = g->factions[0].rallyX, .y = g->factions[0].rallyY},
            false);
    if (IsKeyPressed(KEY_HOME))
    {
        ui->camera.target = ViewIso(23, 23);
        ui->camera.zoom = .86f;
    }
    if (IsKeyPressed(KEY_M))
    {
        ui->camera.target = ViewIso(48, 48);
        ui->camera.zoom = .16f;
    }
    if (ViewWorldHit(ui))
    {
        float wheel = GetMouseWheelMove();
        if (wheel)
        {
            Vector2 before = GetScreenToWorld2D(ui->mouse, ui->camera);
            ui->camera.zoom = Clamp(ui->camera.zoom * powf(1.12f, wheel), .14f, 2.3f);
            Vector2 after = GetScreenToWorld2D(ui->mouse, ui->camera);
            ui->camera.target = Vector2Add(ui->camera.target, Vector2Subtract(before, after));
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
        {
            Vector2 d = GetMouseDelta();
            float scale = ViewMinF(GetScreenWidth() / (float)SCREEN_W, GetScreenHeight() / (float)SCREEN_H);
            ui->camera.target.x -= d.x / (scale * ui->camera.zoom);
            ui->camera.target.y -= d.y / (scale * ui->camera.zoom);
        }
    }
    ui->camera.target.x += (IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT)) * dt * 550 / ui->camera.zoom;
    ui->camera.target.y += (IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP)) * dt * 550 / ui->camera.zoom;
    ui->camera.target.x = Clamp(ui->camera.target.x, -MAP_SIZE * 32, MAP_SIZE * 32);
    ui->camera.target.y = Clamp(ui->camera.target.y, 0, MAP_SIZE * 32);
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && ViewWorldHit(ui) && ui->hoverTile >= 0)
    {
        if (ui->build >= 0 || ui->orderMode)
        {
            ui->build = -1;
            ui->orderMode = 0;
            return;
        }
        UnitOrder o = ContextOrder(ui);
        if (ui->selectionCount)
            ViewIssueSelected(ui, CMD_ORDER, 0, o, shift);
        else if (ui->selected >= 0 && g->buildings[ui->selected].owner == 0 &&
                 g->buildings[ui->selected].type == HEARTH)
        {
            char reason[256];
            bool ok = GameCommandApply(g, (GameCommand){.type = CMD_RALLY, .a = o.x, .b = o.y, .order = o},
                                       reason, sizeof(reason));
            ViewToast(ui, ok ? "Rally instruction set for new recruits." : reason);
        }
        return;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ViewWorldHit(ui) && ui->hoverTile >= 0)
    {
        int x = ui->hoverTile % MAP_SIZE, y = ui->hoverTile / MAP_SIZE;
        if (ui->build >= 0)
        {
            if (ViewCommand(ui, CMD_BUILD, ui->build, x, y))
            {
                int id = g->tiles[ui->hoverTile].occupied;
                ViewIssueSelected(ui, CMD_ORDER, 0,
                                  (UnitOrder){.type = ORDER_BUILD,
                                              .targetKind = TARGET_BUILDING,
                                              .target = GameBuildingRef(g, id),
                                              .x = x,
                                              .y = y},
                                  shift);
                ui->selected = id;
                ui->selectedGeneration = g->buildings[id].generation;
                if (!shift)
                    ui->build = -1;
            }
            return;
        }
        if (ui->orderMode)
        {
            UnitOrder o = ContextOrder(ui);
            if (ui->orderMode == VIEW_ORDER_RALLY)
            {
                char why[256];
                bool ok = GameCommandApply(
                    g, (GameCommand){.type = CMD_RALLY, .a = o.x, .b = o.y, .order = o}, why, sizeof(why));
                ViewToast(ui, ok ? "Rally instruction set for new recruits." : why);
            }
            else
            {
                o.type = ui->orderMode;
                if (o.type == ORDER_MOVE || o.type == ORDER_ATTACK_MOVE || o.type == ORDER_DEFEND ||
                    o.type == ORDER_GATHER)
                    o.targetKind = TARGET_TILE;
                ViewIssueSelected(ui, CMD_ORDER, 0, o, shift);
            }
            ui->orderMode = 0;
            return;
        }
        ui->dragging = true;
        ui->dragStart = ui->mouse;
    }
    if (ui->dragging && !IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        ui->dragging = false;
        float distance = Vector2Distance(ui->mouse, ui->dragStart);
        if (distance > 6)
        {
            // Test against the HUD that was visible when the drag ended. Applying the
            // selection during this pass would change its footprint under later units.
            int hits[MAX_CITIZENS], count = 0;
            Rectangle rect = {fminf(ui->mouse.x, ui->dragStart.x), fminf(ui->mouse.y, ui->dragStart.y),
                              fabsf(ui->mouse.x - ui->dragStart.x), fabsf(ui->mouse.y - ui->dragStart.y)};
            for (int i = 0; i < g->citizenCount; i++)
            {
                Citizen *c = &g->citizens[i];
                Vector2 p = GetWorldToScreen2D(ViewIso(c->x / 256.f + .5f, c->y / 256.f + .5f), ui->camera);
                if (CheckCollisionPointRec(p, rect) && ViewWorldPointHit(ui, p))
                    hits[count++] = i;
            }
            if (!shift)
                ui->selectionCount = 0;
            for (int i = 0; i < count; i++)
                Select(ui, hits[i], false);
            ui->selected = -1;
        }
        else
        {
            if (!ViewWorldHit(ui))
                return;
            if (!shift)
                ui->selectionCount = 0;
            int person = PersonAt(ui);
            if (person >= 0 && !g->citizens[person].owner)
            {
                Select(ui, person, shift);
                ui->selected = -1;
            }
            else
            {
                ViewSelectBuilding(ui, BuildingAt(ui));
            }
        }
        ui->selectedCitizen = ui->selectionCount ? ui->selection[0].slot : -1;
        ui->panel = -1;
        ui->buildMenu = false;
        ui->commandPage = 0;
    }
}
