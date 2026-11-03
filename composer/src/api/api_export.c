#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

#include "api_internal.h"
#include "util/plan_util.h"

static const char *skip_reason_text(RenderSkipReason reason) {
    return reason == RENDER_SKIP_NOT_REALIZED ? "not Given or Realized"
           : reason == RENDER_SKIP_MATERIAL   ? "source material"
           : reason == RENDER_SKIP_LAYER      ? "not in the layer"
           : reason == RENDER_SKIP_SEAM       ? "seam"
                                              : "lane is not rendered yet";
}

static ComposerStatus export_failure(ComposerError *error, PlanExportResult result, const char *what,
                                     const PlanExportError *detail) {
    ComposerStatus status = result == PLAN_EXPORT_ERR_ALLOC ? COMPOSER_ERR_ALLOC
                            : result == PLAN_EXPORT_ERR_NO_CONTENT || result == PLAN_EXPORT_ERR_NOT_REPRESENTABLE ||
                                    result == PLAN_EXPORT_ERR_TOO_MANY_VOICES
                                ? COMPOSER_ERR_REQUEST
                                : COMPOSER_ERR_INTERNAL;
    return api_fail(error, status, status == COMPOSER_ERR_REQUEST ? "/plan" : "", "%s%s%s", what,
                    detail && detail->message[0] ? ": " : "", detail ? detail->message : "");
}

ComposerStatus composer_api_export(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    cJSON *request = NULL;
    ComposerStatus status = api_parse_request(request_json, &request, &out->error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (composer_job_step(job, "export", 0, 1)) {
        cJSON_Delete(request);
        return api_fail(&out->error, COMPOSER_ERR_CANCELLED, "", "cancelled");
    }

    const cJSON *plan_member = cJSON_GetObjectItemCaseSensitive(request, "plan");
    const cJSON *midi_member = cJSON_GetObjectItemCaseSensitive(request, "midi");
    const cJSON *musicxml_member = cJSON_GetObjectItemCaseSensitive(request, "musicxml");
    const cJSON *plain_member = cJSON_GetObjectItemCaseSensitive(request, "plain_midi");
    PlanExportOptions options = plan_export_options_default();
    if (!cJSON_IsObject(plan_member)) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/plan", "plan must be a plan object");
    } else if ((midi_member && !cJSON_IsBool(midi_member)) || (musicxml_member && !cJSON_IsBool(musicxml_member)) ||
               (plain_member && !cJSON_IsBool(plain_member))) {
        const char *path = midi_member && !cJSON_IsBool(midi_member)       ? "/midi"
                           : musicxml_member && !cJSON_IsBool(musicxml_member) ? "/musicxml"
                                                                               : "/plain_midi";
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, path, "%s must be true or false", path + 1);
    } else {
        ComposerRatio tempo;
        bool has_tempo = false;
        if (!api_member_rational(request, "tempo", &tempo, &has_tempo, &out->error)) {
            status = COMPOSER_ERR_REQUEST;
        } else if (has_tempo && (tempo.denominator == 0 || tempo.numerator < 20UL * tempo.denominator ||
                                 tempo.numerator > 400UL * tempo.denominator)) {
            status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/tempo",
                              "tempo must be from 20 to 400 quarter notes per minute, a whole number or a fraction like 195/2");
        } else if (has_tempo) {
            options.tempo_numerator = tempo.numerator;
            options.tempo_denominator = tempo.denominator;
        }
    }
    if (status != COMPOSER_OK) {
        cJSON_Delete(request);
        return status;
    }
    if (cJSON_IsTrue(plain_member)) {
        options.track_names = false;
        options.section_markers = false;
    }
    bool want_midi = cJSON_IsTrue(midi_member);
    bool want_musicxml = cJSON_IsTrue(musicxml_member);

    char *plan_text = cJSON_PrintUnformatted(plan_member);
    cJSON_Delete(request);
    if (!plan_text) {
        return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    }
    PiecePlan *plan = NULL;
    PlanJsonError load_error;
    PlanResult loaded = plan_from_json(plan_text, &plan, &load_error);
    free(plan_text);
    if (loaded != PLAN_OK) {
        if (loaded == PLAN_ERR_ALLOC) {
            return api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
        }
        /* "form.children[1].span" is "/plan/form/children/1/span". */
        char pointer[sizeof out->error.path];
        size_t length = (size_t)snprintf(pointer, sizeof pointer, "/plan%s", load_error.path[0] ? "/" : "");
        for (const char *c = load_error.path; *c && length + 1 < sizeof pointer; c++) {
            if (*c == ']') {
                continue;
            }
            pointer[length++] = *c == '.' || *c == '[' ? '/' : *c;
        }
        pointer[length] = '\0';
        return api_fail(&out->error, COMPOSER_ERR_REQUEST, pointer, "cannot read plan: %s%s%s", load_error.message,
                        load_error.path[0] ? " at " : "", load_error.path);
    }

    RenderedPlan rendered;
    PlanExportError detail;
    PlanExportResult result = plan_export_render(plan, &rendered, &detail);
    plan_destroy(plan);
    if (result != PLAN_EXPORT_OK) {
        return export_failure(&out->error, result, "cannot render plan", NULL);
    }

    if (want_midi) {
        result = rendered_plan_midi_bytes(&rendered, &options, &out->midi, &out->midi_size, &detail);
        if (result != PLAN_EXPORT_OK) {
            status = export_failure(&out->error, result, "MIDI export failed", &detail);
        }
    }
    if (status == COMPOSER_OK && want_musicxml) {
        result = rendered_plan_musicxml_text(&rendered, &options, &out->musicxml, &detail);
        if (result != PLAN_EXPORT_OK) {
            status = export_failure(&out->error, result, "MusicXML export failed", &detail);
        }
    }

    cJSON *response = status == COMPOSER_OK ? cJSON_CreateObject() : NULL;
    cJSON *skipped = response ? cJSON_AddArrayToObject(response, "skipped") : NULL;
    for (size_t i = 0; skipped && i < rendered.skipped_count; i++) {
        const RenderSkip *skip = &rendered.skipped[i];
        cJSON *item = cJSON_CreateObject();
        if (!item || !cJSON_AddNumberToObject(item, "span", (double)skip->span) ||
            !cJSON_AddStringToObject(item, "lane", plan_lane_name(skip->lane)) ||
            !cJSON_AddStringToObject(item, "reason", skip_reason_text(skip->reason)) ||
            !cJSON_AddItemToArray(skipped, item)) {
            cJSON_Delete(item);
            skipped = NULL;
        }
    }
    rendered_plan_destroy(&rendered);
    if (status == COMPOSER_OK && !skipped) {
        cJSON_Delete(response);
        status = api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
    } else if (status == COMPOSER_OK) {
        status = api_respond_json(out, response);
    }
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
