#define _XOPEN_SOURCE 700

#include "composition_service.h"
#include "composer_service.h"
#include "corpus_service.h"
#include "id_service.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <limits.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PATH_SIZE 1024
#define ID_MAX_TRIES 8
#define MAX_VERSION 9999

/* meta.json is read, changed and renamed over; one lock keeps two requests
   from losing each other's change. A command runs outside it. */
static pthread_mutex_t meta_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---- files ---------------------------------------------------------- */

static char *dup_string(const char *value) {
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);
    if (copy) {
        memcpy(copy, value, len);
    }
    return copy;
}

/* Creates one directory. Returns 1 if created, 0 if it already exists, -1 on
   any other failure. */
static int make_dir(const char *path) {
    if (mkdir(path, 0755) == 0) {
        return 1;
    }
    return errno == EEXIST ? 0 : -1;
}

static bool path_is_dir(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool path_is_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int remove_entry(const char *path, const struct stat *st, int type, struct FTW *walk) {
    (void)st;
    (void)type;
    (void)walk;
    return remove(path);
}

/* Removes a directory and everything under it. */
static void remove_tree(const char *path) {
    nftw(path, remove_entry, 16, FTW_DEPTH | FTW_PHYS);
}

/* Writes data to "<path>.tmp", then renames it over path. */
static bool write_file_atomic(const char *path, const void *data, size_t size) {
    char tmp_path[PATH_SIZE + 8];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path) >= (int)sizeof(tmp_path)) {
        return false;
    }
    FILE *file = fopen(tmp_path, "wb");
    if (!file) {
        return false;
    }
    bool ok = size == 0 || fwrite(data, 1, size, file) == size;
    ok = (fclose(file) == 0) && ok;
    if (!ok || rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return false;
    }
    return true;
}

/* Reads a whole file into a NUL-terminated buffer (caller frees). */
static char *read_whole_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    size_t capacity = 4096;
    size_t length = 0;
    char *buffer = malloc(capacity);
    while (buffer) {
        size_t n = fread(buffer + length, 1, capacity - length - 1, file);
        length += n;
        if (n == 0 || feof(file)) {
            break;
        }
        if (length + 1 >= capacity) {
            capacity *= 2;
            char *grown = realloc(buffer, capacity);
            if (!grown) {
                free(buffer);
                buffer = NULL;
                break;
            }
            buffer = grown;
        }
    }
    bool failed = ferror(file) != 0;
    fclose(file);
    if (!buffer || failed) {
        free(buffer);
        return NULL;
    }
    buffer[length] = '\0';
    if (out_size) {
        *out_size = length;
    }
    return buffer;
}

static bool copy_file(const char *from, const char *to) {
    size_t size = 0;
    char *data = read_whole_file(from, &size);
    if (!data) {
        return false;
    }
    bool ok = write_file_atomic(to, data, size);
    free(data);
    return ok;
}

static bool join_path(char *out, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int written = vsnprintf(out, size, format, args);
    va_end(args);
    return written >= 0 && (size_t)written < size;
}

/* The absolute path of the store, created on first use. Every path handed to
   the Composer starts from it. */
static bool store_root(const char *compositions_dir, char *out, size_t size) {
    if (make_dir(compositions_dir) < 0) {
        return false;
    }
    char resolved[PATH_MAX];
    if (!realpath(compositions_dir, resolved)) {
        return false;
    }
    return join_path(out, size, "%s", resolved);
}

/* The current time as "YYYY-MM-DDTHH:MM:SSZ". */
static void iso_utc_now(char *out, size_t out_size) {
    time_t now = time(NULL);
    struct tm parts;
    gmtime_r(&now, &parts);
    strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &parts);
}

/* ---- responses ------------------------------------------------------ */

void ursatz_gui_composition_response_destroy(UrsatzGuiCompositionResponse *response) {
    if (response) {
        free(response->body);
        response->body = NULL;
    }
}

/* Takes ownership of object. */
static UrsatzGuiCompositionResponse respond(int status, cJSON *object) {
    UrsatzGuiCompositionResponse response = {status, NULL};
    if (object) {
        response.body = cJSON_PrintUnformatted(object);
        cJSON_Delete(object);
    }
    if (!response.body) {
        response.status = 500;
        response.body = dup_string("{\"error\":\"out of memory\"}");
    }
    return response;
}

static UrsatzGuiCompositionResponse respond_error(int status, const char *error, const char *stderr_text) {
    cJSON *object = cJSON_CreateObject();
    if (object) {
        cJSON_AddStringToObject(object, "error", error);
        if (stderr_text) {
            cJSON_AddStringToObject(object, "stderr", stderr_text);
        }
    }
    return respond(status, object);
}

static UrsatzGuiCompositionResponse respond_not_found(const char *what) {
    return respond_error(404, what, NULL);
}

static UrsatzGuiCompositionResponse respond_failed(void) {
    return respond_error(500, "composition store failed", NULL);
}

/* ---- argument lists ------------------------------------------------- */

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
    bool ok;
} ArgList;

static void args_init(ArgList *args) {
    args->items = NULL;
    args->count = 0;
    args->capacity = 0;
    args->ok = true;
}

static void args_destroy(ArgList *args) {
    for (size_t i = 0; i < args->count; i++) {
        free(args->items[i]);
    }
    free(args->items);
    args->items = NULL;
    args->count = 0;
}

/* Keeps a NULL after the last item, the way ursatz_gui_composer_run wants it. */
static void args_add(ArgList *args, const char *value) {
    if (!args->ok) {
        return;
    }
    if (args->count + 2 > args->capacity) {
        size_t capacity = args->capacity ? args->capacity * 2 : 32;
        char **grown = realloc(args->items, capacity * sizeof(char *));
        if (!grown) {
            args->ok = false;
            return;
        }
        args->items = grown;
        args->capacity = capacity;
    }
    char *copy = dup_string(value);
    if (!copy) {
        args->ok = false;
        return;
    }
    args->items[args->count++] = copy;
    args->items[args->count] = NULL;
}

static void args_addf(ArgList *args, const char *format, ...) {
    char text[PATH_SIZE * 2];
    va_list list;
    va_start(list, format);
    int written = vsnprintf(text, sizeof(text), format, list);
    va_end(list);
    if (written < 0 || (size_t)written >= sizeof(text)) {
        args->ok = false;
        return;
    }
    args_add(args, text);
}

/* ---- request validation --------------------------------------------- */

typedef struct {
    char message[96];
} Validation;

static void invalid(Validation *v, const char *field) {
    snprintf(v->message, sizeof(v->message), "invalid %s", field);
}

static bool one_of(const char *value, const char *const *choices) {
    for (; *choices; choices++) {
        if (strcmp(value, *choices) == 0) {
            return true;
        }
    }
    return false;
}

static bool valid_key(const char *text) {
    if (strlen(text) > 16 || text[0] < 'A' || text[0] > 'G') {
        return false;
    }
    const char *rest = text + 1;
    if (*rest == '#' || *rest == 'b') {
        rest++;
    }
    if (*rest == '\0') {
        return true;
    }
    if (*rest != ' ') {
        return false;
    }
    rest++;
    return strcasecmp(rest, "major") == 0 || strcasecmp(rest, "minor") == 0;
}

static bool valid_meter(const char *text) {
    static const char *const meters[] = {"2/4", "3/4", "4/4", "6/8", NULL};
    return one_of(text, meters);
}

static bool valid_grow_form(const char *text) {
    static const char *const forms[] = {"sentence", "period", "hybrid", NULL};
    return one_of(text, forms);
}

static bool valid_hybrid(const char *text) {
    static const char *const beginnings[] = {"presentation", "antecedent", "cbi", NULL};
    static const char *const endings[] = {"continuation", "consequent", "cadential", NULL};
    const char *plus = strchr(text, '+');
    if (!plus || plus - text > 16 || strlen(plus + 1) > 16) {
        return false;
    }
    char beginning[24];
    memcpy(beginning, text, (size_t)(plus - text));
    beginning[plus - text] = '\0';
    return one_of(beginning, beginnings) && one_of(plus + 1, endings);
}

static bool valid_piece_form(const char *text) {
    static const char *const forms[] = {"binary", "rounded-binary", "ternary", "variations", "loop", "layered", NULL};
    return one_of(text, forms);
}

static bool valid_cadence(const char *text) {
    static const char *const cadences[] = {"PAC", "IAC", "HC", NULL};
    return one_of(text, cadences);
}

static bool valid_first_cadence(const char *text) {
    static const char *const cadences[] = {"HC", "IAC", NULL};
    return one_of(text, cadences);
}

static bool valid_accompaniment(const char *text) {
    static const char *const patterns[] = {"block", "arpeggiated", "alberti", "oom-pah", "any", NULL};
    return one_of(text, patterns);
}

static bool valid_texture(const char *text) {
    static const char *const textures[] = {"homophony", "homorhythm", "melody and accompaniment", "chorale", NULL};
    return one_of(text, textures);
}

/* A form letter as the plan prints it: A, B, A'. */
static bool valid_letter(const char *text) {
    if (text[0] < 'A' || text[0] > 'Z') {
        return false;
    }
    size_t length = 1;
    while (text[length] == '\'') {
        length++;
    }
    return text[length] == '\0' && length <= 8;
}

/* Where a given melody goes: a letter, and optionally ":<bar>". */
static bool valid_at(const char *text) {
    char letter[16];
    const char *colon = strchr(text, ':');
    size_t length = colon ? (size_t)(colon - text) : strlen(text);
    if (length == 0 || length >= sizeof(letter)) {
        return false;
    }
    memcpy(letter, text, length);
    letter[length] = '\0';
    if (!valid_letter(letter)) {
        return false;
    }
    if (!colon) {
        return true;
    }
    size_t digits = strlen(colon + 1);
    if (digits == 0 || digits > 3) {
        return false;
    }
    for (const char *c = colon + 1; *c; c++) {
        if (!isdigit((unsigned char)*c)) {
            return false;
        }
    }
    return true;
}

/* A note list: letters, digits and the punctuation of "D4:q F4:e A4:e D5:h". */
static bool valid_note_text(const char *text) {
    size_t length = strlen(text);
    if (length == 0 || length > 4096) {
        return false;
    }
    for (const char *c = text; *c; c++) {
        if (!(isalnum((unsigned char)*c) || strchr(" #:/.,+-_'\n\r\t", *c))) {
            return false;
        }
    }
    return true;
}

static bool valid_name(const char *text) {
    size_t length = strlen(text);
    if (length == 0 || length > 120) {
        return false;
    }
    for (const char *c = text; *c; c++) {
        if ((unsigned char)*c < 0x20 || *c == 0x7f) {
            return false;
        }
    }
    return true;
}

/* A tempo as the Composer reads it: 20 to 400, a whole number or "n/d". */
static bool valid_tempo_text(const char *text) {
    const char *slash = strchr(text, '/');
    size_t numerator_digits = slash ? (size_t)(slash - text) : strlen(text);
    size_t denominator_digits = slash ? strlen(slash + 1) : 0;
    if (numerator_digits == 0 || numerator_digits > 6 || (slash && (denominator_digits == 0 || denominator_digits > 6))) {
        return false;
    }
    for (const char *c = text; *c; c++) {
        if (!isdigit((unsigned char)*c) && c != slash) {
            return false;
        }
    }
    double numerator = strtod(text, NULL);
    double denominator = slash ? strtod(slash + 1, NULL) : 1.0;
    if (denominator <= 0) {
        return false;
    }
    double bpm = numerator / denominator;
    return bpm >= 20.0 && bpm <= 400.0;
}

/* Each add_* below reads one request field: absent (or null) is fine and adds
   nothing, a bad value sets v and returns false, a good one adds its flag and
   value to args. The require_ forms treat absent as bad. */

static const cJSON *field(const cJSON *request, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(request, key);
    return (item && !cJSON_IsNull(item)) ? item : NULL;
}

static bool text_value(const cJSON *request, const char *key, bool (*valid)(const char *), const char **out,
                       bool required, Validation *v) {
    const cJSON *item = field(request, key);
    *out = NULL;
    if (!item) {
        if (required) {
            invalid(v, key);
        }
        return !required;
    }
    if (!cJSON_IsString(item) || !item->valuestring || !valid(item->valuestring)) {
        invalid(v, key);
        return false;
    }
    *out = item->valuestring;
    return true;
}

static bool add_text(ArgList *args, const cJSON *request, const char *key, const char *flag,
                     bool (*valid)(const char *), bool required, Validation *v) {
    const char *value = NULL;
    if (!text_value(request, key, valid, &value, required, v)) {
        return false;
    }
    if (value) {
        args_add(args, flag);
        args_add(args, value);
    }
    return true;
}

static bool integer_value(const cJSON *request, const char *key, long long low, long long high, bool *present,
                          long long *out, Validation *v) {
    const cJSON *item = field(request, key);
    *present = false;
    if (!item) {
        return true;
    }
    if (!cJSON_IsNumber(item) || item->valuedouble != (double)(long long)item->valuedouble ||
        item->valuedouble < (double)low || item->valuedouble > (double)high) {
        invalid(v, key);
        return false;
    }
    *present = true;
    *out = (long long)item->valuedouble;
    return true;
}

static bool add_integer(ArgList *args, const cJSON *request, const char *key, const char *flag, long long low,
                        long long high, Validation *v) {
    bool present = false;
    long long value = 0;
    if (!integer_value(request, key, low, high, &present, &value, v)) {
        return false;
    }
    if (present) {
        args_add(args, flag);
        args_addf(args, "%lld", value);
    }
    return true;
}

static bool add_seed(ArgList *args, const cJSON *request, Validation *v) {
    return add_integer(args, request, "seed", "--seed", 0, 4294967295LL, v);
}

static bool flag_value(const cJSON *request, const char *key, bool *out, Validation *v) {
    const cJSON *item = field(request, key);
    *out = false;
    if (!item) {
        return true;
    }
    if (!cJSON_IsBool(item)) {
        invalid(v, key);
        return false;
    }
    *out = cJSON_IsTrue(item);
    return true;
}

static bool add_flag(ArgList *args, const cJSON *request, const char *key, const char *flag, Validation *v) {
    bool on = false;
    if (!flag_value(request, key, &on, v)) {
        return false;
    }
    if (on) {
        args_add(args, flag);
    }
    return true;
}

/* tempo is a whole number or an "n/d" string. */
static bool tempo_text(const cJSON *request, char *out, size_t size, bool *present, Validation *v) {
    const cJSON *item = field(request, "tempo");
    *present = false;
    if (!item) {
        return true;
    }
    if (cJSON_IsNumber(item)) {
        char text[32];
        if (item->valuedouble != (double)(long long)item->valuedouble ||
            snprintf(text, sizeof(text), "%lld", (long long)item->valuedouble) <= 0 || !valid_tempo_text(text)) {
            invalid(v, "tempo");
            return false;
        }
        snprintf(out, size, "%s", text);
    } else if (cJSON_IsString(item) && item->valuestring && valid_tempo_text(item->valuestring)) {
        snprintf(out, size, "%s", item->valuestring);
    } else {
        invalid(v, "tempo");
        return false;
    }
    *present = true;
    return true;
}

static bool add_tempo(ArgList *args, const cJSON *request, Validation *v) {
    char text[32];
    bool present = false;
    if (!tempo_text(request, text, sizeof(text), &present, v)) {
        return false;
    }
    if (present) {
        args_add(args, "--tempo");
        args_add(args, text);
    }
    return true;
}

/* ---- meta.json ------------------------------------------------------ */

static cJSON *read_meta(const char *comp_dir) {
    char path[PATH_SIZE];
    if (!join_path(path, sizeof(path), "%s/meta.json", comp_dir)) {
        return NULL;
    }
    char *text = read_whole_file(path, NULL);
    if (!text) {
        return NULL;
    }
    cJSON *meta = cJSON_Parse(text);
    free(text);
    if (meta && !cJSON_IsObject(meta)) {
        cJSON_Delete(meta);
        return NULL;
    }
    return meta;
}

static bool write_meta(const char *comp_dir, const cJSON *meta) {
    char path[PATH_SIZE];
    if (!join_path(path, sizeof(path), "%s/meta.json", comp_dir)) {
        return false;
    }
    char *text = cJSON_Print(meta);
    if (!text) {
        return false;
    }
    bool ok = write_file_atomic(path, text, strlen(text));
    free(text);
    return ok;
}

/* The seed a command's JSON output reports: "seed" (fill, regenerate) or the
   first theme's (grow). NULL when it has none. */
static cJSON *seed_of(const cJSON *summary) {
    const cJSON *seed = cJSON_GetObjectItemCaseSensitive(summary, "seed");
    if (!cJSON_IsNumber(seed)) {
        const cJSON *themes = cJSON_GetObjectItemCaseSensitive(summary, "themes");
        const cJSON *first = cJSON_IsArray(themes) ? cJSON_GetArrayItem(themes, 0) : NULL;
        seed = first ? cJSON_GetObjectItemCaseSensitive(first, "seed") : NULL;
    }
    return cJSON_IsNumber(seed) ? cJSON_CreateNumber(seed->valuedouble) : cJSON_CreateNull();
}

/* The arguments as stored: paths inside the composition are relative to it. */
static cJSON *args_to_json(const ArgList *args, const char *comp_dir) {
    cJSON *array = cJSON_CreateArray();
    size_t prefix = strlen(comp_dir);
    for (size_t i = 0; array && i < args->count; i++) {
        const char *item = args->items[i];
        if (strncmp(item, comp_dir, prefix) == 0 && item[prefix] == '/') {
            item += prefix + 1;
        }
        cJSON_AddItemToArray(array, cJSON_CreateString(item));
    }
    return array;
}

/* One entry of "versions". Takes ownership of summary. */
static cJSON *make_version(long number, const char *action, const ArgList *args, const char *comp_dir,
                           cJSON *summary) {
    char created_at[32];
    iso_utc_now(created_at, sizeof(created_at));
    cJSON *version = cJSON_CreateObject();
    if (!version) {
        cJSON_Delete(summary);
        return NULL;
    }
    cJSON_AddNumberToObject(version, "n", (double)number);
    cJSON_AddStringToObject(version, "action", action);
    cJSON_AddItemToObject(version, "args", args_to_json(args, comp_dir));
    cJSON_AddStringToObject(version, "created_at", created_at);
    cJSON *seed = summary ? seed_of(summary) : cJSON_CreateNull();
    cJSON_AddItemToObject(version, "seed", seed);
    cJSON_AddItemToObject(version, "summary", summary ? summary : cJSON_CreateNull());
    return version;
}

/* ---- running a command ---------------------------------------------- */

/* Runs the Composer. True when it exited 0 (out holds its output). Otherwise
   *failure is the response for the caller to return and out is destroyed. */
static bool run_composer(const ArgList *args, UrsatzGuiComposerOutput *out, UrsatzGuiCompositionResponse *failure) {
    if (!args->ok) {
        *failure = respond_error(500, "out of memory", NULL);
        return false;
    }
    UrsatzGuiComposerResult result = ursatz_gui_composer_run((const char *const *)args->items, out);
    switch (result) {
        case URSATZ_GUI_COMPOSER_OK:
            break;
        case URSATZ_GUI_COMPOSER_ERR_NO_BINARY:
            *failure = respond_error(503, "composer not found", NULL);
            return false;
        case URSATZ_GUI_COMPOSER_ERR_TIMEOUT:
            *failure = respond_error(504, "composer timed out", NULL);
            return false;
        default:
            *failure = respond_error(500, "composer could not be started", NULL);
            return false;
    }
    if (out->exit_status == 0) {
        return true;
    }
    if (out->exit_status == 2) {
        *failure = respond_error(422, "stuck", out->stderr_text);
    } else if (out->exit_status == 1) {
        *failure = respond_error(400, "invalid request", out->stderr_text);
    } else {
        *failure = respond_error(500, "composer failed", out->stderr_text);
    }
    ursatz_gui_composer_output_destroy(out);
    return false;
}

/* The command's JSON output, or NULL when it printed none. */
static cJSON *parse_output(const UrsatzGuiComposerOutput *out) {
    return cJSON_Parse(out->stdout_text);
}

/* ---- creating a composition ----------------------------------------- */

/* Makes compositions/<id>/ and its v1/. The id is drawn again while its
   directory exists. */
static bool create_composition_dir(const char *root, char *id, size_t id_size, char *comp_dir, size_t dir_size) {
    for (int attempt = 0; attempt < ID_MAX_TRIES; attempt++) {
        char *candidate = ursatz_gui_new_id("comp");
        if (!candidate) {
            return false;
        }
        bool fits = join_path(id, id_size, "%s", candidate) && join_path(comp_dir, dir_size, "%s/%s", root, candidate);
        free(candidate);
        if (!fits) {
            return false;
        }
        int made = make_dir(comp_dir);
        if (made < 0) {
            return false;
        }
        if (made == 1) {
            char version_dir[PATH_SIZE];
            if (!join_path(version_dir, sizeof(version_dir), "%s/v1", comp_dir) || make_dir(version_dir) != 1) {
                remove_tree(comp_dir);
                return false;
            }
            return true;
        }
    }
    return false;
}

/* Writes the first meta.json. Takes ownership of request_copy and version. */
static bool write_new_meta(const char *comp_dir, const char *name, const char *kind, cJSON *request_copy,
                           cJSON *version) {
    char created_at[32];
    iso_utc_now(created_at, sizeof(created_at));
    cJSON *meta = cJSON_CreateObject();
    cJSON *versions = cJSON_CreateArray();
    if (!meta || !versions || !request_copy || !version) {
        cJSON_Delete(meta);
        cJSON_Delete(versions);
        cJSON_Delete(request_copy);
        cJSON_Delete(version);
        return false;
    }
    cJSON_AddStringToObject(meta, "name", name);
    cJSON_AddStringToObject(meta, "created_at", created_at);
    cJSON_AddStringToObject(meta, "kind", kind);
    cJSON_AddItemToObject(meta, "request", request_copy);
    cJSON_AddItemToArray(versions, version);
    cJSON_AddItemToObject(meta, "versions", versions);
    bool ok = write_meta(comp_dir, meta);
    cJSON_Delete(meta);
    return ok;
}

/* Saves note text as <comp_dir>/<file_name>. */
static bool save_text(const char *comp_dir, const char *file_name, const char *text) {
    char path[PATH_SIZE];
    return join_path(path, sizeof(path), "%s/%s", comp_dir, file_name) && write_file_atomic(path, text, strlen(text));
}

/* The response of a finished first version. */
static UrsatzGuiCompositionResponse respond_created(const char *id, cJSON *summary) {
    cJSON *object = cJSON_CreateObject();
    if (!object) {
        cJSON_Delete(summary);
        return respond(500, NULL);
    }
    cJSON_AddTrueToObject(object, "ok");
    cJSON_AddStringToObject(object, "id", id);
    cJSON_AddNumberToObject(object, "version", 1);
    cJSON_AddItemToObject(object, "summary", summary);
    return respond(200, object);
}

UrsatzGuiCompositionResponse ursatz_gui_composition_grow(const char *compositions_dir, const cJSON *request) {
    Validation v = {{0}};
    ArgList args;
    args_init(&args);
    const char *name = NULL;
    const char *motive = NULL;
    const char *into = NULL;
    UrsatzGuiCompositionResponse response;

    char root[PATH_SIZE];
    char id[64];
    char comp_dir[PATH_SIZE];
    char motive_path[PATH_SIZE];
    bool created = false;

    if (!cJSON_IsObject(request)) {
        return respond_error(400, "invalid request", NULL);
    }
    if (!text_value(request, "name", valid_name, &name, true, &v) ||
        !text_value(request, "motive", valid_note_text, &motive, true, &v) ||
        !text_value(request, "into", valid_grow_form, &into, true, &v)) {
        return respond_error(400, v.message, NULL);
    }

    if (!store_root(compositions_dir, root, sizeof(root)) ||
        !create_composition_dir(root, id, sizeof(id), comp_dir, sizeof(comp_dir))) {
        return respond_failed();
    }
    created = true;
    if (!save_text(comp_dir, "motive.txt", motive) ||
        !join_path(motive_path, sizeof(motive_path), "%s/motive.txt", comp_dir)) {
        response = respond_failed();
        goto fail;
    }

    args_add(&args, "grow");
    args_add(&args, motive_path);
    args_add(&args, "--into");
    args_add(&args, into);
    bool hybrid_ok = true;
    if (strcmp(into, "hybrid") == 0) {
        hybrid_ok = add_text(&args, request, "hybrid", "--hybrid", valid_hybrid, true, &v);
    }
    if (!hybrid_ok || !add_text(&args, request, "key", "--key", valid_key, true, &v) ||
        !add_text(&args, request, "meter", "--meter", valid_meter, true, &v) ||
        !add_integer(&args, request, "bars", "--bars", 4, 8, &v) ||
        !add_text(&args, request, "cadence", "--cadence", valid_cadence, false, &v) ||
        !add_text(&args, request, "first_cadence", "--first-cadence", valid_first_cadence, false, &v) ||
        !add_text(&args, request, "accompaniment", "--accompaniment", valid_accompaniment, false, &v) ||
        !add_text(&args, request, "texture", "--texture", valid_texture, false, &v) ||
        !add_flag(&args, request, "check", "--check", &v) || !add_seed(&args, request, &v) ||
        !add_tempo(&args, request, &v)) {
        response = respond_error(400, v.message, NULL);
        goto fail;
    }
    args_addf(&args, "--out");
    args_addf(&args, "%s/v1/plan.json", comp_dir);
    args_add(&args, "--midi");
    args_addf(&args, "%s/v1/piece.mid", comp_dir);
    args_add(&args, "--musicxml");
    args_addf(&args, "%s/v1/piece.musicxml", comp_dir);
    args_add(&args, "--format");
    args_add(&args, "json");

    UrsatzGuiComposerOutput output;
    if (!run_composer(&args, &output, &response)) {
        goto fail;
    }
    cJSON *summary = parse_output(&output);
    ursatz_gui_composer_output_destroy(&output);
    if (!summary) {
        response = respond_error(500, "composer output unreadable", NULL);
        goto fail;
    }
    cJSON *version = make_version(1, "grow", &args, comp_dir, cJSON_Duplicate(summary, true));
    pthread_mutex_lock(&meta_mutex);
    bool saved = write_new_meta(comp_dir, name, "grow", cJSON_Duplicate(request, true), version);
    pthread_mutex_unlock(&meta_mutex);
    if (!saved) {
        cJSON_Delete(summary);
        response = respond_failed();
        goto fail;
    }
    args_destroy(&args);
    return respond_created(id, summary);

fail:
    args_destroy(&args);
    if (created) {
        remove_tree(comp_dir);
    }
    return response;
}

UrsatzGuiCompositionResponse ursatz_gui_composition_plan(const char *compositions_dir, const cJSON *request) {
    Validation v = {{0}};
    ArgList plan_args;
    ArgList fill_args;
    args_init(&plan_args);
    args_init(&fill_args);
    const char *name = NULL;
    const char *form = NULL;
    const char *motive = NULL;
    const char *notes = NULL;
    const char *at = NULL;
    UrsatzGuiCompositionResponse response;

    char root[PATH_SIZE];
    char id[64];
    char comp_dir[PATH_SIZE];
    bool created = false;

    if (!cJSON_IsObject(request)) {
        return respond_error(400, "invalid request", NULL);
    }
    const cJSON *given = field(request, "given_melody");
    if (!text_value(request, "name", valid_name, &name, true, &v) ||
        !text_value(request, "form", valid_piece_form, &form, true, &v) ||
        !text_value(request, "motive", valid_note_text, &motive, false, &v)) {
        return respond_error(400, v.message, NULL);
    }
    if (given) {
        if (!cJSON_IsObject(given) || !text_value(given, "notes", valid_note_text, &notes, true, &v) ||
            !text_value(given, "at", valid_at, &at, true, &v)) {
            return respond_error(400, "invalid given_melody", NULL);
        }
    }

    if (!store_root(compositions_dir, root, sizeof(root)) ||
        !create_composition_dir(root, id, sizeof(id), comp_dir, sizeof(comp_dir))) {
        return respond_failed();
    }
    created = true;
    if ((motive && !save_text(comp_dir, "motive.txt", motive)) ||
        (notes && !save_text(comp_dir, "given-melody.txt", notes))) {
        response = respond_failed();
        goto fail;
    }

    args_add(&plan_args, "plan");
    args_add(&plan_args, "--form");
    args_add(&plan_args, form);
    if (!add_text(&plan_args, request, "key", "--key", valid_key, true, &v) ||
        !add_text(&plan_args, request, "meter", "--meter", valid_meter, true, &v) ||
        !add_integer(&plan_args, request, "bars", "--bars", 1, 256, &v) ||
        !add_integer(&plan_args, request, "count", "--count", 1, 8, &v) ||
        !add_flag(&plan_args, request, "modulate", "--modulate", &v) ||
        !add_text(&plan_args, request, "accompaniment", "--accompaniment", valid_accompaniment, false, &v) ||
        !add_text(&plan_args, request, "texture", "--texture", valid_texture, false, &v) ||
        !add_seed(&plan_args, request, &v)) {
        response = respond_error(400, v.message, NULL);
        goto fail;
    }
    if (motive) {
        args_add(&plan_args, "--motive");
        args_addf(&plan_args, "%s/motive.txt", comp_dir);
    }
    if (notes) {
        args_add(&plan_args, "--given-melody");
        args_addf(&plan_args, "%s/given-melody.txt", comp_dir);
        args_add(&plan_args, "--at");
        args_add(&plan_args, at);
    }
    args_add(&plan_args, "--out");
    args_addf(&plan_args, "%s/v1/plan.json", comp_dir);
    args_add(&plan_args, "--format");
    args_add(&plan_args, "json");

    args_add(&fill_args, "fill");
    args_addf(&fill_args, "%s/v1/plan.json", comp_dir);
    if (!add_seed(&fill_args, request, &v) || !add_flag(&fill_args, request, "check", "--check", &v) ||
        !add_tempo(&fill_args, request, &v)) {
        response = respond_error(400, v.message, NULL);
        goto fail;
    }
    args_add(&fill_args, "--out");
    args_addf(&fill_args, "%s/v1/plan.json", comp_dir);
    args_add(&fill_args, "--midi");
    args_addf(&fill_args, "%s/v1/piece.mid", comp_dir);
    args_add(&fill_args, "--musicxml");
    args_addf(&fill_args, "%s/v1/piece.musicxml", comp_dir);
    args_add(&fill_args, "--format");
    args_add(&fill_args, "json");

    UrsatzGuiComposerOutput plan_output;
    if (!run_composer(&plan_args, &plan_output, &response)) {
        goto fail;
    }
    cJSON *plan_summary = parse_output(&plan_output);
    ursatz_gui_composer_output_destroy(&plan_output);

    UrsatzGuiComposerOutput fill_output;
    if (!plan_summary) {
        response = respond_error(500, "composer output unreadable", NULL);
        goto fail;
    }
    if (!run_composer(&fill_args, &fill_output, &response)) {
        cJSON_Delete(plan_summary);
        goto fail;
    }
    cJSON *fill_summary = parse_output(&fill_output);
    ursatz_gui_composer_output_destroy(&fill_output);
    if (!fill_summary) {
        cJSON_Delete(plan_summary);
        response = respond_error(500, "composer output unreadable", NULL);
        goto fail;
    }

    /* The version's summary holds both outputs; its seed is the fill's. */
    cJSON *summary = cJSON_CreateObject();
    if (!summary) {
        cJSON_Delete(plan_summary);
        cJSON_Delete(fill_summary);
        response = respond_failed();
        goto fail;
    }
    cJSON_AddItemToObject(summary, "plan", plan_summary);
    cJSON_AddItemToObject(summary, "fill", fill_summary);
    cJSON *seed = seed_of(fill_summary);
    cJSON *stored = cJSON_Duplicate(summary, true);
    cJSON *version = make_version(1, "plan", &plan_args, comp_dir, stored);
    if (version) {
        cJSON_ReplaceItemInObjectCaseSensitive(version, "seed", seed);
        cJSON_AddItemToObject(version, "fill_args", args_to_json(&fill_args, comp_dir));
    } else {
        cJSON_Delete(seed);
    }
    pthread_mutex_lock(&meta_mutex);
    bool saved = write_new_meta(comp_dir, name, "plan", cJSON_Duplicate(request, true), version);
    pthread_mutex_unlock(&meta_mutex);
    if (!saved) {
        cJSON_Delete(summary);
        response = respond_failed();
        goto fail;
    }
    args_destroy(&plan_args);
    args_destroy(&fill_args);
    return respond_created(id, summary);

fail:
    args_destroy(&plan_args);
    args_destroy(&fill_args);
    if (created) {
        remove_tree(comp_dir);
    }
    return response;
}

/* ---- reading the store ---------------------------------------------- */

/* A composition directory from a request id, or false when the id is not
   valid or the directory does not exist. */
static bool find_composition(const char *compositions_dir, const char *id, char *comp_dir, size_t size) {
    char root[PATH_SIZE];
    if (!id || strlen(id) > 63 || !ursatz_gui_corpus_is_valid_id(id) ||
        !store_root(compositions_dir, root, sizeof(root)) || !join_path(comp_dir, size, "%s/%s", root, id)) {
        return false;
    }
    return path_is_dir(comp_dir);
}

static const char *text_of(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}

typedef struct {
    char id[64];
    char created_at[32];
    cJSON *entry;
} ListEntry;

static int compare_entries(const void *left, const void *right) {
    const ListEntry *a = left;
    const ListEntry *b = right;
    int by_time = strcmp(b->created_at, a->created_at);
    return by_time != 0 ? by_time : strcmp(a->id, b->id);
}

UrsatzGuiCompositionResponse ursatz_gui_composition_list(const char *compositions_dir) {
    char root[PATH_SIZE];
    if (!store_root(compositions_dir, root, sizeof(root))) {
        return respond_failed();
    }
    DIR *dir = opendir(root);
    if (!dir) {
        return respond_failed();
    }

    ListEntry *entries = NULL;
    size_t count = 0;
    size_t capacity = 0;
    struct dirent *item;
    while ((item = readdir(dir)) != NULL) {
        char comp_dir[PATH_SIZE];
        if (item->d_name[0] == '.' || strlen(item->d_name) > 63 || !ursatz_gui_corpus_is_valid_id(item->d_name) ||
            !join_path(comp_dir, sizeof(comp_dir), "%s/%s", root, item->d_name)) {
            continue;
        }
        cJSON *meta = read_meta(comp_dir);
        if (!meta) {
            continue;
        }
        if (count == capacity) {
            size_t grown_capacity = capacity ? capacity * 2 : 16;
            ListEntry *grown = realloc(entries, grown_capacity * sizeof(ListEntry));
            if (!grown) {
                cJSON_Delete(meta);
                break;
            }
            entries = grown;
            capacity = grown_capacity;
        }
        const cJSON *versions = cJSON_GetObjectItemCaseSensitive(meta, "versions");
        cJSON *entry = cJSON_CreateObject();
        if (entry) {
            cJSON_AddStringToObject(entry, "id", item->d_name);
            cJSON_AddStringToObject(entry, "name", text_of(meta, "name"));
            cJSON_AddStringToObject(entry, "kind", text_of(meta, "kind"));
            cJSON_AddStringToObject(entry, "created_at", text_of(meta, "created_at"));
            cJSON_AddNumberToObject(entry, "versions", cJSON_IsArray(versions) ? cJSON_GetArraySize(versions) : 0);
            /* d_name was checked above to fit; created_at is cut to fit. */
            join_path(entries[count].id, sizeof(entries[count].id), "%s", item->d_name);
            snprintf(entries[count].created_at, sizeof(entries[count].created_at), "%.31s", text_of(meta, "created_at"));
            entries[count].entry = entry;
            count++;
        }
        cJSON_Delete(meta);
    }
    closedir(dir);

    if (count > 1) {
        qsort(entries, count, sizeof(ListEntry), compare_entries);
    }
    cJSON *object = cJSON_CreateObject();
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; i < count; i++) {
        if (array) {
            cJSON_AddItemToArray(array, entries[i].entry);
        } else {
            cJSON_Delete(entries[i].entry);
        }
    }
    free(entries);
    if (!object || !array) {
        cJSON_Delete(object);
        cJSON_Delete(array);
        return respond(500, NULL);
    }
    cJSON_AddTrueToObject(object, "ok");
    cJSON_AddItemToObject(object, "compositions", array);
    return respond(200, object);
}

UrsatzGuiCompositionResponse ursatz_gui_composition_get(const char *compositions_dir, const char *id) {
    char comp_dir[PATH_SIZE];
    if (!find_composition(compositions_dir, id, comp_dir, sizeof(comp_dir))) {
        return respond_not_found("composition not found");
    }
    cJSON *meta = read_meta(comp_dir);
    if (!meta) {
        return respond_not_found("composition not found");
    }
    cJSON_AddTrueToObject(meta, "ok");
    cJSON_AddStringToObject(meta, "id", id);
    return respond(200, meta);
}

UrsatzGuiCompositionResponse ursatz_gui_composition_rename(const char *compositions_dir, const char *id,
                                                           const cJSON *request) {
    char comp_dir[PATH_SIZE];
    if (!find_composition(compositions_dir, id, comp_dir, sizeof(comp_dir))) {
        return respond_not_found("composition not found");
    }
    Validation v = {{0}};
    const char *name = NULL;
    if (!cJSON_IsObject(request) || !text_value(request, "name", valid_name, &name, true, &v)) {
        return respond_error(400, "invalid name", NULL);
    }

    pthread_mutex_lock(&meta_mutex);
    cJSON *meta = read_meta(comp_dir);
    bool saved = false;
    if (meta) {
        cJSON *replacement = cJSON_CreateString(name);
        saved = replacement && cJSON_ReplaceItemInObjectCaseSensitive(meta, "name", replacement) &&
                write_meta(comp_dir, meta);
        if (!saved) {
            cJSON_Delete(replacement);
        }
        cJSON_Delete(meta);
    }
    pthread_mutex_unlock(&meta_mutex);
    if (!saved) {
        return respond_failed();
    }

    cJSON *object = cJSON_CreateObject();
    if (object) {
        cJSON_AddTrueToObject(object, "ok");
        cJSON_AddStringToObject(object, "id", id);
        cJSON_AddStringToObject(object, "name", name);
    }
    return respond(200, object);
}

UrsatzGuiCompositionResponse ursatz_gui_composition_delete(const char *compositions_dir, const char *id) {
    char comp_dir[PATH_SIZE];
    if (!find_composition(compositions_dir, id, comp_dir, sizeof(comp_dir))) {
        return respond_not_found("composition not found");
    }
    pthread_mutex_lock(&meta_mutex);
    remove_tree(comp_dir);
    pthread_mutex_unlock(&meta_mutex);
    cJSON *object = cJSON_CreateObject();
    if (object) {
        cJSON_AddTrueToObject(object, "ok");
    }
    return respond(200, object);
}

/* A version or variation number from a URL: digits only, 1 to MAX_VERSION. */
static bool parse_number(const char *text, long *out) {
    if (!text || text[0] == '\0' || strlen(text) > 4) {
        return false;
    }
    for (const char *c = text; *c; c++) {
        if (!isdigit((unsigned char)*c)) {
            return false;
        }
    }
    long value = strtol(text, NULL, 10);
    if (value < 1 || value > MAX_VERSION) {
        return false;
    }
    *out = value;
    return true;
}

char *ursatz_gui_composition_read_version_file(const char *compositions_dir, const char *id, const char *version,
                                               UrsatzGuiCompositionFile file, size_t *out_size) {
    static const char *const names[] = {"plan.json", "piece.musicxml", "piece.mid"};
    char comp_dir[PATH_SIZE];
    long number = 0;
    if (!parse_number(version, &number) || !find_composition(compositions_dir, id, comp_dir, sizeof(comp_dir))) {
        return NULL;
    }
    char path[PATH_SIZE];
    if (!join_path(path, sizeof(path), "%s/v%ld/%s", comp_dir, number, names[file]) || !path_is_file(path)) {
        return NULL;
    }
    return read_whole_file(path, out_size);
}
