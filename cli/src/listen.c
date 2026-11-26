#include "listen.h"
#include "cli_tools.h"

#include <stddef.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/files.h>
#include <common/hash.h>


#include "cli_options.h"
#include "cli_params.h"
#include "flags.h"
#include "motive_input.h"
#include "session.h"
#include "workflow.h"

/* The most samples one run makes, and the largest seed a run starts from (seed + index stays a valid seed). */
#define LISTEN_COUNT_MAX 64
#define LISTEN_COUNT_DEFAULT 10
#define LISTEN_SEED_MAX 4294967231UL
#define LISTEN_PATH_SIZE 1024

typedef struct {
    bool print_settings;
    const char *what_text;
    const char *count_text;
    const char *seed_text;
    const char *out_dir;
    const char *settings_path;
    const char *material_text;
    const char *meter_text;
    const char *style_path;
    const char *tempo_text;
    const char *strategy_text;
    const char *into_text;
    const char *development_text;
    const char *bars_text;
    const char *motif_path;
    const char *theme_request_path;
    const char *blueprint_path; /* --blueprint: a theme request with a blueprint, as JSON or as Ursatz Text (R-747) */
    const char *blend_path;
    const char *blend_weight_text;
    const char *blend_weights_text;
    const char *session_text; /* --session: the data mode (R-674) */
    const char *round_text;
    const char *batch_text;
    const char *taste_text;
    bool check;
    bool plans; /* --plans: each theme sample's JSON holds its plan */
    bool themes;
    bool meter_given;
    unsigned long count;
    unsigned long seed;
} Options;

/* One sample's result: the files it wrote and why it wrote none. */
typedef struct {
    char files[3][LISTEN_PATH_SIZE];
    size_t file_count;
    char status[512];
    bool ok;
} Sample;


static const CliOptionRow ROWS[] = {
    {"--print-settings", offsetof(Options, print_settings), true},
    {"--check", offsetof(Options, check), true},
    {"--plans", offsetof(Options, plans), true},
    {"--what", offsetof(Options, what_text), false},
    {"--count", offsetof(Options, count_text), false},
    {"--seed", offsetof(Options, seed_text), false},
    {"--out-dir", offsetof(Options, out_dir), false},
    {"--settings", offsetof(Options, settings_path), false},
    {"--material", offsetof(Options, material_text), false},
    {"--meter", offsetof(Options, meter_text), false},
    {"--style", offsetof(Options, style_path), false},
    {"--tempo", offsetof(Options, tempo_text), false},
    {"--strategy", offsetof(Options, strategy_text), false},
    {"--into", offsetof(Options, into_text), false},
    {"--development", offsetof(Options, development_text), false},
    {"--bars", offsetof(Options, bars_text), false},
    {"--motif", offsetof(Options, motif_path), false},
    {"--theme-request", offsetof(Options, theme_request_path), false},
    {"--blueprint", offsetof(Options, blueprint_path), false},
    {"--blend", offsetof(Options, blend_path), false},
    {"--blend-weight", offsetof(Options, blend_weight_text), false},
    {"--blend-weights", offsetof(Options, blend_weights_text), false},
    {"--session", offsetof(Options, session_text), false},
    {"--round", offsetof(Options, round_text), false},
    {"--batch", offsetof(Options, batch_text), false},
    {"--taste", offsetof(Options, taste_text), false},
};

static int read_options(const CliArgs *args, Options *options) {
    memset(options, 0, sizeof(*options));
    cli_options_read(args, ROWS, sizeof(ROWS) / sizeof(ROWS[0]), options);
    return 0;
}

static int check_options(Options *options) {
    if (!options->what_text || (strcmp(options->what_text, "motifs") != 0 && strcmp(options->what_text, "themes") != 0)) {
        return cli_fail(CLI_EXIT_USAGE, "--what must be motifs or themes\n%s", cli_usage_of("listen"));
    }
    options->themes = strcmp(options->what_text, "themes") == 0;
    if (!options->session_text && (options->round_text || options->batch_text || options->taste_text)) {
        return cli_fail(CLI_EXIT_USAGE, "--round, --batch and --taste need --session");
    }
    if (options->session_text && (!options->round_text || !options->batch_text)) {
        return cli_fail(CLI_EXIT_USAGE, "--session needs --round <n> and --batch <name>\n%s", cli_usage_of("listen"));
    }
    if (!options->themes && (options->into_text || options->development_text || options->bars_text || options->motif_path ||
                             options->theme_request_path || options->blueprint_path || options->blend_path || options->blend_weight_text ||
                             options->blend_weights_text || options->check || options->plans)) {
        return cli_fail(CLI_EXIT_USAGE, "--into, --development, --bars, --check, --plans, --motif, --theme-request, --blueprint and the --blend options need --what themes");
    }
    if (options->motif_path && options->theme_request_path) {
        return cli_fail(CLI_EXIT_USAGE, "--motif and --theme-request cannot be given together");
    }
    if (options->blueprint_path && (options->motif_path || options->theme_request_path)) {
        return cli_fail(CLI_EXIT_USAGE, "--blueprint cannot be given together with --motif or --theme-request");
    }
    if ((options->blend_weight_text || options->blend_weights_text) && !options->blend_path) {
        return cli_fail(CLI_EXIT_USAGE, "--blend-weight and --blend-weights need --blend");
    }
    if (options->blend_path && !options->style_path) {
        return cli_fail(CLI_EXIT_USAGE, "--blend needs --style");
    }
    if (options->into_text && strcmp(options->into_text, "sentence") != 0 && strcmp(options->into_text, "period") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--into must be sentence or period, not '%s'", options->into_text);
    }
    if (options->development_text && strcmp(options->development_text, "melody") != 0 &&
        strcmp(options->development_text, "balanced") != 0 && strcmp(options->development_text, "theme") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--development must be melody, balanced or theme, not '%s'", options->development_text);
    }
    if (options->bars_text && strcmp(options->bars_text, "4") != 0 && strcmp(options->bars_text, "8") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--bars must be 4 or 8, not '%s'", options->bars_text);
    }
    if (!options->into_text) {
        options->into_text = "sentence";
    }
    if (!options->development_text) {
        options->development_text = "balanced";
    }
    if (!options->bars_text) {
        options->bars_text = "8";
    }
    if (!options->out_dir) {
        return cli_needs("listen", "--out-dir <dir>", true);
    }
    options->count = LISTEN_COUNT_DEFAULT;
    if (options->count_text &&
        (!cli_whole_number(options->count_text, LISTEN_COUNT_MAX, &options->count) || options->count < 1)) {
        return cli_fail(CLI_EXIT_USAGE, "--count must be a whole number from 1 to %d, not '%s'", LISTEN_COUNT_MAX,
                        options->count_text);
    }
    options->seed = 1;
    if (options->seed_text && !cli_whole_number(options->seed_text, LISTEN_SEED_MAX, &options->seed)) {
        return cli_fail(CLI_EXIT_USAGE, "--seed must be a whole number from 0 to %lu, not '%s'", LISTEN_SEED_MAX,
                        options->seed_text);
    }
    if (!options->material_text) {
        options->material_text = "major";
    }
    options->meter_given = options->meter_text != NULL;
    if (!options->meter_text) {
        options->meter_text = "4/4";
    }
    if (!options->strategy_text) {
        options->strategy_text = "segments";
    }
    if (strcmp(options->strategy_text, "segments") != 0 && strcmp(options->strategy_text, "drawn") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--strategy must be segments or drawn, not '%s'", options->strategy_text);
    }
    return 0;
}

/* --- files ----------------------------------------------------------------------------------- */

/* The file read as a JSON document (*out), or the usage status after printing why not. */
static int read_json_file(const char *path, const char *what, cJSON **out) {
    char *text = NULL;
    if (!cli_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read %s '%s'", what, path);
    }
    *out = cJSON_Parse(text);
    free(text);
    if (!*out) {
        return cli_fail(CLI_EXIT_USAGE, "%s '%s' is not valid JSON", what, path);
    }
    return 0;
}

/* The theme request of a blueprint file (*out): the JSON of a theme request with a blueprint member as it is, or Ursatz Text (R-747) read by the
   workshop's tool blueprint-parse. The usage status after printing why not. */
static int read_blueprint_file(const char *path, cJSON **out) {
    char *text = NULL;
    if (!cli_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read blueprint '%s'", path);
    }
    const char *first = text;
    while (*first == ' ' || *first == '\t' || *first == '\r' || *first == '\n') {
        first++;
    }
    if (*first == '{') {
        *out = cJSON_Parse(text);
        free(text);
        if (!*out) {
            return cli_fail(CLI_EXIT_USAGE, "blueprint '%s' is not valid JSON", path);
        }
    } else {
        Workshop *workshop = cli_tools();
        cJSON *params = cJSON_CreateObject();
        if (!workshop || !params || !cJSON_AddStringToObject(params, "text", text)) {
            free(text);
            cJSON_Delete(params);
            return cli_fail(CLI_EXIT_UNSATISFIABLE, workshop ? "out of memory" : "a knowledge snapshot is required");
        }
        free(text);
        WorkshopReply reply;
        UrsatzError error;
        memset(&error, 0, sizeof error);
        UrsatzStatus status = workshop_tool(workshop, "blueprint-parse", params, NULL, &reply, &error);
        cJSON_Delete(params);
        if (status != URSATZ_OK) {
            workshop_reply_destroy(&reply);
            return cli_fail(CLI_EXIT_USAGE, "blueprint '%s': %s", path, error.message);
        }
        *out = cJSON_DetachItemFromObjectCaseSensitive(reply.json, "request");
        workshop_reply_destroy(&reply);
        if (!*out) {
            return cli_fail(CLI_EXIT_UNSATISFIABLE, "the blueprint text did not parse into a request");
        }
    }
    if (!cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(*out, "blueprint"))) {
        cJSON_Delete(*out);
        *out = NULL;
        return cli_fail(CLI_EXIT_USAGE, "blueprint '%s' is not a theme request with a blueprint member", path);
    }
    return 0;
}

static int check_readable(const char *path, const char *what) {
    char *text = NULL;
    if (!cli_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read %s '%s'", what, path);
    }
    free(text);
    return 0;
}

static int make_directory(const char *path) {
    UrsatzError error;
    if (common_dir_make(path, &error) != URSATZ_OK) {
        return cli_fail(CLI_EXIT_WRITE, "cannot create '%s': %s", error.path, error.message);
    }
    return 0;
}

/* Writes <out-dir>/<index><extension>; the path is stored in sample->files. */
static int write_sample_file(const Options *options, unsigned long index, const char *extension, const void *data, size_t size,
                             Sample *sample) {
    char *path = sample->files[sample->file_count];
    if (snprintf(path, LISTEN_PATH_SIZE, "%s/%02lu%s", options->out_dir, index, extension) >= LISTEN_PATH_SIZE) {
        return cli_fail(CLI_EXIT_WRITE, "file name in '%s' is too long", options->out_dir);
    }
    if (!cli_write_bytes(path, data, size)) {
        return cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", path);
    }
    sample->file_count++;
    return 0;
}

/* --- the settings ---------------------------------------------------------------------------- */

/* The preferences document of the default settings, as the vocabulary serves it. */
static int print_default_settings(void) {
    CliResponse response;
    memset(&response, 0, sizeof response);
    if (cli_composer("vocabulary", NULL, NULL, &response) != URSATZ_OK) {
        cli_response_destroy(&response);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read the vocabulary");
    }
    cJSON *vocabulary = cJSON_Parse(response.json);
    cli_response_destroy(&response);
    const cJSON *preferences = cJSON_GetObjectItemCaseSensitive(vocabulary, "preferences");
    if (!preferences) {
        cJSON_Delete(vocabulary);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "the vocabulary has no preferences");
    }
    cli_print_json(preferences);
    cJSON_Delete(vocabulary);
    return 0;
}

/* --- the manifest ---------------------------------------------------------------------------- */

typedef struct {
    FILE *file;
    const Options *options;
    const cJSON *preferences; /* the document sent, or NULL */
} Manifest;

/* The file the themes are made from, a theme request or a blueprint (the manifest records either as theme_request); NULL for neither. */
static const char *request_path(const Options *options) {
    return options->theme_request_path ? options->theme_request_path : options->blueprint_path;
}

/* One line of listen.jsonl for the sample. */
static int manifest_add(const Manifest *manifest, unsigned long index, unsigned long seed, const Sample *sample) {
    cJSON *line = cJSON_CreateObject();
    cJSON *files = line ? cJSON_CreateArray() : NULL;
    bool ok = files != NULL;
    for (size_t i = 0; ok && i < sample->file_count; i++) {
        ok = cJSON_AddItemToArray(files, cJSON_CreateString(sample->files[i]));
    }
    ok = ok && cJSON_AddNumberToObject(line, "index", (double)index) && cJSON_AddNumberToObject(line, "seed", (double)seed) &&
         cJSON_AddStringToObject(line, "what", manifest->options->what_text) && cJSON_AddStringToObject(line, "motif", manifest->options->motif_path ? manifest->options->motif_path : "generated") &&
         (request_path(manifest->options) ? cJSON_AddStringToObject(line, "theme_request", request_path(manifest->options)) != NULL
                                          : cJSON_AddNullToObject(line, "theme_request") != NULL);
    ok = ok && (manifest->options->style_path ? cJSON_AddStringToObject(line, "style", manifest->options->style_path)
                                              : cJSON_AddNullToObject(line, "style"));
    ok = ok && (manifest->options->blend_path ? cJSON_AddStringToObject(line, "blend", manifest->options->blend_path) != NULL
                                              : cJSON_AddNullToObject(line, "blend") != NULL);
    ok = ok && (manifest->options->settings_path ? cJSON_AddStringToObject(line, "settings", manifest->options->settings_path)
                                                 : cJSON_AddNullToObject(line, "settings"));
    ok = ok && (manifest->preferences ? cJSON_AddItemToObject(line, "preferences", cJSON_Duplicate(manifest->preferences, true))
                                      : cJSON_AddNullToObject(line, "preferences") != NULL);
    ok = ok && cJSON_AddItemToObject(line, "files", files) && cJSON_AddStringToObject(line, "status", sample->status);
    char *text = ok ? cJSON_PrintUnformatted(line) : NULL;
    if (!ok) {
        cJSON_Delete(files);
    }
    cJSON_Delete(line);
    if (!text) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    bool wrote = fprintf(manifest->file, "%s\n", text) > 0 && fflush(manifest->file) == 0;
    cJSON_free(text);
    return wrote ? 0 : cli_fail(CLI_EXIT_WRITE, "cannot write listen.jsonl");
}

/* --- the samples ----------------------------------------------------------------------------- */

static void refuse(Sample *sample, UrsatzStatus status, const UrsatzError *error) {
    const char *kind = status == URSATZ_ERR_UNSATISFIABLE ? "stuck" : "refused";
    if (error->path[0]) {
        snprintf(sample->status, sizeof sample->status, "%s: %.64s: %.400s", kind, error->path, error->message);
    } else {
        snprintf(sample->status, sizeof sample->status, "%s: %.480s", kind, error->message);
    }
}

static char *seed_text(unsigned long seed, char *buffer, size_t size) {
    snprintf(buffer, size, "%lu", seed);
    return buffer;
}

/* The request of a random motive: the sample's seed, the mode, meter, strategy, style and settings, and the motives to avoid (R-673). */
static cJSON *random_motive_request(const Options *options, unsigned long seed, const cJSON *preferences, const cJSON *avoid) {
    char number[32];
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddStringToObject(request, "material", options->material_text) &&
              cJSON_AddStringToObject(request, "meter", options->meter_text) &&
              cJSON_AddStringToObject(request, "strategy", options->strategy_text) && cJSON_AddNumberToObject(request, "count", 1) &&
              cJSON_AddStringToObject(request, "seed", seed_text(seed, number, sizeof number)) &&
              (!options->style_path || cJSON_AddStringToObject(request, "style", options->style_path)) &&
              (!preferences || cJSON_AddItemToObject(request, "preferences", cJSON_Duplicate(preferences, true))) &&
              (!avoid || cJSON_AddItemToObject(request, "avoid", cJSON_Duplicate(avoid, true)));
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* The render request of a motive document: the motive as the json input, with the tempo, asking for both files. */
static cJSON *render_request(const Options *options, const cJSON *motive) {
    cJSON *request = cJSON_CreateObject();
    cJSON *input = request ? cJSON_AddObjectToObject(request, "input") : NULL;
    char *motive_text = motive ? cJSON_PrintUnformatted(motive) : NULL;
    bool ok = input && motive_text && cJSON_AddStringToObject(input, "format", "json") &&
              cJSON_AddStringToObject(input, "text", motive_text) && cJSON_AddTrueToObject(request, "midi") &&
              cJSON_AddTrueToObject(request, "musicxml") &&
              (!options->tempo_text || cJSON_AddStringToObject(request, "tempo", options->tempo_text));
    cJSON_free(motive_text);
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* Calls an API entry with the request object (consumed). */
static UrsatzStatus call(const char *entry, cJSON *request, CliResponse *response) {
    if (!request) {
        memset(response, 0, sizeof *response);
        snprintf(response->error.message, sizeof response->error.message, "out of memory");
        return URSATZ_ERR_ALLOC;
    }
    UrsatzStatus status = cli_composer(entry, request, NULL, response);
    cJSON_Delete(request);
    return status;
}

/* The most motives a varied batch hands to the next sample: the API's limit of avoid. */
#define LISTEN_AVOID_MAX 64

/* One motif: a random motive, rendered to MIDI and MusicXML, and its API response as json. */
static int make_motif(const Options *options, unsigned long index, unsigned long seed, const cJSON *preferences, cJSON *avoid,
                      Sample *sample) {
    CliResponse drawn;
    UrsatzStatus status = call("random_motive", random_motive_request(options, seed, preferences, avoid), &drawn);
    if (status != URSATZ_OK) {
        refuse(sample, status, &drawn.error);
        cli_response_destroy(&drawn);
        return 0;
    }
    cJSON *document = cJSON_Parse(drawn.json);
    cli_response_destroy(&drawn);
    const cJSON *first = document ? cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "motives"), 0) : NULL;
    /* A varied batch (R-673) hands each written motive to the samples after it, the last AVOID_MAX of them. */
    const cJSON *motive_document = cJSON_GetObjectItemCaseSensitive(first, "motive");
    cJSON *kept = avoid && motive_document ? cJSON_Duplicate(motive_document, true) : NULL;
    CliResponse rendered;
    status = call("motive_render", render_request(options, cJSON_GetObjectItemCaseSensitive(first, "motive")), &rendered);
    cJSON_Delete(document);
    if (status != URSATZ_OK) {
        cJSON_Delete(kept);
        refuse(sample, status, &rendered.error);
        cli_response_destroy(&rendered);
        return 0;
    }
    int result = write_sample_file(options, index, ".mid", rendered.midi, rendered.midi_size, sample);
    if (result == 0) {
        result = write_sample_file(options, index, ".musicxml", rendered.musicxml, strlen(rendered.musicxml), sample);
    }
    if (result == 0) {
        result = write_sample_file(options, index, ".json", rendered.json, strlen(rendered.json), sample);
    }
    cli_response_destroy(&rendered);
    if (result == 0) {
        sample->ok = true;
        snprintf(sample->status, sizeof sample->status, "ok");
        if (kept) {
            while (cJSON_GetArraySize(avoid) >= LISTEN_AVOID_MAX) {
                cJSON_DeleteItemFromArray(avoid, 0);
            }
            if (cJSON_AddItemToArray(avoid, kept)) {
                kept = NULL;
            }
        }
    }
    cJSON_Delete(kept);
    return result;
}

/* --- themes ---------------------------------------------------------------------------------- */

/* Sets a member of an object, replacing one of the same name (item is consumed). */
static bool set_member(cJSON *object, const char *name, cJSON *item) {
    if (!item) {
        return false;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(object, name);
    return cJSON_AddItemToObject(object, name, item);
}

/* The style, blend and tempo members of the options, and check and plans, on a theme request. */
static bool add_brief(cJSON *request, const Options *options) {
    bool ok = true;
    const struct {
        const char *name;
        const char *text;
    } texts[] = {{"style", options->style_path},
                 {"blend", options->blend_path},
                 {"blend_weight", options->blend_weight_text},
                 {"blend_weights", options->blend_weights_text},
                 {"tempo", options->tempo_text}};
    for (size_t i = 0; ok && i < sizeof texts / sizeof texts[0]; i++) {
        ok = !texts[i].text || set_member(request, texts[i].name, cJSON_CreateString(texts[i].text));
    }
    return ok && (!options->check || set_member(request, "check", cJSON_CreateTrue())) &&
           (!options->plans || set_member(request, "plans", cJSON_CreateTrue()));
}

/* The members every theme request takes from the sample: the seed, one theme, both files, the settings. */
static bool add_sample(cJSON *request, unsigned long seed, const cJSON *preferences) {
    char number[32];
    return set_member(request, "seed", cJSON_CreateString(seed_text(seed, number, sizeof number))) &&
           set_member(request, "suggest", cJSON_CreateString("1")) && set_member(request, "midi", cJSON_CreateTrue()) &&
           set_member(request, "musicxml", cJSON_CreateTrue()) &&
           (!preferences || set_member(request, "preferences", cJSON_Duplicate(preferences, true)));
}

/* The motif as the `motive` command reads it: the motive document {"motive", "analysis"} the API answers with, in key C. */
static int read_motif_file(const Options *options, cJSON **document) {
    char key[48];
    snprintf(key, sizeof key, "C %s", options->material_text);
    MotiveInput input;
    memset(&input, 0, sizeof input);
    input.path = options->motif_path;
    input.meter_text = options->meter_given ? options->meter_text : NULL;
    input.key_text = key;
    cJSON *request = NULL;
    MotiveFile file;
    memset(&file, 0, sizeof file);
    int status = motive_input_text_request(&input, &request, &file);
    if (status != 0) {
        return status;
    }
    /* A MIDI file was read already; its motive is the text input of the request. */
    CliResponse response;
    UrsatzStatus called = call("motive", request, &response);
    if (called != URSATZ_OK) {
        MotiveInputNames names = {options->motif_path, NULL, NULL, NULL};
        status = motive_input_fail(&file, called, &response.error, &names);
    } else {
        *document = cJSON_Parse(response.json);
        status = *document ? 0 : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    cli_response_destroy(&response);
    return status;
}

/* The same analysis for a generated motive of a random motive response. */
static UrsatzStatus analyze_motive(const cJSON *motive, CliResponse *response) {
    cJSON *request = cJSON_CreateObject();
    cJSON *input = request ? cJSON_AddObjectToObject(request, "input") : NULL;
    char *text = motive ? cJSON_PrintUnformatted(motive) : NULL;
    bool ok = input && text && cJSON_AddStringToObject(input, "format", "json") && cJSON_AddStringToObject(input, "text", text);
    cJSON_free(text);
    if (!ok) {
        cJSON_Delete(request);
        request = NULL;
    }
    return call("motive", request, response);
}

/* The theme request of a motive document: key C, the options as the brief, the motive as M1 in the basic slot. */
static cJSON *theme_request_from_motive(const Options *options, const cJSON *document, unsigned long seed,
                                        const cJSON *preferences) {
    const cJSON *motive = cJSON_GetObjectItemCaseSensitive(document, "motive");
    const cJSON *analysis = cJSON_GetObjectItemCaseSensitive(document, "analysis");
    const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(analysis, "degrees_text");
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(analysis, "pickup");
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(motive, "harmony");
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(motive, "features");
    const cJSON *meter = cJSON_GetObjectItemCaseSensitive(motive, "meter");
    if (!cJSON_IsString(degrees)) {
        return NULL;
    }
    char meter_text[32];
    const cJSON *beats = cJSON_GetObjectItemCaseSensitive(meter, "beats_per_bar");
    const cJSON *unit = cJSON_GetObjectItemCaseSensitive(meter, "beat_unit");
    if (options->meter_given || !cJSON_IsNumber(beats) || !cJSON_IsNumber(unit)) {
        snprintf(meter_text, sizeof meter_text, "%s", options->meter_text);
    } else {
        snprintf(meter_text, sizeof meter_text, "%.0f/%.0f", beats->valuedouble, unit->valuedouble);
    }
    cJSON *request = cJSON_CreateObject();
    cJSON *list = request ? cJSON_AddArrayToObject(request, "motives") : NULL;
    cJSON *entry = list ? cJSON_CreateObject() : NULL;
    cJSON *slots = request ? cJSON_CreateObject() : NULL;
    bool ok = entry && slots && cJSON_AddItemToArray(list, entry) && cJSON_AddItemToObject(request, "slots", slots);
    if (!ok) {
        cJSON_Delete(slots);
        cJSON_Delete(request);
        return NULL;
    }
    ok = cJSON_AddStringToObject(request, "schema", "ursatz-theme-request") && cJSON_AddNumberToObject(request, "schema_version", 2) &&
         cJSON_AddStringToObject(request, "into", options->into_text) && cJSON_AddStringToObject(request, "key", "C") &&
         cJSON_AddStringToObject(request, "material", options->material_text) && cJSON_AddStringToObject(request, "meter", meter_text) &&
         cJSON_AddNumberToObject(request, "bars", options->bars_text[0] - '0') &&
         cJSON_AddStringToObject(request, "development", options->development_text) &&
         cJSON_AddStringToObject(entry, "id", "M1") && cJSON_AddStringToObject(entry, "form", "full") &&
         cJSON_AddStringToObject(entry, "degrees", degrees->valuestring) &&
         cJSON_AddStringToObject(entry, "pickup", cJSON_IsString(pickup) ? pickup->valuestring : "0") &&
         (cJSON_IsString(harmony) && harmony->valuestring[0] ? cJSON_AddStringToObject(entry, "harmony", harmony->valuestring) != NULL
                                                              : cJSON_AddNullToObject(entry, "harmony") != NULL) &&
         cJSON_AddItemToObject(entry, "features", cJSON_IsArray(features) ? cJSON_Duplicate(features, true) : cJSON_CreateArray()) &&
         cJSON_AddStringToObject(entry, "role", "melodic") && cJSON_AddStringToObject(slots, "basic", "M1") &&
         add_brief(request, options) && add_sample(request, seed, preferences);
    if (!ok) {
        cJSON_Delete(request);
        return NULL;
    }
    return request;
}

/* One theme: the request composed, with its MIDI, MusicXML and API response written. */
static int make_theme(const Options *options, unsigned long index, unsigned long seed, const cJSON *preferences,
                      const cJSON *motif_document, const cJSON *theme_request, cJSON *avoid, Sample *sample) {
    cJSON *request = NULL;
    cJSON *kept = NULL; /* the generated motif, which a varied batch (R-722) hands to the themes after this one */
    if (theme_request) {
        request = cJSON_Duplicate(theme_request, true);
        if (request && !(add_brief(request, options) && add_sample(request, seed, preferences))) {
            cJSON_Delete(request);
            request = NULL;
        }
    } else if (motif_document) {
        request = theme_request_from_motive(options, motif_document, seed, preferences);
    } else {
        CliResponse drawn;
        UrsatzStatus status = call("random_motive", random_motive_request(options, seed, preferences, avoid), &drawn);
        if (status != URSATZ_OK) {
            refuse(sample, status, &drawn.error);
            cli_response_destroy(&drawn);
            return 0;
        }
        cJSON *document = cJSON_Parse(drawn.json);
        cli_response_destroy(&drawn);
        const cJSON *first = document ? cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "motives"), 0) : NULL;
        const cJSON *drawn_motive = cJSON_GetObjectItemCaseSensitive(first, "motive");
        kept = avoid && drawn_motive ? cJSON_Duplicate(drawn_motive, true) : NULL;
        CliResponse analyzed;
        status = analyze_motive(cJSON_GetObjectItemCaseSensitive(first, "motive"), &analyzed);
        cJSON_Delete(document);
        if (status != URSATZ_OK) {
            refuse(sample, status, &analyzed.error);
            cli_response_destroy(&analyzed);
            cJSON_Delete(kept);
            return 0;
        }
        cJSON *analysis = cJSON_Parse(analyzed.json);
        cli_response_destroy(&analyzed);
        request = analysis ? theme_request_from_motive(options, analysis, seed, preferences) : NULL;
        cJSON_Delete(analysis);
    }
    if (!request) {
        snprintf(sample->status, sizeof sample->status, "refused: the theme request could not be built");
        cJSON_Delete(kept);
        return 0;
    }
    CliResponse response;
    UrsatzStatus status = call("theme", request, &response);
    if (status != URSATZ_OK) {
        refuse(sample, status, &response.error);
        cli_response_destroy(&response);
        cJSON_Delete(kept);
        return 0;
    }
    int result = write_sample_file(options, index, ".mid", response.midi, response.midi_size, sample);
    if (result == 0) {
        result = write_sample_file(options, index, ".musicxml", response.musicxml, strlen(response.musicxml), sample);
    }
    if (result == 0) {
        result = write_sample_file(options, index, ".json", response.json, strlen(response.json), sample);
    }
    cli_response_destroy(&response);
    if (result == 0) {
        sample->ok = true;
        snprintf(sample->status, sizeof sample->status, "ok");
        if (kept) {
            while (cJSON_GetArraySize(avoid) >= LISTEN_AVOID_MAX) {
                cJSON_DeleteItemFromArray(avoid, 0);
            }
            if (cJSON_AddItemToArray(avoid, kept)) {
                kept = NULL;
            }
        }
    }
    cJSON_Delete(kept);
    return result;
}

/* A request the API would refuse (the mode, meter, strategy, style or settings) is a usage error, not a refused sample. */
static int validate_options(const Options *options, const cJSON *preferences) {
    CliResponse response;
    cJSON *request = random_motive_request(options, options->seed, preferences, NULL);
    if (!request) {
        return cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    UrsatzStatus status = cli_composer_validate("random_motive", request, &response);
    cJSON_Delete(request);
    int result = 0;
    if (status != URSATZ_OK) {
        const char *settings = strncmp(response.error.path, "/preferences", 12) == 0 ? options->settings_path : NULL;
        if (settings) {
            result = cli_fail(CLI_EXIT_USAGE, "settings '%s': %s: %s", settings, response.error.path + 12, response.error.message);
        } else {
            result = cli_fail_api(status, &response.error, NULL);
        }
    }
    cli_response_destroy(&response);
    return result;
}

/* --- the data mode (R-674): the settings of each sample resolved by the workshop, the batch recorded in a listening session ---------------- */

typedef struct {
    CliSession *session;
    char taste[96];       /* the flag's, else the session's */
    unsigned long share_numerator; /* explore.share of the batch's settings, n/d */
    unsigned long share_denominator;
    int distance;         /* explore.distance */
    cJSON *samples;       /* the samples of the batch, for ratings.batch */
    char *paths[8];       /* the absolute paths made for the session, freed at the end */
} DataMode;

/* A path made absolute (the workshop works in the data folder), in place; false when memory ran out. */
static bool absolutize(const char **path, DataMode *data) {
    if (!*path) {
        return true;
    }
    for (size_t i = 0; i < sizeof(data->paths) / sizeof(data->paths[0]); i++) {
        if (!data->paths[i]) {
            data->paths[i] = cli_path_absolute(*path);
            *path = data->paths[i];
            return data->paths[i] != NULL;
        }
    }
    return false;
}

/* The answer of settings.resolve for params (consumed); *out is a copy the caller deletes. 0 or the exit status after printing the failure. */
static int resolve_settings(DataMode *data, cJSON *params, cJSON **out) {
    *out = NULL;
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = cli_session_query(data->session, "settings.resolve", params, &reply, &error);
    cJSON_Delete(params);
    if (status != URSATZ_OK) {
        int exit_status = cli_fail(status == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s", error.message, error.path[0] ? " at " : "", error.path);
        workshop_reply_destroy(&reply);
        return exit_status;
    }
    *out = cJSON_Duplicate(reply.json, true);
    workshop_reply_destroy(&reply);
    return *out ? 0 : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
}

static cJSON *resolve_params(const Options *options, const DataMode *data, const cJSON *settings) {
    cJSON *params = cJSON_CreateObject();
    if (!params || !cJSON_AddStringToObject(params, "kind", options->themes ? "theme" : "motif") || !cJSON_AddStringToObject(params, "taste", data->taste) ||
        (settings && !cJSON_AddItemToObject(params, "settings", cJSON_Duplicate(settings, true)))) {
        cJSON_Delete(params);
        return NULL;
    }
    return params;
}

/* Opens the session, reads the listening session's taste and the batch's explore share and distance. 0 or the exit status. */
static int data_mode_open(Options *options, DataMode *data, const cJSON *settings) {
    bool ok = absolutize(&options->out_dir, data) && absolutize(&options->settings_path, data) && absolutize(&options->style_path, data) &&
              absolutize(&options->blend_path, data) && absolutize(&options->theme_request_path, data) && absolutize(&options->blueprint_path, data) &&
              absolutize(&options->motif_path, data);
    if (!ok) {
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    int status = cli_session_open(cli_global(), &data->session);
    if (status != 0) {
        return status;
    }
    cJSON *params = cJSON_CreateObject();
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    if (!params || !cJSON_AddStringToObject(params, "session", options->session_text)) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    UrsatzStatus queried = cli_session_query(data->session, "ratings.session", params, &reply, &error);
    cJSON_Delete(params);
    if (queried != URSATZ_OK) {
        int exit_status = cli_fail(queried == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s", error.message, error.path[0] ? " at " : "", error.path);
        workshop_reply_destroy(&reply);
        return exit_status;
    }
    snprintf(data->taste, sizeof data->taste, "%s", options->taste_text ? options->taste_text : cli_json_string(reply.json, "taste"));
    workshop_reply_destroy(&reply);
    /* the batch's own explore share and distance (the taste and settings of the run, before any exploring) */
    cJSON *base_params = resolve_params(options, data, settings);
    cJSON *base = NULL;
    if (!base_params || !cJSON_AddTrueToObject(base_params, "all")) {
        cJSON_Delete(base_params);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    status = resolve_settings(data, base_params, &base);
    if (status != 0) {
        return status;
    }
    data->share_numerator = 0;
    data->share_denominator = 1;
    data->distance = 2;
    const cJSON *row;
    cJSON_ArrayForEach(row, cJSON_GetObjectItemCaseSensitive(base, "values")) {
        const char *key = cli_json_string(row, "key");
        const cJSON *value = cJSON_GetObjectItemCaseSensitive(row, "value");
        if (strcmp(key, "explore.share") == 0 && cJSON_IsString(value)) {
            unsigned long n = 0;
            unsigned long d = 1;
            if (sscanf(value->valuestring, "%lu/%lu", &n, &d) == 2 && d > 0) {
                data->share_numerator = n;
                data->share_denominator = d;
            }
        } else if (strcmp(key, "explore.distance") == 0 && cJSON_IsNumber(value)) {
            data->distance = value->valueint;
        }
    }
    cJSON_Delete(base);
    data->samples = cJSON_CreateArray();
    return data->samples ? 0 : cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
}

/* Whether sample i (from 0) explores: floor((i + 1) * share) > floor(i * share). */
static bool explores(const DataMode *data, unsigned long i) {
    return (i + 1) * data->share_numerator / data->share_denominator > i * data->share_numerator / data->share_denominator;
}

/* The resolution of sample i: *resolution is the answer of settings.resolve, *document its document (the settings the sample is made with, or NULL). */
static int resolve_sample(const Options *options, DataMode *data, const cJSON *settings, unsigned long i, unsigned long seed, cJSON **resolution) {
    cJSON *params = resolve_params(options, data, settings);
    cJSON *explore = params && explores(data, i) ? cJSON_AddObjectToObject(params, "explore") : NULL;
    if (!params || (explores(data, i) && (!explore || !cJSON_AddNumberToObject(explore, "seed", (double)seed) || !cJSON_AddNumberToObject(explore, "distance", data->distance)))) {
        cJSON_Delete(params);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    return resolve_settings(data, params, resolution);
}

/* One sample of the batch: {index, seed, status, files: [{name, sha256}], settings}. */
static int record_sample(DataMode *data, unsigned long index, unsigned long seed, const Sample *sample, cJSON *resolution) {
    cJSON *entry = cJSON_CreateObject();
    cJSON *files = entry ? cJSON_CreateArray() : NULL;
    bool ok = files && cJSON_AddNumberToObject(entry, "index", (double)index) && cJSON_AddNumberToObject(entry, "seed", (double)seed) &&
              cJSON_AddStringToObject(entry, "status", sample->status);
    for (size_t i = 0; ok && i < sample->file_count; i++) {
        char *bytes = NULL;
        size_t size = 0;
        char hash[65];
        const char *base = strrchr(sample->files[i], '/');
        if (!cli_read_file(sample->files[i], &bytes, &size)) {
            ok = false;
            break;
        }
        common_sha256_hex(bytes, size, hash);
        free(bytes);
        cJSON *file = cJSON_CreateObject();
        ok = file && cJSON_AddStringToObject(file, "name", base ? base + 1 : sample->files[i]) && cJSON_AddStringToObject(file, "sha256", hash) && cJSON_AddItemToArray(files, file);
        if (!ok) {
            cJSON_Delete(file);
        }
    }
    ok = ok && cJSON_AddItemToObject(entry, "files", files);
    files = ok ? NULL : files;
    ok = ok && cJSON_AddItemToObject(entry, "settings", resolution ? cJSON_Duplicate(resolution, true) : cJSON_CreateNull()) && cJSON_AddItemToArray(data->samples, entry);
    if (!ok) {
        cJSON_Delete(files);
        cJSON_Delete(entry);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot record sample %lu", index);
    }
    return 0;
}

/* After the last sample: one ratings.batch with every sample. 0 or the exit status. */
static int record_batch(const Options *options, DataMode *data) {
    cJSON *command = cJSON_CreateObject();
    cJSON *params = command ? cJSON_AddObjectToObject(command, "params") : NULL;
    if (!params || !cJSON_AddStringToObject(command, "command", "ratings.batch") || !cJSON_AddStringToObject(params, "session", options->session_text) ||
        !cJSON_AddNumberToObject(params, "round", atof(options->round_text)) || !cJSON_AddStringToObject(params, "batch", options->batch_text) ||
        !cJSON_AddItemToObject(params, "samples", data->samples)) {
        cJSON_Delete(command);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "out of memory");
    }
    data->samples = NULL;
    WorkshopReply reply;
    UrsatzError error;
    memset(&error, 0, sizeof error);
    UrsatzStatus status = cli_session_command(data->session, command, NULL, &reply, &error);
    cJSON_Delete(command);
    if (status != URSATZ_OK) {
        int exit_status = cli_fail(status == URSATZ_ERR_REQUEST ? CLI_EXIT_USAGE : CLI_EXIT_UNSATISFIABLE, "%s%s%s", error.message, error.path[0] ? " at " : "", error.path);
        workshop_reply_destroy(&reply);
        return exit_status;
    }
    workshop_reply_destroy(&reply);
    return 0;
}

static void data_mode_close(DataMode *data) {
    if (data->session) {
        cli_session_close(data->session);
    }
    cJSON_Delete(data->samples);
    for (size_t i = 0; i < sizeof(data->paths) / sizeof(data->paths[0]); i++) {
        free(data->paths[i]);
    }
}

int run_listen(const CliArgs *args) {
    Options options;
    int status = read_options(args, &options);
    if (status != 0) {
        return status;
    }
    if (options.print_settings) {
        return print_default_settings();
    }
    status = check_options(&options);
    if (status != 0) {
        return status;
    }
    cJSON *preferences = NULL;
    if (options.settings_path) {
        status = read_json_file(options.settings_path, "settings file", &preferences);
        if (status != 0) {
            return status;
        }
    }
    if (options.style_path) {
        status = check_readable(options.style_path, "style file");
    }
    if (status == 0 && options.blend_path) {
        status = check_readable(options.blend_path, "blend file");
    }
    cJSON *theme_request = NULL;
    if (status == 0 && options.theme_request_path) {
        status = read_json_file(options.theme_request_path, "theme request", &theme_request);
        if (status == 0 && !cJSON_IsObject(theme_request)) {
            status = cli_fail(CLI_EXIT_USAGE, "theme request '%s' is not a JSON object", options.theme_request_path);
        }
    }
    if (status == 0 && options.blueprint_path) {
        status = read_blueprint_file(options.blueprint_path, &theme_request);
    }
    /* What is sent: the settings file, else the preferences the theme request carries. */
    const cJSON *sent = preferences ? preferences : cJSON_GetObjectItemCaseSensitive(theme_request, "preferences");
    cJSON *motif_document = NULL;
    if (status == 0 && options.motif_path) {
        status = read_motif_file(&options, &motif_document);
    }
    DataMode data;
    memset(&data, 0, sizeof data);
    if (status == 0 && options.session_text) {
        status = data_mode_open(&options, &data, preferences);
    }
    if (status == 0) {
        status = validate_options(&options, preferences);
    }
    if (status == 0) {
        status = make_directory(options.out_dir);
    }
    char manifest_path[LISTEN_PATH_SIZE];
    FILE *file = NULL;
    if (status == 0) {
        snprintf(manifest_path, sizeof manifest_path, "%s/listen.jsonl", options.out_dir);
        file = fopen(manifest_path, "ab");
        if (!file) {
            status = cli_fail(CLI_EXIT_WRITE, "cannot write '%s'", manifest_path);
        }
    }
    Manifest manifest = {file, &options, sent};
    unsigned long written = 0;
    /* A varied batch (settings with candidates.variety above 0, R-673, themes R-722) collects its motives to hand to the samples after them. */
    cJSON *avoid = NULL;
    for (unsigned long i = 0; status == 0 && i < options.count; i++) {
        Sample sample;
        memset(&sample, 0, sizeof sample);
        /* The settings of the sample: the batch's, or in data mode the workshop's resolution for this sample (the taste, the explore share). */
        cJSON *resolution = NULL;
        const cJSON *sample_preferences = data.session ? NULL : preferences;
        if (data.session) {
            status = resolve_sample(&options, &data, preferences, i, options.seed + i, &resolution);
            const cJSON *document = cJSON_GetObjectItemCaseSensitive(resolution, "document");
            sample_preferences = cJSON_IsObject(document) ? document : NULL;
        }
        manifest.preferences = data.session ? sample_preferences : sent;
        const cJSON *variety = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(manifest.preferences, "candidates"), "variety");
        /* Themes made from a motif or a request of the user have no generated motif to vary. */
        bool generated = !options.themes || (!motif_document && !theme_request);
        if (status == 0 && !avoid && generated && cJSON_IsNumber(variety) && variety->valuedouble > 0) {
            avoid = cJSON_CreateArray();
        }
        if (status == 0) {
            status = options.themes ? make_theme(&options, i + 1, options.seed + i, sample_preferences, motif_document, theme_request, avoid, &sample)
                                    : make_motif(&options, i + 1, options.seed + i, sample_preferences, variety && avoid ? avoid : NULL, &sample);
        }
        if (status == 0) {
            status = manifest_add(&manifest, i + 1, options.seed + i, &sample);
        }
        if (status == 0 && data.session) {
            status = record_sample(&data, i + 1, options.seed + i, &sample, resolution);
        }
        cJSON_Delete(resolution);
        if (status == 0) {
            written += sample.ok ? 1 : 0;
            printf("%02lu  seed %lu  %s\n", i + 1, options.seed + i, sample.status);
        }
    }
    if (status == 0 && data.session) {
        status = record_batch(&options, &data);
    }
    data_mode_close(&data);
    if (file) {
        fclose(file);
    }
    cJSON_Delete(avoid);
    cJSON_Delete(preferences);
    cJSON_Delete(theme_request);
    cJSON_Delete(motif_document);
    if (status != 0) {
        return status;
    }
    return written > 0 ? 0 : CLI_EXIT_UNSATISFIABLE;
}
