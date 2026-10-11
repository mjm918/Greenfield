#include "widgets.h"
#include "context.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>

const Color INK = {37, 48, 42, 255}, PAPER = {240, 234, 213, 255}, MUTED = {161, 174, 151, 255},
            ACCENT = {221, 180, 93, 255}, SAGE = {136, 177, 116, 255}, RUST = {192, 104, 74, 255};

float ViewMinF(float a, float b)
{
    return a < b ? a : b;
}

Color ViewMix(Color a, Color b, float f)
{
    return (Color){(unsigned char)(a.r + (b.r - a.r) * f), (unsigned char)(a.g + (b.g - a.g) * f),
                   (unsigned char)(a.b + (b.b - a.b) * f), 255};
}

void ViewText(ViewContext *ui, const char *text, float x, float y, float size, Color color)
{
    DrawTextEx(ui->font, text, (Vector2){roundf(x), roundf(y)}, size, 0.5f, color);
}

void ViewTitle(ViewContext *ui, const char *text, float x, float y, float size, Color color)
{
    DrawTextEx(ui->title, text, (Vector2){roundf(x), roundf(y)}, size, 0.5f, color);
}

void ViewLabel(ViewContext *ui, const char *text, float x, float y)
{
    ViewText(ui, text, x, y, 14, MUTED);
}

bool ViewHit(ViewContext *ui, Rectangle r)
{
    return CheckCollisionPointRec(ui->mouse, r);
}

bool ViewClick(ViewContext *ui, Rectangle r)
{
    if (!ui->allowInput || ui->clickConsumed || !ViewHit(ui, r) || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return false;
    // A submenu may be drawn immediately after opening. One click activates one control.
    ui->clickConsumed = true;
    return true;
}

void ViewRounded(Rectangle r, Color color)
{
    DrawRectangle((int)r.x + 2, (int)r.y, (int)r.width - 4, (int)r.height, color);
    DrawRectangle((int)r.x, (int)r.y + 2, (int)r.width, (int)r.height - 4, color);
}

void ViewBorder(Rectangle r, Color color)
{
    DrawLine((int)r.x + 2, (int)r.y, (int)(r.x + r.width) - 3, (int)r.y, color);
    DrawLine((int)r.x + 2, (int)(r.y + r.height) - 1, (int)(r.x + r.width) - 3, (int)(r.y + r.height) - 1,
             color);
    DrawLine((int)r.x, (int)r.y + 2, (int)r.x, (int)(r.y + r.height) - 3, color);
    DrawLine((int)(r.x + r.width) - 1, (int)r.y + 2, (int)(r.x + r.width) - 1, (int)(r.y + r.height) - 3,
             color);
}

void ViewWrapText(ViewContext *ui, const char *text, float x, float y, float width, float size, Color color)
{
    char word[512], line[512] = {0};
    size_t pos = 0;
    float yy = y;
    while (text[pos])
    {
        int n = 0;
        while (text[pos] && text[pos] != ' ' && text[pos] != '\n' && n < 250)
            word[n++] = text[pos++];
        word[n] = 0;
        bool newline = text[pos] == '\n';
        if (text[pos])
            pos++;
        char candidate[512];
        snprintf(candidate, sizeof(candidate), "%s%s%s", line, line[0] ? " " : "", word);
        if (MeasureTextEx(ui->font, candidate, size, 0.5f).x > width && line[0])
        {
            ViewText(ui, line, x, yy, size, color);
            yy += size + 5;
            snprintf(line, sizeof(line), "%s", word);
        }
        else
            snprintf(line, sizeof(line), "%s", candidate);
        if (newline)
        {
            ViewText(ui, line, x, yy, size, color);
            yy += size + 5;
            line[0] = 0;
        }
    }
    if (line[0])
        ViewText(ui, line, x, yy, size, color);
}

bool ViewButton(ViewContext *ui, Rectangle r, const char *text, bool selected, bool enabled)
{
    bool hover = ViewHit(ui, r) && ui->allowInput;
    Color base = selected ? (Color){92, 111, 72, 255} : (Color){53, 66, 54, 255};
    if (hover && enabled)
        base = ViewMix(base, ACCENT, 0.16f);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && hover && enabled)
        base = ViewMix(base, BLACK, 0.15f);
    ViewRounded(r, base);
    ViewBorder(r, selected ? ACCENT : Fade(MUTED, enabled ? 0.4f : 0.15f));
    float width = MeasureTextEx(ui->font, text, 16, 0.5f).x;
    ViewText(ui, text, r.x + (r.width - width) / 2, r.y + (r.height - 16) / 2 - 1, 16,
             enabled ? PAPER : Fade(MUTED, 0.6f));
    return enabled && ViewClick(ui, r);
}

void ViewToast(ViewContext *ui, const char *text)
{
    snprintf(ui->toast, sizeof(ui->toast), "%s", text);
    ui->toastTime = 5;
}

void ViewBar(float x, float y, float width, float value, Color color)
{
    ViewRounded((Rectangle){x, y, width, 5}, Fade(INK, 0.7f));
    ViewRounded((Rectangle){x, y, width * Clamp(value, 0, 1), 5}, color);
}

void ViewCost(ViewContext *ui, const int *cost, float x, float y, float width)
{
    char text[200] = {0};
    size_t used = 0;
    for (int r = 0; r < RESOURCE_COUNT; r++)
        if (cost[r])
            used += (size_t)snprintf(text + used, sizeof(text) - used, "%s%d %s", used ? "  " : "", cost[r],
                                     RESOURCE_NAMES[r]);
    ViewWrapText(ui, text, x, y, width, 14, ACCENT);
}
