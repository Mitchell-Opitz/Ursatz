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

/* ---- changing a composition ----------------------------------------- */

/* The version a request names, with its composition. */
typedef struct {
    char comp_dir[PATH_SIZE];
    char plan_path[PATH_SIZE];
    char version_dir[PATH_SIZE];
    cJSON *meta;
    long version;
} Source;

static const cJSON *versions_of(const cJSON *meta) {
    const cJSON *versions = cJSON_GetObjectItemCaseSensitive(meta, "versions");
    return cJSON_IsArray(versions) ? versions : NULL;
}

static bool has_version(const cJSON *meta, long number) {
    const cJSON *item;
    cJSON_ArrayForEach(item, versions_of(meta)) {
        const cJSON *n = cJSON_GetObjectItemCaseSensitive(item, "n");
        if (cJSON_IsNumber(n) && (long)n->valuedouble == number) {
            return true;
        }
    }
    return false;
}

static long highest_version(const cJSON *meta) {
    long highest = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, versions_of(meta)) {
        const cJSON *n = cJSON_GetObjectItemCaseSensitive(item, "n");
        if (cJSON_IsNumber(n) && (long)n->valuedouble > highest) {
            highest = (long)n->valuedouble;
        }
    }
    return highest;
}

static void source_destroy(Source *source) {
    cJSON_Delete(source->meta);
    source->meta = NULL;
}

/* Finds the composition and the version of request. False with *failure set:
   404 for a composition or version that is not there, 400 for a bad number. */
static bool open_source(const char *compositions_dir, const char *id, const cJSON *request, Source *source,
                        UrsatzGuiCompositionResponse *failure) {
    memset(source, 0, sizeof(*source));
    if (!find_composition(compositions_dir, id, source->comp_dir, sizeof(source->comp_dir))) {
        *failure = respond_not_found("composition not found");
        return false;
    }
    Validation v = {{0}};
    bool present = false;
    long long number = 0;
    if (!cJSON_IsObject(request)) {
        *failure = respond_error(400, "invalid request", NULL);
        return false;
    }
    if (!integer_value(request, "version", 1, MAX_VERSION, &present, &number, &v) || !present) {
        *failure = respond_error(400, "invalid version", NULL);
        return false;
    }
    source->version = (long)number;
    source->meta = read_meta(source->comp_dir);
    if (!source->meta || !has_version(source->meta, source->version) ||
        !join_path(source->version_dir, sizeof(source->version_dir), "%s/v%ld", source->comp_dir, source->version) ||
        !join_path(source->plan_path, sizeof(source->plan_path), "%s/plan.json", source->version_dir) ||
        !path_is_file(source->plan_path)) {
        source_destroy(source);
        *failure = respond_not_found("version not found");
        return false;
    }
    return true;
}

/* Makes the directory of the next version. Returns its number, 0 on failure.
   mkdir settles two requests that pick the same number. */
static long reserve_version(const char *comp_dir, const cJSON *meta, char *version_dir, size_t size) {
    for (long number = highest_version(meta) + 1; number <= MAX_VERSION; number++) {
        if (!join_path(version_dir, size, "%s/v%ld", comp_dir, number)) {
            return 0;
        }
        int made = make_dir(version_dir);
        if (made == 1) {
            return number;
        }
        if (made < 0) {
            return 0;
        }
    }
    return 0;
}

/* Adds a version to meta.json, in order of its number. Takes ownership. */
static bool append_version(const char *comp_dir, cJSON *version) {
    if (!version) {
        return false;
    }
    pthread_mutex_lock(&meta_mutex);
    cJSON *meta = read_meta(comp_dir);
    cJSON *versions = meta ? cJSON_GetObjectItemCaseSensitive(meta, "versions") : NULL;
    bool ok = false;
    if (cJSON_IsArray(versions)) {
        const cJSON *number = cJSON_GetObjectItemCaseSensitive(version, "n");
        int position = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, versions) {
            const cJSON *n = cJSON_GetObjectItemCaseSensitive(item, "n");
            if (cJSON_IsNumber(n) && cJSON_IsNumber(number) && n->valuedouble > number->valuedouble) {
                break;
            }
            position++;
        }
        ok = cJSON_InsertItemInArray(versions, position, version);
        if (ok) {
            version = NULL;
            ok = write_meta(comp_dir, meta);
        }
    }
    pthread_mutex_unlock(&meta_mutex);
    cJSON_Delete(version);
    cJSON_Delete(meta);
    return ok;
}

static void args_append(ArgList *args, const ArgList *more) {
    for (size_t i = 0; i < more->count; i++) {
        args_add(args, more->items[i]);
    }
    args->ok = args->ok && more->ok;
}

/* The tempo the composition was made with, so a new version sounds at the
   same speed. It was checked when the composition was made. */
static void add_stored_tempo(ArgList *args, const Source *source) {
    const cJSON *request = cJSON_GetObjectItemCaseSensitive(source->meta, "request");
    Validation ignored = {{0}};
    if (cJSON_IsObject(request)) {
        add_tempo(args, request, &ignored);
    }
}

/* --node <id> | --section <letter>, and --lane. Exactly one of node and
   section; lane and node are refused when only a section is allowed. */
static bool add_target(ArgList *args, const cJSON *request, bool section_only, Validation *v) {
    const cJSON *node = field(request, "node");
    const cJSON *section = field(request, "section");
    if ((node != NULL) == (section != NULL) || (section_only && (node || field(request, "lane")))) {
        invalid(v, "target");
        return false;
    }
    if (node) {
        if (!add_integer(args, request, "node", "--node", 1, 999999, v)) {
            return false;
        }
    } else if (!add_text(args, request, "section", "--section", valid_letter, true, v)) {
        return false;
    }
    static const char *const lanes[] = {"harmony", "bass", "melody", "inner", NULL};
    const cJSON *lane = field(request, "lane");
    if (lane && (!cJSON_IsString(lane) || !lane->valuestring || !one_of(lane->valuestring, lanes))) {
        invalid(v, "lane");
        return false;
    }
    if (lane) {
        args_add(args, "--lane");
        args_add(args, lane->valuestring);
    }
    return true;
}

static bool add_required_integer(ArgList *args, const cJSON *request, const char *key, const char *flag,
                                 long long low, long long high, Validation *v) {
    if (!field(request, key)) {
        invalid(v, key);
        return false;
    }
    return add_integer(args, request, key, flag, low, high, v);
}

/* Runs args, whose --out is in version_dir, and records the result as version
   `number` of the composition. A failure removes version_dir and leaves
   meta.json as it was. copy_piece takes the piece files from the source
   version (lock changes the plan only). */
static UrsatzGuiCompositionResponse finish_version(const Source *source, const char *id, const char *action,
                                                   const ArgList *args, const char *version_dir, long number,
                                                   bool copy_piece) {
    UrsatzGuiComposerOutput output;
    UrsatzGuiCompositionResponse response;
    if (!run_composer(args, &output, &response)) {
        remove_tree(version_dir);
        return response;
    }
    cJSON *summary = parse_output(&output);
    ursatz_gui_composer_output_destroy(&output);
    if (!summary) {
        remove_tree(version_dir);
        return respond_error(500, "composer output unreadable", NULL);
    }

    bool copied = true;
    if (copy_piece) {
        static const char *const pieces[] = {"piece.mid", "piece.musicxml"};
        for (size_t i = 0; i < 2; i++) {
            char from[PATH_SIZE];
            char to[PATH_SIZE];
            copied = copied && join_path(from, sizeof(from), "%s/%s", source->version_dir, pieces[i]) &&
                     join_path(to, sizeof(to), "%s/%s", version_dir, pieces[i]) && copy_file(from, to);
        }
    }
    cJSON *version = copied ? make_version(number, action, args, source->comp_dir, cJSON_Duplicate(summary, true))
                            : NULL;
    if (!append_version(source->comp_dir, version)) {
        cJSON_Delete(summary);
        remove_tree(version_dir);
        return respond_failed();
    }

    cJSON *object = cJSON_CreateObject();
    if (!object) {
        cJSON_Delete(summary);
        return respond(500, NULL);
    }
    cJSON_AddTrueToObject(object, "ok");
    cJSON_AddStringToObject(object, "id", id);
    cJSON_AddNumberToObject(object, "version", (double)number);
    cJSON_AddItemToObject(object, "summary", summary);
    return respond(200, object);
}

/* lock and regenerate: <command> <plan> <target> --out <new plan> ... */
static UrsatzGuiCompositionResponse change_plan(const char *compositions_dir, const char *id,
                                                const cJSON *request, bool lock) {
    Source source;
    UrsatzGuiCompositionResponse response;
    if (!open_source(compositions_dir, id, request, &source, &response)) {
        return response;
    }

    Validation v = {{0}};
    ArgList target;
    args_init(&target);
    if (!add_target(&target, request, false, &v) || (!lock && !add_seed(&target, request, &v))) {
        args_destroy(&target);
        source_destroy(&source);
        return respond_error(400, v.message, NULL);
    }

    char version_dir[PATH_SIZE];
    long number = reserve_version(source.comp_dir, source.meta, version_dir, sizeof(version_dir));
    if (number == 0) {
        args_destroy(&target);
        source_destroy(&source);
        return respond_failed();
    }

    ArgList args;
    args_init(&args);
    args_add(&args, lock ? "lock" : "regenerate");
    args_add(&args, source.plan_path);
    args_append(&args, &target);
    args_add(&args, "--out");
    args_addf(&args, "%s/plan.json", version_dir);
    if (!lock) {
        args_add(&args, "--midi");
        args_addf(&args, "%s/piece.mid", version_dir);
        args_add(&args, "--musicxml");
        args_addf(&args, "%s/piece.musicxml", version_dir);
        add_stored_tempo(&args, &source);
    }
    args_add(&args, "--format");
    args_add(&args, "json");

    response = finish_version(&source, id, lock ? "lock" : "regenerate", &args, version_dir, number, lock);
    args_destroy(&args);
    args_destroy(&target);
    source_destroy(&source);
    return response;
}

UrsatzGuiCompositionResponse ursatz_gui_composition_lock(const char *compositions_dir, const char *id,
                                                         const cJSON *request) {
    return change_plan(compositions_dir, id, request, true);
}

UrsatzGuiCompositionResponse ursatz_gui_composition_regenerate(const char *compositions_dir, const char *id,
                                                               const cJSON *request) {
    return change_plan(compositions_dir, id, request, false);
}

/* variations-<k> as a directory name; false for any other name. */
static bool variation_set_number(const char *name, long *out) {
    return strncmp(name, "variations-", 11) == 0 && parse_number(name + 11, out);
}

/* The directory of variations set `set`, in whichever version it was made
   from. False when there is none. */
static bool find_variation_dir(const char *comp_dir, const cJSON *meta, long set, char *out, size_t size,
                               long *from_version) {
    const cJSON *item;
    cJSON_ArrayForEach(item, versions_of(meta)) {
        const cJSON *n = cJSON_GetObjectItemCaseSensitive(item, "n");
        if (!cJSON_IsNumber(n)) {
            continue;
        }
        if (join_path(out, size, "%s/v%ld/variations-%ld", comp_dir, (long)n->valuedouble, set) && path_is_dir(out)) {
            *from_version = (long)n->valuedouble;
            return true;
        }
    }
    return false;
}

/* Makes v<version>/variations-<k> with the next k of the whole composition,
   so a set is named by k alone. Returns k, 0 on failure. */
static long reserve_variation_dir(const Source *source, char *dir, size_t size) {
    pthread_mutex_lock(&meta_mutex);
    long highest = 0;
    const cJSON *item;
    cJSON_ArrayForEach(item, versions_of(source->meta)) {
        const cJSON *n = cJSON_GetObjectItemCaseSensitive(item, "n");
        char version_path[PATH_SIZE];
        if (!cJSON_IsNumber(n) ||
            !join_path(version_path, sizeof(version_path), "%s/v%ld", source->comp_dir, (long)n->valuedouble)) {
            continue;
        }
        DIR *handle = opendir(version_path);
        struct dirent *entry;
        while (handle && (entry = readdir(handle)) != NULL) {
            long set = 0;
            if (variation_set_number(entry->d_name, &set) && set > highest) {
                highest = set;
            }
        }
        if (handle) {
            closedir(handle);
        }
    }
    long set = highest + 1;
    bool made = set <= MAX_VERSION && join_path(dir, size, "%s/variations-%ld", source->version_dir, set) &&
                make_dir(dir) == 1;
    pthread_mutex_unlock(&meta_mutex);
    return made ? set : 0;
}

UrsatzGuiCompositionResponse ursatz_gui_composition_variations(const char *compositions_dir, const char *id,
                                                               const cJSON *request) {
    Source source;
    UrsatzGuiCompositionResponse response;
    if (!open_source(compositions_dir, id, request, &source, &response)) {
        return response;
    }

    Validation v = {{0}};
    bool harmonize = false;
    ArgList target;
    args_init(&target);
    if (!flag_value(request, "harmonize", &harmonize, &v) || !add_target(&target, request, harmonize, &v) ||
        !add_required_integer(&target, request, "count", "--count", 1, 16, &v) || !add_seed(&target, request, &v)) {
        args_destroy(&target);
        source_destroy(&source);
        return respond_error(400, v.message, NULL);
    }

    char set_dir[PATH_SIZE];
    long set = reserve_variation_dir(&source, set_dir, sizeof(set_dir));
    if (set == 0) {
        args_destroy(&target);
        source_destroy(&source);
        return respond_failed();
    }

    ArgList args;
    args_init(&args);
    args_add(&args, harmonize ? "harmonize" : "variations");
    args_add(&args, source.plan_path);
    args_append(&args, &target);
    args_add(&args, "--out-dir");
    args_add(&args, set_dir);
    args_add(&args, "--midi");
    args_add(&args, "--musicxml");
    add_stored_tempo(&args, &source);
    args_add(&args, "--format");
    args_add(&args, "json");

    UrsatzGuiComposerOutput output;
    cJSON *result = NULL;
    if (run_composer(&args, &output, &response)) {
        result = parse_output(&output);
        ursatz_gui_composer_output_destroy(&output);
        if (!result) {
            response = respond_error(500, "composer output unreadable", NULL);
        }
    }

    if (result) {
        /* result.json keeps each variation's seed and heard form for choose. */
        char result_path[PATH_SIZE];
        char *text = cJSON_Print(result);
        bool saved = text && join_path(result_path, sizeof(result_path), "%s/result.json", set_dir) &&
                     write_file_atomic(result_path, text, strlen(text));
        free(text);

        cJSON *object = cJSON_CreateObject();
        cJSON *list = cJSON_CreateArray();
        const cJSON *variations = cJSON_GetObjectItemCaseSensitive(result, "variations");
        const cJSON *entry;
        cJSON_ArrayForEach(entry, variations) {
            cJSON *item = cJSON_CreateObject();
            if (item) {
                cJSON_AddItemToObject(item, "index", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(entry, "index"), true));
                cJSON_AddItemToObject(item, "seed", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(entry, "seed"), true));
                cJSON_AddItemToObject(item, "sections", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(entry, "sections"), true));
                cJSON_AddItemToArray(list, item);
            }
        }
        if (saved && object && list) {
            cJSON_AddTrueToObject(object, "ok");
            cJSON_AddStringToObject(object, "id", id);
            cJSON_AddNumberToObject(object, "version", (double)source.version);
            cJSON_AddNumberToObject(object, "set", (double)set);
            cJSON_AddItemToObject(object, "variations", list);
            static const char *const counts[] = {"kept", "requested", "seeds_tried"};
            for (size_t i = 0; i < 3; i++) {
                cJSON_AddItemToObject(object, counts[i],
                                      cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(result, counts[i]), true));
            }
            response = respond(200, object);
        } else {
            cJSON_Delete(object);
            cJSON_Delete(list);
            response = respond_failed();
        }
        cJSON_Delete(result);
    }
    if (response.status != 200) {
        remove_tree(set_dir);
    }

    args_destroy(&args);
    args_destroy(&target);
    source_destroy(&source);
    return response;
}

/* A variation's files and its entry in result.json. */
static bool open_variation(const char *compositions_dir, const char *id, const char *set_text,
                           const char *index_text, Source *source, char *set_dir, size_t size, long *index,
                           long *set, cJSON **entry) {
    long from_version = 0;
    if (!parse_number(set_text, set) || !parse_number(index_text, index) ||
        !find_composition(compositions_dir, id, source->comp_dir, sizeof(source->comp_dir))) {
        return false;
    }
    source->meta = read_meta(source->comp_dir);
    if (!source->meta || !find_variation_dir(source->comp_dir, source->meta, *set, set_dir, size, &from_version)) {
        source_destroy(source);
        return false;
    }
    source->version = from_version;
    char path[PATH_SIZE];
    char *text = join_path(path, sizeof(path), "%s/result.json", set_dir) ? read_whole_file(path, NULL) : NULL;
    cJSON *result = text ? cJSON_Parse(text) : NULL;
    free(text);
    const cJSON *kept = result ? cJSON_GetObjectItemCaseSensitive(result, "variations") : NULL;
    const cJSON *item;
    cJSON_ArrayForEach(item, kept) {
        const cJSON *number = cJSON_GetObjectItemCaseSensitive(item, "index");
        if (cJSON_IsNumber(number) && (long)number->valuedouble == *index) {
            *entry = cJSON_Duplicate(item, true);
            break;
        }
    }
    cJSON_Delete(result);
    if (!*entry) {
        source_destroy(source);
        return false;
    }
    return true;
}

UrsatzGuiCompositionResponse ursatz_gui_composition_choose(const char *compositions_dir, const char *id,
                                                           const char *set_text, const char *index_text) {
    Source source;
    memset(&source, 0, sizeof(source));
    char set_dir[PATH_SIZE];
    long index = 0;
    long set = 0;
    cJSON *entry = NULL;
    if (!open_variation(compositions_dir, id, set_text, index_text, &source, set_dir, sizeof(set_dir), &index, &set,
                        &entry)) {
        return respond_not_found("variation not found");
    }

    char version_dir[PATH_SIZE];
    long number = reserve_version(source.comp_dir, source.meta, version_dir, sizeof(version_dir));
    static const char *const from_names[] = {"json", "mid", "musicxml"};
    static const char *const to_names[] = {"plan.json", "piece.mid", "piece.musicxml"};
    bool copied = number != 0;
    for (size_t i = 0; copied && i < 3; i++) {
        char from[PATH_SIZE];
        char to[PATH_SIZE];
        copied = join_path(from, sizeof(from), "%s/variation-%ld.%s", set_dir, index, from_names[i]) &&
                 join_path(to, sizeof(to), "%s/%s", version_dir, to_names[i]) && copy_file(from, to);
    }

    cJSON *summary = cJSON_CreateObject();
    cJSON *version = NULL;
    if (copied && summary) {
        cJSON_AddNumberToObject(summary, "from_version", (double)source.version);
        cJSON_AddNumberToObject(summary, "set", (double)set);
        cJSON_AddNumberToObject(summary, "index", (double)index);
        cJSON_AddItemToObject(summary, "seed", cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(entry, "seed"), true));
        cJSON_AddItemToObject(summary, "sections",
                              cJSON_Duplicate(cJSON_GetObjectItemCaseSensitive(entry, "sections"), true));
        ArgList none;
        args_init(&none);
        version = make_version(number, "choose", &none, source.comp_dir, cJSON_Duplicate(summary, true));
        args_destroy(&none);
    }
    cJSON_Delete(entry);
    if (!append_version(source.comp_dir, version)) {
        cJSON_Delete(summary);
        if (number != 0) {
            remove_tree(version_dir);
        }
        source_destroy(&source);
        return respond_failed();
    }
    source_destroy(&source);

    cJSON *object = cJSON_CreateObject();
    if (!object) {
        cJSON_Delete(summary);
        return respond(500, NULL);
    }
    cJSON_AddTrueToObject(object, "ok");
    cJSON_AddStringToObject(object, "id", id);
    cJSON_AddNumberToObject(object, "version", (double)number);
    cJSON_AddItemToObject(object, "summary", summary);
    return respond(200, object);
}

char *ursatz_gui_composition_read_variation_file(const char *compositions_dir, const char *id, const char *set_text,
                                                 const char *index_text, UrsatzGuiCompositionFile file,
                                                 size_t *out_size) {
    static const char *const extensions[] = {"json", "musicxml", "mid"};
    Source source;
    memset(&source, 0, sizeof(source));
    char set_dir[PATH_SIZE];
    long index = 0;
    long set = 0;
    cJSON *entry = NULL;
    if (!open_variation(compositions_dir, id, set_text, index_text, &source, set_dir, sizeof(set_dir), &index, &set,
                        &entry)) {
        return NULL;
    }
    cJSON_Delete(entry);
    source_destroy(&source);
    char path[PATH_SIZE];
    if (!join_path(path, sizeof(path), "%s/variation-%ld.%s", set_dir, index, extensions[file]) ||
        !path_is_file(path)) {
        return NULL;
    }
    return read_whole_file(path, out_size);
}

/* ---- reading the store ---------------------------------------------- */

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
