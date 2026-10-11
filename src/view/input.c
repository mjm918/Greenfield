#include "input.h"
#include "actions.h"
#include "animation.h"
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
        float d = b->x + b->y;
        if (d >= depth && CheckCollisionPointRec(mouse, ViewBuildingBounds(ui, b)))
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
    ui->selectedResource = 0;
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
    if (ui->orderMode == ORDER_GATHER)
    {
        int resource = ViewResourceAt(ui);
        return (UnitOrder){.type = ORDER_GATHER,
                           .x = resource >= 0 ? resource % MAP_SIZE : x,
                           .y = resource >= 0 ? resource / MAP_SIZE : y};
    }
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
    else
    {
        int resource = ViewResourceAt(ui);
        if (resource >= 0)
        {
            o.type = ORDER_GATHER;
            o.x = resource % MAP_SIZE;
            o.y = resource / MAP_SIZE;
        }
    }
    return o;
}
static void ClampCamera(ViewContext *ui)
{
    ui->camera.target.x = Clamp(ui->camera.target.x, -MAP_SIZE * 32, MAP_SIZE * 32);
    ui->camera.target.y = Clamp(ui->camera.target.y, 0, MAP_SIZE * 32);
}
static void PanCamera(ViewContext *ui)
{
    Vector2 distance = Vector2Subtract(ui->mouse, ui->dragStart);
    ui->camera.target = Vector2Subtract(ui->panOrigin, Vector2Scale(distance, 1 / ui->camera.zoom));
    ClampCamera(ui);
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
    if ((ui->movingBuilding || ui->confirmDemolish) &&
        (!GameBuildingValid(g, ui->editBuilding) || ui->selectionCount ||
         ui->selected != ui->editBuilding.slot || ui->selectedGeneration != ui->editBuilding.generation))
        ViewCancelBuildingEdit(ui);
    ui->selectedCitizen = ui->selectionCount ? ui->selection[0].slot : -1;
    if (ui->selectedResource && ViewSelectedResource(ui) < 0)
        ui->selectedResource = 0;
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (ui->help)
            ui->help = false;
        else if (ui->menu)
        {
            ui->menu = false;
            ui->confirmNew = false;
        }
        else if (ui->movingBuilding || ui->confirmDemolish)
            ViewCancelBuildingEdit(ui);
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
        else if (ui->selectedResource)
            ui->selectedResource = 0;
        else
            ui->menu = !ui->menu;
    }
    if (IsKeyPressed(KEY_H))
        ui->help = !ui->help;
    if (ui->menu || ui->help || g->lost || (!ui->winSeen && g->won))
    {
        ViewCancelDrag(ui);
        ViewCancelBuildingEdit(ui);
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
        ViewCancelDrag(ui);
        return;
    }
    // A minimap press owns the gesture until release, even outside its rectangle.
    if (ui->mapDragging)
        return;
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
                ViewCancelBuildingEdit(ui);
                ui->selected = -1;
                ui->selectedResource = 0;
                ui->panel = -1;
                ui->buildMenu = false;
                ui->build = -1;
                ui->orderMode = 0;
                ui->commandPage = 0;
            }
        }
    if (IsKeyPressed(KEY_B))
        ViewBuildToggle(ui);
    if (IsKeyPressed(KEY_Q) && !ui->confirmDemolish &&
        (ui->build >= 0 || ui->movingBuilding ||
         (!ui->selectionCount && ui->selected >= 0 && !g->buildings[ui->selected].owner)))
        ViewRotateBuilding(ui);
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
        if (wheel && !ui->dragging && !ui->panning)
        {
            Vector2 before = GetScreenToWorld2D(ui->mouse, ui->camera);
            ui->camera.zoom = Clamp(ui->camera.zoom * powf(1.12f, wheel), .14f, 2.3f);
            Vector2 after = GetScreenToWorld2D(ui->mouse, ui->camera);
            ui->camera.target = Vector2Add(ui->camera.target, Vector2Subtract(before, after));
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
        {
            ViewCancelDrag(ui);
            ui->panning = true;
            ui->panButton = MOUSE_BUTTON_MIDDLE;
            ui->dragStart = ui->mouse;
            ui->panOrigin = ui->camera.target;
        }
    }
    if (ui->panning && ui->panButton == MOUSE_BUTTON_MIDDLE)
    {
        PanCamera(ui);
        if (!IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
            ViewCancelDrag(ui);
        return;
    }
    ui->camera.target.x += (IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT)) * dt * 550 / ui->camera.zoom;
    ui->camera.target.y += (IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP)) * dt * 550 / ui->camera.zoom;
    ClampCamera(ui);
    if (ui->dragging && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
    {
        ViewCancelDrag(ui);
        return;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && ViewWorldHit(ui) && ui->hoverTile >= 0)
    {
        if (ui->movingBuilding || ui->confirmDemolish)
        {
            ViewCancelBuildingEdit(ui);
            return;
        }
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
        if (ui->movingBuilding)
        {
            char why[256];
            bool ok = GameCommandApply(g,
                                      (GameCommand){.type = CMD_MOVE_BUILDING,
                                                    .entity = ui->editBuilding,
                                                    .a = x, .b = y, .rotation = ui->buildRotation},
                                      why, sizeof(why));
            ViewToast(ui, ok ? "Building moved. Health, progress and queues preserved." : why);
            if (ok)
                ViewCancelBuildingEdit(ui);
            return;
        }
        if (ui->confirmDemolish)
            ViewCancelBuildingEdit(ui);
        if (ui->build >= 0)
        {
            char why[256];
            bool ok = GameCommandApply(g,
                                      (GameCommand){.type = CMD_BUILD, .a = ui->build, .b = x, .c = y,
                                                    .rotation = ui->buildRotation}, why, sizeof(why));
            if (!ok)
                ViewToast(ui, why);
            if (ok)
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
            bool accepted;
            if (ui->orderMode == VIEW_ORDER_RALLY)
            {
                char why[256];
                bool ok = GameCommandApply(
                    g, (GameCommand){.type = CMD_RALLY, .a = o.x, .b = o.y, .order = o}, why, sizeof(why));
                ViewToast(ui, ok ? "Rally instruction set for new recruits." : why);
                accepted = ok;
            }
            else
            {
                o.type = ui->orderMode;
                if (o.type == ORDER_MOVE || o.type == ORDER_ATTACK_MOVE || o.type == ORDER_DEFEND ||
                    o.type == ORDER_GATHER)
                    o.targetKind = TARGET_TILE;
                accepted = ViewIssueSelected(ui, CMD_ORDER, 0, o, shift) > 0;
            }
            if (accepted && !(ui->orderMode == ORDER_GATHER && shift))
                ui->orderMode = 0;
            return;
        }
        ui->dragging = true;
        ui->dragStart = ui->mouse;
        ui->dragAdd = shift;
        ui->panCandidate = !shift && PersonAt(ui) < 0 && BuildingAt(ui) < 0;
        ui->panOrigin = ui->camera.target;
    }
    if (ui->dragging && ui->panCandidate && (ui->panning || Vector2Distance(ui->mouse, ui->dragStart) > 6))
    {
        ui->panning = true;
        ui->panButton = MOUSE_BUTTON_LEFT;
        PanCamera(ui);
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            ViewCancelDrag(ui);
        return;
    }
    if (ui->dragging && !IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        shift = ui->dragAdd;
        ViewCancelDrag(ui);
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
            ui->selectedResource = 0;
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
                if (ui->selected < 0)
                    ui->selectedResource = ViewResourceAt(ui) + 1;
            }
        }
        ui->selectedCitizen = ui->selectionCount ? ui->selection[0].slot : -1;
        ui->panel = -1;
        ui->buildMenu = false;
        ui->commandPage = 0;
    }
}
