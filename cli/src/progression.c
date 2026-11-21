#include "progression.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_tools.h"

#include "cli_options.h"
#include "cli_params.h"
#include "flags.h"
#include "workflow.h"

#define EXIT_USAGE CLI_EXIT_USAGE

#define MAX_BARS 64
#define MAX_SUGGESTIONS 8
#define MAX_FILES (3 * MAX_SUGGESTIONS)

typedef struct {
    const char *key_text;
    const char *meter_text;
    const char *bars_text;
    const char *cadence_text;
    const char *seed_text;
    const char *embellish_text;
    const char *format_text;
    const char *out_path;
    const char *midi_path;
    const char *musicxml_path;
    const char *tempo_text;
    bool plain_midi;
    const char *schema_text;
    const char *sequence_text;
    const char *suggest_text;
    bool list_schemas;
    bool bass;
    bool json;
} Options;


/* --- reading the options -------------------------------------------------- */

/* Reads the options from the parsed arguments; returns 0, or the exit status. The values are checked by the composer API. */
static const CliOptionRow ROWS[] = {
    {"--key", offsetof(Options, key_text), false},
    {"--meter", offsetof(Options, meter_text), false},
    {"--bars", offsetof(Options, bars_text), false},
    {"--cadence", offsetof(Options, cadence_text), false},
    {"--seed", offsetof(Options, seed_text), false},
    {"--embellish", offsetof(Options, embellish_text), false},
    {"--format", offsetof(Options, format_text), false},
    {"--list-schemas", offsetof(Options, list_schemas), true},
    {"--schema", offsetof(Options, schema_text), false},
    {"--sequence", offsetof(Options, sequence_text), false},
    {"--suggest", offsetof(Options, suggest_text), false},
    {"--out", offsetof(Options, out_path), false},
    {"--midi", offsetof(Options, midi_path), false},
    {"--musicxml", offsetof(Options, musicxml_path), false},
    {"--tempo", offsetof(Options, tempo_text), false},
    {"--plain-midi", offsetof(Options, plain_midi), true},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->bass = !cli_switch(args, "--no-bass"); /* --bass is the default; --no-bass wins when both are given */
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    if (options->list_schemas) {
        return 0;
    }
    if (!options->key_text || !options->meter_text || !options->bars_text || !options->cadence_text) {
        return cli_needs("progression", "--key, --meter, --bars and --cadence", false);
    }
    if (options->format_text) {
        if (cli_equals_ignore_case(options->format_text, "json")) {
            options->json = true;
        } else if (!cli_equals_ignore_case(options->format_text, "text")) {
            return cli_fail(EXIT_USAGE, "--format must be text or json, not '%s'", options->format_text);
        }
    }
    return 0;
}

/* The request: the members of the composer API for the options. */
static cJSON *build_request(const Options *options) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddStringToObject(request, "key", options->key_text) &&
              cJSON_AddStringToObject(request, "meter", options->meter_text) &&
              cJSON_AddStringToObject(request, "bars", options->bars_text) &&
              cJSON_AddStringToObject(request, "cadence", options->cadence_text) &&
              (!options->seed_text || cJSON_AddStringToObject(request, "seed", options->seed_text)) &&
              (!options->embellish_text || cJSON_AddStringToObject(request, "embellish", options->embellish_text)) &&
              (options->bass || cJSON_AddFalseToObject(request, "bass")) &&
              (!options->schema_text || cJSON_AddStringToObject(request, "schema", options->schema_text)) &&
              (!options->sequence_text || cJSON_AddStringToObject(request, "sequence", options->sequence_text)) &&
              (!options->suggest_text || cJSON_AddStringToObject(request, "suggest", options->suggest_text)) &&
              (!(options->out_path || options->midi_path) || cJSON_AddTrueToObject(request, "plans")) &&
              (!options->midi_path || cJSON_AddTrueToObject(request, "midi")) &&
              (!options->musicxml_path || cJSON_AddTrueToObject(request, "musicxml")) &&
              (!options->tempo_text || cJSON_AddStringToObject(request, "tempo", options->tempo_text)) &&
              (!options->plain_midi || cJSON_AddTrueToObject(request, "plain_midi"));
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* --- showing the result --------------------------------------------------------------------------- */

static void print_table(const Options *options, const cJSON *entry, const char *cadence) {
    printf("%-4s %-4s %-10s %-12s %s\n", "bar", "beat", "chord", "function", options->bass ? "bass" : "");
    const cJSON *row = NULL;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(entry, "chords")) {
        printf("%-4.0f %-4.0f %-10s %-12s %s\n", cli_json_number(row, "bar"), cli_json_number(row, "beat"),
               cli_json_string(row, "chord"), cli_json_string(row, "function"), cli_json_string(row, "bass"));
    }
    printf("\ncadence %s (heard: %s)\n", cadence, cli_json_string(entry, "heard"));
}

static void print_text(const Options *options, const cJSON *document, const cJSON *const *entries, size_t count,
                       const CliWrittenFiles *written) {
    const char *cadence = cli_json_string(document, "cadence");
    printf("key %s, meter %s, %.0f bars, cadence %s\n", cli_json_string(document, "key"), cli_json_string(document, "meter"),
           cli_json_number(document, "bars"), cadence);
    printf("seed %.0f\n", cli_json_number(document, "seed"));
    for (size_t k = 0; k < count; k++) {
        if (options->suggest_text) {
            printf("\nalternative %zu of %zu\n", k + 1, count);
        } else {
            printf("\n");
        }
        print_table(options, entries[k], cadence);
    }
    for (size_t i = 0; i < written->count; i++) {
        printf("wrote %s\n", written->paths[i]);
    }
}

/* The schema and sequence names, from the document of the list. */
static void print_names(const cJSON *document) {
    printf("Schemas (--schema <name>), as Ursatz's galant-schemas framework names them:\n");
    const cJSON *schema = NULL;
    cJSON_ArrayForEach(schema, cJSON_GetObjectItemCaseSensitive(document, "schemas")) {
        bool major = false;
        bool is_minor = false;
        const cJSON *material = NULL;
        cJSON_ArrayForEach(material, cJSON_GetObjectItemCaseSensitive(schema, "materials")) {
            major = major || strcmp(material->valuestring, "omt.material.major") == 0;
            is_minor = is_minor || strcmp(material->valuestring, "omt.material.natural-minor") == 0;
        }
        printf("  %-24s %-15s %s\n", cli_json_string(schema, "name"), cli_json_string(schema, "function"),
               major && is_minor ? "major, minor" : (major ? "major" : "minor"));
    }
    printf("\nSequences (--sequence <name>), as scale-harmonization names them:\n");
    const cJSON *sequence = NULL;
    cJSON_ArrayForEach(sequence, cJSON_GetObjectItemCaseSensitive(document, "sequences")) {
        printf("  %s\n", sequence->valuestring);
    }
    printf("\nSchemas with weak stages need 4/4 or 6/8; sequences work in every meter.\n");
}

/* --- the command ---------------------------------------------------------------------------------- */

int run_progression(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    CliResponse response;
    if (options.list_schemas) {
        cJSON *request = cJSON_CreateObject();
        if (!request || !cJSON_AddTrueToObject(request, "list_schemas")) {
            cJSON_Delete(request);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
        UrsatzStatus listed = cli_call("progression", request, &response);
        if (listed != URSATZ_OK) {
            status = cli_fail_api(listed, &response.error, NULL);
        } else {
            cJSON *names = cJSON_Parse(response.json);
            print_names(names);
            cJSON_Delete(names);
        }
        cli_response_destroy(&response);
        return status;
    }

    cJSON *request = build_request(&options);
    if (!request) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    UrsatzStatus called = cli_call("progression", request, &response);
    if (called != URSATZ_OK) {
        status = cli_fail_api(called, &response.error, NULL);
        cli_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    if (!document) {
        cli_response_destroy(&response);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    const cJSON *notice = NULL;
    cJSON_ArrayForEach(notice, cJSON_GetObjectItemCaseSensitive(document, "notices")) {
        fprintf(stderr, "Note: %s\n", notice->valuestring);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(document, "notices");

    /* The alternatives, or the one progression the document is. */
    const cJSON *entries[MAX_SUGGESTIONS];
    size_t count = 0;
    const cJSON *alternatives = cJSON_GetObjectItemCaseSensitive(document, "alternatives");
    if (alternatives) {
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, alternatives) {
            if (count < MAX_SUGGESTIONS) {
                entries[count++] = entry;
            }
        }
    } else {
        entries[count++] = document;
    }
    CliAlternativePaths paths = {options.out_path, options.midi_path, options.musicxml_path, NULL};
    CliWrittenFiles written;
    memset(&written, 0, sizeof written);
    status = cli_write_alternatives(&paths, entries, count, &response, options.tempo_text, options.plain_midi, &written);
    if (status == 0) {
        if (options.json) {
            if (alternatives) {
                cJSON *entry = NULL;
                cJSON_ArrayForEach(entry, alternatives) {
                    cli_strip_entry_files(entry);
                }
            } else {
                cli_strip_entry_files(document);
            }
            cli_print_document_with_files(document, &written);
        } else {
            print_text(&options, document, entries, count, &written);
        }
        cli_written_free(&written);
    }
    cJSON_Delete(document);
    cli_response_destroy(&response);
    return status;
}
