#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer.h>
#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/explain.h>
#include <ursatz_composer/form_figure.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/form_return.h>
#include <ursatz_composer/export.h>
#include <ursatz_composer/piece_reopen.h>
#include <plan/plan_json.h>
#include <plan/plan_state.h>
#include <ursatz_composer/regenerate.h>
#include <ursatz_composer/render.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_workflow.h"
#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "structure/request_parse.h"
#include "util/util.h"
#include <ursatz_composer/piece_plan_keys.h>

/*
 * The piece entry (docs/api.md): the piece request composed, or its loop plan regenerated in one section, with the ending
 * version built from the loop's. The seeds are tried one after another; the plans, the result document, the MIDI file and the
 * MusicXML of the loop version are made in memory.
 */

/* ------------------------------------------------------------- the result */

static GrowBrief make_brief(const PieceSpec *request) {
    GrowBrief brief;
    memset(&brief, 0, sizeof(brief));
    brief.tempo = request->tempo;
    brief.tempo_from = request->tempo_from;
    brief.material = request->request.key.material;
    brief.mode_from = request->mode_from;
    brief.has_density = composer_style_density(request->style, &brief.density);
    brief.without = request->without;
    brief.without_count = request->without_count;
    return brief;
}


/* The explanation of the versions as the command writes it: text with a heading for each, or JSON {"<version>": ...}. */
static char *explanation_text(const PieceSpec *request, const PieceMade *piece, bool json) {
    Explanation explanations[API_PIECE_VERSIONS_MAX];
    char *texts[API_PIECE_VERSIONS_MAX] = {NULL};
    size_t built = 0;
    size_t size = json ? 3 : 1;
    char *joined = NULL;
    bool ok = true;
    for (size_t i = 0; ok && i < piece->version_count; i++) {
        /* A compound form is explained from its first composed part. */
        const PieceParts *parts = piece->versions[i].parts;
        ok = (parts ? explain_build(parts->plans[0], &parts->outcomes[0], &explanations[i])
                    : explain_build(piece->versions[i].plan, &piece->versions[i].outcome, &explanations[i])) == EXPLAIN_OK;
        if (ok) {
            built++;
            texts[i] = json ? explain_to_json(&explanations[i]) : explain_to_text(&explanations[i]);
            ok = texts[i] != NULL;
        }
        if (ok) {
            size += strlen(api_piece_version_name(request, i)) + strlen(texts[i]) + 24;
        }
    }
    joined = ok ? malloc(size) : NULL;
    if (joined) {
        size_t used = 0;
        if (json) {
            joined[used++] = '{';
        }
        for (size_t i = 0; i < piece->version_count; i++) {
            const char *name = api_piece_version_name(request, i);
            used += (size_t)(json ? snprintf(joined + used, size - used, "%s\"%s\":%s", i > 0 ? "," : "", name, texts[i])
                                  : snprintf(joined + used, size - used, "== %s version ==\n%s\n", name, texts[i]));
        }
        snprintf(joined + used, size - used, "%s", json ? "}\n" : "");
    }
    for (size_t i = 0; i < piece->version_count; i++) {
        free(texts[i]);
    }
    for (size_t i = 0; i < built; i++) {
        explain_destroy(&explanations[i]);
    }
    return joined;
}

/* {"bars": n} of the performed piece (R-485), or null for a piece of a form with sizes that is not performed. */
static cJSON *performed_json(const PieceSpec *request, const PieceMade *piece) {
    const PieceVersion *version = &piece->versions[0];
    if (!version->performed || !version->performance.plan || request->request.meter.beats_per_bar <= 0) {
        return cJSON_CreateNull();
    }
    PlanSpan span = plan_node_span(version->performance.plan, plan_root(version->performance.plan));
    cJSON *object = cJSON_CreateObject();
    if (!object || !cJSON_AddNumberToObject(object, "bars", api_piece_span_beats(request, span.length) / request->request.meter.beats_per_bar)) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/*
 * The result document: {schema, seed, key, meter, brief, loop, performed, [regenerated,] versions: {<name>: ...}}, the versions named as
 * api_piece_version_names has them ("loop" and "ending", or "piece"), as the command prints it, with no plan paths (the plans
 * are in "plans") and no files.
 */
cJSON *api_piece_document(const PieceSpec *request, const PieceMade *piece, const PieceEdit *edit,
                             const cJSON *regenerate_section, bool from) {
    char key[32];
    char lanes[64];
    char meter[24];
    GrowBrief brief = make_brief(request);
    util_key_name(spelled_key_tonic_pitch_class(&request->request.key), request->request.key.material,
                  key, sizeof key);
    snprintf(meter, sizeof meter, "%d/%d", request->request.meter.beats_per_bar, request->request.meter.beat_unit);
    cJSON *document = cJSON_CreateObject();
    cJSON *loop_region = api_piece_loop_json(piece->versions[0].plan);
    bool ok = document && cJSON_AddStringToObject(document, "schema", "ursatz-piece") &&
              cJSON_AddNumberToObject(document, "seed", (double)piece->seed) && cJSON_AddStringToObject(document, "key", key) &&
              cJSON_AddStringToObject(document, "meter", meter) && api_add(document, "brief", api_brief_json(&brief)) &&
              api_add(document, "loop", loop_region ? loop_region : cJSON_CreateNull()) &&
              /* The game loop's document is as it was: only a form with sizes says whether it is performed. */
              (request->form->request_sections || api_add(document, "performed", performed_json(request, piece)));
    if (ok && from) {
        cJSON *regenerated = cJSON_CreateNull();
        if (regenerate_section) {
            api_piece_lanes_name(edit, lanes, sizeof lanes);
            cJSON_Delete(regenerated);
            regenerated = cJSON_CreateObject();
            if (!regenerated || !cJSON_AddStringToObject(regenerated, "section", regenerate_section->valuestring) ||
                !cJSON_AddStringToObject(regenerated, "lane", lanes)) {
                cJSON_Delete(regenerated);
                regenerated = NULL;
            }
        }
        ok = api_add(document, "regenerated", regenerated);
    }
    cJSON *versions = cJSON_CreateObject();
    for (size_t i = 0; ok && i < piece->version_count; i++) {
        ok = api_add(versions, api_piece_version_name(request, i), api_piece_version_json(request, &piece->versions[i]));
    }
    ok = ok && api_add(document, "versions", versions);
    if (!ok) {
        cJSON_Delete(versions);
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

/* The loop version's MIDI file and MusicXML: the MIDI file has the loopStart and loopEnd markers. */
ComposerStatus api_piece_render_loop(const KnowledgeSnapshot *knowledge, const PieceSpec *request, const PieceMade *piece, const ApiPieceOutputs *outputs,
                                  ComposerResponse *out) {
    if (!outputs->midi && !outputs->musicxml) {
        return COMPOSER_OK;
    }
    PlanExportOptions loop_options = request->export_options;
    Rational start;
    Rational end;
    const Plan *output = api_piece_version_output_plan(&piece->versions[0]);
    if (api_piece_loop_region(output, &start, &end)) {
        loop_options.loop_markers = true;
        loop_options.loop_start = start;
        loop_options.loop_end = end;
    }
    RenderedPlan rendered;
    PlanExportError detail;
    if (plan_export_render(output, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(&out->error, COMPOSER_ERR_REQUEST, outputs->midi ? "/midi" : "/musicxml", "cannot render the plan");
    }
    ComposerStatus status = COMPOSER_OK;
    if (outputs->midi &&
        rendered_plan_midi_bytes(knowledge, &rendered, &loop_options, &out->midi, &out->midi_size, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/midi", "cannot write the MIDI file: %s", detail.message);
    }
    if (status == COMPOSER_OK && outputs->musicxml &&
        rendered_plan_musicxml_text(knowledge, &rendered, &request->export_options, &out->musicxml, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(&out->error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot write the MusicXML: %s", detail.message);
    }
    rendered_plan_destroy(&rendered);
    return status;
}

ComposerStatus api_piece_musicxml(const KnowledgeSnapshot *knowledge, const PieceSpec *request, const Plan *plan, char **out, ComposerError *error) {
    RenderedPlan rendered;
    PlanExportError detail;
    *out = NULL;
    if (plan_export_render(plan, &rendered, &detail) != PLAN_EXPORT_OK) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot render the plan");
    }
    ComposerStatus status = COMPOSER_OK;
    if (rendered_plan_musicxml_text(knowledge, &rendered, &request->export_options, out, &detail) != PLAN_EXPORT_OK) {
        status = api_fail(error, COMPOSER_ERR_REQUEST, "/musicxml", "cannot write the MusicXML: %s", detail.message);
    }
    rendered_plan_destroy(&rendered);
    return status;
}

/*
 * With layers, the realization and the reading of version 0's plan are added as well. Adds "plans": {<name>: ...} for each version, with an explanation "explanation", with ending_musicxml the ending version's
 * MusicXML as "ending_musicxml", and with musicxml_versions the MusicXML of every version after the first as
 * "musicxml_versions": {<name>: ...} to the document.
 */
ComposerStatus api_piece_add_outputs(const KnowledgeSnapshot *knowledge, cJSON *document, const PieceSpec *request, const PieceMade *piece, const ApiPieceOutputs *outputs,
                                  ComposerResponse *out) {
    cJSON *plans = cJSON_CreateObject();
    bool plans_ok = plans != NULL;
    for (size_t i = 0; plans_ok && i < piece->version_count; i++) {
        plans_ok = api_add(plans, api_piece_version_name(request, i), api_piece_plan_object(knowledge, api_piece_version_output_plan(&piece->versions[i])));
    }
    if (!plans_ok || !api_add(document, "plans", plans)) {
        cJSON_Delete(plans);
        return api_fail_alloc(&out->error);
    }
    if (outputs->explain) {
        char *text = explanation_text(request, piece, outputs->explain == 2);
        if (!text || !cJSON_AddStringToObject(document, "explanation", text)) {
            free(text);
            return api_fail(&out->error, COMPOSER_ERR_INTERNAL, "", "cannot write the explanation");
        }
        free(text);
    }
    if (outputs->musicxml_versions) {
        cJSON *musicxml = cJSON_CreateObject();
        for (size_t i = 1; musicxml && i < piece->version_count; i++) {
            char *text = NULL;
            ComposerStatus status = api_piece_musicxml(knowledge, request, api_piece_version_output_plan(&piece->versions[i]), &text, &out->error);
            if (status != COMPOSER_OK) {
                cJSON_Delete(musicxml);
                return status;
            }
            bool added = cJSON_AddStringToObject(musicxml, api_piece_version_name(request, i), text) != NULL;
            free(text);
            if (!added) {
                cJSON_Delete(musicxml);
                musicxml = NULL;
            }
        }
        if (!api_add(document, "musicxml_versions", musicxml)) {
            return api_fail_alloc(&out->error);
        }
    }
    if (outputs->ending_musicxml) {
        char *text = NULL;
        ComposerStatus status = api_piece_musicxml(knowledge, request, api_piece_version_output_plan(&piece->versions[1]), &text, &out->error);
        if (status != COMPOSER_OK) {
            return status;
        }
        bool added = cJSON_AddStringToObject(document, "ending_musicxml", text) != NULL;
        free(text);
        if (!added) {
            return api_fail_alloc(&out->error);
        }
    }
    if (outputs->layers) {
        ComposerStatus layered = api_layers_json(knowledge, api_piece_version_output_plan(&piece->versions[0]), &request->export_options,
                                                 document, &out->error);
        if (layered != COMPOSER_OK) {
            return layered;
        }
    }
    PlanJsonError detail;
    PlanProducer producer = composer_plan_producer(knowledge);
    out->plan_json = plan_to_json(api_piece_version_output_plan(&piece->versions[0]), &producer, &detail);
    if (!out->plan_json) {
        return api_fail(&out->error, COMPOSER_ERR_INTERNAL, "", "cannot write the plan: %s", detail.message);
    }
    return COMPOSER_OK;
}

