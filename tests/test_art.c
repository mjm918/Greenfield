#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
static int errors = 0;
static void CheckAtlas(const char *path, int grid)
{
    Image image = LoadImage(path);
    if (!image.data)
    {
        fprintf(stderr, "Missing atlas: %s\n", path);
        errors++;
        return;
    }
    Color *pixels = LoadImageColors(image);
    int w = image.width / grid, h = image.height / grid;
    for (int cell = 0; cell < grid * grid; cell++)
    {
        int solid = 0, empty = 0;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
            {
                Color c = pixels[(cell / grid * h + y) * image.width + cell % grid * w + x];
                solid += c.a > 100;
                empty += c.a == 0;
            }
        if (solid < w * h / 100 || empty < w * h / 5)
        {
            fprintf(stderr, "Atlas %s cell %d lacks a usable sprite or transparency\n", path, cell);
            errors++;
        }
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
}
int main(int argc, char **argv)
{
    if (argc != 12)
        return 2;
    SetTraceLogLevel(LOG_WARNING);
    CheckAtlas(argv[1], 4);
    CheckAtlas(argv[2], 4);
    CheckAtlas(argv[4], 4);
    CheckAtlas(argv[5], 2);
    CheckAtlas(argv[11], 2);
    for (int i = 6; i < 11; i++)
        CheckAtlas(argv[i], 4);
    Image image = LoadImage(argv[3]);
    if (!image.data)
        return 1;
    if (image.width != 2880 || image.height != 1800)
    {
        fprintf(stderr, "Expected a full-detail 2880x1800 capture, got %dx%d\n", image.width, image.height);
        UnloadImage(image);
        return 1;
    }
    Color *p = LoadImageColors(image);
    int detailedBlocks = 0;
    // The old 720x450 world enlarged into this capture would repeat every 4x4
    // block. Require actual subpixel detail in a world-only region.
    for (int y = 620; y < 1140; y += 4)
        for (int x = 1400; x < 1800; x += 4)
        {
            Color a = p[y * image.width + x];
            bool detailed = false;
            for (int yy = 0; yy < 4; yy++)
                for (int xx = 0; xx < 4; xx++)
                {
                    Color b = p[(y + yy) * image.width + x + xx];
                    if (a.r != b.r || a.g != b.g || a.b != b.b)
                        detailed = true;
                }
            detailedBlocks += detailed;
        }
    if (detailedBlocks < 100)
    {
        fprintf(stderr, "World still looks enlarged from a pixel grid: only %d detailed blocks\n",
                detailedBlocks);
        errors++;
    }
    // Resource totals are light text on a solid dark header. Smooth glyph edges must
    // contain intermediate shades, rather than only foreground/background pixels.
    int smoothText = 0;
    for (int r = 0; r < 7; r++)
        for (int y = 22; y < 64; y++)
            for (int x = (44 + r * 78) * 2; x < (44 + r * 78) * 2 + 40; x++)
            {
                Color c = p[y * image.width + x];
                smoothText += c.r > 45 && c.r < 230 && c.g > 55 && c.g < 225;
            }
    if (smoothText < 100)
    {
        fprintf(stderr, "Resource text lacks smooth glyph edges: %d intermediate pixels\n", smoothText);
        errors++;
    }
    UnloadImageColors(p);
    UnloadImage(image);
    printf("136 sprite cells checked; detailed world blocks: %d; smooth text pixels: %d\n", detailedBlocks,
           smoothText);
    return errors ? 1 : 0;
}
