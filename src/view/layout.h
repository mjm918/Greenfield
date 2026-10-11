#ifndef GREENFIELD_VIEW_LAYOUT_H
#define GREENFIELD_VIEW_LAYOUT_H
#include "context.h"
#include <stddef.h>

// One geometry contract for drawing, picking and controlled input tests.
static inline Rectangle ViewWorldRect(void)
{
    return (Rectangle){0, WORLD_TOP, SCREEN_W, SCREEN_H - WORLD_TOP};
}
static inline Rectangle ViewMapRect(void)
{
    return (Rectangle){1220, 752, 204, 140};
}
static inline const Building *ViewSelectedBuilding(const ViewContext *ui)
{
    if (ui->selectionCount || ui->selected < 0 || ui->selected >= ui->g->buildingCount)
        return NULL;
    const Building *b = &ui->g->buildings[ui->selected];
    return b->alive ? b : NULL;
}
static inline Rectangle ViewSelectionRect(const ViewContext *ui)
{
    if (ui->selectionCount)
        return (Rectangle){16, ui->buildMenu ? 750 : 792, 864, ui->buildMenu ? 142 : 100};
    const Building *b = ViewSelectedBuilding(ui);
    if (b)
    {
        int columns = b->owner              ? 0
                      : !b->complete        ? 2
                      : b->type == HEARTH   ? 4
                      : b->type == WORKSHOP ? 2
                      : b->type == SHELTER  ? 1
                                            : 0;
        return (Rectangle){16, 792, columns ? 292 + columns * 143 : 280, 100};
    }
    return (Rectangle){16, 842, 874, 50};
}
static inline Rectangle ViewCommandRect(const ViewContext *ui, int slot)
{
    Rectangle tray = ViewSelectionRect(ui);
    bool idle = !ui->selectionCount && !ViewSelectedBuilding(ui);
    int columns = idle ? 6 : 4;
    return (Rectangle){(idle ? 28 : 304) + slot % columns * 143,
                       tray.y + (idle ? 8 : 12) + slot / columns * 40, 135, 34};
}
static inline Rectangle ViewBuildRect(const ViewContext *ui, int slot)
{
    return (Rectangle){304 + slot % 7 * 81, ViewSelectionRect(ui).y + 12 + slot / 7 * 62, 75, 56};
}
// Only actual production occupies space. The simulation's queues remain the source of truth.
static inline Rectangle ViewProductionRect(const ViewContext *ui)
{
    const Building *b = ViewSelectedBuilding(ui);
    if (b && b->queueCount)
        return (Rectangle){304, ViewSelectionRect(ui).y - 54, 12 + b->queueCount * 68, 48};
    int count = 0;
    if (!b && !ui->selectionCount)
        for (int i = 0; i < ui->g->buildingCount && count < 3; i++)
        {
            b = &ui->g->buildings[i];
            if (b->alive && !b->owner && b->queueCount)
                count++;
        }
    return (Rectangle){16, ViewSelectionRect(ui).y - 46, count ? 12 + count * 190 : 0, count ? 40 : 0};
}
static inline Rectangle ViewQueueItemRect(const ViewContext *ui, int slot)
{
    Rectangle r = ViewProductionRect(ui);
    return (Rectangle){r.x + 6 + slot * 68, r.y + 7, 62, 34};
}
static inline Rectangle ViewCommandHintRect(const ViewContext *ui)
{
    Rectangle tray = ViewSelectionRect(ui);
    return (Rectangle){tray.x, tray.y - 126, 564, 118};
}
static inline Rectangle ViewObjectiveRect(void)
{
    return (Rectangle){16, 62, 302, 30};
}
static inline Rectangle ViewWarningRect(void)
{
    return (Rectangle){16, 100, 490, 34};
}
static inline Rectangle ViewOrderHintRect(void)
{
    return (Rectangle){528, 62, 420, 30};
}
static inline Rectangle ViewDialogRect(void)
{
    return (Rectangle){200, 150, 1040, 550};
}
static inline bool ViewPanelOpen(const ViewContext *ui)
{
    return ui->panel >= 0 && ui->panel < 4;
}
#define VIEW_ORDER_RALLY ORDER_COUNT
#endif
