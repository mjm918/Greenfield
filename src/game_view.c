#include "game_view.h"
#include "view/context.h"
#include "view/widgets.h"
#include "view/assets.h"
#include "view/world.h"
#include "view/panels.h"
#include "view/hud.h"
#include "view/actions.h"
#include "view/screens.h"
#include "view/input.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void Draw(ViewContext *ui)
{
    BeginTextureMode(ui->canvas);
    ClearBackground(INK);
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    DrawRectangleGradientV(0, 86, PANEL_X, SCREEN_H - 250, (Color){114, 140, 131, 255},
                           (Color){71, 98, 78, 255});
    EndMode2D();
    ViewWorldDraw(ui);
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    ui->allowInput = !ui->menu && !ui->help && !ui->g->lost && (!ui->g->won || ui->winSeen);
    ViewHeader(ui);
    ViewWorldHUD(ui);
    ViewPanelDraw(ui);
    ViewBuildPalette(ui);
    if (ui->menu)
        ViewMenu(ui);
    if (ui->help)
        ViewGuide(ui);
    if (!ui->menu && (ui->g->lost || (!ui->winSeen && ui->g->won)))
        ViewEnding(ui);
    EndMode2D();
    EndTextureMode();
    float scale = ViewMinF(GetScreenWidth() / (float)SCREEN_W, GetScreenHeight() / (float)SCREEN_H);
    Rectangle dst = {(GetScreenWidth() - SCREEN_W * scale) / 2, (GetScreenHeight() - SCREEN_H * scale) / 2,
                     SCREEN_W * scale, SCREEN_H * scale};
    BeginDrawing();
    ClearBackground(INK);
    DrawTexturePro(ui->canvas.texture,
                   (Rectangle){0, 0, ui->canvas.texture.width, -ui->canvas.texture.height}, dst,
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
    // Owned here for the entire view session, including Reference Viewer returns.
    // Modules receive this context explicitly and hold no mutable global UI state.
    static ViewContext context;
    ViewContext *ui = &context;
    bool autosaveBlocked = returning && ui->autosaveBlocked;
    memset(ui, 0, sizeof(*ui));
    ui->autosaveBlocked = autosaveBlocked;
    ui->g = game;
    ui->selected = ui->selectedCitizen = ui->build = ui->areaRole = ui->hoverTile = -1;
    ui->speed = ui->lastSpeed = 1;
    ui->menu = !play && !capture;
    ui->camera =
        (Camera2D){.offset = {PANEL_X / 2.0f, 421}, .target = ViewIso(23, 23), .rotation = 0, .zoom = .86f};
    ViewSaveFolder(ui);
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
        float scale = ViewMinF(1, ViewMinF(width / (float)SCREEN_W, height / (float)SCREEN_H));
        SetWindowSize((int)(SCREEN_W * scale), (int)(SCREEN_H * scale));
    }
    bool assetsReady = ViewAssetsLoad(ui);
    int result = 0;
    if (!assetsReady)
    {
        fprintf(stderr, "The game artwork, fonts, or render target could not be loaded. Keep assets beside the executable.\n");
        result = 1;
        ui->quit = true;
    }
    if (load && !returning)
    {
        if (!ViewLoad(ui, load))
        {
            fprintf(stderr, "%s\n", ui->toast);
            result = 1;
            ui->quit = true;
        }
        else
        {
            ui->menu = false;
            ui->speed = 1;
        }
    }
    ui->lastAutosave = game->tick;
    ui->winSeen = game->won;
    if (capture)
        ui->toastTime = 0;
    while (!WindowShouldClose() && !ui->quit && !ui->reference)
    {
        double frameSeconds = GetFrameTime();
        // Limit camera jumps after a stall without dropping simulation time.
        float dt = (float)frameSeconds;
        if (dt > .25f)
            dt = .25f;
        ui->time += dt;
        ui->toastTime = fmaxf(0, ui->toastTime - dt);
        float scale = ViewMinF(GetScreenWidth() / (float)SCREEN_W, GetScreenHeight() / (float)SCREEN_H);
        Vector2 raw = GetMousePosition();
        ui->mouse = (Vector2){(raw.x - (GetScreenWidth() - SCREEN_W * scale) / 2) / scale,
                             (raw.y - (GetScreenHeight() - SCREEN_H * scale) / 2) / scale};
        Vector2 tile = ViewTileAt(ui, ui->mouse);
        ui->hoverTile = GameTile((int)floorf(tile.x), (int)floorf(tile.y));
        ViewInput(ui, dt);
        if (!ui->menu && !ui->help && !game->lost && (!game->won || ui->winSeen) && !capture)
        {
            GameAdvance(game, frameSeconds, ui->speed, &ui->accumulator);
            if (!ui->autosaveBlocked && game->tick - ui->lastAutosave >= 1200)
                ViewSave(ui);
        }
        Draw(ui);
        if (capture)
        {
            Image img = LoadImageFromTexture(ui->canvas.texture);
            ImageFlipVertical(&img);
            result = ExportImage(img, capture) ? 0 : 1;
            UnloadImage(img);
            break;
        }
    }
    if (ui->reference)
        result = 2;
    ViewAssetsUnload(ui);
    CloseWindow();
    return result;
}
