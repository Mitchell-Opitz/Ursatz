#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/vocabulary/texture_rules.h>

#include <ursatz_composer/explain.h>
#include <plan/plan_json.h>
#include <ursatz_composer/render.h>

#include "api_grow.h"
#include "api_construction.h"
#include "api_seed.h"
#include "api_style.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_workflow.h"
#include "structure/request_parse.h"
#include "util/util.h"

/* `grow` as a composer API entry (docs/cli-grow.md), and the helpers the entries that seed from a corpus or read a style share. */

#define WITHOUT_MAX 8

/* --- the style categories --------------------------------------------------------------------------------- */

/* --- seeding from a corpus ------------------------------------------------------------------------------- */

/* --- the request ----------------------------------------------------------------------------------------- */

typedef struct {
    GrowBriefText text;
    char bars_text[16];
    const char *style;
    const char *blend;
    const char *blend_weight;
    const char *blend_weights;
    const char *without[WITHOUT_MAX];
} GrowRequestText;

typedef struct {
    cJSON *request;
    cJSON *line;                       /* the request with the key resolved, which the motive is read from */
    cJSON *accompaniment_line;         /* the accompaniment motive's request */
    GrowRequestText brief;
    GrowSetup setup;
    ComposerStyle *style;
    Motive motive;
    Motive accompaniment_motive;
    GrowRun run;
    bool ran;
    char stale[1200];
} State;

static bool string_or_number(const cJSON *request, const char *name, char *buffer, size_t size, const char **out,
                             UrsatzError *error) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(request, name);
    *out = NULL;
    if (!item || cJSON_IsNull(item)) {
        return true;
    }
    if (cJSON_IsString(item)) {
        *out = item->valuestring;
    } else if (cJSON_IsNumber(item)) {
        snprintf(buffer, size, "%g", item->valuedouble);
        *out = buffer;
    } else {
        char path[sizeof error->path];
        snprintf(path, sizeof path, "/%s", name);
        api_fail(error, URSATZ_ERR_REQUEST, path, "%s must be text or a number", name);
        return false;
    }
    return true;
}

/* The members of the brief as the text the options of grow take. */
static UrsatzStatus read_brief(const cJSON *request, GrowRequestText *brief, UrsatzError *error) {
    GrowBriefText *text = &brief->text;
    const char *weight = NULL;
    bool voicing = false;
    bool found;
    memset(brief, 0, sizeof(*brief));
    if (!api_member_string(request, "into", &text->into, &found, error) ||
        !api_member_string(request, "hybrid", &text->hybrid, &found, error) ||
        !api_member_string(request, "key", &text->key, &found, error) ||
        !api_read_material(request, NULL, false, &text->material, &text->material_member, error) ||
        !api_member_string(request, "meter", &text->meter, &found, error) ||
        !api_member_string(request, "cadence", &text->cadence, &found, error) ||
        !api_member_string(request, "first_cadence", &text->first_cadence, &found, error) ||
        !api_member_string(request, "accompaniment", &text->accompaniment, &found, error) ||
        !api_member_string(request, "texture", &text->texture, &found, error) ||
        !api_member_bool(request, "voicing", &voicing, &found, error) ||
        !api_member_string(request, "style", &brief->style, &found, error) ||
        !api_member_string(request, "style_weight", &weight, &found, error) ||
        !api_member_string(request, "blend", &brief->blend, &found, error) ||
        !api_member_string(request, "blend_weight", &brief->blend_weight, &found, error) ||
        !api_member_string(request, "blend_weights", &brief->blend_weights, &found, error) ||
        !string_or_number(request, "bars", brief->bars_text, sizeof brief->bars_text, &text->bars, error)) {
        return URSATZ_ERR_REQUEST;
    }
    text->voicing = voicing;
    text->has_style = brief->style != NULL;
    text->style_weight = weight;
    text->accompaniment_motive = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(request, "accompaniment_motive"));
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(request, "style_without");
    if (without && !cJSON_IsNull(without)) {
        if (!cJSON_IsArray(without)) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/style_without", "style_without must be a list of categories");
        }
        const cJSON *category = NULL;
        cJSON_ArrayForEach(category, without) {
            if (!cJSON_IsString(category)) {
                return api_fail(error, URSATZ_ERR_REQUEST, "/style_without", "style_without must be a list of categories");
            }
            UrsatzStatus added = api_style_without_add(brief->without, &text->without_count, WITHOUT_MAX,
                                                         category->valuestring, error);
            if (added != URSATZ_OK) {
                return added;
            }
        }
    }
    return URSATZ_OK;
}

/* The motive request with the key the brief settled on, and the meter and the role of an accompaniment motive. */
static UrsatzStatus make_lines(State *state, UrsatzError *error) {
    char key[24];
    util_key_name(spelled_key_tonic_pitch_class(&state->setup.key), state->setup.key.material, key,
                  sizeof key);
    state->line = cJSON_Duplicate(state->request, true);
    if (!state->line) {
        return api_fail_alloc(error);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(state->line, "key");
    if (!cJSON_AddStringToObject(state->line, "key", key)) {
        return api_fail_alloc(error);
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(state->request, "accompaniment_motive");
    if (cJSON_IsObject(accompaniment)) {
        state->accompaniment_line = cJSON_Duplicate(accompaniment, true);
        if (!state->accompaniment_line) {
            return api_fail_alloc(error);
        }
        cJSON_DeleteItemFromObjectCaseSensitive(state->accompaniment_line, "meter");
        cJSON_DeleteItemFromObjectCaseSensitive(state->accompaniment_line, "role");
        if (!cJSON_AddStringToObject(state->accompaniment_line, "meter", state->brief.text.meter) ||
            !cJSON_AddStringToObject(state->accompaniment_line, "role", "accompaniment")) {
            return api_fail_alloc(error);
        }
    }
    return URSATZ_OK;
}

static UrsatzStatus grow_prepare(State *state, const UrsatzJob *job, ApiOutputOptions *outputs, UrsatzError *error) {
    const cJSON *request = state->request;
    GrowSetup *setup = &state->setup;
    bool found;
    bool check = false;
    unsigned long seed = 0;
    unsigned long suggest = 1;
    const cJSON *passages_member = cJSON_GetObjectItemCaseSensitive(request, "corpus_passages");
    bool has_passages = passages_member && !cJSON_IsNull(passages_member);
    if (cJSON_GetObjectItemCaseSensitive(request, "corpus") || cJSON_GetObjectItemCaseSensitive(request, "like")) {
        return api_fail(error, URSATZ_ERR_REQUEST, cJSON_GetObjectItemCaseSensitive(request, "corpus") ? "/corpus" : "/like",
                        "corpus and like are gone (Composer API 3.0): give corpus_passages, the passages found by "
                        "analyzer_corpus_passages_like");
    }
    bool has_input = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(request, "input"));
    if (has_passages && has_input) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/corpus_passages", "give a motive input or corpus_passages, not both");
    }
    if (!has_passages && !has_input) {
        return api_fail(error, URSATZ_ERR_REQUEST, "/input", "the request needs an input (a motive) or corpus_passages");
    }
    UrsatzStatus status = read_brief(request, &state->brief, error);
    if (status != URSATZ_OK) {
        return status;
    }
    status = api_grow_read_brief(&state->brief.text, setup, error);
    if (status == URSATZ_OK) {
        status = api_grow_read_construction(request, setup, error);
    }
    if (status != URSATZ_OK) {
        return status;
    }
    ComposerStyleWeights weights;
    memset(&weights, 0, sizeof(weights));
    status = api_read_blend(state->brief.style != NULL, state->brief.blend, state->brief.blend_weight, state->brief.blend_weights,
                            &weights, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (!api_member_whole(request, "suggest", 1, GROW_MAX_SUGGESTIONS, &suggest, &found, error)) {
        return URSATZ_ERR_REQUEST;
    }
    setup->suggest = found ? suggest : 1;
    setup->suggested = found;
    bool seeded = false;
    if (!api_member_whole(request, "seed", 0, GROW_SEED_MAX, &seed, &seeded, error) ||
        !api_member_bool(request, "check", &check, &found, error)) {
        return URSATZ_ERR_REQUEST;
    }
    setup->seed = seeded ? seed : api_default_seed();
    status = api_read_output_options(request, outputs, error);
    if (status != URSATZ_OK) {
        return status;
    }
    setup->check = check;
    setup->explain = outputs->explain != 0;
    setup->job = job;

    /* The tempo of the request wins over the style's, and is the tempo of the files. */
    PlanExportOptions tempo_options = plan_export_options_default();
    status = api_read_tempo(request, &tempo_options, error);
    if (status != URSATZ_OK) {
        return status;
    }
    const cJSON *tempo_member = cJSON_GetObjectItemCaseSensitive(request, "tempo");
    bool has_tempo = tempo_member && !cJSON_IsNull(tempo_member);
    if (has_tempo) {
        outputs->export_options.tempo_numerator = tempo_options.tempo_numerator;
        outputs->export_options.tempo_denominator = tempo_options.tempo_denominator;
    }
    if (state->brief.style) {
        status = api_load_blend(state->brief.style, state->brief.blend, &weights, state->brief.without,
                                state->brief.text.without_count, &state->style, error);
        if (status != URSATZ_OK) {
            return status;
        }
    }
    setup->style = state->style;
    setup->without = state->brief.without;
    setup->without_count = state->brief.text.without_count;
    Rational option_tempo = {outputs->export_options.tempo_numerator, outputs->export_options.tempo_denominator};
    status = api_grow_set_tempo_and_mode(setup, has_tempo ? &option_tempo : NULL, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (!has_tempo && setup->tempo.den > 0) {
        outputs->export_options.tempo_numerator = setup->tempo.num;
        outputs->export_options.tempo_denominator = setup->tempo.den;
    }

    status = make_lines(state, error);
    if (status != URSATZ_OK) {
        return status;
    }
    if (has_passages) {
        if (cJSON_GetObjectItemCaseSensitive(request, "harmony") || cJSON_GetObjectItemCaseSensitive(request, "features") ||
            cJSON_GetObjectItemCaseSensitive(request, "role")) {
            return api_fail(error, URSATZ_ERR_REQUEST, "/harmony",
                            "harmony, features and role describe a motive input, not one seeded from a corpus");
        }
        /* The passage is chosen once, by the first seed; the seeds tried after it grow from the same motive. */
        CorpusPassages passages = {0};
        char path[sizeof error->path];
        char message[sizeof error->message];
        if (!corpus_passages_from_json(passages_member, &passages, path, sizeof path, message, sizeof message)) {
            return api_fail(error, URSATZ_ERR_REQUEST, path, "%s", message);
        }
        status = api_seed_motive(setup->knowledge, &passages, &setup->meter, &setup->key, setup->seed, &setup->seed_found, error);
        corpus_passages_destroy(&passages);
        if (status == URSATZ_OK) {
            status = api_seed_source(&setup->seed_found, &setup->source, error);
        }
        if (status != URSATZ_OK) {
            return status;
        }
        api_seed_stale_notice(&setup->seed_found, state->stale, sizeof state->stale);
        setup->seeded = true;
        state->motive = setup->seed_found.motive;
    } else {
        status = api_read_motive(state->line, "", state->setup.knowledge, &state->motive, NULL, error);
        if (status != URSATZ_OK) {
            return status;
        }
    }
    if (state->accompaniment_line) {
        status = api_read_motive(state->accompaniment_line, "/accompaniment_motive", state->setup.knowledge, &state->accompaniment_motive,
                                NULL, error);
        if (status != URSATZ_OK) {
            return status;
        }
        setup->accompaniment_motive = &state->accompaniment_motive;
    }
    setup->motive = &state->motive;
    setup->text = &state->brief.text;
    return URSATZ_OK;
}

/* --- the outputs ----------------------------------------------------------------------------------------- */

/* Adds, to each theme of the document, its plan (with "plans"), its explanation and its MusicXML (when asked). */
static UrsatzStatus add_theme_members(const KnowledgeSnapshot *knowledge, cJSON *document, const GrowRun *run, const ApiOutputOptions *outputs, UrsatzError *error) {
    cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    for (size_t k = 0; k < run->made; k++) {
        cJSON *item = cJSON_GetArrayItem(themes, (int)k);
        const GrowTheme *theme = &run->themes[k];
        if (outputs->plans) {
            cJSON *plan = api_piece_plan_object(knowledge, theme->plan);
            if (!plan || !cJSON_AddItemToObject(item, "plan", plan)) {
                cJSON_Delete(plan);
                return api_fail_alloc(error);
            }
        }
        if (outputs->explain) {
            Explanation explanation;
            ExplainResult built = explain_build_grow(theme->plan, api_find_node(theme->plan, "theme"), &theme->outcome,
                                                     theme->checked ? &theme->check : NULL, run->passed, theme->passed_before,
                                                     &explanation);
            char *text = NULL;
            if (built == EXPLAIN_OK) {
                text = outputs->explain == 2 ? explain_to_json(&explanation) : explain_to_text(&explanation);
                explain_destroy(&explanation);
            }
            if (!text || !cJSON_AddStringToObject(item, "explanation", text)) {
                free(text);
                return api_fail_alloc(error);
            }
            free(text);
        }
        if (outputs->musicxml) {
            RenderedPlan rendered;
            PlanExportError detail;
            char *text = NULL;
            if (plan_export_render(theme->plan, &rendered, &detail) != PLAN_EXPORT_OK) {
                return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/musicxml", "cannot render the plan");
            }
            PlanExportResult written = rendered_plan_musicxml_text(knowledge, &rendered, &outputs->export_options, &text, &detail);
            rendered_plan_destroy(&rendered);
            if (written != PLAN_EXPORT_OK) {
                return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "/musicxml", "cannot write the MusicXML file: %s", detail.message);
            }
            bool ok = cJSON_AddStringToObject(item, "musicxml", text) != NULL;
            free(text);
            if (!ok) {
                return api_fail_alloc(error);
            }
        }
    }
    return URSATZ_OK;
}

/* The plan, the MIDI file and the MusicXML of the first theme. */
static UrsatzStatus render_first(const KnowledgeSnapshot *knowledge, const GrowTheme *theme, const ApiOutputOptions *outputs, cJSON *document,
                                   ComposerResponse *out) {
    ApiOutputs files;
    memset(&files, 0, sizeof(files));
    files.midi = outputs->midi;
    files.musicxml = outputs->musicxml;
    files.layers = outputs->layers;
    files.export_options = outputs->export_options;
    return api_render_outputs(knowledge, theme->plan, &files, document, out);
}

static UrsatzStatus add_notices(cJSON *document, const State *state, const ApiOutputOptions *outputs, UrsatzError *error) {
    (void)outputs;
    char only[96] = "";
    if (state->setup.suggested && state->run.made < state->setup.suggest) {
        snprintf(only, sizeof only, "only %zu different theme%s found for this request", state->run.made,
                 state->run.made == 1 ? "" : "s");
    }
    if (!state->stale[0] && !only[0]) {
        return URSATZ_OK;
    }
    cJSON *notices = cJSON_AddArrayToObject(document, "notices");
    if (!notices || (state->stale[0] && !cJSON_AddItemToArray(notices, cJSON_CreateString(state->stale))) ||
        (only[0] && !cJSON_AddItemToArray(notices, cJSON_CreateString(only)))) {
        return api_fail_alloc(error);
    }
    return URSATZ_OK;
}

/* --- the entry -------------------------------------------------------------------------------------------- */

UrsatzStatus composer_api_grow(const ComposerContext *context, const char *request_json,
                                 const UrsatzJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    CommonPracticeMaterials materials;
    UrsatzStatus context_status = api_require_knowledge(context, &materials, &out->error);
    if (context_status != URSATZ_OK) {
        return context_status;
    }
    State state;
    ApiOutputOptions outputs;
    memset(&state, 0, sizeof(state));
    memset(&outputs, 0, sizeof(outputs));
    state.setup.knowledge = context->knowledge;
    state.setup.materials = &materials;
    UrsatzStatus status = api_parse_request(request_json, &state.request, &out->error);
    if (status == URSATZ_OK) {
        status = grow_prepare(&state, job, &outputs, &out->error);
    }
    if (status == URSATZ_OK) {
        state.ran = true;
        status = api_grow_run(&state.setup, &state.run, &out->error);
    }
    if (status == URSATZ_OK) {
        cJSON *document = api_grow_result_json(&state.setup, &state.run, NULL);
        status = document ? add_theme_members(context->knowledge, document, &state.run, &outputs, &out->error) : api_fail_alloc(&out->error);
        if (status == URSATZ_OK) {
            status = add_notices(document, &state, &outputs, &out->error);
        }
        if (status == URSATZ_OK) {
            status = render_first(context->knowledge, &state.run.themes[0], &outputs, document, out);
        }
        if (status == URSATZ_OK) {
            status = api_respond_json(out, document);
        } else {
            cJSON_Delete(document);
        }
    }
    if (state.ran) {
        api_grow_run_destroy(&state.run);
    }
    composer_style_destroy(state.style);
    cJSON_Delete(state.line);
    cJSON_Delete(state.accompaniment_line);
    cJSON_Delete(state.request);
    if (status != URSATZ_OK) {
        UrsatzError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
