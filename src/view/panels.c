#include "panels.h"
#include "actions.h"
#include "assets.h"
#include "context.h"
#include "widgets.h"

static const char *CITIZEN_NAMES[] = {"Asha", "Tova",  "Oren", "Nara", "Koa",  "Eira", "Bram", "Suri",
                                      "Luma", "Tarin", "Ari",  "Mira", "Ivo",  "Una",  "Ravi", "Nima",
                                      "Arlo", "Yara",  "Finn", "Sena", "Mako", "Eden", "Runa", "Tala"};

static void PanelBase(ViewContext *ui, const char *eyebrow, const char *heading)
{
    DrawRectangle(PANEL_X, 86, SCREEN_W - PANEL_X, SCREEN_H - 86, (Color){35, 48, 39, 255});
    DrawLine(PANEL_X, 86, PANEL_X, SCREEN_H, (Color){95, 105, 78, 255});
    ViewLabel(ui, eyebrow, PANEL_X + 23, 111);
    ViewTitle(ui, heading, PANEL_X + 23, 134, 28, PAPER);
}

static void Objectives(ViewContext *ui)
{
    GameState *g = ui->g;
    PanelBase(ui, "THE FIRST CHAPTER", "A place to belong");
    ViewWrapText(ui, "From a circle around a fire to a village that can endure.", 1153, 178, 260, 16, MUTED);
    const char *names[] = {"Survive the opening winter", "Welcome 20 people",
                           "Give everyone a home",       "Bring in the first harvest",
                           "Store three days of food",   "Defend a raid or make a truce"};
    bool done[] = {g->winterSurvived,   GamePopulation(g) >= 20,     GameHousing(g) >= GamePopulation(g),
                   g->harvestDelivered, GameFoodDaysTenths(g) >= 30, g->raidResolved};
    for (int i = 0; i < 6; i++)
    {
        float y = 241 + i * 51;
        DrawCircleLines(1161, (int)y + 7, 7, done[i] ? SAGE : Fade(MUTED, .6f));
        if (done[i])
            DrawCircle(1161, (int)y + 7, 3, SAGE);
        ViewText(ui, names[i], 1179, y - 1, 16, done[i] ? SAGE : PAPER);
        if (i == 0 && !done[i])
            ViewLabel(ui, TextFormat("%d:%02d until thaw", (WINTER_END - (int)g->tick) / 600,
                             ((WINTER_END - (int)g->tick) / 10) % 60),
                  1179, y + 19);
        if (i == 1)
            ViewLabel(ui, TextFormat("%d of 20 citizens", GamePopulation(g)), 1179, y + 19);
        if (i == 4)
            ViewLabel(ui, TextFormat("%.1f days stored", GameFoodDaysTenths(g) / 10.0), 1179, y + 19);
    }
    DrawLine(1153, 560, 1416, 560, Fade(MUTED, .25f));
    ViewLabel(ui, "NEXT ARRIVALS", 1153, 581);
    ViewText(ui, TextFormat("In %d:%02d", (g->nextArrival - (int)g->tick) / 600,
                    ((g->nextArrival - (int)g->tick) / 10) % 60),
         1153, 602, 19, PAPER);
    ViewWrapText(ui,
        TextFormat("Need 2 spare beds and %d food. Two travelers join at a time.", GamePopulation(g) * 3),
        1153, 633, 260, 16, MUTED);
    ViewLabel(ui, "TRIBE CHRONICLE", 1153, 700);
    for (int i = 0; i < 3 && i < g->eventCount; i++)
    {
        ViewWrapText(ui, g->events[i], 1153, 723 + i * 49, 260, 14, i ? MUTED : PAPER);
    }
}

static void Workforce(ViewContext *ui)
{
    GameState *g = ui->g;
    PanelBase(ui, "EVERY PAIR OF HANDS", "Workforce");
    ViewText(ui, TextFormat("%d %s available for work", GameRoleCount(g, IDLE),
                    GameRoleCount(g, IDLE) == 1 ? "person" : "people"),
         1153, 176, 15, ACCENT);
    for (int r = 0; r < ROLE_COUNT; r++)
    {
        float y = 216 + r * 63;
        ViewText(ui, ROLE_NAMES[r], 1153, y, 16, PAPER);
        ViewLabel(ui, g->areaX[r] < 0 ? "Whole valley" : "Assigned work area", 1153, y + 22);
        if (ViewButton(ui, (Rectangle){1300, y - 4, 26, 27}, "-", false, GameRoleCount(g, r) > 0))
            ViewCommand(ui, CMD_ASSIGN, r, GameRoleCount(g, r) - 1, 0);
        ViewText(ui, TextFormat("%d", GameRoleCount(g, r)), 1335, y, 17, ACCENT);
        if (ViewButton(ui, (Rectangle){1360, y - 4, 26, 27}, "+", false, GameRoleCount(g, IDLE) > 0))
            ViewCommand(ui, CMD_ASSIGN, r, GameRoleCount(g, r) + 1, 0);
        if (ViewButton(ui, (Rectangle){1298, y + 28, 57, 22}, "Area", ui->areaRole == r, true))
            ui->areaRole = ui->areaRole == r ? -1 : r;
        const char *p[] = {"Low", "Norm", "High"};
        if (ViewButton(ui, (Rectangle){1362, y + 28, 54, 22}, p[g->priority[r]], false, true))
            ViewCommand(ui, CMD_PRIORITY, r, (g->priority[r] + 1) % 3, 0);
    }
    ViewWrapText(ui, "High-priority roles take shared work first. Area limits work to nine tiles around your choice.",
             1153, 748, 260, 16, MUTED);
    if (ViewButton(ui, (Rectangle){1153, 815, 126, 33}, "Rally guards", ui->areaRole == GUARD, true))
        ui->areaRole = GUARD;
    if (ViewButton(ui, (Rectangle){1288, 815, 126, 33}, "Clear areas", false, true))
        for (int r = 0; r < ROLE_COUNT; r++)
            ViewCommand(ui, CMD_AREA, r, -1, -1);
}

static void Discoveries(ViewContext *ui)
{
    GameState *g = ui->g;
    PanelBase(ui, "KNOWLEDGE TAKES ROOT", "Discoveries");
    ViewWrapText(ui, "A completed workshop and a crafter turn materials into new possibilities.", 1153, 180, 260, 16,
             MUTED);
    for (int d = 0; d < DISCOVERY_COUNT; d++)
    {
        float y = 245 + d * 144;
        ViewRounded((Rectangle){1153, y, 263, 132}, (Color){45, 59, 46, 255});
        ViewText(ui, DISCOVERIES[d].name, 1166, y + 12, 17, PAPER);
        ViewWrapText(ui, DISCOVERIES[d].description, 1166, y + 39, 236, 14, MUTED);
        ViewCost(ui, DISCOVERIES[d].cost, 1166, y + 77, 236);
        if (g->discovery[d])
            ViewText(ui, "Discovered", 1166, y + 105, 16, SAGE);
        else if (g->research == d)
        {
            ViewBar(1166, y + 106, 232, (float)g->researchProgress / DISCOVERIES[d].labor, ACCENT);
            ViewText(ui, TextFormat("%d%%", g->researchProgress * 100 / DISCOVERIES[d].labor), 1350, y + 13, 16,
                 ACCENT);
        }
        else if (ViewButton(ui, (Rectangle){1300, y + 100, 101, 24}, "Discover", false, g->research < 0))
            ViewCommand(ui, CMD_RESEARCH, d, 0, 0);
    }
}

static void RivalPanel(ViewContext *ui)
{
    GameState *g = ui->g;
    PanelBase(ui, "ACROSS THE RIVER", "The Ashen camp");
    ViewWrapText(ui, "A neighboring tribe watches this valley. They raid supplies when the truce runs out.", 1153,
             181, 260, 16, MUTED);
    ViewSprite(ui->objects, 1, (Vector2){1285, 361}, 145, (Color){226, 201, 174, 255}, false);
    DrawLine(1153, 392, 1416, 392, Fade(MUTED, .3f));
    const char *state = g->raidActive                  ? "Raiders in the valley"
                        : g->truceUntil > (int)g->tick ? "A truce holds"
                        : g->raidWarning               ? "A raid is approaching"
                                                       : "Scouts are watching";
    ViewText(ui, state, 1153, 417, 18, g->raidWarning ? RUST : ACCENT);
    int seconds = (g->truceUntil > (int)g->tick ? g->truceUntil : g->nextRaid) - (int)g->tick;
    if (seconds < 0)
        seconds = 0;
    ViewText(ui, TextFormat("%s %d:%02d", g->truceUntil > (int)g->tick ? "Truce remaining" : "Next known raid",
                    seconds / 600, seconds / 10 % 60),
         1153, 451, 16, PAPER);
    if (g->truceUntil > (int)g->tick)
    {
        int next = g->truceUntil + 1200 - (int)g->tick;
        if (next < 0)
            next = 0;
        ViewText(ui, TextFormat("Next known raid %d:%02d", next / 600, next / 10 % 60), 1153, 476, 14, MUTED);
    }
    ViewWrapText(ui, "Offer 40 food for twelve minutes of peace. An accepted truce also completes the diplomacy "
             "objective.",
             1153, 500, 260, 16, MUTED);
    if (ViewButton(ui, (Rectangle){1153, 581, 263, 43}, "Offer truce  /  40 food", false,
               g->truceUntil <= (int)g->tick))
        ViewCommand(ui, CMD_TRUCE, 0, 0, 0);
    if (ViewButton(ui, (Rectangle){1153, 638, 263, 40}, "Assign guards", false, true))
        ui->panel = 1;
    ViewText(ui, TextFormat("Food taken so far: %d", g->stolen[FOOD]), 1153, 708, 16, PAPER);
    ViewWrapText(ui, "Guards defend near the rally flag. Injured guards withdraw. Palisades block passage; gates "
             "keep your routes open.",
             1153, 746, 260, 16, MUTED);
}

static void Selected(ViewContext *ui)
{
    GameState *g = ui->g;
    PanelBase(ui, "IN YOUR VILLAGE", ui->selectedCitizen >= 0 ? "A member of the tribe"
                                 : ui->selected >= 0      ? "A place with purpose"
                                                         : "The valley");
    if (ui->selectedCitizen >= 0)
    {
        Citizen *c = &g->citizens[ui->selectedCitizen];
        ViewTitle(ui, CITIZEN_NAMES[ui->selectedCitizen % 24], 1153, 195, 26, ACCENT);
        ViewSprite(ui->selectedCitizen % 2 == 0 ? ui->braids : ui->people,
               c->job == JOB_DELIVER ? 8
               : c->job == JOB_FIGHT ? 12
                                     : 0,
               (Vector2){1280, 375}, 88, WHITE, false);
        ViewText(ui, c->role == IDLE ? "Unassigned" : ROLE_NAMES[c->role], 1153, 415, 18, PAPER);
        ViewText(ui, JOB_NAMES[c->job], 1153, 449, 15, MUTED);
        ViewText(ui, TextFormat("Health  %d%%", c->hp / 10), 1153, 499, 16, PAPER);
        ViewBar(1153, 528, 262, c->hp / 1000.0f, SAGE);
        ViewLabel(ui, "CARRYING", 1153, 563);
        int y = 588;
        bool any = false;
        for (int r = 0; r < RESOURCE_COUNT; r++)
            if (c->carry[r])
            {
                ViewText(ui, TextFormat("%d %s", c->carry[r], RESOURCE_NAMES[r]), 1153, (float)y, 15, ACCENT);
                y += 24;
                any = true;
            }
        if (!any)
            ViewText(ui, "Nothing yet", 1153, 588, 16, MUTED);
        ViewWrapText(ui, c->hunger > 900  ? "Hungry. Restore stored food soon to avoid health loss."
                 : c->cold > 1200 ? "Cold. Keep the hearth fueled and build enough shelter."
                                  : "Fed and sheltered citizens slowly regain health.",
                 1153, 668, 260, 16, MUTED);
        if (ViewButton(ui, (Rectangle){1153, 774, 262, 40}, "Manage workforce", false, true))
            ui->panel = 1;
        return;
    }
    if (ui->selected >= 0)
    {
        Building *b = &g->buildings[ui->selected];
        const BuildingDef *d = &BUILDINGS[b->type];
        ViewTitle(ui, b->upgraded ? "Insulated hut" : d->name, 1153, 197, 25, ACCENT);
        int sprite = b->upgraded ? 11 : b->type;
        ViewSprite(ui->objects, sprite, (Vector2){1280, 380}, 155, WHITE, false);
        ViewWrapText(ui, d->description, 1153, 418, 260, 15, PAPER);
        ViewText(ui, b->complete ? "Ready" : TextFormat("Construction  %d%%", b->progress * 100 / d->labor), 1153,
             498, 17, b->complete ? SAGE : ACCENT);
        if (!b->complete)
            ViewBar(1153, 532, 263, (float)b->progress / d->labor, ACCENT);
        if (b->type == SHELTER)
        {
            ViewText(ui, "4 beds", 1153, 554, 17, PAPER);
            if (!b->upgraded &&
                ViewButton(ui, (Rectangle){1153, 600, 263, 43}, "Insulate  /  10 wood, 3 hides", false, b->complete))
                ViewCommand(ui, CMD_UPGRADE, ui->selected, 0, 0);
        }
        if (b->type == FARM)
        {
            ViewText(ui, b->crop == 2   ? "Ready to harvest"
                 : b->crop == 1 ? "Grain is growing"
                                : "Waiting for a farmer",
                 1153, 554, 17, PAPER);
            if (b->crop == 1)
                ViewBar(1153, 588, 263, b->cropProgress / 2400.0f, SAGE);
        }
        if (b->type == WORKSHOP && ViewButton(ui, (Rectangle){1153, 610, 263, 43}, "Open discoveries", false, true))
            ui->panel = 2;
        ViewWrapText(ui, "Select another building or citizen directly in the valley. Construction materials are "
                 "charged once when placed.",
                 1153, 715, 260, 16, MUTED);
        return;
    }
    ViewWrapText(ui, "A river, a sheltered clearing, and enough land to begin again.", 1153, 190, 260, 15, PAPER);
    ViewLabel(ui, "GETTING STARTED", 1153, 266);
    ViewWrapText(ui, "1. Place two shelters near the hearth.\n2. Keep foragers and woodcutters working.\n3. Build a "
             "workshop and assign a crafter.\n4. Discover farming after the thaw.",
             1153, 297, 260, 15, MUTED);
    if (ViewButton(ui, (Rectangle){1153, 521, 263, 42}, "Follow your objectives", false, true))
        ui->panel = 0;
}

void ViewPanelDraw(ViewContext *ui)
{
    switch (ui->panel)
    {
    case 0:
        Objectives(ui);
        break;
    case 1:
        Workforce(ui);
        break;
    case 2:
        Discoveries(ui);
        break;
    case 3:
        RivalPanel(ui);
        break;
    default:
        Selected(ui);
        break;
    }
}
