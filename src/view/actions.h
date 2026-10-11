#ifndef GREENFIELD_VIEW_ACTIONS_H
#define GREENFIELD_VIEW_ACTIONS_H
#include "context.h"

bool ViewCommand(ViewContext *ui, int type, int a, int b, int c);
void ViewSaveFolder(ViewContext *ui);
bool ViewSave(ViewContext *ui);
bool ViewLoad(ViewContext *ui, const char *path);
int ViewIssueSelected(ViewContext *ui, int command, int value, UnitOrder order, bool append);
int ViewSelectedWorkers(const ViewContext *ui);
void ViewSelectIdle(ViewContext *ui);
void ViewResumeConstruction(ViewContext *ui);
void ViewSelectBuilding(ViewContext *ui, int id);
void ViewHoldSelected(ViewContext *ui);
void ViewTogglePanel(ViewContext *ui, int panel);
void ViewBuildToggle(ViewContext *ui);
void ViewCancelDrag(ViewContext *ui);
#endif
