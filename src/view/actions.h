#ifndef GREENFIELD_VIEW_ACTIONS_H
#define GREENFIELD_VIEW_ACTIONS_H
#include "context.h"

bool ViewCommand(ViewContext *ui, int type, int a, int b, int c);
void ViewSaveFolder(ViewContext *ui);
bool ViewSave(ViewContext *ui);
bool ViewLoad(ViewContext *ui, const char *path);
#endif
