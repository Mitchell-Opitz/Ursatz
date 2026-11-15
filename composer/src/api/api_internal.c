#include "api_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

#include <ursatz_composer/preferences.h>

#include "api_vocabulary.h"
#include "util/request_parse.h"

ComposerStatus api_fail(ComposerError *error, ComposerStatus status, const char *path, const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof error->message, format, args);
        va_end(args);
        snprintf(error->path, sizeof error->path, "%s", path ? path : "");
    }
    return status;
}

ComposerStatus api_fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

bool api_add(cJSON *object, const char *name, cJSON *item) {
    if (!object || !item || !cJSON_AddItemToObject(object, name, item)) {
        cJSON_Delete(item);
        return false;
    }
    return true;
}

ComposerStatus api_parse_request(const char *text, cJSON **out, ComposerError *error) {
    *out = NULL;
    if (!text) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request is missing");
    }
    cJSON *root = cJSON_Parse(text);
    if (!root) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request is not valid JSON");
    }
    if (!cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return api_fail(error, COMPOSER_ERR_REQUEST, "", "the request must be a JSON object");
    }
    const cJSON *version = cJSON_GetObjectItemCaseSensitive(root, "api_version");
    if (version && (!cJSON_IsNumber(version) || version->valuedouble != (double)COMPOSER_API_VERSION_MAJOR)) {
        cJSON_Delete(root);
        return api_fail(error, COMPOSER_ERR_REQUEST, "/api_version", "api_version must be %d", COMPOSER_API_VERSION_MAJOR);
    }
    *out = root;
    return COMPOSER_OK;
}

ComposerStatus api_respond_json(ComposerResponse *out, cJSON *value) {
    char *text = value ? cJSON_PrintUnformatted(value) : NULL;
    cJSON_Delete(value);
    if (!text) {
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    out->json = text;
    return COMPOSER_OK;
}

/* The member, or NULL (with *present false) when it is absent or null. */
static const cJSON *member(const cJSON *object, const char *name, bool *present) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    bool found = item && !cJSON_IsNull(item);
    if (present) {
        *present = found;
    }
    return found ? item : NULL;
}

static bool member_fail(ComposerError *error, const char *name, const char *what) {
    char path[sizeof error->path];
    snprintf(path, sizeof path, "/%s", name);
    api_fail(error, COMPOSER_ERR_REQUEST, path, "%s must be %s", name, what);
    return false;
}

bool api_member_string(const cJSON *object, const char *name, const char **out, bool *present, ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsString(item)) {
        return member_fail(error, name, "a string");
    }
    *out = item->valuestring;
    return true;
}

bool api_member_bool(const cJSON *object, const char *name, bool *out, bool *present, ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsBool(item)) {
        return member_fail(error, name, "true or false");
    }
    *out = cJSON_IsTrue(item);
    return true;
}

bool api_member_whole(const cJSON *object, const char *name, unsigned long minimum, unsigned long maximum,
                      unsigned long *out, bool *present, ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    char text[32];
    const char *source = NULL;
    unsigned long value = 0;
    if (cJSON_IsString(item)) {
        source = item->valuestring;
    } else if (cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= 4294967295.0 &&
               item->valuedouble == (double)(unsigned long)item->valuedouble) {
        snprintf(text, sizeof text, "%lu", (unsigned long)item->valuedouble);
        source = text;
    } else if (cJSON_IsNumber(item)) {
        snprintf(text, sizeof text, "%g", item->valuedouble);
        source = text;
    }
    if (!source || !request_parse_whole_number(source, maximum, &value) || value < minimum) {
        char path[sizeof error->path];
        snprintf(path, sizeof path, "/%s", name);
        api_fail(error, COMPOSER_ERR_REQUEST, path, "%s must be a whole number from %lu to %lu, not '%s'", name, minimum,
                 maximum, source ? source : "?");
        return false;
    }
    *out = value;
    return true;
}

unsigned long api_default_seed(void) {
    return ((unsigned long)time(NULL) * 2654435761UL + (unsigned long)clock()) & 4294967295UL;
}

bool api_member_rational(const cJSON *object, const char *name, ComposerRatio *out, bool *present, ComposerError *error) {
    const cJSON *item = member(object, name, present);
    if (!item) {
        return true;
    }
    char text[32];
    const char *source = NULL;
    if (cJSON_IsString(item)) {
        source = item->valuestring;
    } else if (cJSON_IsNumber(item) && item->valuedouble >= 0 && item->valuedouble <= 4294967295.0 &&
               item->valuedouble == (double)(unsigned long)item->valuedouble) {
        snprintf(text, sizeof text, "%lu", (unsigned long)item->valuedouble);
        source = text;
    }
    if (!source || !request_parse_ratio(source, out)) {
        return member_fail(error, name, "a whole number or a fraction like \"3/2\"");
    }
    return true;
}

ComposerStatus api_read_tempo(const cJSON *request, PlanExportOptions *options, ComposerError *error) {
    ComposerRatio tempo;
    bool has_tempo = false;
    if (!api_member_rational(request, "tempo", &tempo, &has_tempo, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (has_tempo && (tempo.denominator == 0 || tempo.numerator < API_TEMPO_MIN * tempo.denominator ||
                      tempo.numerator > API_TEMPO_MAX * tempo.denominator)) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(request, "tempo");
        char text[48] = "";
        if (cJSON_IsString(item)) {
            snprintf(text, sizeof text, "%s", item->valuestring);
        } else if (cJSON_IsNumber(item)) {
            snprintf(text, sizeof text, "%g", item->valuedouble);
        }
        return api_fail(error, COMPOSER_ERR_REQUEST, "/tempo",
                        "tempo must be from 20 to 400 quarter notes per minute, a whole number or a fraction like 195/2, not '%s'",
                        text);
    }
    if (has_tempo) {
        options->tempo_numerator = tempo.numerator;
        options->tempo_denominator = tempo.denominator;
    }
    return COMPOSER_OK;
}

ComposerStatus api_load_style(const char *path, const char *const *without, size_t without_count, const char *path_file,
                              const char *path_without, ComposerStyle **out, ComposerError *error) {
    ComposerStyleResult loaded = composer_style_load_without(path, without, without_count, out);
    if (loaded == COMPOSER_STYLE_OK) {
        return COMPOSER_OK;
    }
    bool argument = loaded == COMPOSER_STYLE_ERR_ARGUMENT;
    return api_fail(error, argument ? COMPOSER_ERR_REQUEST : loaded == COMPOSER_STYLE_ERR_ALLOC ? COMPOSER_ERR_ALLOC
                                                                                                  : COMPOSER_ERR_UNSATISFIABLE,
                    argument ? path_without : path_file, "cannot read style '%s': %s", path,
                    composer_style_result_name(loaded));
}

ComposerStatus api_read_output_options(const cJSON *request, ApiOutputOptions *out, ComposerError *error) {
    bool present;
    bool plain = false;
    const char *explain = NULL;
    memset(out, 0, sizeof(*out));
    out->export_options = plan_export_options_default();
    if (!api_member_bool(request, "midi", &out->midi, &present, error) ||
        !api_member_bool(request, "musicxml", &out->musicxml, &present, error) ||
        !api_member_bool(request, "plain_midi", &plain, &present, error) ||
        !api_member_bool(request, "plans", &out->plans, &present, error) ||
        !api_member_string(request, "explain", &explain, &present, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (explain) {
        if (strcmp(explain, "text") == 0) {
            out->explain = 1;
        } else if (strcmp(explain, "json") == 0) {
            out->explain = 2;
        } else {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/explain", "explain must be \"text\" or \"json\", not '%s'", explain);
        }
    }
    if (plain) {
        out->export_options.track_names = false;
        out->export_options.section_markers = false;
    }
    return COMPOSER_OK;
}

bool api_read_preferences(const cJSON *request, ComposerPreferences *out, bool *present, ComposerError *error) {
    composer_preferences_default(out);
    const cJSON *item = member(request, "preferences", present);
    if (!item) {
        return true;
    }
    if (!cJSON_IsObject(item)) {
        return member_fail(error, "preferences", "an object");
    }
    char path[COMPOSER_PREFERENCES_PATH_MAX / 2]; /* the reader's paths are short; "/preferences" + path fits error->path */
    char message[COMPOSER_PREFERENCES_MESSAGE_MAX];
    if (!composer_preferences_read(item, out, path, sizeof path, message, sizeof message)) {
        char full[sizeof error->path];
        snprintf(full, sizeof full, "/preferences%s", path);
        api_fail(error, COMPOSER_ERR_REQUEST, full, "%s", message);
        return false;
    }
    return true;
}
