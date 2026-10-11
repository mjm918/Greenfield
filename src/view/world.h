#ifndef GREENFIELD_VIEW_WORLD_H
#define GREENFIELD_VIEW_WORLD_H
#include "context.h"

Vector2 ViewIso(float x, float y);
Vector2 ViewTileAt(ViewContext *ui, Vector2 screen);
float ViewSnow(ViewContext *ui);
Vector2 ViewBuildingVisual(ViewContext *ui, const Building *b, Atlas *atlas, int *index, float *width);
Rectangle ViewBuildingBounds(ViewContext *ui, const Building *b);
int ViewResourceAt(const ViewContext *ui);
int ViewResourceNode(const ViewContext *ui, int tile);
int ViewSelectedResource(const ViewContext *ui);
void ViewWorldDraw(ViewContext *ui);
#endif
