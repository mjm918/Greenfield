#ifndef GREENFIELD_VIEW_CONTEXT_H
#define GREENFIELD_VIEW_CONTEXT_H
#include "game.h"
#include "raylib.h"

// Logical screen coordinates. Only world/composition rasterization applies RENDER_SCALE.
#define SCREEN_W 1440
#define SCREEN_H 900
#define RENDER_SCALE 2
#define PANEL_X 1130
#define TILE_W 64.0f
#define TILE_H 32.0f

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
} ViewContext;
#endif
