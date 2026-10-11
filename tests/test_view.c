#include "game.h"
#include "sim/jobs.h"
#include "sim/wildlife.h"
#include "view/actions.h"
#include "view/animation.h"
#include "view/assets.h"
#include "view/context.h"
#include "view/hud.h"
#include "view/input.h"
#include "view/layout.h"
#include "view/panels.h"
#include "view/selection.h"
#include "view/widgets.h"
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
static void AnimationChecks(ViewContext *u)
{
    GameState *g = u->g;
    GameInit(g);
    Citizen *c = &g->citizens[1];
    int tile = GameTile(25, 30);
    JobsReset(c);
    c->job = JOB_GATHER;
    c->target = tile;
    c->work = 1;
    for (int facing = FACING_SE; facing <= FACING_NW; facing++)
    {
        c->facing = facing;
        SpritePose p = ViewCitizenPose(u, 1);
        CHECK(p.atlas == &u->peopleJobs && p.flip == ((facing & 1) != 0));
        CHECK(p.frame / 2 == (facing >= FACING_NE));
    }
    const int nodes[] = {NODE_BERRIES, NODE_STONE, NODE_ORE, NODE_TREE, NODE_GAME};
    const int rows[] = {0, 1, 1, 1, 3};
    for (int i = 0; i < 5; i++)
    {
        g->tiles[tile].node = nodes[i];
        SpritePose p = ViewCitizenPose(u, 1);
        CHECK(p.frame / 4 == rows[i]);
        CHECK(p.atlas == (i < 3 ? &u->peopleJobs : &u->peopleNorth));
    }
    c->job = JOB_REPAIR;
    c->work = 0;
    CHECK(ViewCitizenPose(u, 1).frame / 4 == 2);
    c->job = JOB_PLANT;
    c->work = 1;
    CHECK(ViewCitizenPose(u, 1).frame / 4 == 3);
    JobsReset(c);
    CHECK(ViewCitizenPose(u, 1).atlas == &u->peopleNorth && ViewCitizenPose(u, 1).flip);
    c->job = JOB_DELIVER;
    CHECK(ViewCitizenPose(u, 1).frame / 4 == 2);
    c->pathLength = 2;
    g->tick = 3;
    int frame = ViewCitizenPose(u, 1).frame;
    g->tick += 3;
    CHECK(ViewCitizenPose(u, 1).frame != frame);
    Wildlife *a = &g->wildlife[2];
    a->state = ANIMAL_WALK;
    a->facing = FACING_NE;
    CHECK(ViewAnimalPose(u, 2).frame / 4 == 2);
    frame = ViewAnimalPose(u, 2).frame;
    g->tick += 5;
    CHECK(ViewAnimalPose(u, 2).frame != frame);
    a->state = ANIMAL_RUN;
    a->facing = FACING_SW;
    CHECK(ViewAnimalPose(u, 2).frame / 4 == 1 && ViewAnimalPose(u, 2).flip);
    a->stepX = 18;
    u->menu = u->help = false;
    u->speed = 1;
    u->accumulator = 0;
    Vector2 before = ViewAnimalPosition(u, 2);
    u->accumulator = .05;
    CHECK(ViewAnimalPosition(u, 2).x > before.x);
    u->speed = 0;
    CHECK(ViewAnimalPosition(u, 2).x == a->x / 256.f + .5f);
    GameInit(g);
    u->speed = 1;
    u->accumulator = 0;
}
static void CaptureWorld(ViewContext *u, const char *path)
{
    BeginTextureMode(u->canvas);
    ClearBackground(BLACK);
    ViewWorldDraw(u);
    EndTextureMode();
    Image image = LoadImageFromTexture(u->canvas.texture);
    ImageFlipVertical(&image);
    CHECK(ExportImage(image, path));
    UnloadImage(image);
}
static void RenderAnimationChecks(ViewContext *u)
{
    // Explicit visual fixture, not a campaign: put four workers around one
    // existing bush, then use the actual job owner to turn them toward it.
    GameState *g = u->g;
    GameInit(g);
    u->panel = u->build = u->selected = u->hoverTile = -1;
    u->menu = u->help = u->buildMenu = false;
    u->selectionCount = u->orderMode = u->commandPage = 0;
    u->speed = 0;
    u->camera =
        (Camera2D){.offset = {SCREEN_W / 2.f, SCREEN_H / 2.f}, .target = ViewIso(25.5f, 30.5f), .zoom = 3};
    const int dx[] = {-1, 1, 0, 0}, dy[] = {0, 0, -1, 1};
    for (int i = 0; i < g->citizenCount; i++)
    {
        Citizen *c = &g->citizens[i];
        JobsReset(c);
        c->role = IDLE;
        if (i < 4)
        {
            c->x = (25 + dx[i]) * 256;
            c->y = (30 + dy[i]) * 256;
            c->role = FORAGER;
            c->job = JOB_GATHER;
            c->target = GameTile(25, 30);
        }
    }
    JobsUpdate(g);
    CaptureWorld(u, "evidence/work-facing.png");
    // Picking follows a roaming animal's rendered position, while the order
    // still addresses its hunting ground's authoritative resource inventory.
    Wildlife *a = &g->wildlife[2];
    a->x += 256;
    a->goalX = a->x;
    Vector2 p = ViewAnimalPosition(u, 2), foot = ViewIso(p.x, p.y);
    u->camera.target = foot;
    foot.y += 5;
    Rectangle bounds = ViewPoseBounds(ViewAnimalPose(u, 2), foot, 52);
    u->mouse =
        GetWorldToScreen2D((Vector2){bounds.x + bounds.width / 2, bounds.y + bounds.height / 2}, u->camera);
    u->hoverTile = GameTile(a->x / 256, a->y / 256);
    CHECK(ViewAnimalAt(u) == a->home && u->hoverTile != a->home);
    u->selection[0] = GameCitizenRef(g, 0);
    u->selectionCount = 1;
    pressedMouse = MOUSE_BUTTON_RIGHT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    CHECK(g->citizens[0].orders[0].type == ORDER_GATHER);
    CHECK(GameTile(g->citizens[0].orders[0].x, g->citizens[0].orders[0].y) == a->home);
    a->wait = 0;
    WildlifeUpdate(g);
    CHECK(a->state == ANIMAL_WALK);
    CaptureWorld(u, "evidence/animal-walking.png");
    g->citizens[0].x = a->x - 256;
    g->citizens[0].y = a->y;
    for (int i = 0; i < 8; i++)
    {
        g->tick++;
        WildlifeUpdate(g);
    }
    CHECK(a->state == ANIMAL_RUN && (a->stepX || a->stepY));
    CaptureWorld(u, "evidence/animal-running.png");
}
static void CaptureControls(ViewContext *u, const char *path)
{
    BeginTextureMode(u->canvas);
    ClearBackground(BLACK);
    ViewWorldDraw(u);
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    u->allowInput = false;
    ViewHeader(u);
    ViewSelectionDraw(u);
    ViewWorldHUD(u);
    ViewCursor(u);
    EndMode2D();
    EndTextureMode();
    Image image = LoadImageFromTexture(u->canvas.texture);
    ImageFlipVertical(&image);
    CHECK(ExportImage(image, path));
    UnloadImage(image);
}
static void RenderControlChecks(ViewContext *u)
{
    GameState *g = u->g;
    GameInit(g);
    ViewCancelDrag(u);
    u->menu = u->help = u->buildMenu = false;
    u->panel = u->selected = u->build = -1;
    u->selectionCount = u->orderMode = 0;
    u->speed = 1;
    for (int i = 0; i < 50; i++)
        JobsUpdate(g);
    CHECK(GameIdleCountFor(g, 0) == 1);
    HudRectClick(u, ViewCommandRect(u, 1));
    CHECK(u->selectionCount == 1 && u->selection[0].slot == 0);
    CHECK(g->citizens[0].role == BUILDER);
    ViewSelectBuilding(u, -1);
    HudClick(u, 740, 24);
    CHECK(u->selectionCount == 1 && u->selectedCitizen == 0);
    CHECK(GameCommandApply(g, (GameCommand){.type = CMD_BUILD, .a = SHELTER, .b = 18, .c = 18}, NULL, 0));
    int site = g->tiles[GameTile(18, 18)].occupied;
    ViewSelectBuilding(u, site);
    int wood = g->factions[0].stock[WOOD], reserved = g->factions[0].reserved[WOOD];
    CHECK(ViewSelectionRect(u).width >= ViewCommandRect(u, 1).x + ViewCommandRect(u, 1).width - 16);
    HudRectClick(u, ViewCommandRect(u, 0));
    CHECK(g->citizens[0].orders[0].type == ORDER_BUILD && g->citizens[0].orders[0].target.slot == site);
    CHECK(g->factions[0].stock[WOOD] == wood && g->factions[0].reserved[WOOD] == reserved);
    for (int i = 0; i < 1500 && g->buildings[site].progress < 60; i++)
        JobsUpdate(g);
    CHECK(g->buildings[site].progress >= 60 && !g->buildings[site].complete);
    u->camera.target = ViewIso(20, 21);
    u->camera.zoom = 1.4f;
    u->mouse = (Vector2){350, 818};
    CaptureControls(u, "evidence/controls-resume.png");
    CHECK(GameCommandApply(g, (GameCommand){.type = CMD_STOP, .entity = GameCitizenRef(g, 0)}, NULL, 0));
    u->selected = -1;
    u->selectionCount = 1;
    u->selection[0] = GameCitizenRef(g, 0);
    HudRectClick(u, ViewCommandRect(u, 2));
    CHECK(u->orderMode == ORDER_REPAIR);
    Point(u, 18.5f, 18.5f);
    pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    CHECK(g->citizens[0].orders[0].type == ORDER_REPAIR);
    int progress = g->buildings[site].progress;
    for (int i = 0; i < 100; i++)
        JobsUpdate(g);
    CHECK(g->buildings[site].progress > progress);
    // The minimap owns a held drag; subsequent points need no second click.
    heldMouse = MOUSE_BUTTON_LEFT;
    HudRectClick(u, ViewMapRect());
    CHECK(u->mapDragging && !u->dragging);
    Vector2 before = u->camera.target;
    u->mouse.x += 25;
    BeginTextureMode(u->canvas);
    BeginMode2D((Camera2D){.zoom = RENDER_SCALE});
    u->allowInput = true;
    u->clickConsumed = false;
    ViewWorldHUD(u);
    EndMode2D();
    EndTextureMode();
    CHECK(u->mapDragging && u->camera.target.x != before.x);
    heldMouse = -1;
    ViewCancelDrag(u);
    u->camera.target = ViewIso(20, 21);
    u->mouse = (Vector2){750, 470};
    u->panning = true;
    CaptureControls(u, "evidence/controls-hand-grab.png");
    ViewCancelDrag(u);
    for (int i = 0; i < 1000 && !g->buildings[site].complete; i++)
        JobsUpdate(g);
    CHECK(g->buildings[site].complete && g->factions[0].reserved[WOOD] == 0);
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
    RenderAnimationChecks(u);
    RenderControlChecks(u);
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
    heldKey = KEY_LEFT_SHIFT;
    heldMouse = pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    u->mouse = (Vector2){1100, 880};
    heldMouse = pressedMouse = -1;
    ViewInput(u, .016f);
    heldKey = 0;
    CHECK(u->selectionCount == 2 && u->selection[0].slot == 2 && u->selection[1].slot == 0);
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
static void ControlChecks(ViewContext *u)
{
    GameState *g = u->g;
    GameInit(g);
    ViewCancelDrag(u);
    u->menu = u->help = u->buildMenu = false;
    u->panel = u->selected = u->build = -1;
    u->orderMode = 0;
    u->camera.target = ViewIso(35, 35);
    u->camera.zoom = .8f;
    u->selectionCount = 1;
    u->selection[0] = GameCitizenRef(g, 0);
    Point(u, 35, 35);
    Vector2 start = u->mouse, camera = u->camera.target;
    int orders = g->citizens[0].orderCount;
    heldMouse = pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    CHECK(u->panCandidate && !u->panning);
    pressedMouse = -1;
    u->mouse.x += 80;
    u->mouse.y += 40;
    ViewInput(u, .016f);
    CHECK(u->panning && fabsf(u->camera.target.x - (camera.x - 100)) < .01f);
    CHECK(fabsf(u->camera.target.y - (camera.y - 50)) < .01f);
    CHECK(u->selectionCount == 1 && g->citizens[0].orderCount == orders);
    // Moving back inside the click threshold does not turn a completed pan into a click.
    u->mouse = start;
    heldMouse = -1;
    ViewInput(u, .016f);
    CHECK(!u->panning && !u->dragging && u->selectionCount == 1);
    // Pan continues across the HUD; releasing there cannot issue an order.
    heldMouse = pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    u->mouse = (Vector2){600, 850};
    ViewInput(u, .016f);
    CHECK(u->panning && !ViewWorldHit(u));
    heldMouse = -1;
    ViewInput(u, .016f);
    CHECK(!u->dragging && u->selectionCount == 1 && g->citizens[0].orderCount == orders);
    // A gesture beginning on the HUD never pans, even after entering the world.
    camera = u->camera.target;
    heldMouse = pressedMouse = MOUSE_BUTTON_MIDDLE;
    ViewInput(u, .016f);
    pressedMouse = -1;
    u->mouse = (Vector2){700, 400};
    ViewInput(u, .016f);
    CHECK(!u->panning && u->camera.target.x == camera.x && u->camera.target.y == camera.y);
    heldMouse = -1;
    // Opening an overlay cancels any captured drag.
    Point(u, 35, 35);
    heldMouse = pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    pressedMouse = -1;
    u->mouse.x += 30;
    ViewInput(u, .016f);
    ViewTogglePanel(u, 0);
    CHECK(!u->dragging && !u->panning && !u->panCandidate);
    heldMouse = -1;
    u->panel = -1;
    // Idle means available now, not the remembered role or a queued order.
    GameInit(g);
    for (int i = 0; i < g->citizenCount; i++)
        g->citizens[i].job = JOB_GATHER;
    JobsReset(&g->citizens[0]);
    g->citizens[0].role = BUILDER;
    JobsReset(&g->citizens[8]);
    JobsReset(&g->citizens[1]);
    g->citizens[1].orders[0] = (UnitOrder){.type = ORDER_MOVE, .x = 30, .y = 30};
    g->citizens[1].orderCount = 1;
    ViewSelectIdle(u);
    CHECK(u->selectionCount == 1 && u->selectedCitizen == 0 && u->selection[0].slot == 0);
    CHECK(GameIdleCountFor(g, 0) == 1 && strstr(u->toast, "1 idle worker"));
    CHECK(u->camera.target.x == ViewIso(g->citizens[0].x / 256.f + .5f, g->citizens[0].y / 256.f + .5f).x);
    g->citizens[0].job = JOB_BUILD;
    ViewSelectIdle(u);
    CHECK(u->selectionCount == 1 && strstr(u->toast, "No idle workers"));
    GameInit(g);
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
    heldMouse = pressedMouse = MOUSE_BUTTON_MIDDLE;
    ViewInput(u, .016f);
    pressedMouse = -1;
    u->mouse.x += 20;
    u->mouse.y += 10;
    ViewInput(u, .016f);
    heldMouse = -1;
    ViewInput(u, .016f);
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
    // Shift-drag selects living friendly citizens without moving the camera.
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
    heldKey = KEY_LEFT_SHIFT;
    heldMouse = pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(u, .016f);
    u->mouse = (Vector2){fmaxf(a.x, b.x) + 20, fmaxf(a.y, b.y) + 20};
    heldMouse = pressedMouse = -1;
    ViewInput(u, .016f);
    heldKey = 0;
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
    ControlChecks(u);
    AnimationChecks(u);
    if (argc > 1 && !strcmp(argv[1], "--render"))
        RenderChecks(u);
    printf("%d view checks, %d failures\n", checks, failures);
    free(u);
    free(g);
    return failures ? 1 : 0;
}
