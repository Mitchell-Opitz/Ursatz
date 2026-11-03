#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/motive.h>

#include "api_internal.h"
#include "api_piece.h"

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

/* The onset in beats of the meter as a request writes it: "3" or "3/2". */
static bool onset_text(Rational onset, int beat_unit, char *out, size_t size) {
    Rational beats;
    if (rational_mul_int(onset, beat_unit, &beats) != RATIONAL_OK) {
        return false;
    }
    if (beats.den == 1) {
        snprintf(out, size, "%lld", (long long)beats.num);
    } else {
        snprintf(out, size, "%lld/%lld", (long long)beats.num, (long long)beats.den);
    }
    return true;
}

/* Sets a string member that the request leaves out. */
static bool default_string(cJSON *object, const char *name, const char *value) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return (item && !cJSON_IsNull(item)) || cJSON_AddStringToObject(object, name, value) != NULL;
}

static ComposerStatus validate_motive(const cJSON *request, cJSON *filled, ComposerError *error) {
    Motive motive;
    MotiveOptions options;
    ComposerStatus status = api_read_motive(request, "", &motive, &options, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    PlanMeter meter = motive_meter(&motive);
    char text[32];
    snprintf(text, sizeof text, "%d/%d", meter.beats_per_bar, meter.beat_unit);
    bool ok = default_string(filled, "meter", text) && default_string(filled, "role", motive_role_name(motive.accompaniment));
    cJSON *input = cJSON_GetObjectItemCaseSensitive(filled, "input");
    if (ok && cJSON_IsObject(input)) {
        ok = default_string(input, "format", "auto") && default_string(input, "form", "full");
    }
    if (ok && options.has_harmony) {
        cJSON_DeleteItemFromObjectCaseSensitive(filled, "harmony");
        cJSON *list = cJSON_AddArrayToObject(filled, "harmony");
        ok = list != NULL;
        for (size_t i = 0; ok && i < options.harmony_count; i++) {
            char numeral[MOTIVE_CHORD_NUMERAL_MAX];
            char onset[32];
            cJSON *chord = cJSON_CreateObject();
            ok = chord && motive_chord_numeral(&options.harmony[i], numeral, sizeof numeral) &&
                 onset_text(options.harmony[i].onset, meter.beat_unit, onset, sizeof onset) &&
                 cJSON_AddStringToObject(chord, "numeral", numeral) && cJSON_AddStringToObject(chord, "onset", onset) &&
                 cJSON_AddItemToArray(list, chord);
            if (!ok) {
                cJSON_Delete(chord);
            }
        }
    }
    return ok ? COMPOSER_OK : fail_alloc(error);
}

ComposerStatus composer_api_validate(const char *kind, const char *request_json, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    ComposerError *error = &out->error;
    cJSON *request = NULL;
    cJSON *filled = NULL;
    cJSON *root = NULL;
    ComposerStatus status = kind ? COMPOSER_OK : api_fail(error, COMPOSER_ERR_REQUEST, "/kind", "kind is missing");
    if (status == COMPOSER_OK && strcmp(kind, "motive") != 0 && strcmp(kind, "theme") != 0 && strcmp(kind, "piece") != 0 &&
        strcmp(kind, "random_motive") != 0) {
        status = api_fail(error, COMPOSER_ERR_REQUEST, "/kind", "kind must be motive, theme, piece or random_motive, not '%s'",
                          kind);
    }
    if (status == COMPOSER_OK) {
        status = api_parse_request(request_json, &request, error);
    }
    if (status == COMPOSER_OK) {
        filled = cJSON_Duplicate(request, true);
        root = cJSON_CreateObject();
        if (!filled || !root) {
            status = fail_alloc(error);
        }
    }
    if (status == COMPOSER_OK) {
        if (strcmp(kind, "motive") == 0) {
            status = validate_motive(request, filled, error);
        } else if (strcmp(kind, "theme") == 0) {
            status = api_theme_validate(request, error);
        } else if (strcmp(kind, "piece") == 0) {
            status = api_piece_validate(request, filled, error);
        } else {
            status = api_random_validate(request, filled, error);
        }
    }
    if (status == COMPOSER_OK && !cJSON_AddItemToObject(root, "request", filled)) {
        status = fail_alloc(error);
    } else if (status == COMPOSER_OK) {
        filled = NULL;
    }
    cJSON_Delete(filled);
    cJSON_Delete(request);
    if (status != COMPOSER_OK) {
        cJSON_Delete(root);
        return status;
    }
    return api_respond_json(out, root);
}
