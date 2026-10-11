#include "screens.h"
#include "actions.h"
#include "context.h"
#include "widgets.h"
#include "world.h"

void ViewMenu(ViewContext *ui)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade((Color){15, 27, 22, 255}, .83f));
    ui->allowInput = true;
    Rectangle card = {438, 137, 565, 624};
    ViewRounded(card, (Color){35, 48, 39, 255});
    ViewBorder(card, Fade(ACCENT, .45f));
    ViewLabel(ui, "A GREENFIELD CHRONICLE", 503, 180);
    ViewTitle(ui, "Founding a Tribe", 501, 215, 42, PAPER);
    ViewWrapText(ui, "The ice is retreating. Your story begins with eight people and the warmth of a single fire.",
             503, 279, 429, 17, MUTED);
    if (ui->confirmNew)
    {
        ViewWrapText(ui,
            "Begin a new village? Your saved slot stays intact until you save or the next autosave runs.",
            503, 382, 427, 17, PAPER);
        if (ViewButton(ui, (Rectangle){503, 480, 427, 47}, "Begin a new village", true, true))
        {
            GameInit(ui->g);
            ui->menu = false;
            ui->confirmNew = false;
            ui->winSeen = false;
            ui->build = ui->selected = ui->selectedCitizen = ui->areaRole = -1;
            ui->speed = 1;
            ui->accumulator = 0;
            ui->lastAutosave = 0;
            ui->autosaveBlocked = false;
            ui->camera.target = ViewIso(23, 23);
            ui->camera.zoom = .86f;
            ui->panel = 0;
        }
        if (ViewButton(ui, (Rectangle){503, 542, 427, 43}, "Back", false, true))
            ui->confirmNew = false;
        return;
    }
    if (ViewButton(ui, (Rectangle){503, 364, 427, 47}, ui->g->tick ? "Resume village" : "Begin your story", true,
               !ui->g->lost))
    {
        ui->menu = false;
        ui->speed = ui->lastSpeed;
    }
    if (ViewButton(ui, (Rectangle){503, 425, 207, 41}, "New Game", false, true))
        ui->confirmNew = true;
    if (ViewButton(ui, (Rectangle){723, 425, 207, 41}, "Continue / Load", false, FileExists(ui->savePath)) &&
        ViewLoad(ui, ui->savePath))
    {
        ui->menu = false;
        ui->speed = 1;
    }
    if (ViewButton(ui, (Rectangle){503, 479, 427, 41}, "Save village   F5", false, !ui->g->lost))
        ViewSave(ui);
    if (ViewButton(ui, (Rectangle){503, 533, 427, 41}, "Reference Viewer", false, true))
        ui->reference = true;
    if (ViewButton(ui, (Rectangle){503, 587, 207, 41}, "Field guide", false, true))
    {
        ui->help = true;
        ui->menu = false;
    }
    if (ViewButton(ui, (Rectangle){723, 587, 207, 41}, "Quit", false, true))
        ui->quit = true;
    ViewLabel(ui, "SINGLE PLAYER    /    OFFLINE    /    ONE LOCAL SAVE", 503, 664);
    ViewWrapText(ui, ui->toastTime > 0 ? ui->toast
             : ui->autosaveBlocked
                 ? "Autosave paused after a load error. Load a usable save or explicitly "
                   "save this village to resume autosaving."
                 : "Autosaves every two minutes of simulation time. New villages reach their "
                   "first chapter in about 45-60 minutes at 1x.",
             503, 692, 426, 14, MUTED);
}

void ViewGuide(ViewContext *ui)
{
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(INK, .89f));
    ui->allowInput = true;
    ViewRounded((Rectangle){252, 90, 936, 720}, (Color){35, 48, 39, 255});
    ViewLabel(ui, "A FEW THINGS TO CARRY WITH YOU", 290, 127);
    ViewTitle(ui, "The field guide", 288, 160, 40, PAPER);
    const char *headings[] = {"01 / Shelter & survival", "02 / Work & deliveries", "03 / Discover & grow",
                              "04 / Share the valley"};
    const char *copy[] = {
        "Build two shelters for your eight founders. The hearth burns one wood every 20 seconds in winter. "
        "Everyone eats one food per minute. Shortages warn you before hunger or cold begins to harm health. "
        "Food and warmth restore health.",
        "Assign roles in Workforce. Reduce a role to free people, then add them elsewhere. Workers walk to "
        "resources, work and carry goods to storage. Construction costs are paid once, when placed. A "
        "blocked route is retried when the path opens.",
        "Build a workshop and assign a crafter. Discover better tools, insulation and storage. After the "
        "twelve-minute winter, discover Farming. Build on fertile river soil and assign farmers. Travelers "
        "arrive when you have spare beds and a food reserve.",
        "Raid warnings give 90 seconds to prepare. Guards defend near the rally flag; a watch post moves the "
        "rally. Pay 40 food for twelve minutes of truce. Secure a harvest, 20 people, beds and three days of "
        "food to found your village."};
    for (int i = 0; i < 4; i++)
    {
        float x = 290 + (i % 2) * 455, y = 240 + (i / 2) * 205;
        ViewText(ui, headings[i], x, y, 21, ACCENT);
        ViewWrapText(ui, copy[i], x, y + 40, 401, 16, PAPER);
    }
    ViewText(ui, "Space pause   1/2/4 speed   B build   W workers   R discoveries   O objectives", 290, 693, 16,
         MUTED);
    ViewText(ui, "Wheel zoom   Right drag / arrows pan   Home recenter   M whole valley   G grid", 290, 720, 16,
         MUTED);
    ViewText(ui, "F5 save   F9 load   Esc menu or cancel   F fullscreen", 290, 747, 16, MUTED);
    if (ViewButton(ui, (Rectangle){974, 740, 173, 41}, "Back to the valley", true, true))
        ui->help = false;
}

void ViewEnding(ViewContext *ui)
{
    GameState *g = ui->g;
    DrawRectangle(0, 0, SCREEN_W, SCREEN_H, Fade(INK, .83f));
    ui->allowInput = true;
    ViewRounded((Rectangle){414, 241, 612, 399}, (Color){35, 48, 39, 255});
    ViewLabel(ui, g->lost ? "THE FIRE HAS GONE OUT" : "CHAPTER ONE COMPLETE", 463, 281);
    ViewTitle(ui, g->lost ? "A quiet valley" : "A village is born", 459, 326, 43, PAPER);
    ViewWrapText(ui, g->lost ? "No one remains to tend the hearth. You can return to your last save, or lead a new "
                       "tribe into the valley."
                     : "Your people have survived the cold, raised a harvest and made a home. The story is "
                       "yours to continue.",
             463, 396, 511, 19, MUTED);
    if (!g->lost && ViewButton(ui, (Rectangle){463, 527, 513, 49}, "Continue building", true, true))
    {
        ui->winSeen = true;
        ui->speed = 1;
    }
    if (g->lost)
    {
        if (ViewButton(ui, (Rectangle){463, 527, 247, 49}, "Load village", true, FileExists(ui->savePath)))
            ViewLoad(ui, ui->savePath);
        if (ViewButton(ui, (Rectangle){728, 527, 248, 49}, "Restart", false, true))
        {
            GameInit(g);
            ui->speed = 1;
            ui->winSeen = false;
            ui->lastAutosave = 0;
            ui->autosaveBlocked = false;
            ui->selected = ui->selectedCitizen = -1;
        }
    }
}
