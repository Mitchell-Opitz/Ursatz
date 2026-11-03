#include "theme.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/plan_json.h>

#include "cli_options.h"
#include "grow_text.h"

#define SCHEMA_NAME "ursatz-theme-request"
#define MAX_SUGGESTIONS 16
#define MAX_FILES (4 * MAX_SUGGESTIONS)

/* --- the command ------------------------------------------------------------------------------------ */

typedef struct {
    bool help;
    bool check;
    bool plain_midi;
    const char *input;
    const char *seed;
    const char *suggest;
    const char *out;
    const char *midi;
    const char *musicxml;
    const char *explain;
    const char *format;
} Options;

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", THEME_USAGE_LINE + 7);
    fprintf(out, "Grows a theme from the motives of a request (JSON, schema %s, version 2): up to 16 motives, which the\n", SCHEMA_NAME);
    fprintf(out, "slots of the theme (basic, contrasting, continuation and cadential) and its single ideas name (a motive may be\n");
    fprintf(out, "in several), and the brief: key, mode, meter, cadences, an accompaniment or a voicing, and a style that gives\n");
    fprintf(out, "the tempo, the mode and the note density. The forms of the ideas are invented. See\n");
    fprintf(out, "docs/theme-request.md.\n\n");
    fprintf(out, "Options (as grow's):\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same output (default: from the clock)\n");
    fprintf(out, "  --suggest <n>        write n different themes (1 to 16); with files, theme k goes to <name>-k.<extension>\n");
    fprintf(out, "  --check              analyze the theme with the Ursatz Analyzer and pass over a seed whose analysis contradicts\n");
    fprintf(out, "                       the plan\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the melody, the bass and the accompaniment as a MIDI file (the tempo of the brief)\n");
    fprintf(out, "  --musicxml <file>    write them as MusicXML\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --explain <file>     write why each candidate was chosen or rejected\n");
    fprintf(out, "  --format <text|json> how to print the theme on stdout (default: text), with the slots and the brief\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid request; 2 the request cannot be satisfied; 3 an output file\n");
    fprintf(out, "could not be written.\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--check") == 0) {
            options->check = true;
        } else if (strcmp(arg, "--plain-midi") == 0) {
            options->plain_midi = true;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed;
        } else if (strcmp(arg, "--suggest") == 0) {
            target = &options->suggest;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out;
        } else if (strcmp(arg, "--midi") == 0) {
            target = &options->midi;
        } else if (strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml;
        } else if (strcmp(arg, "--explain") == 0) {
            target = &options->explain;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer theme --help)", arg);
        } else if (!options->input) {
            options->input = arg;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unexpected argument '%s'; the input is one request file", arg);
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


/* --- the request --------------------------------------------------------------------------------------- */

/* The request file with the options of the command as members, printed as the text the API reads. */
static int build_request(const Options *options, char **out) {
    char *text;
    if (!cli_read_file(options->input, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read the request '%s'", options->input);
    }
    cJSON *request = cJSON_Parse(text);
    free(text);
    if (!request || !cJSON_IsObject(request)) {
        cJSON_Delete(request);
        return cli_fail(CLI_EXIT_USAGE, "the request '%s' is not a JSON object", options->input);
    }
    bool ok = true;
    if (options->seed) {
        ok = ok && cJSON_AddStringToObject(request, "seed", options->seed);
    }
    if (options->suggest) {
        ok = ok && cJSON_AddStringToObject(request, "suggest", options->suggest);
    }
    ok = ok && cJSON_AddBoolToObject(request, "check", options->check) && cJSON_AddBoolToObject(request, "midi", options->midi != NULL) &&
         cJSON_AddBoolToObject(request, "musicxml", options->musicxml != NULL) &&
         cJSON_AddBoolToObject(request, "plain_midi", options->plain_midi);
    /* The plans of the themes after the first are written from the document. */
    ok = ok && cJSON_AddBoolToObject(request, "plans", options->suggest != NULL);
    if (options->explain) {
        size_t length = strlen(options->explain);
        bool json = length >= 5 && cli_equals_ignore_case(options->explain + length - 5, ".json");
        ok = ok && cJSON_AddStringToObject(request, "explain", json ? "json" : "text");
    }
    *out = ok ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    return *out ? 0 : cli_fail(CLI_EXIT_USAGE, "out of memory");
}

/* --- the files ----------------------------------------------------------------------------------------- */

/*
 * Writes every output to a temporary file next to its final name, and renames them all only when every one was written, so
 * that an error leaves no output file behind.
 */
static int write_files(const Options *options, const ComposerResponse *response, const cJSON *document, CliPendingFile *pending,
                       size_t *pending_count) {
    const cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    size_t count = (size_t)cJSON_GetArraySize(themes);
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "tempo"), "qpm");
    double qpm = cJSON_IsNumber(tempo) ? tempo->valuedouble : 0;
    const char *paths[4] = {options->out, options->midi, options->musicxml, options->explain};
    int status = 0;
    *pending_count = 0;
    for (size_t k = 0; k < count && status == 0; k++) {
        const cJSON *theme = cJSON_GetArrayItem(themes, (int)k);
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(theme, "plan");
        for (int kind = 0; kind < 4 && status == 0; kind++) {
            if (!paths[kind]) {
                continue;
            }
            CliPendingFile *entry = &pending[*pending_count];
            if (!cli_alternative_path(paths[kind], k + 1, count > 1, entry->final_path, sizeof(entry->final_path)) ||
                snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", entry->final_path) >=
                    (int)sizeof(entry->temporary)) {
                status = cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", paths[kind]);
                break;
            }
            (*pending_count)++;
            const void *data = NULL;
            size_t size = 0;
            ComposerResponse exported;
            memset(&exported, 0, sizeof exported);
            if (kind == 0 && k == 0) {
                data = response->plan_json;
                size = strlen(response->plan_json);
            } else if (kind == 0) {
                status = cli_write_plan_object(plan, entry->temporary, entry->final_path);
                continue;
            } else if (kind == 3) {
                const cJSON *explanation = cJSON_GetObjectItemCaseSensitive(theme, "explanation");
                data = explanation->valuestring;
                size = strlen(explanation->valuestring);
            } else if (k == 0) {
                data = kind == 1 ? (const void *)response->midi : (const void *)response->musicxml;
                size = kind == 1 ? response->midi_size : strlen(response->musicxml);
            } else {
                status = cli_export_plan(plan, qpm, options->plain_midi, kind == 1, kind != 1, NULL, &exported);
                if (status != 0) {
                    break;
                }
                data = kind == 1 ? (const void *)exported.midi : (const void *)exported.musicxml;
                size = kind == 1 ? exported.midi_size : strlen(exported.musicxml);
            }
            if (!cli_write_bytes(entry->temporary, data, size)) {
                status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': could not write '%s'", entry->final_path,
                                  entry->temporary);
            }
            composer_response_destroy(&exported);
        }
    }
    if (status != 0) {
        cli_discard_pending(pending, *pending_count);
        return status;
    }
    for (size_t i = 0; i < *pending_count; i++) {
        if (rename(pending[i].temporary, pending[i].final_path) != 0) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", pending[i].final_path);
            break;
        }
    }
    if (status != 0) {
        cli_discard_pending(pending, *pending_count);
    }
    return status;
}

/* --- the output ---------------------------------------------------------------------------------------- */

/* The document as `theme --format json` prints it: no plans and explanations, and the files written. */
static void print_json(cJSON *document, const CliPendingFile *pending, size_t pending_count) {
    cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    cJSON *theme = NULL;
    cJSON_ArrayForEach(theme, themes) {
        cJSON_DeleteItemFromObjectCaseSensitive(theme, "plan");
        cJSON_DeleteItemFromObjectCaseSensitive(theme, "explanation");
    }
    cJSON *files = cJSON_AddArrayToObject(document, "files");
    for (size_t i = 0; files && i < pending_count; i++) {
        cJSON_AddItemToArray(files, cJSON_CreateString(pending[i].final_path));
    }
    char *text = cJSON_PrintUnformatted(document);
    printf("%s\n", text ? text : "{}");
    cJSON_free(text);
}

int run_theme(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    if (!options.input) {
        return cli_fail(CLI_EXIT_USAGE, "theme needs a request file\n%s", THEME_USAGE_LINE);
    }
    bool json = false;
    if (options.format) {
        if (cli_equals_ignore_case(options.format, "json")) {
            json = true;
        } else if (!cli_equals_ignore_case(options.format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options.format);
        }
    }
    char *request = NULL;
    status = build_request(&options, &request);
    if (status != 0) {
        return status;
    }
    ComposerResponse response;
    ComposerStatus api_status = composer_api_theme(request, NULL, &response);
    cJSON_free(request);
    if (api_status != COMPOSER_OK) {
        status = cli_fail_api(api_status, &response.error, NULL);
        composer_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    if (!document) {
        composer_response_destroy(&response);
        return cli_fail(CLI_EXIT_USAGE, "the theme could not be read back");
    }
    size_t made = (size_t)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(document, "themes"));
    if (options.suggest) {
        unsigned long asked = strtoul(options.suggest, NULL, 10);
        if (made < asked) {
            fprintf(stderr, "Note: only %zu different theme%s found for this request\n", made, made == 1 ? "" : "s");
        }
    }
    CliPendingFile pending[MAX_FILES];
    size_t pending_count = 0;
    status = write_files(&options, &response, document, pending, &pending_count);
    if (status == 0) {
        if (json) {
            print_json(document, pending, pending_count);
        } else {
            const char *files[MAX_FILES];
            for (size_t i = 0; i < pending_count; i++) {
                files[i] = pending[i].final_path;
            }
            cli_print_grow_text(document, options.suggest != NULL, files, pending_count);
        }
    }
    cJSON_Delete(document);
    composer_response_destroy(&response);
    return status;
}
