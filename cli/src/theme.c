#include "theme.h"
#include "cli_tools.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>


#include "cli_options.h"
#include "grow_text.h"

#define SCHEMA_NAME "ursatz-theme-request"
#define MAX_SUGGESTIONS 16
#define MAX_FILES (4 * MAX_SUGGESTIONS)

/* --- the command ------------------------------------------------------------------------------------ */

typedef struct {
    bool check;
    bool plain_midi;
    const char *input;
    const char *seed;
    const char *suggest;
    const char *out;
    const char *layers;
    const char *midi;
    const char *musicxml;
    const char *explain;
    const char *format;
} Options;


static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    options->check = cli_switch(args, "--check");
    options->plain_midi = cli_switch(args, "--plain-midi");
    options->seed = cli_value(args, "--seed");
    options->suggest = cli_value(args, "--suggest");
    options->out = cli_value(args, "--out");
    options->layers = cli_value(args, "--layers");
    options->midi = cli_value(args, "--midi");
    options->musicxml = cli_value(args, "--musicxml");
    options->explain = cli_value(args, "--explain");
    options->format = cli_value(args, "--format");
    return 0;
}


/* --- the request --------------------------------------------------------------------------------------- */

/* The request file with the options of the command as members. */
static int build_request(const Options *options, cJSON **out) {
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
    ok = ok && (!options->layers || cJSON_AddTrueToObject(request, "layers")) && cJSON_AddBoolToObject(request, "check", options->check) && cJSON_AddBoolToObject(request, "midi", options->midi != NULL) &&
         cJSON_AddBoolToObject(request, "musicxml", options->musicxml != NULL) &&
         cJSON_AddBoolToObject(request, "plain_midi", options->plain_midi);
    /* The plans of the themes after the first are written from the document. */
    ok = ok && cJSON_AddBoolToObject(request, "plans", options->suggest != NULL);
    if (options->explain) {
        size_t length = strlen(options->explain);
        bool json = length >= 5 && cli_equals_ignore_case(options->explain + length - 5, ".json");
        ok = ok && cJSON_AddStringToObject(request, "explain", json ? "json" : "text");
    }
    if (!ok) {
        cJSON_Delete(request);
        *out = NULL;
        return cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    *out = request;
    return 0;
}

/* --- the files ----------------------------------------------------------------------------------------- */

/*
 * Writes every output to a temporary file next to its final name, and renames them all only when every one was written, so
 * that an error leaves no output file behind.
 */
static int write_files(const Options *options, const CliResponse *response, const cJSON *document, CliPendingFile *pending,
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
            CliResponse exported;
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
            cli_response_destroy(&exported);
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
    cJSON_DeleteItemFromObjectCaseSensitive(document, "realization");
    cJSON_DeleteItemFromObjectCaseSensitive(document, "reading");
    cJSON *files = cJSON_AddArrayToObject(document, "files");
    for (size_t i = 0; files && i < pending_count; i++) {
        cJSON_AddItemToArray(files, cJSON_CreateString(pending[i].final_path));
    }
    char *text = cJSON_PrintUnformatted(document);
    printf("%s\n", text ? text : "{}");
    cJSON_free(text);
}

int run_theme(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    if (!options.input) {
        return cli_fail(CLI_EXIT_USAGE, "theme needs a request file\n%s", cli_usage_of("theme"));
    }
    bool json = false;
    if (options.format) {
        if (cli_equals_ignore_case(options.format, "json")) {
            json = true;
        } else if (!cli_equals_ignore_case(options.format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options.format);
        }
    }
    cJSON *request = NULL;
    status = build_request(&options, &request);
    if (status != 0) {
        return status;
    }
    CliResponse response;
    UrsatzStatus api_status = cli_composer("theme", request, NULL, &response);
    cJSON_Delete(request);
    if (api_status != URSATZ_OK) {
        status = cli_fail_api(api_status, &response.error, NULL);
        cli_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    if (!document) {
        cli_response_destroy(&response);
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
    if (status == 0 && options.layers) {
        status = cli_write_layers(options.layers, &response);
    }
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
    cli_response_destroy(&response);
    return status;
}
