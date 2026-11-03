#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/motive.h>

#include "api_internal.h"
#include "util/request_parse.h"

/* The motive request members (docs/api.md): meter, key, pickup, harmony, features, role, and the input to read. */

static ComposerStatus fail_at(ComposerError *error, ComposerStatus status, const char *base, const char *tail,
                              const char *format, ...) {
    if (error) {
        va_list args;
        va_start(args, format);
        vsnprintf(error->message, sizeof error->message, format, args);
        va_end(args);
        snprintf(error->path, sizeof error->path, "%s%s", base ? base : "", tail);
    }
    return status;
}

/* A string member; absent or null leaves *out alone and sets *present false. */
static ComposerStatus string_member(const cJSON *object, const char *base, const char *name, const char **out,
                                    bool *present, ComposerError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    *present = item && !cJSON_IsNull(item);
    if (!*present) {
        return COMPOSER_OK;
    }
    if (!cJSON_IsString(item)) {
        char tail[64];
        snprintf(tail, sizeof tail, "/%s", name);
        return fail_at(error, COMPOSER_ERR_REQUEST, base, tail, "%s must be a string", name);
    }
    *out = item->valuestring;
    return COMPOSER_OK;
}

static bool has_member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return item && !cJSON_IsNull(item);
}

ComposerStatus api_read_motive_extras(const cJSON *object, const char *base, const PlanMeter *meter,
                                      MotiveOptions *options, ComposerError *error) {
    const char *harmony = NULL;
    const char *role = NULL;
    bool has_harmony = false;
    bool has_role = false;
    ComposerStatus status = string_member(object, base, "harmony", &harmony, &has_harmony, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (has_harmony) {
        MotiveError detail;
        if (!meter || motive_harmony_parse(harmony, meter, options->harmony, &options->harmony_count, &detail) != MOTIVE_OK) {
            return fail_at(error, COMPOSER_ERR_REQUEST, base, "/harmony", "harmony '%s' at %s: %s", harmony,
                           meter ? detail.location : "the meter", meter ? detail.message : "is not known");
        }
        options->has_harmony = true;
    }
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(object, "features");
    if (features && !cJSON_IsNull(features)) {
        if (!cJSON_IsArray(features)) {
            return fail_at(error, COMPOSER_ERR_REQUEST, base, "/features", "features must be a list of strings like \"a:0-2\"");
        }
        size_t count = (size_t)cJSON_GetArraySize(features);
        if (count > MOTIVE_MAX_FEATURES) {
            return fail_at(error, COMPOSER_ERR_REQUEST, base, "/features", "a motive has at most %d features",
                           MOTIVE_MAX_FEATURES);
        }
        for (size_t i = 0; i < count; i++) {
            const cJSON *item = cJSON_GetArrayItem(features, (int)i);
            char tail[48];
            snprintf(tail, sizeof tail, "/features/%zu", i);
            if (!cJSON_IsString(item)) {
                return fail_at(error, COMPOSER_ERR_REQUEST, base, tail, "feature must be a string like \"a:0-2\"");
            }
            MotiveError detail;
            if (motive_feature_parse(item->valuestring, &options->features[i], &detail) != MOTIVE_OK) {
                return fail_at(error, COMPOSER_ERR_REQUEST, base, tail, "feature '%s': %s", item->valuestring,
                               detail.message);
            }
            for (size_t j = 0; j < i; j++) {
                if (strcmp(options->features[j].label, options->features[i].label) == 0) {
                    return fail_at(error, COMPOSER_ERR_REQUEST, base, tail, "feature label '%s' is used twice",
                                   options->features[i].label);
                }
            }
        }
        if (count > 0) {
            options->feature_count = count;
            options->has_features = true;
        }
    }
    status = string_member(object, base, "role", &role, &has_role, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (has_role) {
        if (!motive_role_parse(role, &options->accompaniment)) {
            return fail_at(error, COMPOSER_ERR_REQUEST, base, "/role", "role must be melodic or accompaniment, not '%s'",
                           role);
        }
        options->has_role = true;
    }
    return COMPOSER_OK;
}

/* The pickup, in beats of beat_unit, from a number or a string. */
static ComposerStatus read_pickup(const cJSON *request, const char *base, int beat_unit, MotiveOptions *options,
                                  ComposerError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(request, "pickup");
    char number[32];
    const char *text = cJSON_IsString(item) ? item->valuestring : NULL;
    if (cJSON_IsNumber(item)) {
        snprintf(number, sizeof number, "%g", item->valuedouble);
        text = number;
    }
    if (!text) {
        return fail_at(error, COMPOSER_ERR_REQUEST, base, "/pickup", "pickup must be a number of beats like 1 or \"1/2\"");
    }
    Rational pickup;
    if (!request_parse_pickup(text, beat_unit, &pickup)) {
        return fail_at(error, COMPOSER_ERR_REQUEST, base, "/pickup", "pickup '%s' is not a number of beats; write 0, 1 or 1/2",
                       text);
    }
    options->has_pickup = true;
    options->pickup = pickup;
    return COMPOSER_OK;
}

/* The input's format and form as the motive command reads them (--format, --form). */
static ComposerStatus read_format(const cJSON *input, const char *base, MotiveFormat *format, ComposerError *error) {
    char where[sizeof(((ComposerError *)0)->path)];
    snprintf(where, sizeof where, "%s/input", base);
    const char *format_text = NULL;
    const char *form_text = NULL;
    bool has_format = false;
    bool has_form = false;
    ComposerStatus status = string_member(input, where, "format", &format_text, &has_format, error);
    if (status == COMPOSER_OK) {
        status = string_member(input, where, "form", &form_text, &has_form, error);
    }
    if (status != COMPOSER_OK) {
        return status;
    }
    *format = MOTIVE_FORMAT_AUTO;
    if (has_format && !motive_format_parse(format_text, format)) {
        return fail_at(error, COMPOSER_ERR_REQUEST, where, "/format",
                       "format must be auto, midi, musicxml, notes, degrees, rhythm, contour or json, not '%s'", format_text);
    }
    if (has_form) {
        MotiveSketchForm form;
        if (!motive_form_parse(form_text, &form)) {
            return fail_at(error, COMPOSER_ERR_REQUEST, where, "/form", "form must be full, rhythm or contour, not '%s'",
                           form_text);
        }
        /* A rhythm or a contour is read by its own reader; full leaves the format as it is. */
        MotiveFormat wanted = form == MOTIVE_SKETCH_FORM_RHYTHM ? MOTIVE_FORMAT_RHYTHM : MOTIVE_FORMAT_CONTOUR;
        if (form != MOTIVE_SKETCH_FORM_FULL && *format != MOTIVE_FORMAT_AUTO && *format != wanted) {
            return fail_at(error, COMPOSER_ERR_REQUEST, where, "/form", "form %s reads a %s, which format %s does not",
                           form_text, form_text, format_text);
        }
        if (form == MOTIVE_SKETCH_FORM_FULL && (*format == MOTIVE_FORMAT_RHYTHM || *format == MOTIVE_FORMAT_CONTOUR)) {
            return fail_at(error, COMPOSER_ERR_REQUEST, where, "/form",
                           "form full does not read a %s; use form %s or another format", format_text, format_text);
        }
        if (form != MOTIVE_SKETCH_FORM_FULL) {
            *format = wanted;
        }
    }
    return COMPOSER_OK;
}

static ComposerStatus load_failure(ComposerError *error, const char *base, bool bytes, MotiveResult result,
                                   const MotiveError *detail) {
    return fail_at(error, result == MOTIVE_ERR_ALLOC ? COMPOSER_ERR_ALLOC : COMPOSER_ERR_UNSATISFIABLE, base,
                   bytes ? "" : "/input/text", "cannot load the input (%s)%s%s: %s", motive_result_name(result),
                   detail->location[0] ? " at " : "", detail->location, detail->message);
}

static ComposerStatus read_motive(const cJSON *request, const char *base, const unsigned char *bytes, size_t size,
                                  Motive *out, MotiveOptions *options_out, ComposerError *error) {
    MotiveOptions options;
    memset(&options, 0, sizeof options);
    const char *meter_text = NULL;
    const char *key_text = NULL;
    bool present = false;
    ComposerStatus status = string_member(request, base, "meter", &meter_text, &present, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (present) {
        if (!motive_parse_meter(meter_text, &options.meter)) {
            return fail_at(error, COMPOSER_ERR_REQUEST, base, "/meter",
                           "meter '%s' is not a meter; write beats over a beat unit, as in 4/4 or 6/8", meter_text);
        }
        options.has_meter = true;
    }
    status = string_member(request, base, "key", &key_text, &present, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (present) {
        if (!motive_parse_key(key_text, &options.key)) {
            return fail_at(error, COMPOSER_ERR_REQUEST, base, "/key",
                           "unknown key '%s'; write a tonic letter A-G with an optional # or b, then major or minor, "
                           "for example \"D minor\"",
                           key_text);
        }
        options.has_key = true;
    }

    MotiveFormat format = MOTIVE_FORMAT_MIDI;
    const char *text = NULL;
    if (!bytes) {
        const cJSON *input = cJSON_GetObjectItemCaseSensitive(request, "input");
        if (!cJSON_IsObject(input)) {
            return fail_at(error, COMPOSER_ERR_REQUEST, base, "/input", "input must be an object with the motive's text");
        }
        status = read_format(input, base, &format, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        char where[sizeof error->path];
        snprintf(where, sizeof where, "%s/input", base);
        status = string_member(input, where, "text", &text, &present, error);
        if (status == COMPOSER_OK && !present) {
            status = fail_at(error, COMPOSER_ERR_REQUEST, where, "/text", "text must be a string");
        }
        if (status != COMPOSER_OK) {
            return status;
        }
    }
    /* Too many features is a request mistake whatever the input says. */
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(request, "features");
    if (cJSON_IsArray(features) && (size_t)cJSON_GetArraySize(features) > MOTIVE_MAX_FEATURES) {
        return fail_at(error, COMPOSER_ERR_REQUEST, base, "/features", "a motive has at most %d features",
                       MOTIVE_MAX_FEATURES);
    }

    MotiveError detail;
    memset(&detail, 0, sizeof detail);
    MotiveResult result = bytes ? motive_load(bytes, size, format, &options, out, &detail)
                                : motive_load(text, strlen(text), format, &options, out, &detail);
    bool extras = has_member(request, "harmony") || has_member(request, "features") || has_member(request, "role");
    if (result == MOTIVE_OK && (has_member(request, "pickup") || extras)) {
        /* The pickup and the chord onsets are in beats of the motive's meter, which an input may declare, so they are
           read after the first load. */
        PlanMeter meter = motive_meter(out);
        if (has_member(request, "pickup")) {
            status = read_pickup(request, base, meter.beat_unit, &options, error);
            if (status != COMPOSER_OK) {
                return status;
            }
        }
        status = api_read_motive_extras(request, base, &meter, &options, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        result = bytes ? motive_load(bytes, size, format, &options, out, &detail)
                       : motive_load(text, strlen(text), format, &options, out, &detail);
    }
    if (result != MOTIVE_OK) {
        return load_failure(error, base, bytes != NULL, result, &detail);
    }
    if (options_out) {
        *options_out = options;
    }
    return COMPOSER_OK;
}

ComposerStatus api_read_motive(const cJSON *request, const char *base, Motive *out, MotiveOptions *options,
                               ComposerError *error) {
    return read_motive(request, base ? base : "", NULL, 0, out, options, error);
}
