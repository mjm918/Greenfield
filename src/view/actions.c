#include "actions.h"
#include "context.h"
#include "widgets.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

bool ViewCommand(ViewContext *ui, int type, int a, int b, int c)
{
    char reason[256];
    bool ok =
        GameCommandApply(ui->g, (GameCommand){.type = type, .a = a, .b = b, .c = c}, reason, sizeof(reason));
    if (!ok)
        ViewToast(ui, reason);
    return ok;
}

void ViewSaveFolder(ViewContext *ui)
{
    const char *home = getenv("HOME");
    if (!home)
        home = ".";
#ifdef __APPLE__
    snprintf(ui->savePath, sizeof(ui->savePath), "%s/Library/Application Support/Greenfield/country.gfv2",
             home);
#else
    snprintf(ui->savePath, sizeof(ui->savePath), "%s/.greenfield/country.gfv2", home);
#endif
}

bool ViewSave(ViewContext *ui)
{
    // One attempt per autosave interval, even if the filesystem is unavailable.
    ui->lastAutosave = ui->g->tick;
    char parent[4096];
    snprintf(parent, sizeof(parent), "%s", ui->savePath);
    char *slash = strrchr(parent, '/');
    if (slash)
        slash[slash == parent ? 1 : 0] = 0;
    else
        snprintf(parent, sizeof(parent), ".");
    if (mkdir(parent, 0700) && errno != EEXIST)
    {
        ViewToast(ui, "Could not create the save folder. Your previous save is untouched.");
        return false;
    }
    char why[256];
    bool ok = GameSave(ui->g, ui->savePath, why, sizeof(why));
    ViewToast(ui, why);
    if (ok)
        ui->autosaveBlocked = false;
    return ok;
}

bool ViewLoad(ViewContext *ui, const char *path)
{
    char why[256];
    bool ok = GameLoad(ui->g, path, why, sizeof(why));
    ViewToast(ui, why);
    if (ok)
    {
        ui->selectionCount = 0;
        ui->orderMode = 0;
        ui->dragging = false;
        ui->buildMenu = false;
        ui->commandPage = 0;
        ui->panel = -1;
        for (int i = 0; i < 10; i++)
            ui->groupCount[i] = 0;
        ui->selected = ui->selectedCitizen = ui->build = ui->areaRole = -1;
        ui->accumulator = 0;
        ui->lastAutosave = ui->g->tick;
        ui->winSeen = ui->g->won;
        ui->speed = 0;
        ui->autosaveBlocked = false;
    }
    else
    {
        // Preserve the failed slot until a deliberate save, successful load or new game.
        ui->autosaveBlocked = true;
        ui->speed = 0;
        char message[256];
        snprintf(message, sizeof(message), "%.165s Autosave paused. F5 saves the current village.", why);
        ViewToast(ui, message);
    }
    return ok;
}

int ViewSelectedWorkers(const ViewContext *ui)
{
    int count = 0;
    for (int i = 0; i < ui->selectionCount; i++)
        if (GameCitizenValid(ui->g, ui->selection[i]))
        {
            const Citizen *c = &ui->g->citizens[ui->selection[i].slot];
            count += !c->owner && c->unitType == CIVILIAN && c->training < 0;
        }
    return count;
}

void ViewSelectIdle(ViewContext *ui)
{
    ui->selectionCount = 0;
    for (int i = 0; i < ui->g->citizenCount; i++)
    {
        Citizen *c = &ui->g->citizens[i];
        if (c->alive && !c->owner && c->unitType == CIVILIAN && c->role == IDLE && c->training < 0)
            ui->selection[ui->selectionCount++] = GameCitizenRef(ui->g, i);
    }
    ui->selected = -1;
    ui->panel = -1;
    ui->buildMenu = false;
    ui->build = -1;
    ui->commandPage = ui->orderMode = 0;
}

void ViewSelectBuilding(ViewContext *ui, int id)
{
    ui->selectionCount = 0;
    ui->selectedCitizen = -1;
    ui->selected = id;
    ui->selectedGeneration = id >= 0 ? ui->g->buildings[id].generation : 0;
    ui->panel = -1;
    ui->buildMenu = false;
    ui->build = -1;
    ui->commandPage = ui->orderMode = 0;
}

void ViewHoldSelected(ViewContext *ui)
{
    for (int i = 0; i < ui->selectionCount; i++)
    {
        Citizen *c = &ui->g->citizens[ui->selection[i].slot];
        GameCommandApply(ui->g,
                         (GameCommand){.type = CMD_ORDER,
                                       .entity = ui->selection[i],
                                       .order = {.type = ORDER_HOLD, .x = c->x / 256, .y = c->y / 256}},
                         NULL, 0);
    }
}

void ViewTogglePanel(ViewContext *ui, int panel)
{
    ui->panel = ui->panel == panel ? -1 : panel;
    ui->dragging = false;
    ui->buildMenu = false;
    ui->build = -1;
    ui->commandPage = ui->orderMode = 0;
}

void ViewBuildToggle(ViewContext *ui)
{
    if (!ViewSelectedWorkers(ui))
    {
        ViewToast(ui, "Select a worker to open construction.");
        return;
    }
    ui->buildMenu = !ui->buildMenu;
    ui->build = -1;
    ui->panel = -1;
    ui->commandPage = ui->orderMode = 0;
}

void ViewIssueSelected(ViewContext *ui, int command, int value, UnitOrder order, bool append)
{
    char reason[256] = {0};
    int accepted = 0;
    for (int i = 0; i < ui->selectionCount; i++)
    {
        UnitOrder target = order;
        if (command == CMD_TRAIN || command == CMD_EQUIP)
        {
            int best = -1, shortest = MAX_QUEUE;
            for (int b = 0; b < ui->g->buildingCount; b++)
            {
                Building *school = &ui->g->buildings[b];
                if (school->alive && !school->owner && school->complete && school->type == TRAINING_GROUND &&
                    school->queueCount < shortest)
                {
                    best = b;
                    shortest = school->queueCount;
                }
            }
            target.target = GameBuildingRef(ui->g, best);
        }
        if (order.type == ORDER_MOVE || order.type == ORDER_ATTACK_MOVE || order.type == ORDER_DEFEND)
        {
            int x = order.x + i % 4, y = order.y + i / 4;
            if (GameWalkable(ui->g, x, y))
            {
                target.x = x;
                target.y = y;
            }
        }
        accepted += GameCommandApply(
            ui->g,
            (GameCommand){
                .type = command, .a = value, .entity = ui->selection[i], .order = target, .append = append},
            reason, sizeof(reason));
    }
    if (!accepted)
        ViewToast(ui, reason[0] ? reason : "Select citizens first.");
}
