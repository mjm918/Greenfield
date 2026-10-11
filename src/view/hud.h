#ifndef GREENFIELD_VIEW_HUD_H
#define GREENFIELD_VIEW_HUD_H
#include "context.h"
void ViewHeader(ViewContext *ui);
void ViewBuildPalette(ViewContext *ui);
void ViewWorldHUD(ViewContext *ui);
bool ViewHasWarning(const ViewContext *ui);
bool ViewWorldHit(ViewContext *ui);
bool ViewWorldPointHit(const ViewContext *ui, Vector2 point);
#endif
