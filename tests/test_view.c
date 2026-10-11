#include "test_support.h"
#include "view/input.h"
#include "view/world.h"
#include <math.h>
#include <string.h>

// Only platform input is replaced. ViewInput and its command/save adapters are
// the production modules, compiled from the same CMake source list as the app.
static int pressedKey, heldKey, pressedMouse = -1, heldMouse = -1;
static float wheel;
static Vector2 delta;
static bool fullscreen;
bool TestKeyPressed(int key) { return key == pressedKey; }
bool TestKeyDown(int key) { return key == heldKey; }
bool TestMousePressed(int button) { return button == pressedMouse; }
bool TestMouseDown(int button) { return button == heldMouse; }
float TestMouseWheel(void) { return wheel; }
Vector2 TestMouseDelta(void) { return delta; }
int TestScreenWidth(void) { return SCREEN_W; }
int TestScreenHeight(void) { return SCREEN_H; }
void TestFullscreen(void) { fullscreen = !fullscreen; }

static void Key(ViewContext *ui, int key)
{
    pressedKey = key;
    ViewInput(ui, 1.0f / 60);
    pressedKey = 0;
}

static void PointAt(ViewContext *ui, float x, float y)
{
    ui->mouse = GetWorldToScreen2D(ViewIso(x, y), ui->camera);
    Vector2 tile = ViewTileAt(ui, ui->mouse);
    ui->hoverTile = GameTile((int)floorf(tile.x), (int)floorf(tile.y));
}

int main(void)
{
    GameState *g = TestGame();
    ViewContext ui = {.g = g, .speed = 1, .lastSpeed = 1,
                      .selected = -1, .selectedCitizen = -1, .build = -1,
                      .areaRole = -1, .hoverTile = -1,
                      .camera = {.offset = {PANEL_X / 2.0f, 421}, .target = ViewIso(23, 23), .zoom = .86f}};
    // Input selection reads atlas bounds. Provide valid geometry without loading
    // textures; these tests never draw, open a window, or touch the player's slot.
    for (int i = 0; i < 16; i++)
        ui.objects.rect[i] = (Rectangle){0, 0, 64, 64};
    snprintf(ui.savePath, sizeof(ui.savePath), "./view-test.gfv1");

    Key(&ui, KEY_SPACE);
    CHECK(ui.speed == 0 && ui.lastSpeed == 1);
    Key(&ui, KEY_SPACE);
    CHECK(ui.speed == 1);
    Key(&ui, KEY_TWO);
    CHECK(ui.speed == 2);
    Key(&ui, KEY_FOUR);
    CHECK(ui.speed == 4);
    Key(&ui, KEY_ONE);
    CHECK(ui.speed == 1);
    Key(&ui, KEY_W);
    CHECK(ui.panel == 1);
    Key(&ui, KEY_R);
    CHECK(ui.panel == 2);
    Key(&ui, KEY_O);
    CHECK(ui.panel == 0);
    Key(&ui, KEY_G);
    CHECK(ui.grid);
    Key(&ui, KEY_F);
    CHECK(fullscreen);

    Key(&ui, KEY_H);
    Key(&ui, KEY_FOUR);
    CHECK(ui.help && ui.speed == 1);
    Key(&ui, KEY_ESCAPE);
    CHECK(!ui.help && !ui.menu);
    Key(&ui, KEY_ESCAPE);
    Key(&ui, KEY_FOUR);
    CHECK(ui.menu && ui.speed == 1);
    Key(&ui, KEY_ESCAPE);
    CHECK(!ui.menu);

    PointAt(&ui, 18.5f, 18.5f);
    CHECK(ui.hoverTile == GameTile(18, 18));
    Key(&ui, KEY_B);
    CHECK(ui.build == SHELTER);
    int wood = g->stock[WOOD], buildings = g->buildingCount;
    pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(&ui, 1.0f / 60);
    pressedMouse = -1;
    CHECK(g->buildingCount == buildings + 1 && g->stock[WOOD] == wood - 18);
    CHECK(ui.selected == buildings && ui.build == -1);
    // The same placement is rejected through the authoritative simulation command.
    Key(&ui, KEY_B);
    pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(&ui, 1.0f / 60);
    pressedMouse = -1;
    CHECK(g->buildingCount == buildings + 1 && g->stock[WOOD] == wood - 18);
    CHECK(ui.toastTime > 0 && ui.build == SHELTER);
    Key(&ui, KEY_ESCAPE);
    CHECK(ui.build == -1 && !ui.menu);

    PointAt(&ui, 26.5f, 23.5f);
    ui.areaRole = GUARD;
    pressedMouse = MOUSE_BUTTON_LEFT;
    ViewInput(&ui, 1.0f / 60);
    pressedMouse = -1;
    CHECK(g->rallyX == 26 && g->rallyY == 23 && ui.areaRole == -1);

    Key(&ui, KEY_M);
    CHECK(ui.camera.zoom == .32f);
    Key(&ui, KEY_HOME);
    CHECK(ui.camera.zoom == .86f);
    float zoom = ui.camera.zoom;
    wheel = 1;
    ViewInput(&ui, 1.0f / 60);
    wheel = 0;
    CHECK(ui.camera.zoom > zoom);
    float cameraX = ui.camera.target.x;
    heldMouse = MOUSE_BUTTON_RIGHT;
    delta = (Vector2){10, 0};
    ViewInput(&ui, 1.0f / 60);
    heldMouse = -1;
    CHECK(ui.camera.target.x < cameraX);

    Key(&ui, KEY_F5);
    CHECK(FileExists(ui.savePath) && !ui.autosaveBlocked);
    uint64_t saved = GameHash(g);
    GameTick(g);
    Key(&ui, KEY_F9);
    CHECK(GameHash(g) == saved && ui.speed == 0);
    CHECK(ui.selected == -1 && ui.selectedCitizen == -1 && ui.build == -1);
    remove(ui.savePath);
    Key(&ui, KEY_F9);
    CHECK(ui.autosaveBlocked && GameHash(g) == saved);
    Key(&ui, KEY_F5);
    CHECK(!ui.autosaveBlocked && FileExists(ui.savePath));
    remove(ui.savePath);
    remove("./view-test.gfv1.tmp");

    g->lost = 1;
    Key(&ui, KEY_FOUR);
    CHECK(ui.speed == 0);
    g->lost = 0;
    g->won = 1;
    Key(&ui, KEY_FOUR);
    CHECK(ui.speed == 0);
    ui.winSeen = true;
    Key(&ui, KEY_FOUR);
    CHECK(ui.speed == 4);
    free(g);
    return TestReport();
}
