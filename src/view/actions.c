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
    bool ok = GameCommandApply(ui->g, (GameCommand){type, a, b, c}, reason, sizeof(reason));
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
    snprintf(ui->savePath, sizeof(ui->savePath), "%s/Library/Application Support/Greenfield/village.gfv1",
             home);
#else
    snprintf(ui->savePath, sizeof(ui->savePath), "%s/.greenfield/village.gfv1", home);
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
        *slash = 0;
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
