#include "assets.h"
#include "context.h"
#include <stdio.h>

static void AssetName(char *path, size_t n, const char *name)
{
    snprintf(path, n, "%sassets/%s", GetApplicationDirectory(), name);
}

static Atlas LoadAtlas(const char *name, int columns)
{
    Atlas a = {0};
    char path[4096];
    AssetName(path, sizeof(path), name);
    Image img = LoadImage(path);
    if (!img.data)
        return a;
    Color *pixels = LoadImageColors(img);
    int cw = img.width / columns, ch = img.height / columns;
    a.cellHeight = ch;
    for (int i = 0; i < columns * columns; i++)
    {
        int sx = i % columns * cw, sy = i / columns * ch, minx = cw, miny = ch, maxx = 0, maxy = 0;
        for (int y = 0; y < ch; y++)
            for (int x = 0; x < cw; x++)
                if (pixels[(sy + y) * img.width + sx + x].a > 100)
                {
                    if (x < minx)
                        minx = x;
                    if (x > maxx)
                        maxx = x;
                    if (y < miny)
                        miny = y;
                    if (y > maxy)
                        maxy = y;
                }
        a.rect[i] = (Rectangle){sx + minx, sy + miny, maxx - minx + 1, maxy - miny + 1};
        a.baseline[i] = maxy;
        a.pivot[i] = (Vector2){cw * .5f - minx, -miny};
    }
    // Share a foot line within each animation row. Cropping must not recenter a
    // character on its swinging tool or stretch a running animal frame to fit.
    for (int row = 0; row < columns; row++)
    {
        float baseline = 0;
        for (int col = 0; col < columns; col++)
            if (a.baseline[row * columns + col] > baseline)
                baseline = a.baseline[row * columns + col];
        for (int col = 0; col < columns; col++)
            a.pivot[row * columns + col].y += baseline;
    }
    UnloadImageColors(pixels);
    a.texture = LoadTextureFromImage(img);
    GenTextureMipmaps(&a.texture);
    SetTextureFilter(a.texture, TEXTURE_FILTER_TRILINEAR);
    UnloadImage(img);
    return a;
}

void ViewSprite(Atlas a, int index, Vector2 foot, float width, Color tint, bool flip)
{
    Rectangle src = a.rect[index];
    float h = width * src.height / src.width;
    if (flip)
        src.width = -src.width;
    DrawTexturePro(a.texture, src, (Rectangle){foot.x - width / 2, foot.y - h, width, h}, (Vector2){0, 0}, 0,
                   tint);
}

bool ViewAssetsLoad(ViewContext *ui)
{
    ui->objects = LoadAtlas("settlement-atlas.png", 4);
    ui->resources = LoadAtlas("resource-states-atlas.png", 2);
    ui->people = LoadAtlas("citizen-atlas.png", 4);
    ui->braids = LoadAtlas("citizen-braids-atlas.png", 4);
    ui->bronze = LoadAtlas("bronze-atlas.png", 2);
    ui->peopleNorth = LoadAtlas("citizen-north-atlas.png", 4);
    ui->braidsNorth = LoadAtlas("citizen-braids-north-atlas.png", 4);
    ui->peopleJobs = LoadAtlas("citizen-jobs-atlas.png", 4);
    ui->braidsJobs = LoadAtlas("citizen-braids-jobs-atlas.png", 4);
    ui->animals = LoadAtlas("deer-atlas.png", 4);
    ui->canvas = LoadRenderTexture(SCREEN_W * RENDER_SCALE, SCREEN_H * RENDER_SCALE);
    SetTextureFilter(ui->canvas.texture, TEXTURE_FILTER_BILINEAR);
    char fontPath[4096];
    AssetName(fontPath, sizeof(fontPath), "SourceSans3-Regular.ttf");
    ui->font = LoadFontEx(fontPath, 64, NULL, 0);
    AssetName(fontPath, sizeof(fontPath), "SourceSans3-Semibold.ttf");
    ui->title = LoadFontEx(fontPath, 96, NULL, 0);
    SetTextureFilter(ui->font.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(ui->title.texture, TEXTURE_FILTER_BILINEAR);
    return ui->resources.texture.id && ui->peopleNorth.texture.id && ui->braidsNorth.texture.id &&
           ui->peopleJobs.texture.id &&
           ui->braidsJobs.texture.id && ui->animals.texture.id && ui->bronze.texture.id &&
           ui->objects.texture.id && ui->people.texture.id && ui->braids.texture.id && ui->canvas.id &&
           ui->font.texture.id != GetFontDefault().texture.id &&
           ui->title.texture.id != GetFontDefault().texture.id;
}

void ViewAssetsUnload(ViewContext *ui)
{
    UnloadTexture(ui->resources.texture);
    UnloadTexture(ui->bronze.texture);
    UnloadTexture(ui->objects.texture);
    UnloadTexture(ui->people.texture);
    UnloadTexture(ui->braids.texture);
    UnloadTexture(ui->peopleNorth.texture);
    UnloadTexture(ui->braidsNorth.texture);
    UnloadTexture(ui->peopleJobs.texture);
    UnloadTexture(ui->braidsJobs.texture);
    UnloadTexture(ui->animals.texture);
    UnloadRenderTexture(ui->canvas);
    if (ui->font.texture.id != GetFontDefault().texture.id)
        UnloadFont(ui->font);
    if (ui->title.texture.id != GetFontDefault().texture.id)
        UnloadFont(ui->title);
}
