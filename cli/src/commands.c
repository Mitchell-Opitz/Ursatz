#include "commands.h"

#include <string.h>

#include "counterpoint.h"
#include "develop.h"
#include "fill.h"
#include "grow.h"
#include "listen.h"
#include "motive.h"
#include "piece.h"
#include "plan_cmd.h"
#include "progression.h"
#include "random_motive.h"
#include "recognize.h"
#include "regenerate.h"
#include "theme.h"
#include "variants_cmd.h"
#include "variations.h"

/* Each command still reads its own argument list; the wrappers pass it on until the command reads the flag table. */
#define WRAP(name, function) \
    static int name(const CliArgs *args) { return function(args->argc, args->argv); }

WRAP(wrap_progression, run_progression)
WRAP(wrap_motive, run_motive)
WRAP(wrap_develop, run_develop)
WRAP(wrap_variants, run_variants)
WRAP(wrap_recognize, run_recognize)
WRAP(wrap_random_motive, run_random_motive)
WRAP(wrap_counterpoint, run_counterpoint)
WRAP(wrap_grow, run_grow)
WRAP(wrap_theme, run_theme)
WRAP(wrap_piece, run_piece)
WRAP(wrap_plan, run_plan)
WRAP(wrap_fill, run_fill)
WRAP(wrap_lock, run_lock)
WRAP(wrap_regenerate, run_regenerate)
WRAP(wrap_variations, run_variations)
WRAP(wrap_harmonize, run_harmonize)
WRAP(wrap_export, run_export)
WRAP(wrap_listen, run_listen)

const CliCommand CLI_COMMANDS[] = {
    {"progression", "Write a chord progression with a bass line for one phrase", 0, 0, wrap_progression},
    {"motive", "Analyze a motive: its intervals, contour, rhythm, range and harmony", 1, 1, wrap_motive},
    {"develop", "Make motives related to a motive by one operation", 1, 1, wrap_develop},
    {"variants", "Make variants of a motive, or list the operations", 0, 1, wrap_variants},
    {"recognize", "Name the operations that turn one motive into another", 2, 2, wrap_recognize},
    {"random-motive", "Make random motives that keep the hard melodic rules", 0, 0, wrap_random_motive},
    {"counterpoint", "Write a counter-melody against a line", 1, 1, wrap_counterpoint},
    {"grow", "Grow a motive into a sentence, a period or a hybrid theme", 0, 1, wrap_grow},
    {"theme", "Grow a theme from the motives of a request", 1, 1, wrap_theme},
    {"piece", "Compose a piece from a request, or work on one section of it", 0, 1, wrap_piece},
    {"plan", "Write the plan of a whole short piece from a form template", 0, 0, wrap_plan},
    {"fill", "Realize a plan: compose every open theme section", 1, 1, wrap_fill},
    {"lock", "Lock material of a plan as given", 1, 1, wrap_lock},
    {"regenerate", "Write one node of a filled plan again", 1, 1, wrap_regenerate},
    {"variations", "Write several versions of one node of a plan", 1, 1, wrap_variations},
    {"harmonize", "Write several harmonizations of a melody that is given", 1, 1, wrap_harmonize},
    {"export", "Render a plan to a MIDI or MusicXML file", 1, 1, wrap_export},
    {"listen", "Make batches of motifs or themes from fixed seeds for listening", 0, 0, wrap_listen},
};

const size_t CLI_COMMAND_COUNT = sizeof(CLI_COMMANDS) / sizeof(CLI_COMMANDS[0]);

const CliCommand *cli_command_find(const char *name) {
    for (size_t i = 0; name && i < CLI_COMMAND_COUNT; i++) {
        if (strcmp(CLI_COMMANDS[i].name, name) == 0) {
            return &CLI_COMMANDS[i];
        }
    }
    return NULL;
}
