#include "api_grow.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/form/phrase_beginning.h>
#include <theory/form/phrase_ending.h>
#include <theory/form/phrase_level_form.h>
#include <theory/vocabulary/texture_rules.h>

#include <ursatz_composer/analyzer_check.h>
#include <ursatz_composer/compose.h>
#include <ursatz_composer/form_hear.h>
#include <ursatz_composer/grow.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/rejection.h>
#include <ursatz_composer/theme_shape.h>

#include "api_internal.h"
#include "api_vocabulary.h"
#include "grow/grow_internal.h"
#include "structure/plan_util.h"
#include "structure/request_parse.h"
#include "util/util.h"

#define MAX_CHORDS 96
#define MAX_LANE_SPANS 128
#define MAX_DEVELOPMENTS 8
#define SLOT_ROWS 16


/* --- reading the brief ------------------------------------------------------------------------------ */

/* --- the construction -------------------------------------------------------------------------------- */


/* --- styles ------------------------------------------------------------------------------------------ */

UrsatzStatus api_grow_set_tempo_and_mode(GrowSetup *out, const Rational *option_tempo, UrsatzError *error) {
    out->tempo.num = 0;
    out->tempo.den = 0;
    out->tempo_from = "none";
    if (option_tempo) {
        out->tempo = *option_tempo;
        out->tempo_from = "option";
    } else if (composer_style_tempo(out->style, &out->tempo)) {
        out->tempo_from = "style";
    }
    bool from_style = out->text->material && request_equals_ignore_case(out->text->material, "style");
    if (from_style) {
        const PitchMaterial *material = NULL;
        if (!composer_style_mode(out->style, out->materials, &material)) {
            char member_path[32];
            snprintf(member_path, sizeof member_path, "/%s", out->text->material_member);
            return api_fail(error, URSATZ_ERR_REQUEST, member_path, "%s style needs a style with a mode category",
                            out->text->material_member);
        }
        composer_key_set_material(&out->key, material);
    }
    out->mode_from = from_style ? "style" : "request";
    return URSATZ_OK;
}

/* --- growing ---------------------------------------------------------------------------------------- */

typedef struct {
    char *text;
    size_t length;
    size_t capacity;
} Text;

static bool text_append(Text *text, const char *format, ...) __attribute__((format(printf, 2, 3)));

static bool text_append(Text *text, const char *format, ...) {
    char piece[256];
    va_list args;
    va_start(args, format);
    int written = vsnprintf(piece, sizeof(piece), format, args);
    va_end(args);
    if (written < 0 || (size_t)written >= sizeof(piece)) {
        return false;
    }
    if (text->length + (size_t)written + 1 > text->capacity) {
        size_t capacity = text->capacity ? text->capacity * 2 : 1024;
        while (capacity < text->length + (size_t)written + 1) {
            capacity *= 2;
        }
        char *grown = realloc(text->text, capacity);
        if (!grown) {
            return false;
        }
        text->text = grown;
        text->capacity = capacity;
    }
    memcpy(text->text + text->length, piece, (size_t)written + 1);
    text->length += (size_t)written;
    return true;
}

/* Everything the harmony, the bass and the melody say over the theme, as text: two themes with the same text are one. */
static char *signature_of(const Plan *plan, NodeId theme_node) {
    static const PlanLaneKind LANES[3] = {PLAN_LANE_HARMONY, PLAN_LANE_BASS, PLAN_LANE_MELODY};
    PlanSpan span = plan_node_span(plan, theme_node);
    Text text = {0};
    for (int lane = 0; lane < 3; lane++) {
        LaneSpanId ids[MAX_LANE_SPANS];
        size_t count = plan_lane_find(plan, LANES[lane], 0, span, ids, MAX_LANE_SPANS);
        for (size_t i = 0; i < count && i < MAX_LANE_SPANS; i++) {
            PlanSpan at = plan_lane_span_span(plan, ids[i]);
            const PlanLanePayload *payload = plan_lane_span_payload(plan, ids[i]);
            if (!text_append(&text, "|%d@%lld/%lld+%lld/%lld:", lane, at.start.num, at.start.den, at.length.num,
                             at.length.den)) {
                free(text.text);
                return NULL;
            }
            if (LANES[lane] == PLAN_LANE_HARMONY) {
                if (!text_append(&text, "%s", payload->harmony.roman_numeral)) {
                    free(text.text);
                    return NULL;
                }
                continue;
            }
            for (size_t n = 0; n < payload->notes.note_count; n++) {
                const PlanNote *note = &payload->notes.notes[n];
                if (!text_append(&text, "%d@%lld/%lld+%lld/%lld,", note->midi, note->offset.num, note->offset.den,
                                 note->duration.num, note->duration.den)) {
                    free(text.text);
                    return NULL;
                }
            }
        }
    }
    return text.text ? text.text : strdup("");
}

/* The bar (from 0) of a plan time in the theme. */
static long bar_at(Rational time, PlanSpan theme, Rational bar) {
    long long num = ((long long)time.num * theme.start.den - (long long)theme.start.num * time.den) * bar.den;
    long long den = (long long)time.den * theme.start.den * bar.num;
    return den > 0 && num >= 0 ? (long)(num / den) : 0;
}

/*
 * The bar (from 0) of a chord Given by the request that the earliest span the theme still has open lies in or beside, else
 * -1: the chords are what the stage could not write around.
 */
static long open_bar_with_chords(const Plan *plan, NodeId theme_node, const PlanMeter *meter,
                                 const GrowMaterial *material, GrowStage stage) {
    Rational bar;
    PlanSpan theme;
    PlanSpan open = {{0, 1}, {0, 1}};
    Rational open_end;
    bool any = false;
    /* Only the stages that write around the chords can be stopped by them. */
    PlanLaneKind lane = stage == GROW_STAGE_HARMONY ? PLAN_LANE_HARMONY
                        : stage == GROW_STAGE_BASS  ? PLAN_LANE_BASS
                                                    : PLAN_LANE_MELODY;
    if (material->chord_count == 0 || theme_node == PLAN_NODE_NONE ||
        (stage != GROW_STAGE_HARMONY && stage != GROW_STAGE_BASS && stage != GROW_STAGE_MELODY) ||
        plan_meter_bar_length(meter, &bar) != PLAN_OK) {
        return -1;
    }
    theme = plan_node_span(plan, theme_node);
    LaneSpanId ids[MAX_LANE_SPANS];
    size_t count = plan_lane_find(plan, lane, 0, theme, ids, MAX_LANE_SPANS);
    for (size_t i = 0; i < count && i < MAX_LANE_SPANS; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        if (plan_lane_span_state(plan, ids[i]) == PLAN_STATE_CONSTRAINED &&
            (!any || rational_cmp(span.start, open.start) < 0)) {
            open = span;
            any = true;
        }
    }
    if (!any || plan_span_end(open, &open_end) != PLAN_OK) {
        return -1;
    }
    long first = bar_at(open.start, theme, bar);
    long last = bar_at(open_end, theme, bar);
    for (long at = first > 0 ? first - 1 : 0; at <= last; at++) {
        for (size_t c = 0; c < material->chord_count; c++) {
            if (bar_at((Rational){material->chords[c].onset.num, material->chords[c].onset.den}, (PlanSpan){{0, 1}, {0, 1}},
                       bar) == at) {
                return at;
            }
        }
    }
    return -1;
}

static void passed_add(GrowRun *run, const ComposeRejection *rejection) {
    ComposeRejection *grown = realloc(run->passed, (run->passed_count + 1) * sizeof(ComposeRejection));
    if (!grown) {
        free(rejection->items);
        return;
    }
    run->passed = grown;
    run->passed[run->passed_count++] = *rejection;
}

static void theme_destroy(GrowTheme *theme) {
    plan_destroy(theme->plan);
    grow_outcome_destroy(&theme->outcome);
    analyzer_check_destroy(&theme->check);
    free(theme->signature);
    memset(theme, 0, sizeof(*theme));
}

void api_grow_run_destroy(GrowRun *run) {
    for (size_t k = 0; k < run->made; k++) {
        theme_destroy(&run->themes[k]);
    }
    compose_rejections_free(run->passed, run->passed_count);
    memset(run, 0, sizeof(*run));
    run->chord_bar = -1;
}

/* The request of the theme for a seed: the setup and its material, which it points into and must outlive it. */
void api_grow_request_for(const GrowSetup *setup, unsigned long seed, GrowRequest *request) {
    const GrowMaterial *material = setup->material;
    (void)seed;
    memset(request, 0, sizeof(*request));
    request->motive = setup->motive;
    request->source = setup->seeded ? &setup->source : NULL;
    request->key = setup->key;
    request->meter = setup->meter;
    request->bars = setup->bars;
    request->construction = setup->has_construction ? &setup->construction : NULL;
    request->pairing = setup->pairing;
    request->cadence = setup->cadence;
    request->first_cadence = setup->first_cadence;
    if (setup->contrast) {
        request->contrast = *setup->contrast;
    }
    request->has_accompaniment = setup->has_accompaniment;
    request->accompaniment = setup->accompaniment;
    request->accompaniment_motive = setup->accompaniment_motive;
    request->has_voicing = setup->has_voicing;
    request->voicing = setup->voicing;
    for (GrowSlot slot = GROW_SLOT_CONTRASTING; material && slot < GROW_SLOT_COUNT; slot++) {
        request->slot_motives[slot] = material->has_slot[slot] ? &material->slot_motives[slot] : NULL;
    }
    for (int idea = 0; material && idea < GROW_IDEA_COUNT; idea++) {
        request->idea_motives[idea] = material->has_idea[idea] ? &material->idea_motives[idea] : NULL;
    }
    if (material) {
        request->chords = material->chord_count > 0 ? material->chords : NULL;
        request->chord_count = material->chord_count;
    }
}

/* Builds the plan and runs the stages for one seed. GROW_ERR_STUCK is the only result a later seed can mend. */
static GrowResult grow_one(const GrowSetup *setup, unsigned long seed, size_t attempt, GrowTheme *theme, GrowRun *run,
                           bool *cancelled) {
    const GrowMaterial *material = setup->material;
    GrowRequest request;
    api_grow_request_for(setup, seed, &request);
    memset(theme, 0, sizeof(*theme));
    theme->seed = seed;
    GrowResult built = grow_build_plan_checked(&request, &theme->plan, &run->refusal);
    if (built != GROW_OK) {
        return built;
    }
    GrowConfig config;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.basic = setup->motive;
    config.context.development = setup->development;
    if (setup->continuation) {
        config.context.continuation = *setup->continuation;
    }
    config.context.basic_as_written = setup->basic_as_written;
    if (setup->contrast) {
        config.context.contrast = *setup->contrast;
    }
    config.context.motif = setup->motif;
    config.fidelity = setup->fidelity;
    config.melody = setup->melody;
    config.style = setup->style;
    config.style_weight = setup->style_weight;
    config.tempo = setup->tempo;
    config.job = setup->job;
    config.knowledge = setup->knowledge;
    if (material) {
        config.pool = material->pool;
        config.pool_count = material->pool_count;
    }
    GrowResult ran = grow_run(theme->plan, &config, &theme->outcome);
    NodeId theme_node = api_find_node(theme->plan, "theme");
    if (theme->outcome.cancelled) {
        *cancelled = true;
        theme_destroy(theme);
        return GROW_ERR_STUCK;
    }
    if (ran == GROW_ERR_STUCK && material) {
        run->chord_bar = open_bar_with_chords(theme->plan, theme_node, &setup->meter, material, theme->outcome.stuck_stage);
    }
    if (ran == GROW_ERR_STUCK && setup->explain) {
        ComposeRejection rejection;
        memset(&rejection, 0, sizeof(rejection));
        rejection.round = attempt;
        rejection.seed = seed;
        snprintf(rejection.stage, sizeof(rejection.stage), "%s",
                 theme->outcome.stuck ? grow_stage_name(theme->outcome.stuck_stage) : "");
        snprintf(rejection.reason, sizeof(rejection.reason), "stuck");
        passed_add(run, &rejection);
    }
    /* A theme the Analyzer cannot read is not held against it. */
    if (ran == GROW_OK && setup->check && theme_node != PLAN_NODE_NONE) {
        theme->checked = analyzer_check_section(theme->plan, setup->knowledge, theme_node, &theme->check) == ANALYZER_CHECK_OK;
        if (theme->checked && theme->check.contradicts) {
            ComposeRejection rejection;
            if (setup->explain &&
                explain_analyzer_rejection(theme->plan, theme_node, &theme->check, attempt, seed, &rejection) == EXPLAIN_OK) {
                passed_add(run, &rejection);
            }
            ran = GROW_ERR_STUCK;
        }
    }
    if (ran == GROW_OK) {
        theme->signature = theme_node == PLAN_NODE_NONE ? NULL : signature_of(theme->plan, theme_node);
        if (!theme->signature) {
            ran = GROW_ERR_ALLOC;
        }
    }
    if (ran != GROW_OK) {
        theme_destroy(theme);
    }
    return ran;
}

UrsatzStatus api_grow_run(const GrowSetup *setup, GrowRun *run, UrsatzError *error) {
    memset(run, 0, sizeof(*run));
    run->chord_bar = -1;
    run->last_seed = setup->seed;
    const GrowBriefText *text = setup->text;
    unsigned long tries = (unsigned long)GROW_MAX_SEED_TRIES * setup->suggest;
    for (unsigned long attempt = 0; attempt < tries && run->made < setup->suggest; attempt++) {
        if (job_step(setup->job, "grow", (unsigned)attempt, (unsigned)tries)) {
            return api_fail(error, URSATZ_ERR_CANCELLED, "", "cancelled");
        }
        GrowTheme theme;
        bool cancelled = false;
        run->last_seed = (setup->seed + attempt) & GROW_SEED_MAX;
        GrowResult result = grow_one(setup, run->last_seed, attempt, &theme, run, &cancelled);
        if (cancelled) {
            return api_fail(error, URSATZ_ERR_CANCELLED, "", "cancelled");
        }
        if (result == GROW_OK) {
            bool repeated = false;
            for (size_t k = 0; k < run->made; k++) {
                repeated = repeated || strcmp(run->themes[k].signature, theme.signature) == 0;
            }
            if (repeated) {
                theme_destroy(&theme);
            } else {
                theme.passed_before = run->passed_count;
                run->themes[run->made++] = theme;
            }
        } else if (result != GROW_ERR_STUCK) {
            /* A refusal before seeding says why and where: its path is that of the field in a theme request. */
            const GrowRefusal *refusal = &run->refusal;
            bool named = setup->material || strncmp(refusal->path, "/construction", 13) == 0 ||
                         strcmp(refusal->path, "/bars") == 0;
            return refusal->message[0] != '\0'
                       ? api_fail(error, URSATZ_ERR_UNSATISFIABLE, named ? refusal->path : "", "%s", refusal->message)
                       : api_fail(error, URSATZ_ERR_UNSATISFIABLE, "", "%s", grow_result_name(result));
        }
    }
    if (run->made == 0 && run->chord_bar >= 0) {
        return api_fail(error, URSATZ_ERR_UNSATISFIABLE, setup->material ? "/chords" : "", "the given chords in bar %ld do not allow the %s",
                        run->chord_bar, text->cadence ? text->cadence : "PAC");
    }
    if (run->made == 0) {
        return api_fail(error, URSATZ_ERR_UNSATISFIABLE, "",
                        "no theme could be grown for %s, %s, %zu bars, %s from seeds %lu to %lu", text->key, text->meter,
                        setup->bars, text->into, setup->seed, run->last_seed);
    }
    return URSATZ_OK;
}

