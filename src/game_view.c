#include "game_view.h"
#include "view/actions.h"
#include "view/assets.h"
#include "view/context.h"
#include "view/hud.h"
#include "view/input.h"
#include "view/layout.h"
#include "view/panels.h"
#include "view/screens.h"
#include "view/selection.h"
#include "view/widgets.h"
#include "view/world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void Draw(ViewContext *ui)
{
    BeginTextureMode(ui->canvas);
    ClearBackground(INK);
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    DrawRectangleGradientV(0, WORLD_TOP, SCREEN_W, SCREEN_H - WORLD_TOP, (Color){114, 140, 131, 255},
                           (Color){71, 98, 78, 255});
    EndMode2D();
    ViewWorldDraw(ui);
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    ui->clickConsumed = false;
    ui->allowInput = !ui->menu && !ui->help && !ui->g->lost && (!ui->g->won || ui->winSeen);
    ViewHeader(ui);
    ui->allowInput =
        !ui->menu && !ui->help && !ViewPanelOpen(ui) && !ui->g->lost && (!ui->g->won || ui->winSeen);
    ViewSelectionDraw(ui);
    ViewWorldHUD(ui);
    ViewPanelDraw(ui);
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
    // Draw after composition so the hand also remains visible in letterboxing.
    if (ui->showCursor)
    {
        BeginMode2D((Camera2D){.offset = {dst.x, dst.y}, .zoom = scale});
        ViewCursor(ui);
        EndMode2D();
    }
    EndDrawing();
}

int GameViewRun(GameState *game, int argc, char **argv, bool returning)
{
    const char *capture = NULL, *load = NULL;
    bool play = false;
    int capturePanel = -1, captureBuild = -1, captureBuilding = -1, benchmarkFrames = 0;
    bool captureOverview = false, captureSelection = false, captureSoldiers = false;
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--play"))
            play = true;
        else if (!strcmp(argv[i], "--game-capture") && i + 1 < argc)
            capture = argv[++i];
        else if (!strcmp(argv[i], "--load") && i + 1 < argc)
            load = argv[++i];
        else if (!strcmp(argv[i], "--capture-panel") && i + 1 < argc)
            capturePanel = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--capture-building") && i + 1 < argc)
            captureBuilding = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--capture-build") && i + 1 < argc)
            captureBuild = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--capture-overview"))
            captureOverview = true;
        else if (!strcmp(argv[i], "--capture-soldiers"))
            captureSoldiers = true;
        else if (!strcmp(argv[i], "--capture-selection"))
            captureSelection = true;
        else if (!strcmp(argv[i], "--benchmark-frames") && i + 1 < argc)
            benchmarkFrames = atoi(argv[++i]);
        else
        {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return 2 + 1;
        }
    }
    if (capturePanel < -1 || capturePanel > 4 || captureBuilding < -1 || captureBuilding >= BUILDING_COUNT ||
        captureBuild < -1 || captureBuild >= BUILDING_COUNT || benchmarkFrames < 0 || benchmarkFrames > 36000)
        return 3;
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
    ui->panel = -1;
    ui->menu = !play && !capture;
    ui->camera = (Camera2D){.offset = {SCREEN_W / 2.0f, (WORLD_TOP + SCREEN_H) / 2.0f},
                            .target = ViewIso(23, 23),
                            .rotation = 0,
                            .zoom = .86f};
    ViewSaveFolder(ui);
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI |
                   (capture ? FLAG_WINDOW_HIDDEN : FLAG_VSYNC_HINT));
    InitWindow(1440, 900, "Greenfield - Tribe to Country");
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
        fprintf(stderr, "The game artwork, fonts, or render target could not be loaded. Keep assets beside "
                        "the executable.\n");
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
    {
        ui->toastTime = 0;
        ui->panel = capturePanel == 4 ? -1 : capturePanel;
        ui->build = captureBuild;
        if (captureOverview)
        {
            ui->camera.target = ViewIso(48, 48);
            ui->camera.zoom = .16f;
        }
        ui->buildMenu = captureBuild >= 0;
        if (captureSelection || captureSoldiers || captureBuild >= 0)
            for (int i = 0; i < game->citizenCount && ui->selectionCount < 8; i++)
                if (game->citizens[i].alive && !game->citizens[i].owner &&
                    (!captureSoldiers || game->citizens[i].unitType))
                    ui->selection[ui->selectionCount++] = GameCitizenRef(game, i);
        if (captureBuilding >= 0)
            for (int i = 0; i < game->buildingCount; i++)
                if (game->buildings[i].alive && !game->buildings[i].owner &&
                    game->buildings[i].type == captureBuilding)
                {
                    ViewSelectBuilding(ui, i);
                    break;
                }
        if (benchmarkFrames)
            SetTargetFPS(0);
    }
    int rendered = 0;
    double renderTotal = 0, renderMax = 0;
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
        ui->showCursor = !capture && IsCursorOnScreen() && IsWindowFocused();
        if (ui->showCursor && !IsCursorHidden())
            HideCursor();
        else if (!ui->showCursor && IsCursorHidden())
            ShowCursor();
        Vector2 tile = ViewTileAt(ui, ui->mouse);
        ui->hoverTile = GameTile((int)floorf(tile.x), (int)floorf(tile.y));
        if (!IsWindowFocused())
            ViewCancelDrag(ui);
        if (!capture && IsWindowFocused())
            ViewInput(ui, dt);
        else if (captureBuild >= 0)
            ui->hoverTile = GameTile(26, 27);
        if (capture && benchmarkFrames)
            GameAdvance(game, 1.0 / 60, 1, &ui->accumulator);
        if (!ui->menu && !ui->help && !game->lost && (!game->won || ui->winSeen) && !capture)
        {
            GameAdvance(game, frameSeconds, ui->speed, &ui->accumulator);
            if (!ui->autosaveBlocked && game->tick - ui->lastAutosave >= 1200)
                ViewSave(ui);
        }
        double renderStart = GetTime();
        Draw(ui);
        double elapsed = GetTime() - renderStart;
        if (rendered >= 10)
        {
            renderTotal += elapsed;
            renderMax = fmax(renderMax, elapsed);
        }
        rendered++;
        if (capture)
        {
            if (rendered < benchmarkFrames)
                continue;
            if (benchmarkFrames > 10)
                printf("Render benchmark: %d frames, %.2f ms mean, %.2f ms max; %d living citizens.\n",
                       rendered - 10, 1000 * renderTotal / (rendered - 10), renderMax * 1000,
                       GamePopulationFor(game, 0) + GamePopulationFor(game, 1));
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
    ShowCursor();
    CloseWindow();
    return result;
}
