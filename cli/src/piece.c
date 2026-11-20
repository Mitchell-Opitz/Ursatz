#include "piece.h"
#include "cli_tools.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <cJSON.h>


#include "cli_options.h"
#include "workflow.h"
#include "piece_text.h"

#define SCHEMA_NAME "ursatz-piece-request"
#define OUTPUT_MAX 8
#define VARIATIONS_MAX 8
#define PATH_SIZE 1024

typedef struct {
    bool check;
    bool plain_midi;
    const char *input;
    const char *layers;     /* the folder of the three layers of the first version */
    char form[64];          /* the form of the request: "game-loop" for schema version 1 */
    const char *from;       /* the loop plan to work on (regenerate) */
    const char *request;    /* the request that plan was made from */
    const char *regenerate; /* a section id, or "none" */
    const char *lane;       /* harmony, bass, melody, inner or all */
    const char *variations; /* a section id: make variations of it instead of regenerating */
    const char *count;
    const char *out_dir;
    const char *seed;
    const char *out;
    const char *ending_out;
    const char *midi;
    const char *musicxml;
    const char *ending_midi;
    const char *ending_musicxml;
    const char *explain;
    const char *format;
} Options;


static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    options->input = args->input_count > 0 ? args->inputs[0] : NULL;
    options->check = cli_switch(args, "--check");
    options->plain_midi = cli_switch(args, "--plain-midi");
    options->seed = cli_value(args, "--seed");
    options->out = cli_value(args, "--out");
    options->layers = cli_value(args, "--layers");
    options->ending_out = cli_value(args, "--ending-out");
    options->midi = cli_value(args, "--midi");
    options->musicxml = cli_value(args, "--musicxml");
    options->ending_midi = cli_value(args, "--ending-midi");
    options->ending_musicxml = cli_value(args, "--ending-musicxml");
    options->explain = cli_value(args, "--explain");
    options->format = cli_value(args, "--format");
    options->from = cli_value(args, "--from");
    options->request = cli_value(args, "--request");
    options->regenerate = cli_value(args, "--regenerate");
    options->lane = cli_value(args, "--lane");
    options->variations = cli_value(args, "--variations");
    options->count = cli_value(args, "--count");
    options->out_dir = cli_value(args, "--out-dir");
    return 0;
}

/* The options that belong with --from, and the ones that do not. Returns 0 or the status of the usage error printed. */
static int check_edit_options(const Options *options) {
    if (options->from) {
        if (options->input) {
            return cli_fail(CLI_EXIT_USAGE, "with --from the request is given as --request, not as an argument\n%s", cli_usage_of("piece"));
        }
        if (!options->request) {
            return cli_fail(CLI_EXIT_USAGE, "--from needs --request <request.json>\n%s", cli_usage_of("piece"));
        }
        if (!options->regenerate == !options->variations) {
            return cli_fail(CLI_EXIT_USAGE, "--from needs --regenerate <section id>|none or --variations <section id>, not both\n%s",
                            cli_usage_of("piece"));
        }
        if (options->lane && !cli_equals_ignore_case(options->lane, "all") && !cli_equals_ignore_case(options->lane, "harmony") &&
            !cli_equals_ignore_case(options->lane, "bass") && !cli_equals_ignore_case(options->lane, "melody") &&
            !cli_equals_ignore_case(options->lane, "inner")) {
            return cli_fail(CLI_EXIT_USAGE, "--lane must be harmony, bass, melody, inner or all, not '%s'", options->lane);
        }
        if (options->variations) {
            if (!options->lane || !options->count || !options->out_dir) {
                return cli_fail(CLI_EXIT_USAGE, "--variations needs --lane, --count and --out-dir\n%s", cli_usage_of("piece"));
            }
            if (options->out || options->ending_out || options->midi || options->musicxml || options->ending_midi ||
                options->ending_musicxml || options->explain || options->layers) {
                return cli_fail(CLI_EXIT_USAGE, "--variations writes its files to --out-dir; the other output options do not apply");
            }
        } else if (options->count || options->out_dir) {
            return cli_fail(CLI_EXIT_USAGE, "--count and --out-dir go with --variations");
        }
        return 0;
    }
    if (options->request || options->regenerate || options->lane || options->variations || options->count || options->out_dir) {
        return cli_fail(CLI_EXIT_USAGE, "--request, --regenerate, --variations, --lane, --count and --out-dir go with --from\n%s",
                        cli_usage_of("piece"));
    }
    if (!options->input) {
        return cli_fail(CLI_EXIT_USAGE, "piece needs a request file\n%s", cli_usage_of("piece"));
    }
    return 0;
}


/* ------------------------------------------------------------- the request */

/* The request file, with the plan file of every theme fill read into it: the API takes plans, not paths. */
static int read_request_file(const char *path, cJSON **out) {
    char *text;
    *out = NULL;
    if (!cli_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read the request '%s'", path);
    }
    cJSON *request = cJSON_Parse(text);
    free(text);
    if (!request || !cJSON_IsObject(request)) {
        cJSON_Delete(request);
        return cli_fail(CLI_EXIT_USAGE, "the request '%s' is not a JSON object", path);
    }
    const cJSON *section = NULL;
    cJSON_ArrayForEach(section, cJSON_GetObjectItemCaseSensitive(request, "sections")) {
        cJSON *fill = cJSON_GetObjectItemCaseSensitive(section, "fill");
        const cJSON *kind = cJSON_GetObjectItemCaseSensitive(fill, "kind");
        const cJSON *plan_path = cJSON_GetObjectItemCaseSensitive(fill, "plan");
        if (!cJSON_IsObject(fill) || !cJSON_IsString(kind) || !cli_equals_ignore_case(kind->valuestring, "theme") ||
            !cJSON_IsString(plan_path)) {
            continue;
        }
        const cJSON *id = cJSON_GetObjectItemCaseSensitive(section, "id");
        /* The API checks that the plan reads; the file must be there and be JSON. */
        char *plan_text = NULL;
        cJSON *object = cli_read_file(plan_path->valuestring, &plan_text, NULL) ? cJSON_Parse(plan_text) : NULL;
        if (!object) {
            int status = cli_fail(CLI_EXIT_USAGE, "section '%s': cannot read the theme plan '%s': %s",
                                  cJSON_IsString(id) ? id->valuestring : "", plan_path->valuestring,
                                  plan_text ? "not valid JSON" : "could not open the file");
            free(plan_text);
            cJSON_Delete(request);
            return status;
        }
        free(plan_text);
        char *label = strdup(plan_path->valuestring);
        if (!object || !label || !cJSON_ReplaceItemInObject(fill, "plan", object) ||
            !cJSON_AddStringToObject(fill, "source", label)) {
            cJSON_Delete(object);
            free(label);
            cJSON_Delete(request);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
        }
        free(label);
    }
    *out = request;
    return 0;
}

/* The loop plan file to work on, the section and the lane, as members of the API request. */
static int add_edit(const Options *options, cJSON *request) {
    cJSON *plan = NULL;
    int status = cli_read_plan_file(options->from, &plan);
    if (status != 0) {
        return status;
    }
    cJSON *edit = cJSON_CreateObject();
    cJSON *lanes = cJSON_CreateArray();
    bool whole = !options->lane || cli_equals_ignore_case(options->lane, "all");
    bool ok = plan && edit && lanes && cJSON_AddItemToObject(request, "from", plan) &&
              (whole || cJSON_AddItemToArray(lanes, cJSON_CreateString(options->lane))) &&
              cJSON_AddItemToObject(edit, "lanes", lanes);
    if (ok && options->variations) {
        ok = cJSON_AddStringToObject(edit, "section", options->variations) &&
             cJSON_AddStringToObject(edit, "count", options->count) && cJSON_AddItemToObject(request, "variations", edit);
    } else if (ok) {
        ok = (cli_equals_ignore_case(options->regenerate, "none") ? cJSON_AddNullToObject(edit, "section")
                                                                      : cJSON_AddStringToObject(edit, "section", options->regenerate)) &&
             cJSON_AddItemToObject(request, "regenerate", edit);
    }
    return ok ? 0 : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
}

/* The API request: the request file (taken) and the options of the command as members. */
static int build_request(const Options *options, cJSON *piece, cJSON **out) {
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddItemToObject(request, "request", piece);
    if (!ok) {
        cJSON_Delete(piece);
    }
    if (ok && options->from) {
        int status = add_edit(options, request);
        if (status != 0) {
            cJSON_Delete(request);
            return status;
        }
    }
    ok = ok && (!options->seed || cJSON_AddStringToObject(request, "seed", options->seed)) &&
         cJSON_AddBoolToObject(request, "check", options->check) && cJSON_AddBoolToObject(request, "plain_midi", options->plain_midi);
    if (ok && options->variations) {
        ok = cJSON_AddBoolToObject(request, "musicxml", true);
    }
    if (ok && !options->variations) {
        ok = cJSON_AddBoolToObject(request, "midi", options->midi != NULL) &&
             cJSON_AddBoolToObject(request, "musicxml", options->musicxml != NULL) &&
             cJSON_AddBoolToObject(request, "ending_musicxml", options->ending_musicxml != NULL) &&
             (!options->layers || cJSON_AddTrueToObject(request, "layers"));
        if (ok && options->explain) {
            size_t length = strlen(options->explain);
            bool json = length >= 5 && cli_equals_ignore_case(options->explain + length - 5, ".json");
            ok = cJSON_AddStringToObject(request, "explain", json ? "json" : "text");
        }
    }
    if (!ok) {
        cJSON_Delete(request);
        *out = NULL;
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    *out = request;
    return 0;
}

/* The tempo of the brief of a document in quarter notes per minute, 0 for none. */
static double document_tempo(const cJSON *brief) {
    const cJSON *qpm = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(brief, "tempo"), "qpm");
    return cJSON_IsNumber(qpm) ? qpm->valuedouble : 0;
}

/* Sets (or replaces) a string member of object. */
static void set_string(cJSON *object, const char *name, const char *value) {
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    cJSON_AddStringToObject(object, name, value);
}

static void print_document(const cJSON *document) {
    char *text = cJSON_PrintUnformatted(document);
    printf("%s\n", text ? text : "{}");
    cJSON_free(text);
}

/* ------------------------------------------------------------- the files */

typedef enum {
    OUT_LOOP_PLAN,
    OUT_ENDING_PLAN,
    OUT_LOOP_MIDI,
    OUT_LOOP_MUSICXML,
    OUT_ENDING_MIDI,
    OUT_ENDING_MUSICXML,
    OUT_EXPLAIN
} OutputKind;

/* Renames every file written, or removes them all. Returns 0 or the status. */
static int finish_files(CliPendingFile *pending, size_t count, int status) {
    for (size_t i = 0; status == 0 && i < count; i++) {
        if (rename(pending[i].temporary, pending[i].final_path) != 0) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", pending[i].final_path);
        }
    }
    if (status != 0) {
        cli_discard_pending(pending, count);
    }
    return status;
}

/* Starts the pending file for path: the temporary name beside it. */
static int start_file(CliPendingFile *entry, const char *path) {
    if (snprintf(entry->final_path, sizeof(entry->final_path), "%s", path) >= (int)sizeof(entry->final_path) ||
        snprintf(entry->temporary, sizeof(entry->temporary), "%s.tmp", path) >= (int)sizeof(entry->temporary)) {
        return cli_fail(CLI_EXIT_WRITE, "file name '%s' is too long", path);
    }
    return 0;
}

/*
 * Writes every output to a temporary file next to its final name and renames them all only when every one was written, so
 * that an error leaves no output file behind.
 */
static int write_files(const Options *options, const CliResponse *response, const cJSON *document, CliPendingFile *pending,
                       size_t *pending_count) {
    const cJSON *plans = cJSON_GetObjectItemCaseSensitive(document, "plans");
    const cJSON *explanation = cJSON_GetObjectItemCaseSensitive(document, "explanation");
    double qpm = document_tempo(cJSON_GetObjectItemCaseSensitive(document, "brief"));
    const struct {
        OutputKind kind;
        const char *path;
    } wanted[] = {{OUT_LOOP_PLAN, options->out},          {OUT_ENDING_PLAN, options->ending_out},
                  {OUT_LOOP_MIDI, options->midi},         {OUT_LOOP_MUSICXML, options->musicxml},
                  {OUT_ENDING_MIDI, options->ending_midi}, {OUT_ENDING_MUSICXML, options->ending_musicxml},
                  {OUT_EXPLAIN, options->explain}};
    int status = 0;
    *pending_count = 0;
    if ((options->ending_out || options->ending_midi || options->ending_musicxml) &&
        !cJSON_GetObjectItemCaseSensitive(plans, "ending")) {
        return cli_fail(CLI_EXIT_USAGE, "the %s form has no ending version", options->form);
    }
    for (size_t i = 0; i < sizeof(wanted) / sizeof(wanted[0]) && status == 0; i++) {
        if (!wanted[i].path) {
            continue;
        }
        CliPendingFile *entry = &pending[*pending_count];
        status = start_file(entry, wanted[i].path);
        if (status != 0) {
            break;
        }
        (*pending_count)++;
        OutputKind kind = wanted[i].kind;
        if (kind == OUT_LOOP_PLAN || kind == OUT_ENDING_PLAN) {
            status = cli_write_plan_object(kind == OUT_LOOP_PLAN ? plans->child : cJSON_GetObjectItemCaseSensitive(plans, "ending"),
                                           entry->temporary, entry->final_path);
            continue;
        }
        const void *data = NULL;
        size_t size = 0;
        CliResponse exported;
        memset(&exported, 0, sizeof exported);
        if (kind == OUT_EXPLAIN) {
            data = explanation->valuestring;
            size = strlen(explanation->valuestring);
        } else if (kind == OUT_LOOP_MIDI || kind == OUT_LOOP_MUSICXML) {
            data = kind == OUT_LOOP_MIDI ? (const void *)response->midi : (const void *)response->musicxml;
            size = kind == OUT_LOOP_MIDI ? response->midi_size : strlen(response->musicxml);
        } else if (kind == OUT_ENDING_MUSICXML) {
            const cJSON *musicxml = cJSON_GetObjectItemCaseSensitive(document, "ending_musicxml");
            data = musicxml->valuestring;
            size = strlen(musicxml->valuestring);
        } else {
            status = cli_export_plan(cJSON_GetObjectItemCaseSensitive(plans, "ending"), qpm, options->plain_midi, true, false, NULL,
                                     &exported);
            if (status != 0) {
                break;
            }
            data = exported.midi;
            size = exported.midi_size;
        }
        if (!cli_write_bytes(entry->temporary, data, size)) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s': could not write '%s'", entry->final_path, entry->temporary);
        }
        cli_response_destroy(&exported);
    }
    return finish_files(pending, *pending_count, status);
}

/* The document as `piece --format json` prints it: the plans of the versions are the files, and the files are listed. */
static void print_json(cJSON *document, const Options *options, const CliPendingFile *pending, size_t pending_count) {
    cJSON *versions = cJSON_GetObjectItemCaseSensitive(document, "versions");
    const char *paths[2] = {options->out, options->ending_out};
    const cJSON *plan = cJSON_GetObjectItemCaseSensitive(document, "plans")->child;
    for (int v = 0; v < 2 && plan; v++, plan = plan->next) {
        if (paths[v]) {
            set_string(cJSON_GetObjectItemCaseSensitive(versions, plan->string), "plan", paths[v]);
        }
    }
    cJSON_DeleteItemFromObjectCaseSensitive(document, "plans");
    cJSON_DeleteItemFromObjectCaseSensitive(document, "explanation");
    cJSON_DeleteItemFromObjectCaseSensitive(document, "ending_musicxml");
    cJSON_DeleteItemFromObjectCaseSensitive(document, "realization");
    cJSON_DeleteItemFromObjectCaseSensitive(document, "reading");
    cJSON *files = cJSON_AddArrayToObject(document, "files");
    for (size_t i = 0; files && i < pending_count; i++) {
        cJSON_AddItemToArray(files, cJSON_CreateString(pending[i].final_path));
    }
    print_document(document);
}

/* The result of a composed or regenerated piece: the files written, then the result printed. Returns the exit status. */
static int emit(const Options *options, const CliResponse *response, bool json) {
    cJSON *document = cJSON_Parse(response->json);
    if (!document) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "the piece could not be read back");
    }
    CliPendingFile pending[OUTPUT_MAX];
    size_t pending_count = 0;
    int status = write_files(options, response, document, pending, &pending_count);
    if (status == 0 && options->layers) {
        status = cli_write_layers(options->layers, response);
    }
    if (status == 0 && json) {
        print_json(document, options, pending, pending_count);
    } else if (status == 0) {
        const char *files[OUTPUT_MAX];
        for (size_t i = 0; i < pending_count; i++) {
            files[i] = pending[i].final_path;
        }
        cli_print_piece_text(document, files, pending_count);
    }
    cJSON_Delete(document);
    return status;
}

/* ------------------------------------------------------------- variations */

typedef struct {
    char plan[PATH_SIZE];
    char midi[PATH_SIZE];
    char musicxml[PATH_SIZE];
} VariationFiles;

/* Writes what the document holds to path next to its final name, then renames it. */
static int write_result_file(const cJSON *document, const char *path) {
    char temporary[PATH_SIZE + 8];
    snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    char *printed = cJSON_PrintUnformatted(document);
    char *text = printed ? malloc(strlen(printed) + 2) : NULL;
    if (text) {
        sprintf(text, "%s\n", printed);
    }
    bool ok = text && cli_write_bytes(temporary, text, strlen(text));
    cJSON_free(printed);
    free(text);
    if (!ok || rename(temporary, path) != 0) {
        remove(temporary);
        return cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", path);
    }
    return 0;
}

/* variation-<i>.json, .mid and .musicxml of the loop version, and result.json, in the directory. */
static int write_variations(const Options *options, cJSON *document, bool json) {
    cJSON *list = cJSON_GetObjectItemCaseSensitive(document, "variations");
    size_t kept = (size_t)cJSON_GetArraySize(list);
    double qpm = document_tempo(cJSON_GetObjectItemCaseSensitive(document, "brief"));
    if (mkdir(options->out_dir, 0755) != 0 && errno != EEXIST) {
        return cli_fail(CLI_EXIT_WRITE, "cannot create '%s': %s", options->out_dir, strerror(errno));
    }
    VariationFiles files[VARIATIONS_MAX];
    CliPendingFile pending[3 * VARIATIONS_MAX];
    size_t pending_count = 0;
    int status = 0;
    memset(files, 0, sizeof(files));
    for (size_t i = 0; status == 0 && i < kept && i < VARIATIONS_MAX; i++) {
        cJSON *item = cJSON_GetArrayItem(list, (int)i);
        cJSON *loop = (cJSON *)cli_piece_variation_version(item);
        VariationFiles *named = &files[i];
        if (snprintf(named->plan, PATH_SIZE, "%s/variation-%zu.json", options->out_dir, i + 1) >= PATH_SIZE ||
            snprintf(named->midi, PATH_SIZE, "%s/variation-%zu.mid", options->out_dir, i + 1) >= PATH_SIZE ||
            snprintf(named->musicxml, PATH_SIZE, "%s/variation-%zu.musicxml", options->out_dir, i + 1) >= PATH_SIZE) {
            status = cli_fail(CLI_EXIT_WRITE, "file name in '%s' is too long", options->out_dir);
            break;
        }
        const char *paths[3] = {named->plan, named->midi, named->musicxml};
        for (int k = 0; k < 3 && status == 0; k++) {
            status = start_file(&pending[pending_count++], paths[k]);
        }
        if (status != 0) {
            break;
        }
        CliPendingFile *plan_file = &pending[pending_count - 3];
        const cJSON *plan = cJSON_GetObjectItemCaseSensitive(loop, "plan");
        CliResponse exported;
        memset(&exported, 0, sizeof exported);
        status = cli_write_plan_object(plan, plan_file[0].temporary, plan_file[0].final_path);
        if (status == 0) {
            status = cli_export_plan(plan, qpm, options->plain_midi, true, false,
                                     cJSON_GetObjectItemCaseSensitive(item, "loop_markers"), &exported);
        }
        const cJSON *musicxml = cJSON_GetObjectItemCaseSensitive(item, "musicxml");
        if (status == 0 && (!cli_write_bytes(plan_file[1].temporary, exported.midi, exported.midi_size) ||
                            !cli_write_bytes(plan_file[2].temporary, musicxml->valuestring, strlen(musicxml->valuestring)))) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", plan_file[1].final_path);
        }
        cli_response_destroy(&exported);
        /* The document as printed: the plan of the version is its file, and the files are named. */
        set_string(loop, "plan", named->plan);
        set_string(item, "midi", named->midi);
        set_string(item, "musicxml", named->musicxml);
        cJSON_DeleteItemFromObjectCaseSensitive(item, "loop_markers");
    }
    cJSON_DeleteItemFromObjectCaseSensitive(document, "brief");
    if (status == 0) {
        status = finish_files(pending, pending_count, 0);
    } else {
        cli_discard_pending(pending, pending_count);
    }
    char path[PATH_SIZE];
    if (status == 0 && snprintf(path, PATH_SIZE, "%s/result.json", options->out_dir) >= PATH_SIZE) {
        status = cli_fail(CLI_EXIT_WRITE, "file name in '%s' is too long", options->out_dir);
    }
    if (status == 0) {
        status = write_result_file(document, path);
    }
    if (status == 0 && json) {
        print_document(document);
    } else if (status == 0) {
        const char *plans[VARIATIONS_MAX];
        const char *midis[VARIATIONS_MAX];
        const char *musicxmls[VARIATIONS_MAX];
        for (size_t i = 0; i < kept && i < VARIATIONS_MAX; i++) {
            plans[i] = files[i].plan;
            midis[i] = files[i].midi;
            musicxmls[i] = files[i].musicxml;
        }
        cli_print_piece_variations_text(document, plans, midis, musicxmls);
    }
    return status;
}

/* ------------------------------------------------------------- the command */

int run_piece(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    status = check_edit_options(&options);
    if (status != 0) {
        return status;
    }
    bool json = false;
    if (options.format) {
        json = cli_equals_ignore_case(options.format, "json");
        if (!json && !cli_equals_ignore_case(options.format, "text")) {
            return cli_fail(CLI_EXIT_USAGE, "--format must be text or json, not '%s'", options.format);
        }
    }
    cJSON *piece = NULL;
    status = read_request_file(options.from ? options.request : options.input, &piece);
    if (status != 0) {
        return status;
    }
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(piece, "form");
    snprintf(options.form, sizeof options.form, "%s", cJSON_IsString(form) ? form->valuestring : "game-loop");
    cJSON *request = NULL;
    status = build_request(&options, piece, &request);
    if (status != 0) {
        return status;
    }
    CliResponse response;
    UrsatzStatus api_status = cli_composer(options.variations ? "piece_variations" : "piece", request, NULL, &response);
    cJSON_Delete(request);
    if (api_status != URSATZ_OK) {
        status = cli_fail_api(api_status, &response.error, NULL);
    } else if (options.variations) {
        cJSON *document = cJSON_Parse(response.json);
        status = document ? write_variations(&options, document, json) : cli_fail(CLI_EXIT_UNSATISFIABLE, "the variations could not be read back");
        cJSON_Delete(document);
    } else {
        status = emit(&options, &response, json);
    }
    cli_response_destroy(&response);
    return status;
}
