#ifndef GREENFIELD_VIEW_WIDGETS_H
#define GREENFIELD_VIEW_WIDGETS_H
#include "context.h"

extern const Color INK, PAPER, MUTED, ACCENT, SAGE, RUST;
float ViewMinF(float a, float b);
Color ViewMix(Color a, Color b, float f);
void ViewText(ViewContext *ui, const char *text, float x, float y, float size, Color color);
void ViewTitle(ViewContext *ui, const char *text, float x, float y, float size, Color color);
void ViewLabel(ViewContext *ui, const char *text, float x, float y);
bool ViewHit(ViewContext *ui, Rectangle r);
bool ViewClick(ViewContext *ui, Rectangle r);
void ViewRounded(Rectangle r, Color color);
void ViewBorder(Rectangle r, Color color);
void ViewWrapText(ViewContext *ui, const char *text, float x, float y, float width, float size, Color color);
bool ViewButton(ViewContext *ui, Rectangle r, const char *text, bool selected, bool enabled);
void ViewToast(ViewContext *ui, const char *text);
void ViewBar(float x, float y, float width, float value, Color color);
void ViewCost(ViewContext *ui, const int *cost, float x, float y, float width);
void ViewCursor(const ViewContext *ui);
#endif
