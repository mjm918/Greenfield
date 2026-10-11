#ifndef GREENFIELD_VIEW_HUD_H
#define GREENFIELD_VIEW_HUD_H
#include "context.h"
void ViewHeader(ViewContext *ui);
void ViewBuildPalette(ViewContext *ui);
void ViewWorldHUD(ViewContext *ui);
void ViewResourceInfo(ViewContext *ui, int tile, Rectangle rect);
bool ViewHasWarning(const ViewContext *ui);
bool ViewWorldHit(ViewContext *ui);
bool ViewWorldPointHit(const ViewContext *ui, Vector2 point);
#endif
