#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { SCENE_WIDTH = 1206, SCENE_HEIGHT = 1195, MAX_ACTORS = 20 };

typedef struct Actor {
    const char *name;
    Rectangle bounds;
    Vector4 motion; // speed, phase, sway amplitude, signed arm gesture
} Actor;

// One authoritative list drives rendering, hit-testing, and animation checks.
static const Actor ACTORS[] = {
    { "Village visitor", { 571, 637, 46, 70 }, { 2.4f, 0.0f, 1.8f, 0.6f } },
    { "Woodcutter",      { 590, 266, 64, 69 }, { 3.8f, 0.3f, 1.5f, 1.7f } },
    { "Sawyer",          { 675, 270, 61, 68 }, { 3.8f, 2.5f, 1.3f,-1.6f } },
    { "Miner",           { 879, 320, 57, 72 }, { 4.1f, 0.7f, 1.7f, 1.5f } },
    { "Stone worker",    { 967, 312, 66, 70 }, { 3.6f, 2.0f, 1.7f,-1.6f } },
    { "Basket carrier",  { 281, 560, 53, 70 }, { 2.2f, 1.0f, 1.4f,-0.7f } },
    { "Water carrier",   { 672, 535, 48, 65 }, { 2.0f, 1.8f, 1.3f,-0.6f } },
    { "Market visitor",  { 764, 531, 45, 65 }, { 2.5f, 3.0f, 1.3f, 0.6f } },
    { "Merchant",        { 803, 583, 53, 71 }, { 2.1f, 4.0f, 1.4f, 0.8f } },
    { "Field tender",    { 213, 729, 52, 78 }, { 2.8f, 0.8f, 1.8f,-1.1f } },
    { "Seed planter",    { 353, 735, 57, 82 }, { 3.0f, 2.2f, 1.8f,-1.1f } },
    { "Rice farmer",     { 102, 868, 53, 78 }, { 2.5f, 1.7f, 1.8f, 1.0f } },
    { "Paddy worker",    { 155, 842, 62, 80 }, { 3.0f, 3.5f, 1.5f, 1.2f } },
    { "Rice harvester",  { 405, 851, 72, 81 }, { 2.8f, 0.3f, 1.8f, 1.3f } },
    { "Planter",         { 476, 908, 58, 74 }, { 2.5f, 2.4f, 1.7f,-1.1f } },
    { "Canal farmer",    { 584, 849, 59, 82 }, { 3.0f, 4.0f, 1.5f,-1.2f } },
    { "Ox keeper",       { 1011, 810, 62, 77 },{ 2.3f, 2.0f, 1.5f,-0.9f } },
    { "Vegetable farmer",{ 847, 993, 65, 84 }, { 2.8f, 1.3f, 1.6f,-1.3f } },
    { "Lower paddy farmer",{ 579, 1063, 75, 80 },{ 2.7f, 3.0f, 1.6f, 1.2f } },
};
static const int ACTOR_COUNT = sizeof(ACTORS)/sizeof(ACTORS[0]);

typedef struct Scene {
    Texture2D artwork;
    RenderTexture2D canvas;
    Shader shader;
    int timeLoc, amountLoc, selectedLoc, greetingLoc;
} Scene;

typedef struct View {
    float zoom;
    Vector2 center;
} View;

typedef struct Options {
    bool verify, reference;
    const char *capture;
    const char *evidence;
    float time;
} Options;

static void Usage(const char *binary)
{
    printf("Usage: %s [--reference] [--capture file.png] [--time seconds]\n"
           "       %s --verify output-directory\n\n"
           "Space: pause/resume   R: still artwork   H: controls\n"
           "Click a villager: greeting   Tab: next villager\n"
           "Scroll: zoom   Right drag / arrows: pan   Home: fit\n"
           "F: fullscreen   Escape: close\n", binary, binary);
}

static bool ParseOptions(int argc, char **argv, Options *options)
{
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--reference")) options->reference = true;
        else if (!strcmp(argv[i], "--verify") && i + 1 < argc) {
            options->verify = true;
            options->evidence = argv[++i];
        } else if (!strcmp(argv[i], "--capture") && i + 1 < argc) {
            options->capture = argv[++i];
        } else if (!strcmp(argv[i], "--time") && i + 1 < argc) {
            char *end = NULL;
            errno = 0;
            options->time = strtof(argv[++i], &end);
            if (errno || end == argv[i] || *end || !isfinite(options->time) || options->time < 0) {
                fprintf(stderr, "--time requires a finite nonnegative number.\n");
                return false;
            }
        } else {
            fprintf(stderr, "Unknown or incomplete option: %s\n", argv[i]);
            return false;
        }
    }
    if (options->verify && options->capture) {
        fprintf(stderr, "Use --verify or --capture, separately.\n");
        return false;
    }
    return true;
}

static bool AssetPath(char *output, size_t size, const char *name)
{
    int n = snprintf(output, size, "%sassets/%s", GetApplicationDirectory(), name);
    return n >= 0 && (size_t)n < size;
}

// Only these overlay areas use the generated restoration. Every pixel
// outside them still comes from the untouched original JPEG.
static const char *const REPAIR_FILES[] = { "overlay-repair.png", "title-repair.png" };
static const struct { Rectangle bounds; int layer; } REPAIRS[] = {
    { { 1074, 812, 100, 100 }, 0 }, // thumbs-up and its shadow
    { { 1090, 918, 66, 46 }, 0 },   // like count
    { { 1072, 1028, 104, 101 }, 0 },// comment bubble, including its tail
    { { 890, 1058, 310, 122 }, 1 }  // title lettering, backing, and shadow
};

static float RepairWeight(int x, int y, int layer)
{
    float weight = 0;
    for (unsigned i = 0; i < sizeof(REPAIRS)/sizeof(REPAIRS[0]); i++) {
        if (layer >= 0 && REPAIRS[i].layer != layer) continue;
        Rectangle r = REPAIRS[i].bounds;
        float dx = fminf(x - r.x, r.x + r.width - 1 - x);
        float dy = fminf(y - r.y, r.y + r.height - 1 - y);
        float edge = fminf(dx, dy);
        if (edge >= 0) weight = fmaxf(weight, Clamp(edge/6.0f, 0, 1));
    }
    return weight;
}

static bool RestoreOverlays(Image *source)
{
    ImageFormat(source, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color *pixels = source->data;
    for (unsigned layer = 0; layer < sizeof(REPAIR_FILES)/sizeof(REPAIR_FILES[0]); layer++) {
        char path[4096];
        if (!AssetPath(path, sizeof(path), REPAIR_FILES[layer])) return false;
        Image repair = LoadImage(path);
        if (!IsImageValid(repair)) return false;
        Color *patch = LoadImageColors(repair);
        for (int y = 0; y < SCENE_HEIGHT; y++) {
            for (int x = 0; x < SCENE_WIDTH; x++) {
                float weight = RepairWeight(x, y, (int)layer);
                if (weight <= 0) continue;
                int sx = (int)((x + 0.5f)*repair.width/SCENE_WIDTH);
                int sy = (int)((y + 0.5f)*repair.height/SCENE_HEIGHT);
                Color replacement = patch[sy*repair.width + sx];
                Color *original = &pixels[y*SCENE_WIDTH + x];
                original->r = (unsigned char)roundf(Lerp(original->r, replacement.r, weight));
                original->g = (unsigned char)roundf(Lerp(original->g, replacement.g, weight));
                original->b = (unsigned char)roundf(Lerp(original->b, replacement.b, weight));
            }
        }
        UnloadImageColors(patch);
        UnloadImage(repair);
    }
    return true;
}

static bool LoadScene(Scene *scene, Image *source)
{
    char imagePath[4096], shaderPath[4096];
    if (!AssetPath(imagePath, sizeof(imagePath), "ui.jpeg") ||
        !AssetPath(shaderPath, sizeof(shaderPath), "scene.fs")) return false;
    *source = LoadImage(imagePath);
    if (!IsImageValid(*source)) return false;
    if (source->width != SCENE_WIDTH || source->height != SCENE_HEIGHT) {
        fprintf(stderr, "Expected the original 1206 x 1195 ui.jpeg.\n");
        return false;
    }
    if (!RestoreOverlays(source)) return false;
    scene->artwork = LoadTextureFromImage(*source);
    scene->canvas = LoadRenderTexture(SCENE_WIDTH, SCENE_HEIGHT);
    scene->shader = LoadShader(NULL, shaderPath);
    if (!IsTextureValid(scene->artwork) || !IsRenderTextureValid(scene->canvas) ||
        !IsShaderValid(scene->shader) || scene->shader.id == rlGetShaderIdDefault()) return false;
    SetTextureFilter(scene->artwork, TEXTURE_FILTER_POINT);
    SetTextureFilter(scene->canvas.texture, TEXTURE_FILTER_POINT);
    SetTextureWrap(scene->artwork, TEXTURE_WRAP_CLAMP);

    Vector2 size = { SCENE_WIDTH, SCENE_HEIGHT };
    Vector4 bounds[MAX_ACTORS] = { 0 }, motion[MAX_ACTORS] = { 0 };
    for (int i = 0; i < ACTOR_COUNT; i++) {
        Rectangle r = ACTORS[i].bounds;
        bounds[i] = (Vector4){ r.x, r.y, r.width, r.height };
        motion[i] = ACTORS[i].motion;
    }
    SetShaderValue(scene->shader, GetShaderLocation(scene->shader, "sceneSize"), &size, SHADER_UNIFORM_VEC2);
    SetShaderValue(scene->shader, GetShaderLocation(scene->shader, "actorCount"), &ACTOR_COUNT, SHADER_UNIFORM_INT);
    SetShaderValueV(scene->shader, GetShaderLocation(scene->shader, "actorBounds[0]"), bounds, SHADER_UNIFORM_VEC4, MAX_ACTORS);
    SetShaderValueV(scene->shader, GetShaderLocation(scene->shader, "actorMotion[0]"), motion, SHADER_UNIFORM_VEC4, MAX_ACTORS);
    scene->timeLoc = GetShaderLocation(scene->shader, "clockTime");
    scene->amountLoc = GetShaderLocation(scene->shader, "animationAmount");
    scene->selectedLoc = GetShaderLocation(scene->shader, "selectedActor");
    scene->greetingLoc = GetShaderLocation(scene->shader, "greeting");
    return true;
}

static void UnloadScene(Scene *scene, Image source)
{
    if (scene->shader.id && scene->shader.id != rlGetShaderIdDefault()) UnloadShader(scene->shader);
    if (scene->canvas.id) UnloadRenderTexture(scene->canvas);
    if (scene->artwork.id) UnloadTexture(scene->artwork);
    if (source.data) UnloadImage(source);
}

static void RenderScene(const Scene *scene, float time, bool reference, int selected, float greeting)
{
    float amount = reference ? 0.0f : 1.0f;
    SetShaderValue(scene->shader, scene->timeLoc, &time, SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene->shader, scene->amountLoc, &amount, SHADER_UNIFORM_FLOAT);
    SetShaderValue(scene->shader, scene->selectedLoc, &selected, SHADER_UNIFORM_INT);
    SetShaderValue(scene->shader, scene->greetingLoc, &greeting, SHADER_UNIFORM_FLOAT);
    BeginTextureMode(scene->canvas);
    ClearBackground(BLACK);
    BeginShaderMode(scene->shader);
    DrawTexture(scene->artwork, 0, 0, WHITE);
    EndShaderMode();
    EndTextureMode();
}

static Image ReadCanvas(const Scene *scene)
{
    Image image = LoadImageFromTexture(scene->canvas.texture);
    ImageFlipVertical(&image);
    return image;
}

static bool ExportFrame(const Scene *scene, const char *directory, const char *name)
{
    char path[4096];
    int n = snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (n < 0 || (size_t)n >= sizeof(path)) return false;
    Image frame = ReadCanvas(scene);
    bool result = ExportImage(frame, path);
    UnloadImage(frame);
    return result;
}

static bool SamePixel(Color a, Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static bool InActor(int x, int y)
{
    for (int i = 0; i < ACTOR_COUNT; i++) {
        if (CheckCollisionPointRec((Vector2){ x + 0.5f, y + 0.5f }, ACTORS[i].bounds)) return true;
    }
    return false;
}

// GPU evidence, not just a copy/export of the input: read back the same render
// target that is displayed in the native application and compare every channel.
static int VerifyScene(const Scene *scene, Image source, const char *directory)
{
    if (!DirectoryExists(directory)) {
        fprintf(stderr, "Verification output directory does not exist: %s\n", directory);
        return 1;
    }
    RenderScene(scene, 0, true, 0, 0);
    Image reference = ReadCanvas(scene);
    Color *expected = LoadImageColors(source);
    Color *actual = LoadImageColors(reference);
    char originalPath[4096];
    if (!AssetPath(originalPath, sizeof(originalPath), "ui.jpeg")) return 1;
    Image original = LoadImage(originalPath);
    Color *originalPixels = LoadImageColors(original);
    unsigned repairChanges = 0, repairLeaks = 0;
    for (int y = 0; y < SCENE_HEIGHT; y++) {
        for (int x = 0; x < SCENE_WIDTH; x++) {
            int p = y*SCENE_WIDTH + x;
            if (SamePixel(originalPixels[p], actual[p])) continue;
            if (RepairWeight(x, y, -1) > 0) repairChanges++;
            else repairLeaks++;
        }
    }
    UnloadImageColors(originalPixels);
    UnloadImage(original);
    unsigned mismatches = 0;
    int maximumError = 0;
    for (int i = 0; i < SCENE_WIDTH*SCENE_HEIGHT; i++) {
        if (!SamePixel(expected[i], actual[i])) mismatches++;
        int errors[] = { abs(expected[i].r - actual[i].r), abs(expected[i].g - actual[i].g),
                         abs(expected[i].b - actual[i].b), abs(expected[i].a - actual[i].a) };
        for (int c = 0; c < 4; c++) if (errors[c] > maximumError) maximumError = errors[c];
    }
    bool exported = ExportFrame(scene, directory, "reference.png");
    RenderScene(scene, 0, false, 0, 0);
    Image start = ReadCanvas(scene);
    Color *startPixels = LoadImageColors(start);
    exported = ExportFrame(scene, directory, "animated-0.png") && exported;
    RenderScene(scene, 1.25f, false, 0, 0);
    Image later = ReadCanvas(scene);
    Color *laterPixels = LoadImageColors(later);
    exported = ExportFrame(scene, directory, "animated-1.png") && exported;
    unsigned animated = 0, unchangedSceneryErrors = 0;
    unsigned actorChanges[MAX_ACTORS] = { 0 };
    for (int y = 0; y < SCENE_HEIGHT; y++) {
        for (int x = 0; x < SCENE_WIDTH; x++) {
            int p = y*SCENE_WIDTH + x;
            bool changed = !SamePixel(startPixels[p], laterPixels[p]);
            if (changed) animated++;
            for (int i = 0; i < ACTOR_COUNT; i++) {
                if (changed && CheckCollisionPointRec((Vector2){ x + 0.5f, y + 0.5f }, ACTORS[i].bounds))
                    actorChanges[i]++;
            }
            // The top landscape excludes water; only actors and the flag can move.
            bool flag = x >= 194 && x < 287 && y >= 231 && y < 315;
            if (y <= 649 && !flag && !InActor(x, y) && !SamePixel(actual[p], laterPixels[p]))
                unchangedSceneryErrors++;
        }
    }
    unsigned actorsMoving = 0;
    for (int i = 0; i < ACTOR_COUNT; i++) if (actorChanges[i] > 20) actorsMoving++;

    // A paused frame must be deterministic even after another frame was rendered.
    RenderScene(scene, 1.25f, false, 0, 0);
    Image repeat = ReadCanvas(scene);
    Color *repeatPixels = LoadImageColors(repeat);
    unsigned pauseErrors = 0;
    for (int i = 0; i < SCENE_WIDTH*SCENE_HEIGHT; i++)
        if (!SamePixel(laterPixels[i], repeatPixels[i])) pauseErrors++;

    // Greeting interaction must change the selected avatar, without changing others.
    RenderScene(scene, 1.25f, false, 0, 1);
    Image greeting = ReadCanvas(scene);
    Color *greetingPixels = LoadImageColors(greeting);
    unsigned greetingChanges = 0, greetingLeaks = 0;
    for (int y = 0; y < SCENE_HEIGHT; y++) {
        for (int x = 0; x < SCENE_WIDTH; x++) {
            int p = y*SCENE_WIDTH + x;
            if (SamePixel(laterPixels[p], greetingPixels[p])) continue;
            if (CheckCollisionPointRec((Vector2){ x + 0.5f, y + 0.5f }, ACTORS[0].bounds)) greetingChanges++;
            else greetingLeaks++;
        }
    }

    printf("Reference: %u / %d differing pixels; maximum channel error: %d\n",
           mismatches, SCENE_WIDTH*SCENE_HEIGHT, maximumError);
    printf("Animation: %u changed pixels; %u / %d villagers moving\n", animated, actorsMoving, ACTOR_COUNT);
    printf("Unchanged upper scenery: %u differing pixels\n", unchangedSceneryErrors);
    printf("Pause determinism: %u differing pixels\n", pauseErrors);
    printf("Avatar greeting: %u changed pixels; %u outside selected avatar\n", greetingChanges, greetingLeaks);
    printf("Overlay repair: %u changed pixels; %u outside repair regions\n", repairChanges, repairLeaks);
    bool passed = !mismatches && actorsMoving == (unsigned)ACTOR_COUNT && !unchangedSceneryErrors
               && !pauseErrors && greetingChanges > 20 && !greetingLeaks && exported
               && repairChanges > 0 && !repairLeaks;

    char reportPath[4096];
    int n = snprintf(reportPath, sizeof(reportPath), "%s/verification.json", directory);
    FILE *report = n >= 0 && (size_t)n < sizeof(reportPath) ? fopen(reportPath, "w") : NULL;
    if (report) {
        fprintf(report, "{\n  \"passed\": %s,\n  \"width\": %d,\n  \"height\": %d,\n"
                "  \"reference_differing_pixels\": %u,\n  \"maximum_channel_error\": %d,\n"
                "  \"animated_pixels\": %u,\n  \"moving_avatars\": %u,\n  \"avatar_count\": %d,\n"
                "  \"unchanged_scenery_errors\": %u,\n  \"pause_errors\": %u,\n"
                "  \"greeting_pixels\": %u,\n  \"greeting_leaks\": %u,\n"
                "  \"repair_pixels\": %u,\n  \"repair_leaks\": %u\n}\n",
                passed ? "true" : "false", SCENE_WIDTH, SCENE_HEIGHT, mismatches, maximumError,
                animated, actorsMoving, ACTOR_COUNT, unchangedSceneryErrors, pauseErrors, greetingChanges, greetingLeaks,
                repairChanges, repairLeaks);
        if (fclose(report)) passed = false;
    } else passed = false;
    UnloadImageColors(expected); UnloadImageColors(actual);
    UnloadImageColors(startPixels); UnloadImageColors(laterPixels);
    UnloadImageColors(repeatPixels); UnloadImageColors(greetingPixels);
    UnloadImage(reference); UnloadImage(start); UnloadImage(later);
    UnloadImage(repeat); UnloadImage(greeting);
    return passed ? 0 : 1;
}

static Rectangle Viewport(void)
{
    float fit = fminf((float)GetScreenWidth()/SCENE_WIDTH, (float)GetScreenHeight()/SCENE_HEIGHT);
    float width = roundf(SCENE_WIDTH*fit), height = roundf(SCENE_HEIGHT*fit);
    return (Rectangle){ floorf((GetScreenWidth() - width)*0.5f), floorf((GetScreenHeight() - height)*0.5f), width, height };
}

static void ClampView(View *view)
{
    float halfWidth = SCENE_WIDTH/(2*view->zoom), halfHeight = SCENE_HEIGHT/(2*view->zoom);
    view->center.x = Clamp(view->center.x, halfWidth, SCENE_WIDTH - halfWidth);
    view->center.y = Clamp(view->center.y, halfHeight, SCENE_HEIGHT - halfHeight);
}

static Vector2 ToScene(Vector2 mouse, Rectangle viewport, View view)
{
    return (Vector2){ view.center.x + ((mouse.x - viewport.x)/viewport.width - 0.5f)*SCENE_WIDTH/view.zoom,
                      view.center.y + ((mouse.y - viewport.y)/viewport.height - 0.5f)*SCENE_HEIGHT/view.zoom };
}

static void DrawControls(int selected, bool reference, bool paused)
{
    const int w = 326, h = 228;
    int x = 18, y = 18;
    DrawRectangle(x, y, w, h, (Color){ 17, 35, 27, 239 });
    DrawRectangleLines(x, y, w, h, (Color){ 161, 184, 124, 220 });
    DrawText("GREENFIELD", x + 18, y + 16, 21, (Color){ 241, 224, 169, 255 });
    DrawText(reference ? "Still artwork" : paused ? "Animation paused" : "Living village", x + 18, y + 45, 16, RAYWHITE);
    DrawText("Space  Pause / resume\nR      Still / animated\nClick  Greet a villager\nTab    Next villager\nWheel  Zoom     Right drag  Pan\nHome   Fit      F  Fullscreen\nH      Hide controls", x + 18, y + 75, 15, (Color){ 199, 214, 182, 255 });
    DrawText(ACTORS[selected].name, x + 18, y + 200, 15, (Color){ 247, 210, 128, 255 });
}

int ReferenceMain(int argc, char **argv)
{
    Options options = { 0 };
    if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h"))) {
        Usage(argv[0]);
        return 0;
    }
    if (!ParseOptions(argc, argv, &options)) { Usage(argv[0]); return 2; }
    bool headless = options.verify || options.capture;
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_HIGHDPI |
                   (headless ? FLAG_WINDOW_HIDDEN : FLAG_VSYNC_HINT));
    InitWindow(760, 753, "Greenfield | H: controls");
    SetExitKey(KEY_ESCAPE);
    if (!IsWindowReady()) return 1;
    SetWindowMinSize(360, 356);
    if (!headless) {
        int monitor = GetCurrentMonitor();
        float scale = fminf(1.0f, fminf((GetMonitorWidth(monitor) - 70.0f)/SCENE_WIDTH,
                                       (GetMonitorHeight(monitor) - 110.0f)/SCENE_HEIGHT));
        int width = (int)(SCENE_WIDTH*scale), height = (int)(SCENE_HEIGHT*scale);
        SetWindowSize(width, height);
        SetWindowPosition((GetMonitorWidth(monitor) - width)/2, (GetMonitorHeight(monitor) - height)/2);
        SetTargetFPS(60);
    }

    Scene scene = { 0 };
    Image source = { 0 };
    if (!LoadScene(&scene, &source)) {
        fprintf(stderr, "Cannot load scene. Keep the complete assets directory beside the executable.\n");
        UnloadScene(&scene, source);
        CloseWindow();
        return 1;
    }
    if (headless) {
        int result;
        if (options.verify) result = VerifyScene(&scene, source, options.evidence);
        else {
            RenderScene(&scene, options.time, options.reference, 0, 0);
            Image capture = ReadCanvas(&scene);
            result = ExportImage(capture, options.capture) ? 0 : 1;
            UnloadImage(capture);
        }
        UnloadScene(&scene, source);
        CloseWindow();
        return result;
    }

    View view = { 1, { SCENE_WIDTH*0.5f, SCENE_HEIGHT*0.5f } };
    bool reference = options.reference, paused = false, controls = false;
    float time = options.time, greeting = 0;
    int selected = 0;
    while (!WindowShouldClose()) {
        float dt = fminf(GetFrameTime(), 0.05f);
        Rectangle viewport = Viewport();
        Vector2 mouse = GetMousePosition();
        if (IsKeyPressed(KEY_H)) controls = !controls;
        if (IsKeyPressed(KEY_R)) reference = !reference;
        if (IsKeyPressed(KEY_SPACE)) {
            if (reference) { reference = false; paused = false; }
            else paused = !paused;
        }
        if (IsKeyPressed(KEY_F)) ToggleBorderlessWindowed();
        if (IsKeyPressed(KEY_HOME)) view = (View){ 1, { SCENE_WIDTH*0.5f, SCENE_HEIGHT*0.5f } };
        if (IsKeyPressed(KEY_TAB)) {
            selected = (selected + (IsKeyDown(KEY_LEFT_SHIFT) ? ACTOR_COUNT - 1 : 1))%ACTOR_COUNT;
            greeting = 1;
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mouse, viewport)) {
            Vector2 world = ToScene(mouse, viewport, view);
            for (int i = 0; i < ACTOR_COUNT; i++) {
                if (CheckCollisionPointRec(world, ACTORS[i].bounds)) { selected = i; greeting = 1; break; }
            }
        }
        float wheel = GetMouseWheelMove();
        if (wheel && CheckCollisionPointRec(mouse, viewport)) {
            Vector2 before = ToScene(mouse, viewport, view);
            view.zoom = Clamp(view.zoom*powf(1.15f, wheel), 1.0f, 5.0f);
            Vector2 after = ToScene(mouse, viewport, view);
            view.center = Vector2Add(view.center, Vector2Subtract(before, after));
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            view.center.x -= delta.x*SCENE_WIDTH/(viewport.width*view.zoom);
            view.center.y -= delta.y*SCENE_HEIGHT/(viewport.height*view.zoom);
        }
        view.center.x += (IsKeyDown(KEY_RIGHT) - IsKeyDown(KEY_LEFT))*dt*360/view.zoom;
        view.center.y += (IsKeyDown(KEY_DOWN) - IsKeyDown(KEY_UP))*dt*360/view.zoom;
        ClampView(&view);
        if (!paused && !reference) {
            time += dt;
            greeting = fmaxf(0, greeting - dt*0.45f);
        }
        RenderScene(&scene, time, reference, selected, greeting);

        // Render to an exact-size canvas first, then fit it without stretching.
        float width = SCENE_WIDTH/view.zoom, height = SCENE_HEIGHT/view.zoom;
        Rectangle crop = { view.center.x - width*0.5f, SCENE_HEIGHT - (view.center.y - height*0.5f) - height, width, -height };
        BeginDrawing();
        ClearBackground((Color){ 15, 30, 23, 255 });
        DrawTexturePro(scene.canvas.texture, crop, viewport, (Vector2){ 0, 0 }, 0, WHITE);
        if (controls) DrawControls(selected, reference, paused);
        EndDrawing();
    }
    UnloadScene(&scene, source);
    CloseWindow();
    return 0;
}
