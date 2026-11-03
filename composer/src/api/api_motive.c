#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_tones.h>

#include "api_internal.h"

/* "3=passing": the 1-based note and its role (the --tone option's value). */
static ComposerStatus read_tone(const char *text, size_t note_count, const char *path, MotiveToneOverride *out,
                                ComposerError *error) {
    char *end = NULL;
    long note = strtol(text, &end, 10);
    if (end == text || *end != '=' || note < 1 || (unsigned long)note > note_count) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path,
                        "tone '%s': write the note, 1 to %zu, then = and a role, as 3=passing", text, note_count);
    }
    const char *role = end + 1;
    out->note = (size_t)note - 1;
    if (!motive_tone_role_parse(role, &out->role, &out->type)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, path,
                        "tone '%s': the role is chord, passing, neighbor, appoggiatura, suspension, retardation, escape, "
                        "anticipation, double-neighbor, changing-tone or pedal, not '%s'",
                        text, role);
    }
    return COMPOSER_OK;
}

static ComposerStatus read_tones(const cJSON *request, size_t note_count, MotiveToneOverride *overrides, size_t *count,
                                 ComposerError *error) {
    *count = 0;
    const cJSON *tones = cJSON_GetObjectItemCaseSensitive(request, "tones");
    if (!tones || cJSON_IsNull(tones)) {
        return COMPOSER_OK;
    }
    if (!cJSON_IsArray(tones)) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/tones", "tones must be a list of strings like \"3=passing\"");
    }
    if ((size_t)cJSON_GetArraySize(tones) > MOTIVE_MAX_NOTES) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/tones", "at most %d tone options", MOTIVE_MAX_NOTES);
    }
    for (int i = 0; i < cJSON_GetArraySize(tones); i++) {
        const cJSON *item = cJSON_GetArrayItem(tones, i);
        char path[48];
        snprintf(path, sizeof path, "/tones/%d", i);
        if (!cJSON_IsString(item)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, path, "tone must be a string like \"3=passing\"");
        }
        ComposerStatus status = read_tone(item->valuestring, note_count, path, &overrides[i], error);
        if (status != COMPOSER_OK) {
            return status;
        }
        (*count)++;
    }
    return COMPOSER_OK;
}

/* The motive's input: text in the request, or the bytes of a MIDI file when bytes is not NULL. */
static ComposerStatus analyze_request(const cJSON *request, const unsigned char *bytes, size_t size,
                                      const ComposerJob *job, ComposerResponse *out) {
    if (composer_job_step(job, "motive", 0, 1)) {
        return api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
    }
    Motive motive;
    ComposerStatus status = bytes ? api_read_motive_bytes(request, "", bytes, size, &motive, NULL, &out->error)
                                  : api_read_motive(request, "", &motive, NULL, &out->error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (!motive_harmony_bar_limit_ok(&motive)) {
        return api_fail(&out->error, COMPOSER_ERR_REQUEST, "/harmony", "at most %d harmonies per bar",
                        MOTIVE_MAX_CHORDS_PER_BAR);
    }
    MotiveToneOverride overrides[MOTIVE_MAX_NOTES];
    size_t override_count = 0;
    status = read_tones(request, motive.note_count, overrides, &override_count, &out->error);
    if (status != COMPOSER_OK) {
        return status;
    }
    MotiveAnalysis analysis;
    if (override_count > 0) {
        MotiveAnalysis plain;
        if (motive_analyze(&motive, &plain) && !plain.tones_known) {
            return api_fail(&out->error, COMPOSER_ERR_REQUEST, "/tones",
                            "tone needs a motive with a key and pitches to label against its base harmony");
        }
    }
    if (!motive_analyze_with_tones(&motive, overrides, override_count, &analysis)) {
        return api_fail(&out->error, COMPOSER_ERR_UNSATISFIABLE, "", "cannot analyze the input");
    }
    out->json = motive_analysis_to_json(&motive, &analysis);
    return out->json ? COMPOSER_OK : api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

static ComposerStatus run_request(const char *request_json, const unsigned char *bytes, size_t size,
                                  const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *request = NULL;
    ComposerStatus status = api_parse_request(request_json, &request, &out->error);
    if (status != COMPOSER_OK) {
        return status;
    }
    status = analyze_request(request, bytes, size, job, out);
    cJSON_Delete(request);
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}

ComposerStatus composer_api_motive(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    return run_request(request_json, NULL, 0, job, out);
}

ComposerStatus composer_api_motive_from_midi(const uint8_t *bytes, size_t size, const char *request_json,
                                             const ComposerJob *job, ComposerResponse *out) {
    if (!bytes) {
        memset(out, 0, sizeof *out);
        return api_fail(&out->error, COMPOSER_ERR_REQUEST, "", "the MIDI bytes are missing");
    }
    return run_request(request_json, bytes, size, job, out);
}
