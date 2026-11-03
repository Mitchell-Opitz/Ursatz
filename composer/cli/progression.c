#include "progression.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>

#include "cli_options.h"
#include "workflow.h"

#define EXIT_USAGE CLI_EXIT_USAGE

#define MAX_BARS 64
#define MAX_SUGGESTIONS 8
#define MAX_FILES (3 * MAX_SUGGESTIONS)

typedef struct {
    bool help;
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

static void print_help(FILE *out) {
    fprintf(out, "Usage: ursatz-composer progression --key <key> --meter <n/m> --bars <n> --cadence <type> [options]\n");
    fprintf(out, "\n");
    fprintf(out, "Writes a chord progression for one phrase, with a bass line, ending in the cadence asked for.\n");
    fprintf(out, "\n");
    fprintf(out, "Required:\n");
    fprintf(out, "  --key <key>          tonic and mode: a letter A-G, an optional # or b, then major or minor,\n");
    fprintf(out, "                       in one argument (\"D minor\", \"F# major\", \"Bb minor\"); any case\n");
    fprintf(out, "  --meter <n/m>        2/4, 3/4, 4/4 or 6/8\n");
    fprintf(out, "  --bars <n>           length of the phrase, 1 to %d; a cadence needs at least 2 bars in 4/4 and\n", MAX_BARS);
    fprintf(out, "                       6/8, 3 in 2/4 and 3/4 (more for a plagal cadence)\n");
    fprintf(out, "  --cadence <type>     PAC, IAC, HC, DC or PC (perfect or imperfect authentic, half, deceptive,\n");
    fprintf(out, "                       plagal); any case\n");
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --seed <n>           whole number from 0; the same seed gives the same output. Default: derived\n");
    fprintf(out, "                       from the clock. The seed used is always printed (if no bass line suits the\n");
    fprintf(out, "                       progression, the next seeds are tried and the one that worked is printed)\n");
    fprintf(out, "  --embellish <level>  none (default), low, medium or high: how often chords are prolonged with\n");
    fprintf(out, "                       neighbor, passing and six-four chords, given strong predominants and\n");
    fprintf(out, "                       sevenths, and led into by applied chords\n");
    fprintf(out, "  --bass, --no-bass    write a bass line (default) or only the chords\n");
    fprintf(out, "  --schema <name>      build the start of the phrase on a galant schema (see --list-schemas)\n");
    fprintf(out, "  --sequence <name>    build a continuation on a harmonic sequence (see --list-schemas)\n");
    fprintf(out, "  --list-schemas       list the schema and sequence names and exit\n");
    fprintf(out, "  --suggest <n>        write n alternatives (1 to %d) instead of one; with files, alternative k goes\n", MAX_SUGGESTIONS);
    fprintf(out, "                       to <name>-k.<extension>\n");
    fprintf(out, "  --out <file.json>    write the plan as JSON\n");
    fprintf(out, "  --midi <file>        write the bass line as a MIDI file (needs the bass)\n");
    fprintf(out, "  --musicxml <file>    write the bass line as MusicXML (needs the bass)\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: write a tempo, quarter notes per minute from 20 to 400 (a whole number or\n");
    fprintf(out, "                       a fraction like 195/2). Default: none, the file default of 120 applies\n");
    fprintf(out, "  --plain-midi         MIDI: no track names and no section markers\n");
    fprintf(out, "  --format <text|json> how to print the progression on stdout (default: text)\n");
    fprintf(out, "  --help               this text\n");
    fprintf(out, "\n");
    fprintf(out, "Exit status: 0 success; 1 usage or an invalid argument; 2 the request cannot be satisfied;\n");
    fprintf(out, "3 an output file could not be written.\n");
    fprintf(out, "\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer progression --key \"D minor\" --meter 4/4 --bars 8 --cadence PAC --seed 7 \\\n");
    fprintf(out, "      --embellish medium --out phrase.json --midi phrase.mid\n");
    fprintf(out, "  ursatz-composer progression --key \"G major\" --meter 4/4 --bars 8 --cadence PAC --schema romanesca\n");
    fprintf(out, "  ursatz-composer progression --key \"A minor\" --meter 3/4 --bars 6 --cadence HC --suggest 3\n");
}

/* --- reading the options -------------------------------------------------- */

/* Splits argv into options; returns 0, or the exit status. The values are checked by the composer API. */
static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    options->bass = true;
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--bass") == 0) {
            options->bass = true;
        } else if (strcmp(arg, "--no-bass") == 0) {
            options->bass = false;
        } else if (strcmp(arg, "--key") == 0) {
            target = &options->key_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
        } else if (strcmp(arg, "--bars") == 0) {
            target = &options->bars_text;
        } else if (strcmp(arg, "--cadence") == 0) {
            target = &options->cadence_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--embellish") == 0) {
            target = &options->embellish_text;
        } else if (strcmp(arg, "--format") == 0) {
            target = &options->format_text;
        } else if (strcmp(arg, "--list-schemas") == 0) {
            options->list_schemas = true;
        } else if (strcmp(arg, "--schema") == 0) {
            target = &options->schema_text;
        } else if (strcmp(arg, "--sequence") == 0) {
            target = &options->sequence_text;
        } else if (strcmp(arg, "--suggest") == 0) {
            target = &options->suggest_text;
        } else if (strcmp(arg, "--out") == 0) {
            target = &options->out_path;
        } else if (strcmp(arg, "--midi") == 0) {
            target = &options->midi_path;
        } else if (strcmp(arg, "--musicxml") == 0) {
            target = &options->musicxml_path;
        } else if (strcmp(arg, "--tempo") == 0) {
            target = &options->tempo_text;
        } else if (strcmp(arg, "--plain-midi") == 0) {
            options->plain_midi = true;
        } else {
            return cli_fail(EXIT_USAGE, "unexpected argument '%s' (see ursatz-composer progression --help)", arg);
        }
        if (target) {
            if (i + 1 >= argc) {
                return cli_fail(EXIT_USAGE, "%s needs a value", arg);
            }
            if (*target) {
                return cli_fail(EXIT_USAGE, "%s given twice", arg);
            }
            *target = argv[++i];
        }
    }
    if (options->help || options->list_schemas) {
        return 0;
    }
    if (!options->key_text || !options->meter_text || !options->bars_text || !options->cadence_text) {
        return cli_fail(EXIT_USAGE, "progression needs --key, --meter, --bars and --cadence (see --help)");
    }
    if (options->format_text) {
        if (request_equals_ignore_case(options->format_text, "json")) {
            options->json = true;
        } else if (!request_equals_ignore_case(options->format_text, "text")) {
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
        bool minor = false;
        const cJSON *mode = NULL;
        cJSON_ArrayForEach(mode, cJSON_GetObjectItemCaseSensitive(schema, "modes")) {
            major = major || strcmp(mode->valuestring, "major") == 0;
            minor = minor || strcmp(mode->valuestring, "minor") == 0;
        }
        printf("  %-24s %-15s %s\n", cli_json_string(schema, "name"), cli_json_string(schema, "function"),
               major && minor ? "major, minor" : (major ? "major" : "minor"));
    }
    printf("\nSequences (--sequence <name>), as scale-harmonization names them:\n");
    const cJSON *sequence = NULL;
    cJSON_ArrayForEach(sequence, cJSON_GetObjectItemCaseSensitive(document, "sequences")) {
        printf("  %s\n", sequence->valuestring);
    }
    printf("\nSchemas with weak stages need 4/4 or 6/8; sequences work in every meter.\n");
}

/* --- the command ---------------------------------------------------------------------------------- */

int run_progression(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
    }
    ComposerResponse response;
    if (options.list_schemas) {
        cJSON *request = cJSON_CreateObject();
        if (!request || !cJSON_AddTrueToObject(request, "list_schemas")) {
            cJSON_Delete(request);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
        ComposerStatus listed = cli_call(composer_api_progression, request, &response);
        if (listed != COMPOSER_OK) {
            status = cli_fail_api(listed, &response.error, NULL);
        } else {
            cJSON *names = cJSON_Parse(response.json);
            print_names(names);
            cJSON_Delete(names);
        }
        composer_response_destroy(&response);
        return status;
    }

    cJSON *request = build_request(&options);
    if (!request) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    ComposerStatus called = cli_call(composer_api_progression, request, &response);
    if (called != COMPOSER_OK) {
        status = cli_fail_api(called, &response.error, NULL);
        composer_response_destroy(&response);
        return status;
    }
    cJSON *document = cJSON_Parse(response.json);
    if (!document) {
        composer_response_destroy(&response);
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
    composer_response_destroy(&response);
    return status;
}
