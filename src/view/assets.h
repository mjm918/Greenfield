#ifndef GREENFIELD_VIEW_ASSETS_H
#define GREENFIELD_VIEW_ASSETS_H
#include "context.h"

void ViewSprite(Atlas a, int index, Vector2 foot, float width, Color tint, bool flip);
bool ViewAssetsLoad(ViewContext *ui);
void ViewAssetsUnload(ViewContext *ui);
#endif
