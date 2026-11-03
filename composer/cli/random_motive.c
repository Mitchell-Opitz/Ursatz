#include "random_motive.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>

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
    fprintf(out, "  --mode <mode>        major or minor (required); random draws it from the seed\n");
    fprintf(out, "  --meter <n/m>        beats over a beat unit, as in 4/4 or 6/8 (required); random draws 4/4, 3/4, 2/4 or 6/8 from the seed\n");
    fprintf(out, "  --key <key>          for example \"D minor\"; its mode must be --mode. Default: C major or A minor\n");
    fprintf(out, "  --form <form>        full (pitches and rhythm, the default), rhythm or contour\n");
    motive_extras_print_help(out, true);
    fprintf(out, "  --style <file>       a style model file: draw 8 candidates for each motive and keep the closest to it\n");
    fprintf(out, "  --style-without <c>  read the style without the category c (repeatable, at most 8; needs --style)\n");
    fprintf(out, "  --count <n>          how many motives, 1 to %ld (default %d)\n", cli_limit("random_motive_count"), RANDOM_MOTIVE_COUNT_DEFAULT);
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

static const cJSON *member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static void print_text(const cJSON *document) {
    printf("Seed: %.0f\n", member(document, "seed")->valuedouble);
    const cJSON *drawn = member(document, "drawn");
    if (drawn) {
        const cJSON *mode = member(drawn, "mode");
        const cJSON *meter = member(drawn, "meter");
        printf("Drawn:%s%s%s%s\n", mode ? " mode " : "", mode ? mode->valuestring : "", meter ? (mode ? ", meter " : " meter ") : "",
               meter ? meter->valuestring : "");
    }
    size_t number = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, member(document, "motives")) {
        const cJSON *analysis = member(item, "analysis");
        const cJSON *form = member(analysis, "form");
        const char *form_name = cJSON_IsString(form) ? form->valuestring : "full";
        const cJSON *text = member(analysis, strcmp(form_name, "rhythm") == 0    ? "rhythm_text"
                                             : strcmp(form_name, "contour") == 0 ? "contour_text"
                                                                                 : "notes_text");
        number++;
        printf("\nMotive %zu: %s\n", number, cJSON_IsString(text) ? text->valuestring : "");
        const cJSON *degrees = member(analysis, "degrees_text");
        if (strcmp(form_name, "full") == 0) {
            printf("  degrees: %s\n", cJSON_IsString(degrees) ? degrees->valuestring : "");
        }
    }
}

/* Adds the options to the request. */
static bool add_members(cJSON *request, const Options *options) {
    bool ok = true;
    const struct {
        const char *name;
        const char *text;
    } texts[] = {{"mode", options->mode_text}, {"meter", options->meter_text}, {"key", options->key_text},
                 {"form", options->form_text}, {"style", options->style_path}, {"count", options->count_text},
                 {"seed", options->seed_text}, {"role", options->extras.role_text}};
    for (size_t i = 0; ok && i < sizeof texts / sizeof texts[0]; i++) {
        ok = !texts[i].text || cJSON_AddStringToObject(request, texts[i].name, texts[i].text) != NULL;
    }
    cJSON *without = ok && options->without.count > 0 ? cJSON_AddArrayToObject(request, "style_without") : NULL;
    for (size_t i = 0; ok && i < options->without.count; i++) {
        ok = without && cJSON_AddItemToArray(without, cJSON_CreateString(options->without.names[i]));
    }
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
    if (!options.mode_text || !options.meter_text) {
        return cli_fail(CLI_EXIT_USAGE, "random-motive needs --mode and --meter\n%s", RANDOM_MOTIVE_USAGE_LINE);
    }
    cJSON *request = cJSON_CreateObject();
    char *text = request && add_members(request, &options) ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    if (!text) {
        return cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    ComposerResponse response;
    ComposerStatus api_status = composer_api_random_motive(text, NULL, &response);
    free(text);
    if (api_status != COMPOSER_OK) {
        status = cli_fail_api(api_status, &response.error, NULL);
        composer_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    composer_response_destroy(&response);
    if (!document) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    if (options.json) {
        cli_print_json(document);
    } else {
        print_text(document);
    }
    cJSON_Delete(document);
    return 0;
}
