#ifndef GREENFIELD_VIEW_WORLD_H
#define GREENFIELD_VIEW_WORLD_H
#include "context.h"

Vector2 ViewIso(float x, float y);
Vector2 ViewTileAt(ViewContext *ui, Vector2 screen);
float ViewSnow(ViewContext *ui);
Vector2 ViewBuildingVisual(const Building *b, int *index, float *width);
void ViewWorldDraw(ViewContext *ui);
#endif
