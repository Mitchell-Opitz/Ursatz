#include "listen.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>

#include "cli_options.h"

/* The most samples one run makes, and the largest seed a run starts from (seed + index stays a valid seed). */
#define LISTEN_COUNT_MAX 64
#define LISTEN_COUNT_DEFAULT 10
#define LISTEN_SEED_MAX 4294967231UL
#define LISTEN_PATH_SIZE 1024

typedef struct {
    bool help;
    bool print_settings;
    const char *what_text;
    const char *count_text;
    const char *seed_text;
    const char *out_dir;
    const char *settings_path;
    const char *mode_text;
    const char *meter_text;
    const char *style_path;
    const char *tempo_text;
    const char *strategy_text;
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

static void print_help(FILE *out) {
    fprintf(out, "Usage:\n%s\n\n", LISTEN_USAGE_LINE + 7);
    fprintf(out, "Makes a batch of motifs from fixed seeds, with a settings file, and writes them as files a listening\n");
    fprintf(out, "session plays and reads. Sample i (from 0) uses the seed --seed + i, so the same options, settings and seed\n");
    fprintf(out, "give the same files. See docs/cli-listen.md.\n\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  --what <what>        motifs (required)\n");
    fprintf(out, "  --out-dir <dir>      where the files are written; created if missing (required)\n");
    fprintf(out, "  --count <n>          how many samples, 1 to %d (default %d)\n", LISTEN_COUNT_MAX, LISTEN_COUNT_DEFAULT);
    fprintf(out, "  --seed <n>           whole number from 0 for the first sample (default 1)\n");
    fprintf(out, "  --settings <file>    a preferences document (docs/preferences.md); absent: none is sent\n");
    fprintf(out, "  --print-settings     print the default preferences document and exit\n");
    fprintf(out, "  --mode <mode>        major (default) or minor\n");
    fprintf(out, "  --meter <n/m>        beats over a beat unit (default 4/4)\n");
    fprintf(out, "  --style <file>       a style model file: draw 8 candidates for each motif and keep the closest\n");
    fprintf(out, "  --tempo <bpm>        MIDI and MusicXML: quarter notes per minute from 20 to 400\n");
    fprintf(out, "  --strategy <name>    segments (default) or drawn\n");
    fprintf(out, "  --help               this text\n\n");
    fprintf(out, "Files, i from 01: <i>.mid, <i>.musicxml, <i>.json (the API response) and one line per sample in listen.jsonl.\n");
    fprintf(out, "Exit status: 0 at least one sample was written; 1 usage, an invalid option or an unreadable file; 2 none could\n");
    fprintf(out, "be composed; 3 an output file could not be written.\n\n");
    fprintf(out, "Example:\n");
    fprintf(out, "  ursatz-composer listen --what motifs --count 10 --seed 1 --settings mine.json --out-dir round-1\n");
}

static int read_options(int argc, char **argv, Options *options) {
    memset(options, 0, sizeof(*options));
    for (int i = 2; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;
        if (strcmp(arg, "--help") == 0) {
            options->help = true;
        } else if (strcmp(arg, "--print-settings") == 0) {
            options->print_settings = true;
        } else if (strcmp(arg, "--what") == 0) {
            target = &options->what_text;
        } else if (strcmp(arg, "--count") == 0) {
            target = &options->count_text;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &options->seed_text;
        } else if (strcmp(arg, "--out-dir") == 0) {
            target = &options->out_dir;
        } else if (strcmp(arg, "--settings") == 0) {
            target = &options->settings_path;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &options->mode_text;
        } else if (strcmp(arg, "--meter") == 0) {
            target = &options->meter_text;
        } else if (strcmp(arg, "--style") == 0) {
            target = &options->style_path;
        } else if (strcmp(arg, "--tempo") == 0) {
            target = &options->tempo_text;
        } else if (strcmp(arg, "--strategy") == 0) {
            target = &options->strategy_text;
        } else {
            return cli_fail(CLI_EXIT_USAGE, "unknown option '%s' (see ursatz-composer listen --help)", arg);
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
    if (!options->what_text || strcmp(options->what_text, "motifs") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--what must be motifs\n%s", LISTEN_USAGE_LINE);
    }
    if (!options->out_dir) {
        return cli_fail(CLI_EXIT_USAGE, "listen needs --out-dir <dir>\n%s", LISTEN_USAGE_LINE);
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
    if (!options->mode_text) {
        options->mode_text = "major";
    }
    if (strcmp(options->mode_text, "major") != 0 && strcmp(options->mode_text, "minor") != 0) {
        return cli_fail(CLI_EXIT_USAGE, "--mode must be major or minor, not '%s'", options->mode_text);
    }
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

static int check_readable(const char *path, const char *what) {
    char *text = NULL;
    if (!cli_read_file(path, &text, NULL)) {
        return cli_fail(CLI_EXIT_USAGE, "cannot read %s '%s'", what, path);
    }
    free(text);
    return 0;
}

static int make_directory(const char *path) {
    char partial[LISTEN_PATH_SIZE];
    if (snprintf(partial, sizeof partial, "%s", path) >= (int)sizeof partial) {
        return cli_fail(CLI_EXIT_WRITE, "directory name '%s' is too long", path);
    }
    for (char *c = partial + 1; *c; c++) {
        if (*c == '/') {
            *c = '\0';
            if (mkdir(partial, 0755) != 0 && errno != EEXIST) {
                return cli_fail(CLI_EXIT_WRITE, "cannot create '%s': %s", partial, strerror(errno));
            }
            *c = '/';
        }
    }
    if (mkdir(partial, 0755) != 0 && errno != EEXIST) {
        return cli_fail(CLI_EXIT_WRITE, "cannot create '%s': %s", partial, strerror(errno));
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
    ComposerResponse response;
    memset(&response, 0, sizeof response);
    if (composer_api_vocabulary(&response) != COMPOSER_OK) {
        composer_response_destroy(&response);
        return cli_fail(CLI_EXIT_UNSATISFIABLE, "cannot read the vocabulary");
    }
    cJSON *vocabulary = cJSON_Parse(response.json);
    composer_response_destroy(&response);
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

/* One line of listen.jsonl for the sample. */
static int manifest_add(const Manifest *manifest, unsigned long index, unsigned long seed, const Sample *sample) {
    cJSON *line = cJSON_CreateObject();
    cJSON *files = line ? cJSON_CreateArray() : NULL;
    bool ok = files != NULL;
    for (size_t i = 0; ok && i < sample->file_count; i++) {
        ok = cJSON_AddItemToArray(files, cJSON_CreateString(sample->files[i]));
    }
    ok = ok && cJSON_AddNumberToObject(line, "index", (double)index) && cJSON_AddNumberToObject(line, "seed", (double)seed) &&
         cJSON_AddStringToObject(line, "what", manifest->options->what_text) && cJSON_AddStringToObject(line, "motif", "generated") &&
         cJSON_AddNullToObject(line, "theme_request");
    ok = ok && (manifest->options->style_path ? cJSON_AddStringToObject(line, "style", manifest->options->style_path)
                                              : cJSON_AddNullToObject(line, "style"));
    ok = ok && cJSON_AddNullToObject(line, "blend");
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

static void refuse(Sample *sample, ComposerStatus status, const ComposerError *error) {
    const char *kind = status == COMPOSER_ERR_UNSATISFIABLE ? "stuck" : "refused";
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

/* The request of a random motive: the sample's seed, the mode, meter, strategy, style and settings. */
static cJSON *random_motive_request(const Options *options, unsigned long seed, const cJSON *preferences) {
    char number[32];
    cJSON *request = cJSON_CreateObject();
    bool ok = request && cJSON_AddStringToObject(request, "mode", options->mode_text) &&
              cJSON_AddStringToObject(request, "meter", options->meter_text) &&
              cJSON_AddStringToObject(request, "strategy", options->strategy_text) && cJSON_AddNumberToObject(request, "count", 1) &&
              cJSON_AddStringToObject(request, "seed", seed_text(seed, number, sizeof number)) &&
              (!options->style_path || cJSON_AddStringToObject(request, "style", options->style_path)) &&
              (!preferences || cJSON_AddItemToObject(request, "preferences", cJSON_Duplicate(preferences, true)));
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
static ComposerStatus call(ComposerStatus (*entry)(const char *, const ComposerJob *, ComposerResponse *), cJSON *request,
                           ComposerResponse *response) {
    memset(response, 0, sizeof *response);
    char *text = request ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    if (!text) {
        response->error.message[0] = '\0';
        snprintf(response->error.message, sizeof response->error.message, "out of memory");
        return COMPOSER_ERR_ALLOC;
    }
    ComposerStatus status = entry(text, NULL, response);
    cJSON_free(text);
    return status;
}

/* One motif: a random motive, rendered to MIDI and MusicXML, and its API response as json. */
static int make_motif(const Options *options, unsigned long index, unsigned long seed, const cJSON *preferences, Sample *sample) {
    ComposerResponse drawn;
    ComposerStatus status = call(composer_api_random_motive, random_motive_request(options, seed, preferences), &drawn);
    if (status != COMPOSER_OK) {
        refuse(sample, status, &drawn.error);
        composer_response_destroy(&drawn);
        return 0;
    }
    cJSON *document = cJSON_Parse(drawn.json);
    composer_response_destroy(&drawn);
    const cJSON *first = document ? cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "motives"), 0) : NULL;
    ComposerResponse rendered;
    status = call(composer_api_motive_render, render_request(options, cJSON_GetObjectItemCaseSensitive(first, "motive")), &rendered);
    cJSON_Delete(document);
    if (status != COMPOSER_OK) {
        refuse(sample, status, &rendered.error);
        composer_response_destroy(&rendered);
        return 0;
    }
    int result = write_sample_file(options, index, ".mid", rendered.midi, rendered.midi_size, sample);
    if (result == 0) {
        result = write_sample_file(options, index, ".musicxml", rendered.musicxml, strlen(rendered.musicxml), sample);
    }
    if (result == 0) {
        result = write_sample_file(options, index, ".json", rendered.json, strlen(rendered.json), sample);
    }
    composer_response_destroy(&rendered);
    if (result == 0) {
        sample->ok = true;
        snprintf(sample->status, sizeof sample->status, "ok");
    }
    return result;
}

/* A request the API would refuse (the mode, meter, strategy, style or settings) is a usage error, not a refused sample. */
static int validate_options(const Options *options, const cJSON *preferences) {
    ComposerResponse response;
    cJSON *request = random_motive_request(options, options->seed, preferences);
    char *text = request ? cJSON_PrintUnformatted(request) : NULL;
    cJSON_Delete(request);
    if (!text) {
        return cli_fail(CLI_EXIT_USAGE, "out of memory");
    }
    memset(&response, 0, sizeof response);
    ComposerStatus status = composer_api_validate("random_motive", text, &response);
    cJSON_free(text);
    int result = 0;
    if (status != COMPOSER_OK) {
        const char *settings = strncmp(response.error.path, "/preferences", 12) == 0 ? options->settings_path : NULL;
        if (settings) {
            result = cli_fail(CLI_EXIT_USAGE, "settings '%s': %s: %s", settings, response.error.path + 12, response.error.message);
        } else {
            result = cli_fail_api(status, &response.error, NULL);
        }
    }
    composer_response_destroy(&response);
    return result;
}

int run_listen(int argc, char **argv) {
    Options options;
    int status = read_options(argc, argv, &options);
    if (status != 0) {
        return status;
    }
    if (options.help) {
        print_help(stdout);
        return 0;
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
    Manifest manifest = {file, &options, preferences};
    unsigned long written = 0;
    for (unsigned long i = 0; status == 0 && i < options.count; i++) {
        Sample sample;
        memset(&sample, 0, sizeof sample);
        status = make_motif(&options, i + 1, options.seed + i, preferences, &sample);
        if (status == 0) {
            status = manifest_add(&manifest, i + 1, options.seed + i, &sample);
        }
        if (status == 0) {
            written += sample.ok ? 1 : 0;
            printf("%02lu  seed %lu  %s\n", i + 1, options.seed + i, sample.status);
        }
    }
    if (file) {
        fclose(file);
    }
    cJSON_Delete(preferences);
    if (status != 0) {
        return status;
    }
    return written > 0 ? 0 : CLI_EXIT_UNSATISFIABLE;
}
