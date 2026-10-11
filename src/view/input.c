#include "input.h"
#include "actions.h"
#include "context.h"
#include "raymath.h"
#include "widgets.h"
#include "world.h"
#include <math.h>

static bool WorldHit(ViewContext *ui)
{
    if (ui->g->foodWarning || ui->g->coldWarning || ui->g->raidWarning || ui->g->raidActive)
        if (ViewHit(ui, (Rectangle){22, 232, 536, 39}))
            return false;
    return ui->mouse.x < PANEL_X && ui->mouse.y > 218 && ui->mouse.y < 630 &&
           !ViewHit(ui, (Rectangle){928, 586, 179, 127});
}

void ViewInput(ViewContext *ui, float dt)
{
    GameState *g = ui->g;
    if (IsKeyPressed(KEY_ESCAPE))
    {
        if (ui->help)
            ui->help = false;
        else if (ui->build >= 0 || ui->areaRole >= 0)
        {
            ui->build = -1;
            ui->areaRole = -1;
        }
        else
            ui->menu = !ui->menu;
    }
    if (IsKeyPressed(KEY_H))
        ui->help = !ui->help;
    if (ui->menu || ui->help || g->lost || (!ui->winSeen && g->won))
        return;
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
    if (IsKeyPressed(KEY_ONE))
        ui->speed = 1;
    if (IsKeyPressed(KEY_TWO))
        ui->speed = 2;
    if (IsKeyPressed(KEY_FOUR))
        ui->speed = 4;
    if (IsKeyPressed(KEY_F5))
        ViewSave(ui);
    if (IsKeyPressed(KEY_F9))
        ViewLoad(ui, ui->savePath);
    if (IsKeyPressed(KEY_O))
        ui->panel = 0;
    if (IsKeyPressed(KEY_W))
        ui->panel = 1;
    if (IsKeyPressed(KEY_R))
        ui->panel = 2;
    if (IsKeyPressed(KEY_B))
        ui->build = ui->build >= 0 ? -1 : SHELTER;
    if (IsKeyPressed(KEY_G))
        ui->grid = !ui->grid;
    if (IsKeyPressed(KEY_HOME))
    {
        ui->camera.target = ViewIso(23, 23);
        ui->camera.zoom = .86f;
    }
    if (IsKeyPressed(KEY_M))
    {
        ui->camera.target = ViewIso(24, 24);
        ui->camera.zoom = .32f;
    }
    if (IsKeyPressed(KEY_F))
        ToggleBorderlessWindowed();
    if (ui->mouse.x < PANEL_X)
    {
        float wheel = GetMouseWheelMove();
        if (wheel)
        {
            Vector2 before = GetScreenToWorld2D(ui->mouse, ui->camera);
            ui->camera.zoom = Clamp(ui->camera.zoom * powf(1.12f, wheel), .3f, 2.3f);
            Vector2 after = GetScreenToWorld2D(ui->mouse, ui->camera);
            ui->camera.target = Vector2Add(ui->camera.target, Vector2Subtract(before, after));
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        {
            Vector2 d = GetMouseDelta();
            float scale = ViewMinF(GetScreenWidth() / (float)SCREEN_W, GetScreenHeight() / (float)SCREEN_H);
            ui->camera.target.x -= d.x / (scale * ui->camera.zoom);
            ui->camera.target.y -= d.y / (scale * ui->camera.zoom);
        }
    }
    ui->camera.target.x += (IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT)) * dt * 550 / ui->camera.zoom;
    ui->camera.target.y += (IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP)) * dt * 550 / ui->camera.zoom;
    ui->camera.target.x = Clamp(ui->camera.target.x, -1450, 1450);
    ui->camera.target.y = Clamp(ui->camera.target.y, 0, 1536);
    if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || !WorldHit(ui) || ui->hoverTile < 0)
        return;
    int x = ui->hoverTile % MAP_SIZE, y = ui->hoverTile / MAP_SIZE;
    if (ui->build >= 0)
    {
        if (ViewCommand(ui, CMD_BUILD, ui->build, x, y))
        {
            ui->selected = g->buildingCount - 1;
            ViewToast(ui, "Foundation placed. Builders will bring it to life.");
            if (!IsKeyDown(KEY_LEFT_SHIFT))
                ui->build = -1;
        }
        return;
    }
    if (ui->areaRole >= 0)
    {
        if (ui->areaRole == GUARD)
            ViewCommand(ui, CMD_RALLY, x, y, 0);
        else
            ViewCommand(ui, CMD_AREA, ui->areaRole, x, y);
        ui->areaRole = -1;
        return;
    }
    int nearest = -1;
    float dist = 30;
    for (int i = 0; i < g->citizenCount; i++)
        if (g->citizens[i].alive)
        {
            Vector2 p = GetWorldToScreen2D(
                ViewIso(g->citizens[i].x / 256.0f + .5f, g->citizens[i].y / 256.0f + .5f), ui->camera);
            p.y -= 14 * ui->camera.zoom;
            float d = Vector2Distance(ui->mouse, p);
            if (d < dist)
            {
                dist = d;
                nearest = i;
            }
        }
    // Use the rendered foot anchors and depth so roofs are selectable too.
    Vector2 world = GetScreenToWorld2D(ui->mouse, ui->camera);
    int building = g->tiles[ui->hoverTile].occupied;
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
        Vector2 foot = ViewBuildingVisual(b, &index, &width);
        Rectangle src = ui->objects.rect[index];
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
        ui->selectedCitizen = nearest;
        ui->selected = -1;
        ui->panel = 4;
        return;
    }
    if (x >= 37 && y < 13)
    {
        ui->panel = 3;
        return;
    }
    ui->selectedCitizen = -1;
    ui->selected = building;
    ui->panel = 4;
}
