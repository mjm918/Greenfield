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
        puts("Greenfield - Tribe to Country\n  --play                      Begin a new campaign\n  --load "
             "PATH                 Load a specific local save\n  --game-capture PATH         Capture the "
             "playable scene and exit\n  --reference                 Open the original cleaned "
             "illustration\n  --capture PATH --time SEC   Capture the animated reference\n  --verify "
             "DIRECTORY         Verify reference fidelity\n"
             "Capture options: --capture-panel 0..4, --capture-overview, --capture-selection,\n"
             "--capture-build 0..12, --capture-building 0..12, --capture-soldiers, --benchmark-frames N "
             "(with --game-capture).\n\n"
             "Space pause, F1/F2/F3 speed, Ctrl+number save group, number recall group.\n"
             "Drag selects, Shift queues, right-click orders, middle-drag / arrows pan.\n"
             "A attack-move, D defend, X hold, T retreat, L plunder, S stop.\n"
             "B worker construction, W workforce, R discoveries, O objectives, H help, Esc menu.\n"
             "Wheel zoom, Home recenter, M full map, F5 save, F9 load, F fullscreen.");
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
