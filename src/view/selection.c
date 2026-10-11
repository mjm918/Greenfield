#include "selection.h"
#include "actions.h"
#include "assets.h"
#include "hud.h"
#include "layout.h"
#include "widgets.h"
#include "world.h"
#include <stddef.h>

static bool Action(ViewContext *ui, int slot, const char *name, bool enabled, const char *hint,
                   const int *cost)
{
    Rectangle r = ViewCommandRect(ui, slot);
    bool clicked = ViewButton(ui, r, name, false, enabled);
    if (ui->allowInput && ViewHit(ui, r) && hint)
    {
        Rectangle h = ViewCommandHintRect(ui);
        ViewRounded(h, INK);
        ViewBorder(h, Fade(ACCENT, .35f));
        ViewTitle(ui, name, h.x + 12, h.y + 10, 19, ACCENT);
        ViewWrapText(ui, hint, h.x + 12, h.y + 37, h.width - 24, 16, PAPER);
        if (cost)
            ViewCost(ui, cost, h.x + 12, h.y + 87, h.width - 24);
    }
    return clicked;
}
static bool Command(ViewContext *ui, GameCommand cmd)
{
    char why[256] = {0};
    bool ok = GameCommandApply(ui->g, cmd, why, sizeof(why));
    if (!ok)
        ViewToast(ui, why);
    return ok;
}
static void OrderButton(ViewContext *ui, int slot, const char *text, int type, const char *hint)
{
    if (Action(ui, slot, text, true, hint, NULL))
    {
        ui->orderMode = ui->orderMode == type ? 0 : type;
        ui->build = -1;
        if (ui->orderMode == ORDER_GATHER)
            ViewToast(ui, "Gather selected. Left-click a tree, berry bush, deposit or animal.");
    }
    if (ui->orderMode == type)
        ViewSelectedBorder(ViewCommandRect(ui, slot));
}
static void Orders(ViewContext *ui)
{
    GameState *g = ui->g;
    int workers = ViewSelectedWorkers(ui);
    if (ui->commandPage)
    {
        bool school = GameBuildingCount(g, TRAINING_GROUND, true) > 0;
        const char *names[] = {"", "Spearman", "Slinger", "Axeman"};
        for (int u = SPEARMAN; u < UNIT_COUNT; u++)
            if (Action(ui, u - 1, names[u], school && workers,
                       u == SPEARMAN ? "Train an existing civilian. Counters slingers. 30 seconds."
                       : u == SLINGER
                           ? "Train an existing civilian. Counters axemen. 30 seconds."
                           : "Train an existing civilian. Counters spears and damages buildings. 30 seconds.",
                       UNITS[u].cost))
                ViewIssueSelected(ui, CMD_TRAIN, u, (UnitOrder){0}, false);
        if (Action(ui, 4, "Bronze equipment", school && g->factions[0].age == BRONZE_AGE,
                   "Equip selected soldiers at a training ground. Cost per soldier.", EQUIPMENT_COST))
            ViewIssueSelected(ui, CMD_EQUIP, 0, (UnitOrder){0}, false);
        if (Action(ui, 5, "Demobilize", workers < ui->selectionCount,
                   "Return selected soldiers to civilian work.", NULL))
            ViewIssueSelected(ui, CMD_DEMOBILIZE, 0, (UnitOrder){0}, false);
        if (Action(ui, 3, "Back", true, NULL, NULL))
            ui->commandPage = 0;
        return;
    }
    if (workers)
    {
        if (Action(ui, 0, "Build  B", true,
                   "Open the construction palette. Select a building, then place it in the valley.", NULL))
        {
            ViewBuildToggle(ui);
            return;
        }
        OrderButton(ui, 1, "Gather", ORDER_GATHER, "Click a natural resource. Shift appends the order.");
        OrderButton(ui, 2, "Build / repair", ORDER_REPAIR,
                    "Click an unfinished site to resume building, or a damaged building to repair it.");
        OrderButton(ui, 3, "Deliver", ORDER_DELIVER,
                    "Deliver carried goods to the nearest available friendly depot.");
        if (Action(ui, 4, "Workforce  W", true, "Assign jobs to selected civilians or find idle workers.",
                   NULL))
            ViewTogglePanel(ui, 1);
    }
    else
    {
        OrderButton(ui, 0, "Move", ORDER_MOVE,
                    "Click a destination. Right-click also gives contextual orders.");
        OrderButton(ui, 1, "Attack-move  A", ORDER_ATTACK_MOVE,
                    "Move to a destination, engaging enemies on the way.");
        OrderButton(ui, 2, "Defend  D", ORDER_DEFEND, "Guard an area and respond to nearby enemies.");
        OrderButton(ui, 3, "Plunder  L", ORDER_PLUNDER,
                    "Click an enemy depot. Loot must be carried home to become usable.");
        if (Action(ui, 4, "Hold  X", true, "Hold the current position and engage enemies in range.", NULL))
            ViewHoldSelected(ui);
    }
    if (Action(ui, 5, "Stop  S", true, "Clear queued orders and stop working or moving.", NULL))
        ViewIssueSelected(ui, CMD_STOP, 0, (UnitOrder){0}, false);
    if (Action(ui, 6, "Retreat  T", true, "Return to the communal center's rally area.", NULL))
        ViewIssueSelected(
            ui, CMD_ORDER, 0,
            (UnitOrder){.type = ORDER_RETREAT, .x = g->factions[0].rallyX, .y = g->factions[0].rallyY},
            false);
    if (Action(ui, 7, "Training", true, "Train civilians, equip soldiers, or return soldiers to work.", NULL))
        ui->commandPage = 1;
}
static void Queue(ViewContext *ui, Building *b)
{
    if (!b->queueCount)
        return;
    Rectangle tray = ViewProductionRect(ui);
    ViewRounded(tray, INK);
    ViewBorder(tray, Fade(ACCENT, .35f));
    for (int q = 0; q < b->queueCount; q++)
    {
        Production *p = &b->queue[q];
        Rectangle r = ViewQueueItemRect(ui, q);
        const char *name = p->type == QUEUE_RECRUIT ? "Adult" : p->type == QUEUE_TRAIN ? "Train" : "Equip";
        if (ViewButton(ui, r, name, false, !b->owner))
        {
            Command(ui, (GameCommand){
                            .type = CMD_CANCEL, .a = q, .entity = GameBuildingRef(ui->g, ui->selected)});
            return;
        }
        ViewBar(r.x + 4, r.y + 28, r.width - 8, (float)p->progress / p->duration, ACCENT);
        if (ui->allowInput && ViewHit(ui, r))
        {
            Rectangle h = {tray.x, tray.y - 36, 340, 30};
            ViewRounded(h, INK);
            ViewText(
                ui, TextFormat("%s %d%% / click to cancel and refund", name, p->progress * 100 / p->duration),
                h.x + 10, h.y + 7, 16, PAPER);
        }
    }
}
static void BuildingCommands(ViewContext *ui, Building *b)
{
    GameState *g = ui->g;
    Faction *f = &g->factions[0];
    float y = ViewSelectionRect(ui).y;
    if (b->owner)
        return;
    if (!b->complete)
    {
        if (Action(ui, 0, "Resume building", true,
                   "Assign the nearest idle worker. Or select workers and right-click this site. Reserved "
                   "materials are reused.",
                   NULL))
            ViewResumeConstruction(ui);
        if (Action(ui, 1, "Cancel building", true,
                   "Remove this construction site and refund its reserved materials.", NULL))
            Command(ui,
                    (GameCommand){.type = CMD_CANCEL, .a = -3, .entity = GameBuildingRef(g, ui->selected)});
        return;
    }
    if (b->type == HEARTH)
    {
        if (Action(ui, 0, "Recruit adult", true,
                   "Recruit an adult. Requires spare housing. Recruits follow the rally instruction.",
                   RECRUIT_COST))
            Command(ui, (GameCommand){.type = CMD_RECRUIT, .entity = GameBuildingRef(g, ui->selected)});
        char why[256];
        bool ready = GameCanAdvance(g, 0, why, sizeof(why));
        if (f->advancing)
        {
            if (Action(ui, 1, "Cancel upgrade", true, "Refund the center advancement project.", NULL))
                ViewCommand(ui, CMD_CANCEL, -2, 0, 0);
        }
        else if (Action(ui, 1, "Advance age", ready, why, f->age < BRONZE_AGE ? AGES[f->age + 1].cost : NULL))
            ViewCommand(ui, CMD_ADVANCE, 0, 0, 0);
        if (Action(
                ui, 2, "Set rally point", true,
                "Click a resource, workplace or destination for new recruits. Right-click also sets rally.",
                NULL))
            ui->orderMode = VIEW_ORDER_RALLY;
        if (Action(ui, 3, "Objectives  O", true, "View milestones and advancement requirements.", NULL))
            ViewTogglePanel(ui, 0);
        if (f->advancing)
        {
            ViewText(ui, TextFormat("Upgrading: %d%%", f->advanceProgress * 100 / AGES[f->age + 1].labor),
                     304, y + 56, 16, ACCENT);
            ViewBar(304, y + 82, 564, (float)f->advanceProgress / AGES[f->age + 1].labor, ACCENT);
        }
        else
            ViewText(ui,
                     TextFormat("Food reserve %.1f days / %d of %d beds used", GameFoodDaysTenths(g) / 10.f,
                                GamePopulation(g), GameHousing(g)),
                     304, y + 62, 16, MUTED);
    }
    else if (b->type == WORKSHOP)
    {
        if (Action(ui, 0, "Discoveries  R", true,
                   "Open the technology tree. Research requires a staffed workshop.", NULL))
            ViewTogglePanel(ui, 2);
        if (Action(ui, 1, "Workforce  W", true,
                   "Select workers and assign crafters, or right-click this workshop.", NULL))
            ViewTogglePanel(ui, 1);
        if (f->research >= 0)
        {
            ViewText(ui, DISCOVERIES[f->research].name, 304, y + 56, 16, ACCENT);
            ViewBar(304, y + 82, 278, (float)f->researchProgress / DISCOVERIES[f->research].labor, ACCENT);
        }
    }
    else if (b->type == SHELTER)
    {
        if (Action(ui, 0, b->upgraded ? "Insulated" : "Insulate shelter",
                   !b->upgraded && f->discovery[INSULATION],
                   "Research Insulation first. Each shelter upgrade costs 10 wood and 3 hides.", NULL))
            ViewCommand(ui, CMD_UPGRADE, ui->selected, 0, 0);
    }
}
static void SettlementCommands(ViewContext *ui)
{
    GameState *g = ui->g;
    Rectangle production = ViewProductionRect(ui);
    if (production.width)
    {
        ViewRounded(production, INK);
        ViewBorder(production, Fade(ACCENT, .35f));
        int row = 0;
        for (int i = 0; i < g->buildingCount && row < 3; i++)
        {
            Building *b = &g->buildings[i];
            if (!b->alive || b->owner || !b->queueCount)
                continue;
            if (ViewButton(ui, (Rectangle){production.x + 6 + row * 190, production.y + 5, 184, 30},
                           TextFormat("%s / %d queued", BUILDINGS[b->type].name, b->queueCount), false, true))
            {
                ViewSelectBuilding(ui, i);
                return;
            }
            row++;
        }
    }
    if (Action(ui, 0, "Communal center", g->factions[0].center >= 0,
               "Select the center to recruit citizens and advance ages.", NULL))
    {
        ViewSelectBuilding(ui, g->factions[0].center);
        return;
    }
    if (Action(ui, 1, "Idle workers", true, "Select all idle civilians.", NULL))
    {
        ViewSelectIdle(ui);
        return;
    }
    if (Action(ui, 2, "Workforce  W", true, "Inspect jobs and assign selected workers.", NULL))
        ViewTogglePanel(ui, 1);
    if (Action(ui, 3, "Objectives  O", true, "View milestones and advancement requirements.", NULL))
        ViewTogglePanel(ui, 0);
    if (Action(ui, 4, "Discoveries  R", true, "View technologies and research progress.", NULL))
        ViewTogglePanel(ui, 2);
    if (Action(ui, 5, "Diplomacy", true, "Negotiate a truce or a lasting treaty.", NULL))
        ViewTogglePanel(ui, 3);
}

void ViewSelectionDraw(ViewContext *ui)
{
    GameState *g = ui->g;
    Rectangle tray = ViewSelectionRect(ui);
    float x = tray.x + 12, y = tray.y;
    ViewRounded(tray, (Color){28, 39, 33, 255});
    ViewBorder(tray, Fade(ACCENT, .45f));
    if ((ui->selectionCount || ViewSelectedBuilding(ui)) && tray.width > 280)
        DrawLine(292, y + 12, 292, SCREEN_H - 20, Fade(MUTED, .2f));
    if (ui->selectionCount)
    {
        if (ui->buildMenu)
        {
            ViewBuildPalette(ui);
            return;
        }
        Citizen *c = &g->citizens[ui->selection[0].slot];
        int workers = ViewSelectedWorkers(ui), soldiers = 0, hp = 0;
        for (int i = 0; i < ui->selectionCount; i++)
        {
            hp += g->citizens[ui->selection[i].slot].hp;
            soldiers += g->citizens[ui->selection[i].slot].unitType != CIVILIAN;
        }
        ViewTitle(ui,
                  ui->selectionCount == 1 ? UNIT_NAMES[c->unitType]
                                          : TextFormat("%d citizens selected", ui->selectionCount),
                  x, y + 10, 22, PAPER);
        ViewText(ui,
                 ui->commandPage           ? "Training and equipment"
                 : ui->selectionCount == 1 ? JOB_NAMES[c->job]
                 : workers + soldiers == ui->selectionCount
                     ? TextFormat("%d workers / %d soldiers", workers, soldiers)
                     : TextFormat("%d workers / %d other", workers, ui->selectionCount - workers),
                 x, y + 38, 17, MUTED);
        ViewBar(x, y + 65, 252, (float)hp / (1000 * ui->selectionCount), SAGE);
        ViewText(ui, TextFormat("Health %d%% / %d orders", hp / (10 * ui->selectionCount), c->orderCount), x,
                 y + 76, 16, PAPER);
        Orders(ui);
    }
    else if (ViewSelectedBuilding(ui))
    {
        Building *b = &g->buildings[ui->selected];
        ViewTitle(ui,
                  b->type == HEARTH && g->factions[b->owner].age >= FARMING_AGE
                      ? (g->factions[b->owner].age == BRONZE_AGE ? "Council Hall" : "Village center")
                      : BUILDINGS[b->type].name,
                  x, y + 10, 22, PAPER);
        const char *status =
            b->owner       ? "Ashen tribe"
            : !b->complete ? TextFormat("Construction %d%%", b->progress * 100 / BUILDINGS[b->type].labor)
            : b->type == TRAINING_GROUND ? "Select citizens to train"
            : b->type == FARM            ? (b->crop == 2   ? "Ready to harvest"
                                            : b->crop == 1 ? "Grain growing"
                                                           : "Reseeding: 2 wood")
                                         : "Your settlement";
        ViewText(ui, status, x, y + 38, 17, b->owner ? RUST : MUTED);
        ViewBar(x, y + 65, 252, (float)b->hp / GameBuildingMaxHealth(g, b), SAGE);
        ViewText(ui, TextFormat("Health %d / %d", b->hp, GameBuildingMaxHealth(g, b)), x, y + 76, 16, PAPER);
        Queue(ui, b);
        BuildingCommands(ui, b);
        if (ui->allowInput && ViewHit(ui, (Rectangle){tray.x, tray.y, 280, tray.height}))
        {
            Rectangle h = ViewCommandHintRect(ui);
            ViewRounded(h, INK);
            ViewTitle(ui, BUILDINGS[b->type].name, h.x + 12, h.y + 10, 19, ACCENT);
            ViewWrapText(ui, BUILDINGS[b->type].description, h.x + 12, h.y + 38, h.width - 24, 16, PAPER);
            ViewText(ui,
                     b->owner ? "Select your units and right-click to attack."
                              : "Right-click with workers to build, work, repair or deliver.",
                     h.x + 12, h.y + 88, 16, MUTED);
        }
    }
    else
        SettlementCommands(ui);
}
