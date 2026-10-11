#include "game_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int ReferenceMain(int argc, char **argv);
int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--reference") || !strcmp(argv[i], "--verify") || !strcmp(argv[i], "--capture"))
            return ReferenceMain(argc, argv);
    if (argc > 1 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
    {
        puts("Greenfield - Founding a Tribe\n  --play                      Begin a new village\n  --load "
             "PATH                 Load a specific local save\n  --game-capture PATH         Capture the "
             "playable scene and exit\n  --reference                 Open the original cleaned "
             "illustration\n  --capture PATH --time SEC   Capture the animated reference\n  --verify "
             "DIRECTORY         Verify reference fidelity\n\nIn game: Space pause, 1/2/4 speed, B build, W "
             "workforce, R discoveries,\nO objectives, Esc menu, wheel zoom, right-drag / arrows pan, Home "
             "recenter.\nF5 save, F9 load, F fullscreen. Click objects to inspect.");
        return 0;
    }
    GameState *game = calloc(1, sizeof(*game));
    if (!game)
        return 1;
    GameInit(game);
    bool returning = false;
    int result;
    do
    {
        result = GameViewRun(game, argc, argv, returning);
        if (result == 2)
        {
            char *reference[] = {argv[0], "--reference"};
            ReferenceMain(2, reference);
            returning = true;
        }
    } while (result == 2);
    free(game);
    return result;
}
