#include "random_motive.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_random.h>

#include "cli_options.h"
#include "motive_extras.h"

#define RANDOM_MOTIVE_COUNT_DEFAULT 1

typedef struct {
    const char *mode_text;
    const char *meter_text;
    const char *key_text;
    const char *form_text;
    const char *style_path;
    CliStyleWithout without;
    const char *count_text;
    const char *seed_text;
    MotiveExtras extras;
    bool json;
    bool help;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", RANDOM_MOTIVE_USAGE_LINE + 7);
    fprintf(out, "Makes random motives that keep the hard melodic rules of standard counterpoint. Their length, pickup,\n");
    fprintf(out, "number of notes, rhythm and range are not options: they are drawn from defaults in the Composer. The\n");
    fprintf(out, "same options and seed give the same motives.\n\n");
    fprintf(out, "  --mode <mode>        major or minor (required)\n");
    fprintf(out, "  --meter <n/m>        beats over a beat unit, as in 4/4 or 6/8 (required)\n");
    fprintf(out, "  --key <key>          for example \"D minor\"; its mode must be --mode. Default: C major or A minor\n");
    fprintf(out, "  --form <form>        full (pitches and rhythm, the default), rhythm or contour\n");
    motive_extras_print_help(out, true);
    fprintf(out, "  --style <file>       a style model file: draw 8 candidates for each motive and keep the closest to it\n");
    fprintf(out, "  --style-without <c>  read the style without the category c (repeatable, at most 8; needs --style)\n");
    fprintf(out, "  --count <n>          how many motives, 1 to %d (default %d)\n", MOTIVE_RANDOM_MAX_COUNT, RANDOM_MOTIVE_COUNT_DEFAULT);
    fprintf(out, "  --seed <n>           whole number from 0. Default: from the clock. The seed is printed\n");
    fprintf(out, "  --json               print {\"seed\", \"motives\"}, each motive as the motive command's --json document\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument, or an unreadable style file; 2 no motive could be made.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer random-motive --mode minor --meter 3/4 --count 4 --seed 7\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    int extras_status = 0;
    for (int i = 2; i < argc; i++) {
        const char *argument = argv[i];
        if (strcmp(argument, "--help") == 0) {
            options->help = true;
        } else if (motive_extras_take(&options->extras, argc, argv, &i, true, &extras_status)) {
            if (extras_status != 0) {
                return extras_status;
            }
        } else if (strcmp(argument, "--json") == 0) {
            options->json = true;
        } else if (strcmp(argument, "--style-without") == 0) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", argument);
            }
            int added = cli_style_without_add(&options->without, argv[++i]);
            if (added != 0) {
                return added;
            }
        } else if (strcmp(argument, "--mode") == 0 || strcmp(argument, "--meter") == 0 ||
                   strcmp(argument, "--key") == 0 || strcmp(argument, "--form") == 0 ||
                   strcmp(argument, "--style") == 0 || strcmp(argument, "--count") == 0 ||
                   strcmp(argument, "--seed") == 0) {
            const char **slot = strcmp(argument, "--mode") == 0    ? &options->mode_text
                                : strcmp(argument, "--meter") == 0 ? &options->meter_text
                                : strcmp(argument, "--key") == 0   ? &options->key_text
                                : strcmp(argument, "--form") == 0  ? &options->form_text
                                : strcmp(argument, "--style") == 0 ? &options->style_path
                                : strcmp(argument, "--count") == 0 ? &options->count_text
                                                                   : &options->seed_text;
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", argument);
            }
            if (*slot) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice", argument);
            }
            *slot = argv[++i];
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s'", argument);
        }
    }
    return 0;
}

/* Reads the options into *out, without the style. Returns 0 or the status of the error it printed. */
static int read_request(const Options *options, MotiveRandomOptions *out) {
    memset(out, 0, sizeof(*out));
    if (!options->mode_text || !options->meter_text) {
        return cli_fail(CLI_EXIT_USAGE, "random-motive needs --mode and --meter\n%s", RANDOM_MOTIVE_USAGE_LINE);
    }
    if (cli_equals_ignore_case(options->mode_text, "major")) {
        out->mode = PLAN_MODE_MAJOR;
    } else if (cli_equals_ignore_case(options->mode_text, "minor")) {
        out->mode = PLAN_MODE_MINOR;
    } else {
        return cli_fail(CLI_EXIT_USAGE, "--mode must be major or minor, not '%s'", options->mode_text);
    }
    if (!motive_parse_meter(options->meter_text, &out->meter)) {
        return cli_fail(CLI_EXIT_USAGE, "meter '%s' is not a meter; write beats over a beat unit, as in 4/4 or 6/8",
                        options->meter_text);
    }
    if (options->key_text) {
        if (!motive_parse_key(options->key_text, &out->key)) {
            return cli_fail(CLI_EXIT_USAGE,
                            "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                            "for example \"D minor\"",
                            options->key_text);
        }
        if (out->key.mode != out->mode) {
            return cli_fail(CLI_EXIT_USAGE, "key '%s' is not %s, the mode asked for", options->key_text,
                            options->mode_text);
        }
        out->has_key = true;
    }
    out->form = MOTIVE_FORM_FULL;
    if (options->form_text && !motive_form_parse(options->form_text, &out->form)) {
        return cli_fail(CLI_EXIT_USAGE, "--form must be full, rhythm or contour, not '%s'", options->form_text);
    }
    unsigned long count = RANDOM_MOTIVE_COUNT_DEFAULT;
    if (options->count_text &&
        (!cli_parse_whole_number(options->count_text, MOTIVE_RANDOM_MAX_COUNT, &count) || count < 1)) {
        return cli_fail(CLI_EXIT_USAGE, "--count must be a whole number from 1 to %d, not '%s'",
                        MOTIVE_RANDOM_MAX_COUNT, options->count_text);
    }
    out->count = count;
    out->seed = cli_default_seed();
    if (options->seed_text && !cli_parse_whole_number(options->seed_text, CLI_SEED_MAX, &out->seed)) {
        return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to %lu, not '%s'", CLI_SEED_MAX,
                        options->seed_text);
    }
    return 0;
}

static void print_text(unsigned long seed, const Motive *motives, size_t count) {
    printf("Seed: %lu\n", seed);
    for (size_t i = 0; i < count; i++) {
        char *text = cli_motive_text(&motives[i]);
        printf("\nMotive %zu: %s\n", i + 1, text ? text : "");
        free(text);
        if (motives[i].form == MOTIVE_FORM_FULL) {
            char *degrees = motive_to_degrees_text(&motives[i]);
            printf("  degrees: %s\n", degrees ? degrees : "");
            free(degrees);
        }
    }
}

/* Prints a document of text (ending in a newline) with each line indented by two spaces and no final newline. */
static void print_indented(const char *text) {
    printf("    ");
    for (const char *c = text; *c; c++) {
        if (*c == '\n' && c[1] != '\0') {
            printf("\n    ");
        } else if (*c != '\n') {
            putchar(*c);
        }
    }
}

/* {"seed", "motives": [<motive --json document>]}. False on failure. */
static bool print_json(unsigned long seed, const Motive *motives, size_t count) {
    char **documents = calloc(count, sizeof(*documents));
    bool ok = documents != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        MotiveAnalysis analysis;
        ok = motive_analyze(&motives[i], &analysis);
        documents[i] = ok ? motive_analysis_to_json(&motives[i], &analysis) : NULL;
        ok = documents[i] != NULL;
    }
    if (ok) {
        printf("{\n  \"seed\": %lu,\n  \"motives\": [\n", seed);
        for (size_t i = 0; i < count; i++) {
            print_indented(documents[i]);
            printf("%s\n", i + 1 < count ? "," : "");
        }
        printf("  ]\n}\n");
    }
    for (size_t i = 0; documents && i < count; i++) {
        free(documents[i]);
    }
    free(documents);
    return ok;
}

int run_random_motive(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (options.without.count > 0 && !options.style_path) {
        return cli_fail(CLI_EXIT_USAGE, "--style-without needs --style");
    }
    MotiveRandomOptions request;
    status = read_request(&options, &request);
    if (status != 0) {
        return status;
    }
    MotiveOptions extras;
    memset(&extras, 0, sizeof(extras));
    status = motive_extras_options(&options.extras, NULL, &extras);
    if (status != 0) {
        return status;
    }
    ComposerStyle *style = NULL;
    if (options.style_path) {
        status = cli_load_style(options.style_path, options.without.names, options.without.count, &style);
        if (status != 0) {
            return status;
        }
        request.style = style;
    }
    Motive motives[MOTIVE_RANDOM_MAX_COUNT];
    size_t made = 0;
    MotiveRandomResult result = motive_random(&request, motives, &made);
    composer_style_destroy(style);
    if (result != MOTIVE_RANDOM_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot make random motives (%s)", motive_random_result_name(result));
    }
    for (size_t i = 0; extras.has_role && i < made; i++) {
        motives[i].role = extras.role;
    }
    if (options.json) {
        if (!print_json(request.seed, motives, made)) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
    } else {
        print_text(request.seed, motives, made);
    }
    return 0;
}
