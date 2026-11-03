#include "theme.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/plan_json.h>

#include "cli_options.h"
#include "util/request_parse.h"
#include "util/util.h"

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
    if (!util_read_file(options->input, &text, NULL)) {
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
        bool json = length >= 5 && request_equals_ignore_case(options->explain + length - 5, ".json");
        ok = ok && cJSON_AddStringToObject(request, "explain", json ? "json" : "text");
    }
    *out = ok ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    return *out ? 0 : cli_fail(CLI_EXIT_USAGE, "out of memory");
}

/* --- the files ----------------------------------------------------------------------------------------- */

/* Writes data to the temporary file; false when it cannot be. */
static bool write_temporary(const char *path, const void *data, size_t size) {
    FILE *file = fopen(path, "wb");
    bool wrote = file && fwrite(data, 1, size, file) == size;
    bool closed = file && fclose(file) == 0;
    if (!wrote || !closed) {
        remove(path);
    }
    return wrote && closed;
}

/* The plan object of a theme as the library writes plan files. */
static int write_plan(const cJSON *plan, const char *temporary, const char *final_path) {
    char *text = cJSON_PrintUnformatted(plan);
    PiecePlan *loaded = NULL;
    PlanJsonError error;
    memset(&error, 0, sizeof error);
    bool ok = text && plan_from_json(text, &loaded, &error) == PLAN_OK;
    cJSON_free(text);
    if (ok && plan_write_json_file(loaded, temporary, &error) != PLAN_OK) {
        ok = false;
    }
    plan_destroy(loaded);
    return ok ? 0 : cli_fail(CLI_EXIT_WRITE, "cannot write '%s': %s", final_path, error.message[0] ? error.message : "the plan");
}

/* The MIDI file or MusicXML of the plan of theme k (k from 1), through the export entry. */
static int export_theme(const cJSON *plan, double qpm, bool plain_midi, bool midi, ComposerResponse *out) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddItemToObject(request, "plan", cJSON_Duplicate(plan, true)) &&
              cJSON_AddBoolToObject(request, "midi", midi) && cJSON_AddBoolToObject(request, "musicxml", !midi) &&
              cJSON_AddBoolToObject(request, "plain_midi", plain_midi);
    if (ok && qpm > 0) {
        /* A tempo is a whole number or half-steps like 195/2: the document wrote it with %g. */
        char text[48] = "";
        for (long den = 1; den <= 16 && !text[0]; den++) {
            double scaled = qpm * (double)den;
            if (scaled - (double)(long)(scaled + 0.5) < 1e-6 && (double)(long)(scaled + 0.5) - scaled < 1e-6) {
                snprintf(text, sizeof text, den == 1 ? "%ld" : "%ld/%ld", (long)(scaled + 0.5), den);
            }
        }
        ok = text[0] && cJSON_AddStringToObject(request, "tempo", text);
    }
    char *printed = ok ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    if (!printed) {
        return cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
    }
    ComposerStatus status = composer_api_export(printed, NULL, out);
    cJSON_free(printed);
    return status == COMPOSER_OK ? 0 : cli_fail(CLI_EXIT_WRITE, "cannot render the plan");
}

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
                status = write_plan(plan, entry->temporary, entry->final_path);
                continue;
            } else if (kind == 3) {
                const cJSON *explanation = cJSON_GetObjectItemCaseSensitive(theme, "explanation");
                data = explanation->valuestring;
                size = strlen(explanation->valuestring);
            } else if (k == 0) {
                data = kind == 1 ? (const void *)response->midi : (const void *)response->musicxml;
                size = kind == 1 ? response->midi_size : strlen(response->musicxml);
            } else {
                status = export_theme(plan, qpm, options->plain_midi, kind == 1, &exported);
                if (status != 0) {
                    break;
                }
                data = kind == 1 ? (const void *)exported.midi : (const void *)exported.musicxml;
                size = kind == 1 ? exported.midi_size : strlen(exported.musicxml);
            }
            if (!write_temporary(entry->temporary, data, size)) {
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

static double number_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsNumber(item) ? item->valuedouble : 0;
}

static const char *string_of(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* A ratio the way the text writes it: a whole number, else with %g. */
static void print_number(double value) {
    if (value == (double)(unsigned long)value) {
        printf("%lu", (unsigned long)value);
    } else {
        printf("%g", value);
    }
}

static void print_structure(const cJSON *structure) {
    /* The nodes are listed depth first: a node is inside the one before it while it starts before that one ends. */
    double ends[16];
    size_t depth = 0;
    const cJSON *node = NULL;
    cJSON_ArrayForEach(node, structure) {
        double start = number_of(node, "start");
        double end = number_of(node, "end");
        while (depth > 0 && start >= ends[depth - 1]) {
            depth--;
        }
        printf("%*s%s (%s) bars %g-%g\n", 2 + 2 * (int)depth, "", string_of(node, "name"), string_of(node, "kind"), start, end);
        if (depth < 16) {
            ends[depth++] = end;
        }
    }
}

static void print_theme_text(const cJSON *theme) {
    printf("seed %.0f\n", number_of(theme, "seed"));
    printf("\nstructure (bars from the start of the theme)\n");
    print_structure(cJSON_GetObjectItemCaseSensitive(theme, "structure"));
    printf("\n%-4s %-4s %-10s %-12s %s\n", "bar", "beat", "chord", "function", "bass");
    const cJSON *row = NULL;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(theme, "chords")) {
        printf("%-4.0f %-4.0f %-10s %-12s %s\n", number_of(row, "bar"), number_of(row, "beat"), string_of(row, "chord"),
               string_of(row, "function"), string_of(row, "bass"));
    }
    printf("\nmotive developments (m0 is the motive given)\n");
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(theme, "developments")) {
        const char *idea = string_of(row, "idea");
        printf("  %s for %s: %s\n", string_of(row, "motive"), idea[0] ? idea : "(unused)", string_of(row, "development"));
    }
    printf("\nslots (the motive each idea is made from)\n");
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(theme, "slots")) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(row, "item");
        const cJSON *chain = cJSON_GetObjectItemCaseSensitive(row, "chain");
        const cJSON *connection = cJSON_GetObjectItemCaseSensitive(row, "connection");
        printf("  %s: %s slot, motive %s, %s", string_of(row, "idea"), string_of(row, "slot"), string_of(row, "motive"),
               string_of(row, "source"));
        if (cJSON_IsString(item)) {
            printf(" %s", item->valuestring);
        }
        if (cJSON_IsString(chain)) {
            printf(" (%s)", chain->valuestring);
        }
        if (cJSON_IsNumber(connection)) {
            printf(", connection %.0f%%", connection->valuedouble);
        }
        printf("\n");
    }
    const cJSON *score = cJSON_GetObjectItemCaseSensitive(theme, "score");
    double numerator = number_of(score, "numerator");
    double denominator = number_of(score, "denominator");
    unsigned long percent = denominator > 0 ? (unsigned long)((100.0 * numerator + denominator / 2) / denominator) : 0;
    printf("\nstandard-form rule score %.0f/%.0f (%lu%%)\n", numerator, denominator, percent);
    printf("%s\n", string_of(theme, "heard_text"));
    const cJSON *distance = cJSON_GetObjectItemCaseSensitive(theme, "style_distance");
    if (cJSON_IsObject(distance)) {
        printf("\nstyle distance (the Analyzer's, 0 is the style, 1 is nothing alike)\n");
        static const char *const NAMES[] = {"harmony", "rhythm", "melody", "form", "overall"};
        for (size_t i = 0; i < sizeof NAMES / sizeof NAMES[0]; i++) {
            const cJSON *value = cJSON_GetObjectItemCaseSensitive(distance, NAMES[i]);
            if (cJSON_IsNumber(value)) {
                printf("  %-8s %.4f\n", NAMES[i], value->valuedouble);
            }
        }
    }
}

static void print_text(const cJSON *document, const Options *options, const CliPendingFile *pending, size_t pending_count) {
    printf("%s (%s), key %s, meter %s, %.0f bars, cadence %s", string_of(document, "into"), string_of(document, "pairing"),
           string_of(document, "key"), string_of(document, "meter"), number_of(document, "bars"), string_of(document, "cadence"));
    if (cJSON_GetObjectItemCaseSensitive(document, "first_cadence")) {
        printf(", first cadence %s", string_of(document, "first_cadence"));
    }
    if (cJSON_GetObjectItemCaseSensitive(document, "accompaniment")) {
        printf(", accompaniment %s", string_of(document, "accompaniment"));
        if (cJSON_GetObjectItemCaseSensitive(document, "texture")) {
            printf(" (%s)", string_of(document, "texture"));
        }
    }
    const char *voicing = string_of(document, "voicing");
    if (voicing[0]) {
        printf(", %s", strcmp(voicing, "chorale") == 0 ? "chorale (four parts)" : "voiced in four parts");
    }
    printf("\n");
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(document, "tempo");
    if (cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(tempo, "qpm"))) {
        printf("tempo: ");
        print_number(number_of(tempo, "qpm"));
        printf("%s\n", strcmp(string_of(tempo, "from"), "style") == 0 ? " (from the style)" : "");
    }
    const cJSON *brief = cJSON_GetObjectItemCaseSensitive(document, "brief");
    const cJSON *mode = cJSON_GetObjectItemCaseSensitive(brief, "mode");
    printf("brief: mode %s (%s)", string_of(mode, "mode"), string_of(mode, "from"));
    const cJSON *brief_tempo = cJSON_GetObjectItemCaseSensitive(brief, "tempo");
    if (cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(brief_tempo, "qpm"))) {
        printf(", tempo ");
        print_number(number_of(brief_tempo, "qpm"));
        printf(" (%s)", string_of(brief_tempo, "from"));
    }
    if (cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(brief, "density"))) {
        printf(", density ");
        print_number(number_of(brief, "density"));
        printf(" attacks per beat");
    }
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(brief, "without");
    if (cJSON_GetArraySize(without) > 0) {
        printf(", without");
        const cJSON *name = NULL;
        size_t at = 0;
        cJSON_ArrayForEach(name, without) {
            printf("%s %s", at++ ? "," : "", name->valuestring);
        }
    }
    printf("\n");
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(document, "chords")) {
        printf("chords bar %.0f: %s\n", number_of(entry, "bar"), string_of(entry, "text"));
    }
    const cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    size_t count = (size_t)cJSON_GetArraySize(themes);
    for (size_t k = 0; k < count; k++) {
        if (options->suggest) {
            printf("\ntheme %zu of %zu\n", k + 1, count);
        } else {
            printf("\n");
        }
        print_theme_text(cJSON_GetArrayItem(themes, (int)k));
    }
    for (size_t i = 0; i < pending_count; i++) {
        printf("wrote %s\n", pending[i].final_path);
    }
}

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
        if (request_equals_ignore_case(options.format, "json")) {
            json = true;
        } else if (!request_equals_ignore_case(options.format, "text")) {
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
            print_text(document, &options, pending, pending_count);
        }
    }
    cJSON_Delete(document);
    composer_response_destroy(&response);
    return status;
}
