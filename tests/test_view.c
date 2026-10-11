#include "game.h"
#include "view/actions.h"
#include "view/assets.h"
#include "view/context.h"
#include "view/hud.h"
#include "view/input.h"
#include "view/layout.h"
#include "view/panels.h"
#include "view/selection.h"
#include "view/world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int failures, checks, pressedKey, heldKey, pressedMouse = -1, heldMouse = -1;
static float wheel;
static Vector2 delta;
static bool fullscreen;
#define CHECK(x)                                                                                             \
    do                                                                                                       \
    {                                                                                                        \
        checks++;                                                                                            \
        if (!(x))                                                                                            \
        {                                                                                                    \
            fprintf(stderr, "FAIL view line %d: %s\n", __LINE__, #x);                                        \
            failures++;                                                                                      \
        }                                                                                                    \
    } while (0)
bool TestKeyPressed(int k)
{
    return k == pressedKey;
}
bool TestKeyDown(int k)
{
    return k == heldKey;
}
bool TestMousePressed(int b)
{
    return b == pressedMouse;
}
bool TestMouseDown(int b)
{
    return b == heldMouse;
}
float TestMouseWheel(void)
{
    return wheel;
}
Vector2 TestMouseDelta(void)
{
    return delta;
}
int TestScreenWidth(void)
{
    return SCREEN_W;
}
int TestScreenHeight(void)
{
    return SCREEN_H;
}
void TestFullscreen(void)
{
    fullscreen = !fullscreen;
}
static void Key(ViewContext *u, int key)
{
    pressedKey = key;
    ViewInput(u, 1.f / 60);
    pressedKey = 0;
}
static void Point(ViewContext *u, float x, float y)
{
    u->mouse = GetWorldToScreen2D(ViewIso(x, y), u->camera);
    u->hoverTile = GameTile((int)x, (int)y);
}
const char *TestApplicationDirectory(void)
{
    return VIEW_TEST_ROOT;
}
static void HudClick(ViewContext *u, float x, float y)
{
    u->mouse = (Vector2){x, y};
    pressedMouse = MOUSE_BUTTON_LEFT;
    u->clickConsumed = false;
    u->allowInput = !u->menu && !u->help;
    BeginTextureMode(u->canvas);
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    ViewHeader(u);
    u->allowInput = !u->menu && !u->help && !ViewPanelOpen(u);
    ViewSelectionDraw(u);
    ViewWorldHUD(u);
    ViewPanelDraw(u);
    EndMode2D();
    EndTextureMode();
    pressedMouse = -1;
}
static void HudRectClick(ViewContext *u, Rectangle r)
{
    HudClick(u, r.x + r.width / 2, r.y + r.height / 2);
}
static void RenderChecks(ViewContext *u)
{
    GameState *g = u->g;
    GameInit(g);
    u->panel = -1;
    u->selected = -1;
    u->allowInput = true;
    u->selection[0] = GameCitizenRef(g, 0);
    u->selectionCount = 1;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(SCREEN_W, SCREEN_H, "Greenfield HUD verification");
    CHECK(IsWindowReady());
    if (!IsWindowReady())
        return;
    CHECK(ViewAssetsLoad(u));
    // Opening Build must not activate the Hearth occupying the same position.
    HudRectClick(u, ViewCommandRect(u, 0));
    CHECK(u->buildMenu && u->build == -1);
    HudRectClick(u, ViewBuildRect(u, SHELTER));
    CHECK(u->build == SHELTER);
    HudRectClick(u, ViewBuildRect(u, BUILDING_COUNT));
    CHECK(!u->buildMenu && u->build == -1);
    // Training occupies the same cells as unit orders; opening it cannot issue one.
    HudRectClick(u, ViewCommandRect(u, 7));
    CHECK(u->commandPage == 1 && !u->orderMode);
    Key(u, KEY_ESCAPE);
    CHECK(u->commandPage == 0);
    HudRectClick(u, ViewCommandRect(u, 4));
    CHECK(u->panel == 1);
    HudClick(u, 1125, 190);
    CHECK(!ViewPanelOpen(u));
    // Genuine construction and labor provide enough housing for recruitment.
    g->factions[1].aiEnabled = 0;
    int placed = 0;
    for (int y = 17; y < 30 && placed < 3; y += 3)
        for (int x = 17; x < 30 && placed < 3; x += 3)
            if (GameCanPlace(g, SHELTER, x, y, NULL, 0))
                placed += GameCommandApply(g, (GameCommand){.type = CMD_BUILD, .a = SHELTER, .b = x, .c = y},
                                           NULL, 0);
    for (int i = 0; i < 8; i++)
        GameCommandApply(g, (GameCommand){.type = CMD_ROLE, .entity = GameCitizenRef(g, i), .a = BUILDER},
                         NULL, 0);
    for (int t = 0; t < 6000 && GameHousing(g) < 12; t++)
        GameTick(g);
    CHECK(GameHousing(g) >= 12);
    ViewSelectBuilding(u, g->factions[0].center);
    int food = g->factions[0].stock[FOOD];
    HudRectClick(u, ViewCommandRect(u, 0));
    Building *center = &g->buildings[u->selected];
    CHECK(center->queueCount == 1);
    CHECK(g->factions[0].stock[FOOD] == food - RECRUIT_COST[FOOD]);
    Rectangle queueItem = ViewQueueItemRect(u, 0);
    u->mouse = (Vector2){queueItem.x + 10, queueItem.y + 10};
    CHECK(!ViewWorldHit(u));
    // Cancelling the only queue item must not read its now-empty duration.
    HudRectClick(u, ViewQueueItemRect(u, 0));
    CHECK(center->queueCount == 0 && g->factions[0].stock[FOOD] == food);
    CHECK(ViewProductionRect(u).width == 0 && ViewWorldHit(u));
    HudRectClick(u, ViewCommandRect(u, 2));
    CHECK(u->orderMode == VIEW_ORDER_RALLY);
    u->orderMode = 0;
    u->selection[0] = GameCitizenRef(g, 0);
    u->selectionCount = 1;
    u->selected = -1;
    g->citizens[0].unitType = SPEARMAN;
    HudRectClick(u, ViewCommandRect(u, 1));
    CHECK(u->orderMode == ORDER_ATTACK_MOVE && !u->buildMenu);
    HudRectClick(u, ViewCommandRect(u, 4));
    CHECK(g->citizens[0].orders[0].type == ORDER_HOLD);
    // Production controls are blocked while the ledger is open.
    ViewSelectBuilding(u, g->factions[0].center);
    ViewTogglePanel(u, 0);
    food = g->factions[0].stock[FOOD];
    HudRectClick(u, ViewCommandRect(u, 0));
    CHECK(g->factions[0].stock[FOOD] == food && center->queueCount == 0);
    int speed = u->speed;
    HudClick(u, 1110, 24);
    CHECK(u->speed != speed && ViewPanelOpen(u));
    ViewTogglePanel(u, 0);
    // The minimap remains interactive, without starting a world selection underneath.
    Vector2 cameraBefore = u->camera.target;
    HudRectClick(u, ViewMapRect());
    CHECK(cameraBefore.x != u->camera.target.x || cameraBefore.y != u->camera.target.y);
    CHECK(!ViewWorldHit(u) && !u->dragging);
    // Measure the longest fixed header labels using the actual packaged font.
    for (int i = 0; i < AGE_COUNT; i++)
        CHECK(MeasureTextEx(u->font, AGES[i].name, 16, .5f).x <= 124);
    ViewAssetsUnload(u);
    CloseWindow();
}
static void CompactSelectionChecks(ViewContext *u)
{
    GameState *g = u->g;
    GameInit(g);
    u->menu = u->help = u->buildMenu = false;
    u->panel = u->selected = u->build = -1;
    u->orderMode = 0;
    u->selectionCount = 1;
    u->selection[0] = GameCitizenRef(g, 2);
    Vector2 anchor = ViewIso(28.5f, 28.5f);
    u->camera.target = (Vector2){anchor.x - (1040 - u->camera.offset.x) / u->camera.zoom,
                                 anchor.y - (850 - u->camera.offset.y) / u->camera.zoom};
    g->citizens[0].x = 28 * 256;
    g->citizens[0].y = 28 * 256;
    g->citizens[1].x = 23 * 256;
    g->citizens[1].y = 30 * 256;
    // One person is visible in the bottom gap; another is behind the command tray.
    CHECK(ViewWorldPointHit(u, GetWorldToScreen2D(ViewIso(28.5f, 28.5f), u->camera)));
    CHECK(!ViewWorldPointHit(u, GetWorldToScreen2D(ViewIso(23.5f, 30.5f), u->camera)));
    u->mouse = (Vector2){800, 770};
    u->hoverTile = GameTile(28, 28);
    heldMouse = pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    u->mouse = (Vector2){1100, 880};
    heldMouse = pressedMouse = -1;
    ViewInput(u, .016f);
    CHECK(u->selectionCount == 1 && u->selection[0].slot == 0);
    u->selectionCount = 0;
    u->mouse = (Vector2){50, 815};
    CHECK(ViewWorldHit(u));
    u->mouse = (Vector2){50, 860};
    CHECK(!ViewWorldHit(u));
    CHECK(ViewProductionRect(u).width == 0);
    for (int i = 0; i < g->buildingCount; i++)
        if (g->buildings[i].alive && !g->buildings[i].owner && g->buildings[i].type == STOCKPILE)
        {
            ViewSelectBuilding(u, i);
            break;
        }
    u->mouse = (Vector2){700, 850};
    CHECK(ViewWorldHit(u));
}
int main(int argc, char **argv)
{
    GameState *g = calloc(1, sizeof(*g));
    GameInit(g);
    ViewContext *u = calloc(1, sizeof(*u));
    u->g = g;
    u->speed = u->lastSpeed = 1;
    u->panel = -1;
    u->selected = u->selectedCitizen = u->build = u->areaRole = u->hoverTile = -1;
    u->camera = (Camera2D){
        .offset = {SCREEN_W / 2.f, (WORLD_TOP + SCREEN_H) / 2.f}, .target = ViewIso(23, 23), .zoom = .86f};
    for (int i = 0; i < 16; i++)
        u->objects.rect[i] = (Rectangle){0, 0, 64, 64};
    strcpy(u->savePath, argc > 1 ? "hud-view-test.gfv2" : "view-test.gfv2");
    Key(u, KEY_SPACE);
    CHECK(u->speed == 0);
    Key(u, KEY_SPACE);
    CHECK(u->speed == 1);
    Key(u, KEY_F2);
    CHECK(u->speed == 2);
    Key(u, KEY_F3);
    CHECK(u->speed == 4);
    Key(u, KEY_F1);
    CHECK(u->speed == 1);
    Key(u, KEY_H);
    Key(u, KEY_F3);
    CHECK(u->help && u->speed == 1);
    Key(u, KEY_ESCAPE);
    Key(u, KEY_ESCAPE);
    Key(u, KEY_F3);
    CHECK(u->menu && u->speed == 1);
    Key(u, KEY_ESCAPE);
    u->selection[0] = GameCitizenRef(g, 0);
    u->selectionCount = 1;
    heldKey = KEY_LEFT_CONTROL;
    Key(u, KEY_ONE);
    heldKey = 0;
    u->selectionCount = 0;
    Key(u, KEY_ONE);
    CHECK(u->selectionCount == 1);
    CHECK(u->speed == 1);
    Point(u, 25, 28);
    pressedMouse = MOUSE_BUTTON_RIGHT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    CHECK(g->citizens[0].orderCount == 1);
    CHECK(g->citizens[0].orders[0].type == ORDER_MOVE);
    heldKey = KEY_LEFT_SHIFT;
    Point(u, 26, 28);
    pressedMouse = MOUSE_BUTTON_RIGHT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    heldKey = 0;
    CHECK(g->citizens[0].orderCount == 2);
    Key(u, KEY_S);
    CHECK(g->citizens[0].orderCount == 0);
    Point(u, 18, 18);
    Key(u, KEY_B);
    CHECK(u->buildMenu && u->build == -1);
    u->build = SHELTER;
    pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    CHECK(g->tiles[GameTile(18, 18)].occupied >= 0);
    CHECK(g->factions[0].reserved[WOOD] == 18);
    CHECK(g->citizens[0].orders[0].type == ORDER_BUILD);
    Vector2 before = u->camera.target;
    heldMouse = MOUSE_BUTTON_MIDDLE;
    delta = (Vector2){20, 10};
    ViewInput(u, .016f);
    heldMouse = -1;
    delta = (Vector2){0};
    CHECK(u->camera.target.x != before.x);
    Key(u, KEY_M);
    CHECK(u->camera.zoom == .16f);
    Key(u, KEY_HOME);
    CHECK(u->camera.zoom == .86f);
    Key(u, KEY_F);
    CHECK(fullscreen);
    u->buildMenu = false;
    Key(u, KEY_W);
    CHECK(u->panel == 1);
    Key(u, KEY_R);
    CHECK(u->panel == 2);
    Key(u, KEY_O);
    CHECK(u->panel == 0);
    Key(u, KEY_ESCAPE);
    CHECK(u->panel == -1 && !u->menu);
    // Drag selects living friendly citizens; Shift adds without replacing.
    g->citizens[0].x = 24 * 256;
    g->citizens[0].y = 27 * 256;
    g->citizens[1].x = 25 * 256;
    g->citizens[1].y = 27 * 256;
    u->camera.target = ViewIso(23, 23);
    u->camera.zoom = .86f;
    u->build = -1;
    u->selectionCount = 0;
    Vector2 a = GetWorldToScreen2D(ViewIso(24.5f, 27.5f), u->camera);
    Vector2 b = GetWorldToScreen2D(ViewIso(25.5f, 27.5f), u->camera);
    u->mouse = (Vector2){fminf(a.x, b.x) - 20, fminf(a.y, b.y) - 20};
    u->hoverTile = GameTile(24, 27);
    heldMouse = pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    u->mouse = (Vector2){fmaxf(a.x, b.x) + 20, fmaxf(a.y, b.y) + 20};
    heldMouse = pressedMouse = -1;
    ViewInput(u, .016f);
    CHECK(u->selectionCount >= 2);
    CHECK(u->panel == -1);
    int orders = g->citizens[0].orderCount;
    u->mouse = (Vector2){80, 115};
    u->hoverTile = GameTile(25, 25);
    g->factions[0].coldWarning = 1;
    pressedMouse = MOUSE_BUTTON_RIGHT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    CHECK(g->citizens[0].orderCount == orders);
    // Both formerly blocked side edges are now playable, but HUD and modals are not.
    u->mouse = (Vector2){1370, 400};
    CHECK(ViewWorldHit(u));
    u->mouse = (Vector2){700, 65};
    CHECK(ViewWorldHit(u));
    u->mouse = (Vector2){700, ViewSelectionRect(u).y - 2};
    CHECK(ViewWorldHit(u));
    u->mouse = (Vector2){700, ViewSelectionRect(u).y + 2};
    CHECK(!ViewWorldHit(u));
    // Gaps around the compact tray are usable terrain, not an invisible full-width dock.
    u->mouse = (Vector2){1040, 850};
    CHECK(ViewWorldHit(u));
    Camera2D cameraBefore = u->camera;
    Vector2 destination = ViewIso(28, 28);
    u->camera.target = (Vector2){destination.x - (1040 - u->camera.offset.x) / u->camera.zoom,
                                 destination.y - (850 - u->camera.offset.y) / u->camera.zoom};
    Point(u, 28, 28);
    pressedMouse = MOUSE_BUTTON_RIGHT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    CHECK(g->citizens[u->selection[0].slot].orders[0].x == 28 &&
          g->citizens[u->selection[0].slot].orders[0].y == 28);
    u->camera = cameraBefore;
    u->mouse = (Vector2){1240, 850};
    CHECK(!ViewWorldHit(u));
    u->buildMenu = true;
    u->mouse = (Vector2){700, 780};
    CHECK(!ViewWorldHit(u));
    u->buildMenu = false;
    CHECK(ViewWorldHit(u));
    u->mouse = (Vector2){700, 20};
    CHECK(!ViewWorldHit(u));
    u->mouse = (Vector2){50, 75};
    CHECK(!ViewWorldHit(u));
    u->mouse = (Vector2){1370, 400};
    Key(u, KEY_R);
    CHECK(ViewPanelOpen(u) && !ViewWorldHit(u));
    orders = g->citizens[0].orderCount;
    pressedMouse = MOUSE_BUTTON_RIGHT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    CHECK(g->citizens[0].orderCount == orders);
    Key(u, KEY_R);
    CHECK(!ViewPanelOpen(u));
    Key(u, KEY_B);
    CHECK(u->buildMenu);
    Key(u, KEY_ESCAPE);
    CHECK(!u->buildMenu && !u->menu);
    u->buildMenu = true;
    u->build = SHELTER;
    Key(u, KEY_ESCAPE);
    CHECK(u->build == -1 && u->buildMenu);
    Key(u, KEY_ESCAPE);
    CHECK(!u->buildMenu && !u->menu);
    ViewSelectBuilding(u, g->factions[0].center);
    CHECK(!u->selectionCount && u->selected == g->factions[0].center);
    u->build = SHELTER;
    u->buildMenu = true;
    Key(u, KEY_ONE);
    CHECK(u->build == -1 && !u->buildMenu && !u->orderMode);
    u->menu = true;
    u->buildMenu = true;
    Key(u, KEY_ESCAPE);
    CHECK(!u->menu && u->buildMenu);
    ViewSelectBuilding(u, g->factions[0].center);
    Key(u, KEY_B);
    CHECK(!u->buildMenu && u->build == -1);
    // A center's explicit rally command uses the existing simulation command.
    u->orderMode = VIEW_ORDER_RALLY;
    Point(u, 27, 29);
    pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    CHECK(g->factions[0].rallyX == 27 && g->factions[0].rallyY == 29);
    CHECK(!u->orderMode);
    u->selection[0] = GameCitizenRef(g, 0);
    u->selectionCount = 1;
    g->citizens[0].unitType = SPEARMAN;
    Key(u, KEY_B);
    CHECK(!u->buildMenu);
    g->citizens[0].unitType = CIVILIAN;
    ViewHoldSelected(u);
    CHECK(g->citizens[0].orders[0].type == ORDER_HOLD);
    Key(u, KEY_S);
    ViewSelectIdle(u);
    CHECK(u->selectionCount > 0 && !ViewPanelOpen(u));
    CHECK((float)ViewWorldRect().height / SCREEN_H >= .94f);
    CHECK(ViewSave(u));
    uint64_t hash = GameHash(g);
    g->tick = 100;
    CHECK(ViewLoad(u, u->savePath));
    CHECK(GameHash(g) == hash);
    CHECK(!u->selectionCount && u->speed == 0);
    CHECK(!ViewLoad(u, "missing-should-never-exist.gfv2"));
    CHECK(u->autosaveBlocked);
    CHECK(ViewSave(u));
    CHECK(!u->autosaveBlocked);
    remove(u->savePath);
    CompactSelectionChecks(u);
    if (argc > 1 && !strcmp(argv[1], "--render"))
        RenderChecks(u);
    printf("%d view checks, %d failures\n", checks, failures);
    free(u);
    free(g);
    return failures ? 1 : 0;
}
