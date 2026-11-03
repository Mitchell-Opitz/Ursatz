#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-texture/texture_rules.h>

#include <ursatz_composer/explain.h>
#include <ursatz_composer/plan_json.h>
#include <ursatz_composer/render.h>

#include "api_grow.h"
#include "api_internal.h"
#include "api_piece.h"
#include "api_workflow.h"
#include "util/request_parse.h"
#include "util/util.h"

/* `grow` as a composer API entry (docs/cli-grow.md), and the helpers the entries that seed from a corpus or read a style share. */

#define WITHOUT_MAX 8

static ComposerStatus fail_alloc(ComposerError *error) {
    return api_fail(error, COMPOSER_ERR_ALLOC, "", "out of memory");
}

/* --- the style categories --------------------------------------------------------------------------------- */

static const char *const STYLE_CATEGORIES[] = {
    STYLE_CATEGORY_CHORD_NUMERAL,     STYLE_CATEGORY_CHORD_PROGRESSION_2, STYLE_CATEGORY_CHORD_PROGRESSION_3,
    STYLE_CATEGORY_CADENCE_TYPE,      STYLE_CATEGORY_HARMONIC_RHYTHM,     STYLE_CATEGORY_DURATION,
    STYLE_CATEGORY_METER,             STYLE_CATEGORY_SYNCOPATION,         STYLE_CATEGORY_MELODIC_INTERVAL,
    STYLE_CATEGORY_CONTOUR,           STYLE_CATEGORY_PHRASE_RANGE,        STYLE_CATEGORY_MOTIVE_DEGREES,
    STYLE_CATEGORY_MOTIVE_INTERVALS,  STYLE_CATEGORY_MOTIVE_RHYTHM,       STYLE_CATEGORY_MOTIVE_NCT_FIGURE,
    STYLE_CATEGORY_THEME_FORM,        STYLE_CATEGORY_PHRASE_LENGTH,       STYLE_CATEGORY_SECTION_LETTERS,
    STYLE_CATEGORY_MODE,              STYLE_CATEGORY_TEMPO,               STYLE_CATEGORY_NOTE_DENSITY};

ComposerStatus api_style_category_check(const char *name, ComposerError *error) {
    bool known = false;
    for (size_t i = 0; i < sizeof STYLE_CATEGORIES / sizeof *STYLE_CATEGORIES; i++) {
        known = known || strcmp(name, STYLE_CATEGORIES[i]) == 0;
    }
    if (!known) {
        char list[640];
        size_t used = 0;
        list[0] = '\0';
        for (size_t i = 0; i < sizeof STYLE_CATEGORIES / sizeof *STYLE_CATEGORIES && used < sizeof list; i++) {
            int written = snprintf(list + used, sizeof list - used, "%s%s", i ? ", " : "", STYLE_CATEGORIES[i]);
            used += written > 0 ? (size_t)written : 0;
        }
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without",
                        "style_without: '%s' is not a style category (the categories are %s)", name, list);
    }
    return COMPOSER_OK;
}

ComposerStatus api_style_without_add(const char **names, size_t *count, size_t capacity, const char *name,
                                     ComposerError *error) {
    ComposerStatus checked = api_style_category_check(name, error);
    if (checked != COMPOSER_OK) {
        return checked;
    }
    for (size_t i = 0; i < *count; i++) {
        if (strcmp(names[i], name) == 0) {
            return COMPOSER_OK;
        }
    }
    if (*count >= capacity) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without is given at most %zu times", capacity);
    }
    names[(*count)++] = name;
    return COMPOSER_OK;
}

/* --- seeding from a corpus ------------------------------------------------------------------------------- */

static ComposerStatus seed_failure(CorpusSeedResult result, const char *corpus_dir, const char *what, ComposerError *error) {
    switch (result) {
    case CORPUS_SEED_ERR_NO_MATCH:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/like",
                        "no passage of the corpus '%s' is similar to %s (a score of 1/2 or more)", corpus_dir, what);
    case CORPUS_SEED_ERR_QUERY:
        return api_fail(error, COMPOSER_ERR_REQUEST, "/like", "cannot read %s as a passage to look for", what);
    case CORPUS_SEED_ERR_CORPUS:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/corpus",
                        "cannot read the corpus '%s' (the output directory of ursatz-analyzer --corpus)", corpus_dir);
    default:
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/like", "%s", corpus_seed_result_name(result));
    }
}

ComposerStatus api_seed_source(const CorpusSeed *seed, PlanSource *out, ComposerError *error) {
    memset(out, 0, sizeof(*out));
    if (snprintf(out->piece, PLAN_SOURCE_PIECE_MAX, "%s", seed->piece) >= PLAN_SOURCE_PIECE_MAX) {
        return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "", "the source piece's path is too long to record");
    }
    out->start_measure = seed->start_measure;
    out->start_beat = seed->start_beat;
    out->end_measure = seed->end_measure;
    out->end_beat = seed->end_beat;
    out->score_numerator = seed->score_numerator;
    out->score_denominator = seed->score_denominator;
    return COMPOSER_OK;
}

ComposerStatus api_seed_motive(const char *corpus_dir, const char *like_path, const PlanMeter *meter, const AnalysisKey *key,
                               unsigned long seed, CorpusSeed *out, ComposerError *error) {
    CorpusSeedResult result = corpus_seed_motive(corpus_dir, like_path, meter, key, seed, out);
    return result == CORPUS_SEED_OK ? COMPOSER_OK : seed_failure(result, corpus_dir, like_path, error);
}

void api_seed_stale_notice(const CorpusSeed *seed, const char *corpus_dir, char *out, size_t size) {
    out[0] = '\0';
    if (seed->stale_documents > 0) {
        snprintf(out, size, "%zu stale documents in '%s' (analyzed by another version; run the corpus again)",
                 seed->stale_documents, corpus_dir);
    }
}

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
    bool midi;
    bool musicxml;
    bool plans;
    int explain; /* 0 none, 1 text, 2 json */
    PlanExportOptions export_options;
} Outputs;

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
                             ComposerError *error) {
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
        api_fail(error, COMPOSER_ERR_REQUEST, path, "%s must be text or a number", name);
        return false;
    }
    return true;
}

/* The members of the brief as the text the options of grow take. */
static ComposerStatus read_brief(const cJSON *request, GrowRequestText *brief, ComposerError *error) {
    GrowBriefText *text = &brief->text;
    const char *weight = NULL;
    bool voicing = false;
    bool found;
    memset(brief, 0, sizeof(*brief));
    if (!api_member_string(request, "into", &text->into, &found, error) ||
        !api_member_string(request, "hybrid", &text->hybrid, &found, error) ||
        !api_member_string(request, "key", &text->key, &found, error) ||
        !api_member_string(request, "mode", &text->mode, &found, error) ||
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
        return COMPOSER_ERR_REQUEST;
    }
    text->voicing = voicing;
    text->has_style = brief->style != NULL;
    text->style_weight = weight;
    text->accompaniment_motive = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(request, "accompaniment_motive"));
    const cJSON *without = cJSON_GetObjectItemCaseSensitive(request, "style_without");
    if (without && !cJSON_IsNull(without)) {
        if (!cJSON_IsArray(without)) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without must be a list of categories");
        }
        const cJSON *category = NULL;
        cJSON_ArrayForEach(category, without) {
            if (!cJSON_IsString(category)) {
                return api_fail(error, COMPOSER_ERR_REQUEST, "/style_without", "style_without must be a list of categories");
            }
            ComposerStatus added = api_style_without_add(brief->without, &text->without_count, WITHOUT_MAX,
                                                         category->valuestring, error);
            if (added != COMPOSER_OK) {
                return added;
            }
        }
    }
    return COMPOSER_OK;
}

static ComposerStatus read_outputs(const cJSON *request, Outputs *outputs, ComposerError *error) {
    bool found;
    bool plain = false;
    const char *explain = NULL;
    memset(outputs, 0, sizeof(*outputs));
    outputs->export_options = plan_export_options_default();
    if (!api_member_bool(request, "midi", &outputs->midi, &found, error) ||
        !api_member_bool(request, "musicxml", &outputs->musicxml, &found, error) ||
        !api_member_bool(request, "plain_midi", &plain, &found, error) ||
        !api_member_bool(request, "plans", &outputs->plans, &found, error) ||
        !api_member_string(request, "explain", &explain, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    if (explain) {
        if (strcmp(explain, "text") == 0) {
            outputs->explain = 1;
        } else if (strcmp(explain, "json") == 0) {
            outputs->explain = 2;
        } else {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/explain", "explain must be \"text\" or \"json\", not '%s'", explain);
        }
    }
    if (plain) {
        outputs->export_options.track_names = false;
        outputs->export_options.section_markers = false;
    }
    return COMPOSER_OK;
}

/* The motive request with the key the brief settled on, and the meter and the role of an accompaniment motive. */
static ComposerStatus make_lines(State *state, ComposerError *error) {
    char key[24];
    util_key_name(analysis_key_tonic_pitch_class(&state->setup.key), state->setup.key.mode == KEY_STRUCTURE_MODE_MINOR, key,
                  sizeof key);
    state->line = cJSON_Duplicate(state->request, true);
    if (!state->line) {
        return fail_alloc(error);
    }
    cJSON_DeleteItemFromObjectCaseSensitive(state->line, "key");
    if (!cJSON_AddStringToObject(state->line, "key", key)) {
        return fail_alloc(error);
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(state->request, "accompaniment_motive");
    if (cJSON_IsObject(accompaniment)) {
        state->accompaniment_line = cJSON_Duplicate(accompaniment, true);
        if (!state->accompaniment_line) {
            return fail_alloc(error);
        }
        cJSON_DeleteItemFromObjectCaseSensitive(state->accompaniment_line, "meter");
        cJSON_DeleteItemFromObjectCaseSensitive(state->accompaniment_line, "role");
        if (!cJSON_AddStringToObject(state->accompaniment_line, "meter", state->brief.text.meter) ||
            !cJSON_AddStringToObject(state->accompaniment_line, "role", "accompaniment")) {
            return fail_alloc(error);
        }
    }
    return COMPOSER_OK;
}

static ComposerStatus grow_prepare(State *state, const ComposerJob *job, Outputs *outputs, ComposerError *error) {
    const cJSON *request = state->request;
    GrowSetup *setup = &state->setup;
    bool found;
    bool check = false;
    unsigned long seed = 0;
    unsigned long suggest = 1;
    const char *corpus = NULL;
    const char *like = NULL;
    if (!api_member_string(request, "corpus", &corpus, &found, error) || !api_member_string(request, "like", &like, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    bool has_input = cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(request, "input"));
    if (like && !corpus) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/like", "like needs corpus");
    }
    if (corpus && !like) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/corpus", "corpus needs like <file>");
    }
    if (corpus && has_input) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/corpus", "give a motive input or corpus, not both");
    }
    if (!corpus && !has_input) {
        return api_fail(error, COMPOSER_ERR_REQUEST, "/input", "the request needs an input (a motive) or a corpus with like");
    }
    ComposerStatus status = read_brief(request, &state->brief, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    status = api_grow_read_brief(&state->brief.text, setup, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    ComposerStyleWeights weights;
    memset(&weights, 0, sizeof(weights));
    status = api_read_blend(state->brief.style != NULL, state->brief.blend, state->brief.blend_weight, state->brief.blend_weights,
                            &weights, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (!api_member_whole(request, "suggest", 1, GROW_MAX_SUGGESTIONS, &suggest, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    setup->suggest = found ? suggest : 1;
    setup->suggested = found;
    bool seeded = false;
    if (!api_member_whole(request, "seed", 0, GROW_SEED_MAX, &seed, &seeded, error) ||
        !api_member_bool(request, "check", &check, &found, error)) {
        return COMPOSER_ERR_REQUEST;
    }
    setup->seed = seeded ? seed : api_default_seed();
    status = read_outputs(request, outputs, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    setup->check = check;
    setup->explain = outputs->explain != 0;
    setup->job = job;

    /* The tempo of the request wins over the style's, and is the tempo of the files. */
    PlanExportOptions tempo_options = plan_export_options_default();
    status = api_read_tempo(request, &tempo_options, error);
    if (status != COMPOSER_OK) {
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
        if (status != COMPOSER_OK) {
            return status;
        }
    }
    setup->style = state->style;
    setup->without = state->brief.without;
    setup->without_count = state->brief.text.without_count;
    ComposerRatio option_tempo = {outputs->export_options.tempo_numerator, outputs->export_options.tempo_denominator};
    status = api_grow_set_tempo_and_mode(setup, has_tempo ? &option_tempo : NULL, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (!has_tempo && setup->tempo.denominator > 0) {
        outputs->export_options.tempo_numerator = setup->tempo.numerator;
        outputs->export_options.tempo_denominator = setup->tempo.denominator;
    }

    status = make_lines(state, error);
    if (status != COMPOSER_OK) {
        return status;
    }
    if (corpus) {
        if (cJSON_GetObjectItemCaseSensitive(request, "harmony") || cJSON_GetObjectItemCaseSensitive(request, "features") ||
            cJSON_GetObjectItemCaseSensitive(request, "role")) {
            return api_fail(error, COMPOSER_ERR_REQUEST, "/harmony",
                            "harmony, features and role describe a motive input, not one seeded from a corpus");
        }
        /* The passage is chosen once, by the first seed; the seeds tried after it grow from the same motive. */
        status = api_seed_motive(corpus, like, &setup->meter, &setup->key, setup->seed, &setup->seed_found, error);
        if (status == COMPOSER_OK) {
            status = api_seed_source(&setup->seed_found, &setup->source, error);
        }
        if (status != COMPOSER_OK) {
            return status;
        }
        api_seed_stale_notice(&setup->seed_found, corpus, state->stale, sizeof state->stale);
        setup->seeded = true;
        state->motive = setup->seed_found.motive;
    } else {
        status = api_read_motive(state->line, "", &state->motive, NULL, error);
        if (status != COMPOSER_OK) {
            return status;
        }
    }
    if (state->accompaniment_line) {
        status = api_read_motive(state->accompaniment_line, "/accompaniment_motive", &state->accompaniment_motive, NULL, error);
        if (status != COMPOSER_OK) {
            return status;
        }
        setup->accompaniment_motive = &state->accompaniment_motive;
    }
    setup->motive = &state->motive;
    setup->text = &state->brief.text;
    return COMPOSER_OK;
}

/* --- the outputs ----------------------------------------------------------------------------------------- */

/* Adds, to each theme of the document, its plan (with "plans"), its explanation and its MusicXML (when asked). */
static ComposerStatus add_theme_members(cJSON *document, const GrowRun *run, const Outputs *outputs, ComposerError *error) {
    cJSON *themes = cJSON_GetObjectItemCaseSensitive(document, "themes");
    for (size_t k = 0; k < run->made; k++) {
        cJSON *item = cJSON_GetArrayItem(themes, (int)k);
        const GrowTheme *theme = &run->themes[k];
        if (outputs->plans) {
            cJSON *plan = api_piece_plan_object(theme->plan);
            if (!plan || !cJSON_AddItemToObject(item, "plan", plan)) {
                cJSON_Delete(plan);
                return fail_alloc(error);
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
                return fail_alloc(error);
            }
            free(text);
        }
        if (outputs->musicxml) {
            RenderedPlan rendered;
            PlanExportError detail;
            char *text = NULL;
            if (plan_export_render(theme->plan, &rendered, &detail) != PLAN_EXPORT_OK) {
                return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot render the plan");
            }
            PlanExportResult written = rendered_plan_musicxml_text(&rendered, &outputs->export_options, &text, &detail);
            rendered_plan_destroy(&rendered);
            if (written != PLAN_EXPORT_OK) {
                return api_fail(error, COMPOSER_ERR_UNSATISFIABLE, "/musicxml", "cannot write the MusicXML file: %s", detail.message);
            }
            bool ok = cJSON_AddStringToObject(item, "musicxml", text) != NULL;
            free(text);
            if (!ok) {
                return fail_alloc(error);
            }
        }
    }
    return COMPOSER_OK;
}

/* The plan, the MIDI file and the MusicXML of the first theme. */
static ComposerStatus render_first(const GrowTheme *theme, const Outputs *outputs, ComposerResponse *out) {
    ApiOutputs files;
    memset(&files, 0, sizeof(files));
    files.midi = outputs->midi;
    files.musicxml = outputs->musicxml;
    files.export_options = outputs->export_options;
    return api_render_outputs(theme->plan, &files, out);
}

static ComposerStatus add_notices(cJSON *document, const State *state, const Outputs *outputs, ComposerError *error) {
    (void)outputs;
    char only[96] = "";
    if (state->setup.suggested && state->run.made < state->setup.suggest) {
        snprintf(only, sizeof only, "only %zu different theme%s found for this request", state->run.made,
                 state->run.made == 1 ? "" : "s");
    }
    if (!state->stale[0] && !only[0]) {
        return COMPOSER_OK;
    }
    cJSON *notices = cJSON_AddArrayToObject(document, "notices");
    if (!notices || (state->stale[0] && !cJSON_AddItemToArray(notices, cJSON_CreateString(state->stale))) ||
        (only[0] && !cJSON_AddItemToArray(notices, cJSON_CreateString(only)))) {
        return fail_alloc(error);
    }
    return COMPOSER_OK;
}

/* --- the entry -------------------------------------------------------------------------------------------- */

ComposerStatus composer_api_grow(const char *request_json, const ComposerJob *job, ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    State state;
    Outputs outputs;
    memset(&state, 0, sizeof(state));
    memset(&outputs, 0, sizeof(outputs));
    ComposerStatus status = api_parse_request(request_json, &state.request, &out->error);
    if (status == COMPOSER_OK) {
        status = grow_prepare(&state, job, &outputs, &out->error);
    }
    if (status == COMPOSER_OK) {
        state.ran = true;
        status = api_grow_run(&state.setup, &state.run, &out->error);
    }
    if (status == COMPOSER_OK) {
        cJSON *document = api_grow_result_json(&state.setup, &state.run, NULL);
        status = document ? add_theme_members(document, &state.run, &outputs, &out->error) : fail_alloc(&out->error);
        if (status == COMPOSER_OK) {
            status = add_notices(document, &state, &outputs, &out->error);
        }
        if (status == COMPOSER_OK) {
            status = render_first(&state.run.themes[0], &outputs, out);
        }
        if (status == COMPOSER_OK) {
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
    if (status != COMPOSER_OK) {
        ComposerError error = out->error;
        composer_response_destroy(out);
        out->error = error;
    }
    return status;
}
