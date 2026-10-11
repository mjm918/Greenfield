#include "panels.h"
#include "actions.h"
#include "layout.h"
#include "widgets.h"

static void Objectives(ViewContext *ui)
{
    GameState *g = ui->g;
    Faction *f = &g->factions[0];
    ViewTitle(ui, f->village ? "Establish your country" : "Found a permanent village", 232, 272, 24, PAPER);
    const char *names[] = {"Reach Bronze Age and complete the Council Hall", "House 50 living citizens",
                           "Store three days of food", "Secure both marked resource sites",
                           "Sign a treaty or obtain the rival's surrender"};
    bool done[] = {f->age == BRONZE_AGE && f->center >= 0 && g->buildings[f->center].complete,
                   GamePopulation(g) >= 50 && GameHousing(g) >= GamePopulation(g),
                   GameFoodDaysTenths(g) >= 30, GameSiteControl(g, 0) == 2,
                   g->treaty || g->factions[1].defeated};
    if (!f->village)
    {
        names[0] = "Survive the opening winter";
        names[1] = "House 20 living citizens";
        names[2] = "Deliver your first harvest";
        names[3] = "Store three days of food";
        names[4] = "Defend a raid or agree a truce";
        done[0] = g->winterSurvived;
        done[1] = GamePopulation(g) >= 20 && GameHousing(g) >= GamePopulation(g);
        done[2] = f->harvestDelivered;
        done[3] = GameFoodDaysTenths(g) >= 30;
        done[4] = f->raidResolved;
    }
    for (int i = 0; i < 5; i++)
    {
        float y = 327 + i * 47;
        DrawCircleLines(240, y + 8, 7, done[i] ? SAGE : MUTED);
        if (done[i])
            DrawCircle(240, y + 8, 3, SAGE);
        ViewText(ui, names[i], 260, y, 17, done[i] ? SAGE : PAPER);
    }
    ViewText(ui,
             TextFormat("People %d/%d   Beds %d   Food %.1f days", GamePopulation(g), f->village ? 50 : 20,
                        GameHousing(g), GameFoodDaysTenths(g) / 10.f),
             232, 593, 18, ACCENT);
    ViewText(ui, TextFormat("Camp warmth %d%%   /   Resource sites %d/2", f->warmth, GameSiteControl(g, 0)),
             232, 625, 17, MUTED);
    DrawLine(726, 272, 726, 658, Fade(MUTED, .25f));
    ViewLabel(ui, "NEXT AGE", 756, 277);
    if (f->age == BRONZE_AGE)
    {
        ViewTitle(ui, "Bronze Age", 756, 306, 26, ACCENT);
        ViewWrapText(ui,
                     "Your Council Hall leads the valley. Complete the country objectives, then continue "
                     "building in the same world.",
                     756, 359, 448, 18, PAPER);
        return;
    }
    const AgeDef *next = &AGES[f->age + 1];
    ViewTitle(ui, next->name, 756, 306, 26, ACCENT);
    ViewCost(ui, next->cost, 756, 353, 448);
    char why[256];
    bool ready = GameCanAdvance(g, 0, why, sizeof(why));
    ViewWrapText(ui, f->advancing ? "Builders are upgrading the communal center." : why, 756, 404, 448, 18,
                 PAPER);
    if (f->advancing)
    {
        ViewBar(756, 508, 448, (float)f->advanceProgress / next->labor, ACCENT);
        if (ViewButton(ui, (Rectangle){756, 548, 448, 42}, "Cancel advancement and refund", false, true))
            ViewCommand(ui, CMD_CANCEL, -2, 0, 0);
    }
    else if (ViewButton(ui, (Rectangle){756, 548, 448, 42}, "Advance age", false, ready))
        ViewCommand(ui, CMD_ADVANCE, 0, 0, 0);
    ViewWrapText(ui, "Upgrades unlock new choices. Buildings and equipment still require investment.", 756,
                 616, 448, 16, MUTED);
}
static void Workforce(ViewContext *ui)
{
    ViewText(
        ui,
        TextFormat("%d selected workers. Counts below show actual current roles.", ViewSelectedWorkers(ui)),
        232, 273, 18, PAPER);
    for (int r = 0; r < ROLE_COUNT; r++)
    {
        float x = 232 + r % 3 * 328, y = 313 + r / 3 * 99;
        ViewRounded((Rectangle){x, y, 313, 88}, (Color){44, 59, 47, 255});
        ViewTitle(ui, TextFormat("%s  /  %d", ROLE_NAMES[r], GameRoleCount(ui->g, r)), x + 14, y + 13, 19,
                  PAPER);
        if (ViewButton(ui, (Rectangle){x + 14, y + 45, 285, 31}, "Assign selected workers", false,
                       ViewSelectedWorkers(ui) > 0))
            ViewIssueSelected(ui, CMD_ROLE, r, (UnitOrder){0}, false);
    }
    if (ViewButton(ui, (Rectangle){232, 629, 280, 38}, "Select idle citizens", false, true))
        ViewSelectIdle(ui);
    ViewText(ui, "Right-click resources for precise work areas. Soldiers still need food and housing.", 535,
             639, 16, MUTED);
}
static void Discoveries(ViewContext *ui)
{
    Faction *f = &ui->g->factions[0];
    ViewText(ui, "Staff a workshop. Discoveries require reserved materials and actual crafter labor.", 232,
             268, 18, PAPER);
    for (int d = 0; d < DISCOVERY_COUNT; d++)
    {
        float x = 232 + d % 4 * 246, y = 310 + d / 4 * 180;
        ViewRounded((Rectangle){x, y, 232, 166}, (Color){44, 59, 47, 255});
        ViewTitle(ui, DISCOVERIES[d].name, x + 12, y + 12, 19, PAPER);
        ViewCost(ui, DISCOVERIES[d].cost, x + 12, y + 42, 208);
        ViewWrapText(ui, DISCOVERIES[d].description, x + 12, y + 80, 208, 15, MUTED);
        if (f->discovery[d])
            ViewText(ui, "Discovered", x + 12, y + 137, 17, SAGE);
        else if (f->research == d)
        {
            ViewBar(x + 12, y + 145, 111, (float)f->researchProgress / DISCOVERIES[d].labor, ACCENT);
            if (ViewButton(ui, (Rectangle){x + 133, y + 132, 87, 28}, "Cancel", false, true))
                ViewCommand(ui, CMD_CANCEL, -1, 0, 0);
        }
        else if (ViewButton(ui, (Rectangle){x + 12, y + 132, 208, 28}, "Research", false, f->research < 0))
            ViewCommand(ui, CMD_RESEARCH, d, 0, 0);
    }
    ViewWrapText(ui,
                 "Cultivation + storage unlock advancement to Farming Village. Farms unlock when advancement "
                 "completes.",
                 982, 512, 216, 17, MUTED);
}
static void Rivals(ViewContext *ui)
{
    GameState *g = ui->g;
    Faction *f = &g->factions[1];
    ViewTitle(ui, "The Ashen tribe", 232, 272, 26, PAPER);
    ViewText(ui,
             TextFormat("%s  /  %d people  /  %d beds", AGES[f->age].name, GamePopulationFor(g, 1),
                        GameHousingFor(g, 1)),
             232, 314, 18, ACCENT);
    const char *state = f->defeated                    ? "The rival has surrendered"
                        : g->treaty                    ? "A lasting treaty holds"
                        : g->truceUntil > (int)g->tick ? "A temporary truce holds"
                        : f->raidActive                ? "Raiders in the valley"
                        : f->raidWarning               ? "A raid is mustering"
                                                       : "Relations are hostile";
    ViewTitle(ui, state, 232, 373, 23, ACCENT);
    int until = (g->truceUntil > (int)g->tick ? g->truceUntil : f->nextRaid) - (int)g->tick;
    if (until < 0)
        until = 0;
    if (!g->treaty && !f->defeated)
        ViewText(ui,
                 TextFormat("%s %d:%02d",
                            g->truceUntil > (int)g->tick ? "Truce remaining" : "Next threat window",
                            until / 600, until / 10 % 60),
                 232, 418, 18, PAPER);
    ViewWrapText(ui,
                 "Their workers gather, build and train. Interrupting deliveries or production weakens their "
                 "economy and army.",
                 232, 482, 445, 18, PAPER);
    ViewWrapText(
        ui, "Use L, then click an enemy depot to plunder. Destroy their communal center to force surrender.",
        232, 587, 445, 17, MUTED);
    DrawLine(716, 272, 716, 661, Fade(MUTED, .25f));
    ViewTitle(ui, "Negotiate peace", 750, 272, 24, PAPER);
    if (ViewButton(ui, (Rectangle){750, 324, 450, 42},
                   TextFormat("Temporary truce  /  %d food", GameTruceCost(g)), false,
                   !g->treaty && !f->defeated && g->truceUntil <= (int)g->tick))
        ViewCommand(ui, CMD_TRUCE, 0, 0, 0);
    ViewWrapText(
        ui, "Twelve minutes of peace. Tribute goes to the rival's stores. Attacks are blocked during peace.",
        750, 390, 450, 18, PAPER);
    char why[256];
    bool ready = GameCanTreaty(g, why, sizeof(why));
    if (ViewButton(ui, (Rectangle){750, 491, 450, 42}, "Lasting treaty  /  200 food + 30 bronze", false,
                   ready))
        ViewCommand(ui, CMD_TREATY, 0, 0, 0);
    ViewWrapText(ui, why, 750, 559, 450, 18, MUTED);
}
void ViewPanelDraw(ViewContext *ui)
{
    if (!ViewPanelOpen(ui))
        return;
    bool interactive = !ui->menu && !ui->help && !ui->g->lost && (!ui->g->won || ui->winSeen);
    ui->allowInput = interactive;
    DrawRectangle(0, WORLD_TOP, SCREEN_W, SCREEN_H - WORLD_TOP, Fade(INK, .64f));
    Rectangle r = ViewDialogRect();
    ViewRounded(r, (Color){33, 47, 39, 255});
    ViewBorder(r, Fade(ACCENT, .45f));
    ViewTitle(ui, "Settlement ledger", 232, 174, 28, PAPER);
    if (ViewButton(ui, (Rectangle){1090, 172, 110, 34}, "Close  Esc", false, true))
        ui->panel = -1;
    const char *tabs[] = {"Objectives  O", "Workforce  W", "Discoveries  R", "Diplomacy"};
    for (int i = 0; i < 4; i++)
        if (ViewButton(ui, (Rectangle){232 + i * 246, 220, 232, 32}, tabs[i], ui->panel == i, true))
            ui->panel = i;
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
        Rivals(ui);
        break;
    default:
        break;
    }
    // Feedback remains visible above the modal, including authoritative validation failures.
    if (ui->toastTime > 0)
    {
        ViewRounded((Rectangle){330, 710, 780, 66}, INK);
        ViewWrapText(ui, ui->toast, 346, 725, 748, 17, PAPER);
    }
}
