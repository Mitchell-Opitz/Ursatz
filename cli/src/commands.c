#include "commands.h"

#include <string.h>

#include "render.h"
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

#include "flags.h"

static const char *const FLAGS_PROGRESSION[] = {"--bars", "--bass", "--cadence", "--embellish", "--format", "--key", "--list-schemas", "--meter", "--midi", "--musicxml", "--no-bass", "--out", "--plain-midi", "--schema", "--seed", "--sequence", "--suggest", "--tempo", NULL};
static const char *const FLAGS_MOTIVE[] = {"--feature", "--form", "--format", "--harmony", "--input-format", "--key", "--meter", "--midi", "--musicxml", "--pickup", "--render-key", "--role", "--tempo", "--tone", NULL};
static const char *const FLAGS_DEVELOP[] = {"--amount", "--axis", "--by", "--degree", "--drop", "--input-format", "--interval", "--keep-contour", "--keep-rhythm", "--key", "--length", "--meter", "--midi", "--min", "--musicxml", "--op", "--plain-midi", "--position", "--ratio", "--seed", "--suggest", "--tempo", "--tonal", NULL};
static const char *const FLAGS_VARIANTS[] = {"--basic", "--basic-format", "--basic-harmony", "--basic-pickup", "--chain", "--count", "--distance", "--each-op", "--feature", "--form", "--format", "--function", "--harmony", "--input-format", "--key", "--list-ops", "--meter", "--pickup", "--role", "--seed", NULL};
static const char *const FLAGS_RECOGNIZE[] = {"--basic", "--basic-format", "--basic-harmony", "--basic-pickup", "--depth", "--feature", "--form", "--format", "--harmony", "--input-format", "--key", "--meter", "--pickup", "--role", "--target-feature", "--target-format", "--target-harmony", "--target-pickup", NULL};
static const char *const FLAGS_RANDOM_MOTIVE[] = {"--count", "--form", "--format", "--key", "--material", "--meter", "--role", "--seed", "--strategy", "--style", "--style-without", NULL};
static const char *const FLAGS_COUNTERPOINT[] = {"--above", "--below", "--format", "--key", "--material", "--meter", "--midi", "--musicxml", "--out", "--plain-midi", "--seed", "--species", "--suggest", "--tempo", NULL};
static const char *const FLAGS_GROW[] = {"--accompaniment", "--accompaniment-format", "--accompaniment-motive", "--accompaniment-pickup", "--bars", "--blend", "--blend-weight", "--blend-weights", "--cadence", "--check", "--explain", "--feature", "--first-cadence", "--format", "--from-corpus", "--harmony", "--hybrid", "--into", "--key", "--like", "--material", "--meter", "--midi", "--musicxml", "--out", "--plain-midi", "--role", "--seed", "--style", "--style-weight", "--style-without", "--suggest", "--tempo", "--texture", "--voicing", NULL};
static const char *const FLAGS_THEME[] = {"--check", "--explain", "--format", "--layers", "--midi", "--musicxml", "--out", "--plain-midi", "--seed", "--suggest", NULL};
static const char *const FLAGS_PIECE[] = {"--check", "--count", "--ending-midi", "--ending-musicxml", "--ending-out", "--explain", "--format", "--from", "--lane", "--layers", "--midi", "--musicxml", "--out", "--out-dir", "--plain-midi", "--regenerate", "--request", "--seed", "--variations", NULL};
static const char *const FLAGS_PLAN[] = {"--accompaniment", "--at", "--bars", "--blend", "--blend-weight", "--blend-weights", "--cells", "--count", "--form", "--format", "--from-corpus", "--given-bass", "--given-melody", "--given-progression", "--key", "--like", "--like-progression", "--material", "--meter", "--modulate", "--motive", "--out", "--seed", "--style", "--style-weight", "--style-without", "--texture", NULL};
static const char *const FLAGS_FILL[] = {"--blend", "--blend-weight", "--blend-weights", "--candidates", "--check", "--explain", "--format", "--layers", "--midi", "--musicxml", "--out", "--plain-midi", "--record", "--seed", "--style", "--style-weight", "--style-without", "--tempo", NULL};
static const char *const FLAGS_LOCK[] = {"--format", "--lane", "--node", "--out", "--section", NULL};
static const char *const FLAGS_REGENERATE[] = {"--format", "--lane", "--midi", "--musicxml", "--node", "--out", "--plain-midi", "--section", "--seed", "--tempo", NULL};
static const char *const FLAGS_VARIATIONS[] = {"--count", "--format", "--lane", "--node", "--out-dir", "--plain-midi", "--section", "--seed", "--tempo", "--write-midi", "--write-musicxml", NULL};
static const char *const FLAGS_HARMONIZE[] = {"--count", "--format", "--out-dir", "--plain-midi", "--section", "--seed", "--tempo", "--write-midi", "--write-musicxml", NULL};
static const char *const FLAGS_RENDER[] = {"--midi", "--musicxml", "--plain-midi", "--tempo", NULL};
static const char *const FLAGS_LISTEN[] = {"--bars", "--blend", "--blend-weight", "--blend-weights", "--check", "--count", "--development", "--into", "--material", "--meter", "--motif", "--out-dir", "--print-settings", "--seed", "--settings", "--strategy", "--style", "--tempo", "--theme-request", "--what", NULL};

const CliCommand CLI_COMMANDS[] = {
    {"progression", "Write a chord progression with a bass line for one phrase", FLAGS_PROGRESSION, 0, 0, run_progression},
    {"motive", "Analyze a motive: its intervals, contour, rhythm, range and harmony", FLAGS_MOTIVE, 1, 1, run_motive},
    {"develop", "Make motives related to a motive by one operation", FLAGS_DEVELOP, 1, 1, run_develop},
    {"variants", "Make variants of a motive, or list the operations", FLAGS_VARIANTS, 0, 1, run_variants},
    {"recognize", "Name the operations that turn one motive into another", FLAGS_RECOGNIZE, 2, 2, run_recognize},
    {"random-motive", "Make random motives that keep the hard melodic rules", FLAGS_RANDOM_MOTIVE, 0, 0, run_random_motive},
    {"counterpoint", "Write a counter-melody against a line", FLAGS_COUNTERPOINT, 1, 1, run_counterpoint},
    {"grow", "Grow a motive into a sentence, a period or a hybrid theme", FLAGS_GROW, 0, 1, run_grow},
    {"theme", "Grow a theme from the motives of a request", FLAGS_THEME, 1, 1, run_theme},
    {"piece", "Compose a piece from a request, or work on one section of it", FLAGS_PIECE, 0, 1, run_piece},
    {"plan", "Write the plan of a whole short piece from a form template", FLAGS_PLAN, 0, 0, run_plan},
    {"fill", "Realize a plan: compose every open theme section", FLAGS_FILL, 1, 1, run_fill},
    {"lock", "Lock material of a plan as given", FLAGS_LOCK, 1, 1, run_lock},
    {"regenerate", "Write one node of a filled plan again", FLAGS_REGENERATE, 1, 1, run_regenerate},
    {"variations", "Write several versions of one node of a plan", FLAGS_VARIATIONS, 1, 1, run_variations},
    {"harmonize", "Write several harmonizations of a melody that is given", FLAGS_HARMONIZE, 1, 1, run_harmonize},
    {"render", "Render a plan to a MIDI or MusicXML file", FLAGS_RENDER, 1, 1, run_render},
    {"listen", "Make batches of motifs or themes from fixed seeds for listening", FLAGS_LISTEN, 0, 0, run_listen},
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
