#include "variations.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cli_options.h"
#include "fill.h"

#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/variations.h>

typedef struct {
    bool help;
    const char *input;
    CliTargetOptions target;
    const char *count_text;
    const char *seed_text;
    const char *out_dir;
    const char *format_text;
    bool midi;
    bool musicxml;
    bool json;
    unsigned long count;
    unsigned long seed;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", VARIATIONS_USAGE_LINE + 7);
    fprintf(out, "Writes several distinct versions of one node of a filled plan. The node is written again (as regenerate\n");
    fprintf(out, "writes it) on a copy of the plan for the seeds N, N+1, ..., at most four times --count of them; a version\n");
    fprintf(out, "is kept when it sounds different from the plan and from the versions kept before. A seed that gets stuck\n");
    fprintf(out, "is skipped. The plan file is not changed. Given material is never changed.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --node <id>          the node's id in the plan file (composer.id)\n");
    fprintf(out, "  --section <letter>   the section with that form letter (\"A\", \"B\", \"A'\") instead of --node\n");
    fprintf(out, "  --lane <lane>        harmony, bass, melody or inner: only that lane of the node and of what is under it\n");
    fprintf(out, "  --count <n>          how many versions to keep, 1 to %d\n", PLAN_VARIATIONS_MAX);
    fprintf(out, "  --seed <n>           whole number from 0 for the first seed tried. Default: from the clock. The seed of\n");
    fprintf(out, "                       every version is printed\n");
    fprintf(out, "  --out-dir <dir>      where variation-<i>.json (the plan) is written, i from 1; the directory is created\n");
    fprintf(out, "  --midi               also write variation-<i>.mid\n");
    fprintf(out, "  --musicxml           also write variation-<i>.musicxml\n");
    fprintf(out, "  --format <text|json> how to print the result on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 at least one version was kept; 1 usage or an invalid argument; 2 none was kept, the plan\n");
    fprintf(out, "cannot be read, the node is not found, is Given or cannot be regenerated (nothing is written); 3 an output\n");
    fprintf(out, "file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer variations piece.json --section B --count 4 --seed 3 --out-dir variations --midi\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--midi") == 0) {
            options->midi = true;
        } else if (strcmp(arg, "--musicxml") == 0) {
            options->musicxml = true;
        } else if (strcmp(arg, "--node") == 0) {
            target = &options->target.node_text;
        } else if (strcmp(arg, "--section") == 0) {
            target = &options->target.section_text;
        } else if (strcmp(arg, "--lane") == 0) {
            target = &options->target.lane_text;
        } else if (strcmp(arg, "--count") == 0) {
            target = &options->count_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--out-dir") == 0) {
            target = &options->out_dir;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer variations --help)", arg);
        } else if (!options->input) {
            options->input = arg;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; the input is one plan file", arg);
        }
        if (target) {
            if (i + 1 >= argc) {
                return cli_fail(CLI_EXIT_USAGE, "%s needs a value", arg);
            }
            if (*target) {
                return cli_fail(CLI_EXIT_USAGE, "%s given twice", arg);
            }
            *target = argv[++i];
        }
    }
    return 0;
}

static int check_options(Options *options) {
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "variations needs a plan file\n%s", VARIATIONS_USAGE_LINE);
    }
    int status = cli_check_target(&options->target, "variations", VARIATIONS_USAGE_LINE);
    if (status != 0) {
        return status;
    }
    if (!options->count_text) {
        return cli_fail(CLI_EXIT_USAGE, "variations needs --count <n>\n%s", VARIATIONS_USAGE_LINE);
    }
    if (!cli_parse_whole_number(options->count_text, PLAN_VARIATIONS_MAX, &options->count) || options->count < 1) {
        return cli_fail(CLI_EXIT_USAGE, "--count must be a whole number from 1 to %d, not '%s'", PLAN_VARIATIONS_MAX,
                        options->count_text);
    }
    if (!options->out_dir) {
        return cli_fail(CLI_EXIT_USAGE, "variations needs --out-dir <dir>\n%s", VARIATIONS_USAGE_LINE);
    }
    if (options->seed_text) {
        if (!cli_parse_whole_number(options->seed_text, CLI_SEED_MAX, &options->seed)) {
            return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to 4294967295, not '%s'",
                            options->seed_text);
        }
    } else {
        options->seed = cli_default_seed();
    }
    if (options->format_text) {
        if (cli_equals_ignore_case(options->format_text, "json")) {
            options->json = true;
        } else if (!cli_equals_ignore_case(options->format_text, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
        }
    }
    return 0;
}

/* --- the files and the result --------------------------------------------------- */

#define PATH_SIZE 1024

typedef struct {
    char plan[PATH_SIZE];
    char midi[PATH_SIZE];
    char musicxml[PATH_SIZE];
} VariationPaths;

static int make_directory(const char *path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        return cli_fail(CLI_EXIT_WRITE, "cannot create '%s': %s", path, strerror(errno));
    }
    return 0;
}

/* Writes variation-<i>.json and, when asked, .mid and .musicxml; fills paths. */
static int write_variations(const Options *options, const PlanVariations *variations, VariationPaths *paths) {
    int status = make_directory(options->out_dir);
    for (size_t i = 0; status == 0 && i < variations->count; i++) {
        const PlanVariation *variation = &variations->items[i];
        VariationPaths *entry = &paths[i];
        memset(entry, 0, sizeof(*entry));
        if (snprintf(entry->plan, PATH_SIZE, "%s/variation-%zu.json", options->out_dir, i + 1) >= PATH_SIZE) {
            return cli_fail(CLI_EXIT_WRITE, "file name in '%s' is too long", options->out_dir);
        }
        if (options->midi) {
            snprintf(entry->midi, PATH_SIZE, "%s/variation-%zu.mid", options->out_dir, i + 1);
        }
        if (options->musicxml) {
            snprintf(entry->musicxml, PATH_SIZE, "%s/variation-%zu.musicxml", options->out_dir, i + 1);
        }
        CliOutputPaths outputs = {entry->plan, options->midi ? entry->midi : NULL, options->musicxml ? entry->musicxml : NULL,
                                  NULL, NULL};
        const char *files[CLI_MAX_OUTPUTS];
        size_t file_count = 0;
        status = cli_write_outputs(&outputs, variation->plan, &variation->outcome, files, &file_count);
    }
    return status;
}

static void print_result(const Options *options, const PlanVariations *variations, const VariationPaths *paths) {
    if (options->json) {
        printf("{\"variations\":[");
        for (size_t i = 0; i < variations->count; i++) {
            const PlanVariation *variation = &variations->items[i];
            printf("%s{\"index\":%zu,\"seed\":%lu,\"plan\":", i ? "," : "", i + 1, variation->seed);
            cli_print_json_string(paths[i].plan);
            printf(",\"midi\":");
            if (options->midi) {
                cli_print_json_string(paths[i].midi);
            } else {
                printf("null");
            }
            printf(",\"musicxml\":");
            if (options->musicxml) {
                cli_print_json_string(paths[i].musicxml);
            } else {
                printf("null");
            }
            printf(",\"sections\":");
            cli_print_heard_json(variation->plan, &variation->outcome);
            printf("}");
        }
        printf("],\"kept\":%zu,\"requested\":%lu,\"seeds_tried\":%zu}\n", variations->count, options->count,
               variations->seeds_tried);
        return;
    }
    for (size_t i = 0; i < variations->count; i++) {
        printf("variation %zu  seed %lu  ", i + 1, variations->items[i].seed);
        cli_print_heard_text(variations->items[i].plan, &variations->items[i].outcome);
        printf("\n");
    }
    printf("kept %zu of %lu, %zu seeds tried\n", variations->count, options->count, variations->seeds_tried);
}

/* Writes and prints what plan_variations gave. Returns the exit status. */
static int finish(const Options *options, PlanVariations *variations, const char *what) {
    if (variations->count == 0) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "no version of %s that differs from the plan was found in %zu seeds from %lu", what,
                        variations->seeds_tried, options->seed);
    }
    VariationPaths *paths = calloc(variations->count, sizeof(VariationPaths));
    if (!paths) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    int status = write_variations(options, variations, paths);
    if (status == 0) {
        print_result(options, variations, paths);
    }
    free(paths);
    return status;
}

int run_variations(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    status = check_options(&options);
    if (status != 0) {
        return status;
    }
    PiecePlan *plan = NULL;
    PlanJsonError error;
    if (plan_read_json_file(options.input, &plan, &error) != PLAN_OK) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read plan '%s': %s%s%s", options.input, error.message,
                        error.path[0] ? " at " : "", error.path);
    }
    FillTarget *targets = NULL;
    size_t target_count = 0;
    char what[96];
    cli_describe_target(plan, &options.target, what, sizeof(what));
    status = cli_find_targets(plan, &options.target, &targets, &target_count);
    if (status == 0) {
        ComposeConfig config;
        memset(&config, 0, sizeof(config));
        config.seed = options.seed;
        PlanVariations variations;
        RegenerateResult ran = plan_variations(plan, targets, target_count, &config, options.count, &variations);
        if (ran != REGENERATE_OK) {
            status = cli_report_refused(ran, what);
        } else {
            status = finish(&options, &variations, what);
            plan_variations_destroy(&variations);
        }
    }
    free(targets);
    plan_destroy(plan);
    return status;
}
