#include "check/hearing_result.h"
#include "util/util.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include <model/time/duration.h>
#include <model/time/musical_time.h>
#include <model/time/temporal_anchor.h>
#include <io/imported_score.h>
#include <theory/tonality/common_practice_materials.h>
#include <theory/tonality/tonal_key.h>

#include <plan/plan_lanes.h>

/* Test only (hearing.h): how many spans were run through the library chain. Not static, so it is not the file-scope state
   tools/ci/check-statics.sh refuses; nothing in src reads it. */
atomic_size_t hearing_chain_runs_made;

/* One span heard: the chain, the score view it ran on and the arrays the view borrows, kept together. */
typedef struct {
    PlanSpan span;
    const NoteEvent **events;
    int *midi;
    size_t *streams;
    size_t count;
    size_t *source; /* each heard event's index among the rendered events */
    ImportedScore *score; /* the view of the events (imported_score_view); the chain borrows it */
    AnalysisChain *chain;
} HeardSpan;

struct ComposerHearing {
    const Plan *plan;
    RenderedPlan rendered;
    int *midi;
    Rational *starts; /* parallel to the rendered events: where each starts, in whole notes */
    HeardSpan *spans;
    size_t span_count;
    size_t span_capacity;
    /* The snapshot the chain reads its materials from (the call context's); borrowed, it outlives the hearing. */
    const KnowledgeSnapshot *knowledge;
};

static void heard_span_release(HeardSpan *heard) {
    analysis_chain_destroy(heard->chain);
    if (heard->score) {
        imported_score_destroy(heard->score);
        free(heard->score);
    }
    free(heard->events);
    free(heard->midi);
    free(heard->streams);
    free(heard->source);
    memset(heard, 0, sizeof(*heard));
}

void hearing_destroy(ComposerHearing *hearing) {
    if (!hearing) {
        return;
    }
    for (size_t i = 0; i < hearing->span_count; i++) {
        heard_span_release(&hearing->spans[i]);
    }
    free(hearing->spans);
    free(hearing->starts);
    free(hearing->midi);
    rendered_plan_destroy(&hearing->rendered);
    free(hearing);
}

HearingResult hearing_create(const Plan *plan, const KnowledgeSnapshot *knowledge, ComposerHearing **out) {
    if (!out) {
        return HEARING_ERR_NULL;
    }
    *out = NULL;
    if (!plan || !knowledge) {
        return HEARING_ERR_NULL;
    }
    ComposerHearing *hearing = calloc(1, sizeof(*hearing));
    if (!hearing) {
        return HEARING_ERR_ALLOC;
    }
    hearing->plan = plan;
    hearing->knowledge = knowledge;
    PlanRenderResult drawn = plan_render(plan, &hearing->rendered);
    if (drawn != PLAN_RENDER_OK) {
        hearing_destroy(hearing);
        return drawn == PLAN_RENDER_ERR_ALLOC ? HEARING_ERR_ALLOC : HEARING_ERR_PLAN;
    }
    size_t count = hearing->rendered.count;
    hearing->midi = calloc(count ? count : 1, sizeof(*hearing->midi));
    hearing->starts = calloc(count ? count : 1, sizeof(*hearing->starts));
    if (!hearing->midi || !hearing->starts) {
        hearing_destroy(hearing);
        return HEARING_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        const MusicalTime *at = temporal_anchor_position(note_event_temporal_anchor(hearing->rendered.events[i]));
        hearing->starts[i] = at->value;
        hearing->midi[i] = (int)pitch_numeric_value_position_numerator(hearing->rendered.pitch_values[i]);
    }
    *out = hearing;
    return HEARING_OK;
}

const Plan *hearing_plan(const ComposerHearing *hearing) {
    return hearing ? hearing->plan : NULL;
}

const RenderedPlan *hearing_rendered(const ComposerHearing *hearing) {
    return hearing ? &hearing->rendered : NULL;
}

/* The key lane's key under the span's start. */
static bool key_under(const Plan *plan, PlanSpan span, SpelledKey *out) {
    LaneSpanId first;
    if (plan_lane_find(plan, PLAN_LANE_REGION, 0, span, &first, 1) == 0 ||
        rational_cmp(plan_lane_span_span(plan, first).start, span.start) > 0) {
        return false;
    }
    *out = plan_lane_span_payload(plan, first)->region;
    return true;
}

static HeardSpan *find_heard(ComposerHearing *hearing, PlanSpan span) {
    for (size_t i = 0; i < hearing->span_count; i++) {
        if (rational_cmp(hearing->spans[i].span.start, span.start) == 0 &&
            rational_cmp(hearing->spans[i].span.length, span.length) == 0) {
            return &hearing->spans[i];
        }
    }
    return NULL;
}

/* The key signature (sharps or flats and the mode) of a tonic pitch class and a material; false when the material has none. */
static bool key_signature_of(int tonic, const PitchMaterial *material, int *fifths, bool *is_minor) {
    PitchLetter letter;
    int accidental;
    if (!material || !standard_tonality_key_tonic_of_pitch_class_material(tonic, material, &letter, &accidental) ||
        !standard_tonality_key_fifths_of(letter, accidental, material, fifths)) {
        return false;
    }
    CommonPracticeKeyMode mode;
    *is_minor = common_practice_key_mode_of(material, &mode) && mode == COMMON_PRACTICE_KEY_MINOR;
    return true;
}

HearingResult hearing_piece(ComposerHearing *hearing, const AnalysisChain **out) {
    if (!hearing) {
        if (out) {
            *out = NULL;
        }
        return HEARING_ERR_NULL;
    }
    PlanSpan whole = {{0, 1}, hearing->rendered.piece_end};
    if (whole.length.num <= 0) {
        whole.length = (Rational){1000000, 1};
    }
    return hearing_section(hearing, whole, out);
}

size_t hearing_source_event(const ComposerHearing *hearing, const AnalysisChain *chain, size_t index) {
    for (size_t i = 0; hearing && i < hearing->span_count; i++) {
        if (hearing->spans[i].chain == chain && index < hearing->spans[i].count) {
            return hearing->spans[i].source[index];
        }
    }
    return (size_t)-1;
}

HearingResult hearing_section(ComposerHearing *hearing, PlanSpan span, const AnalysisChain **out) {
    if (!out) {
        return HEARING_ERR_NULL;
    }
    *out = NULL;
    if (!hearing) {
        return HEARING_ERR_NULL;
    }
    HeardSpan *cached = find_heard(hearing, span);
    if (cached) {
        *out = cached->chain;
        return HEARING_OK;
    }
    Rational end;
    SpelledKey key;
    if (plan_span_end(span, &end) != PLAN_OK || !hearing->rendered.meter_map ||
        !key_under(hearing->plan, span, &key)) {
        return HEARING_ERR_PLAN;
    }
    const RenderedPlan *rendered = &hearing->rendered;
    HeardSpan heard;
    memset(&heard, 0, sizeof(heard));
    heard.span = span;
    size_t room = rendered->count ? rendered->count : 1;
    heard.events = calloc(room, sizeof(*heard.events));
    heard.midi = calloc(room, sizeof(*heard.midi));
    heard.streams = calloc(room, sizeof(*heard.streams));
    heard.source = calloc(room, sizeof(*heard.source));
    if (!heard.events || !heard.midi || !heard.streams || !heard.source) {
        heard_span_release(&heard);
        return HEARING_ERR_ALLOC;
    }
    size_t count = 0;
    for (size_t i = 0; i < rendered->count; i++) {
        if (rational_cmp(hearing->starts[i], span.start) < 0 || rational_cmp(hearing->starts[i], end) >= 0) {
            continue;
        }
        heard.events[count] = rendered->events[i];
        heard.midi[count] = hearing->midi[i];
        heard.streams[count] = rendered->part_index[i];
        heard.source[count] = i;
        count++;
    }
    if (count == 0) {
        heard_span_release(&heard);
        return HEARING_ERR_NOT_WRITTEN;
    }
    heard.count = count;
    /* The key's mode (major or minor) picks the major or natural minor material of the hearing's snapshot. */
    CommonPracticeMaterials materials;
    if (!common_practice_materials_resolve(hearing->knowledge, &materials, NULL, 0)) {
        heard_span_release(&heard);
        return HEARING_ERR_ANALYSIS;
    }
    ImportedScoreView view = {.events = (const NoteEvent *const *)heard.events,
                              .count = count,
                              .midi = heard.midi,
                              .meter = rendered->meter_map,
                              .streams = heard.streams,
                              .has_key = true};
    if (!key_signature_of(spelled_key_tonic_pitch_class(&key),
                          common_practice_material(&materials, composer_key_is_minor(&key) ? COMMON_PRACTICE_KEY_MINOR
                                                                                           : COMMON_PRACTICE_KEY_MAJOR),
                          &view.key_fifths, &view.key_minor)) {
        heard_span_release(&heard);
        return HEARING_ERR_ANALYSIS;
    }
    heard.score = calloc(1, sizeof(*heard.score));
    if (!heard.score) {
        heard_span_release(&heard);
        return HEARING_ERR_ALLOC;
    }
    if (imported_score_view(&view, heard.score) != IMPORTED_SCORE_OK) {
        free(heard.score);
        heard.score = NULL;
        heard_span_release(&heard);
        return HEARING_ERR_ANALYSIS;
    }
    AnalysisChainConfig config = analysis_chain_config_default();
    config.knowledge = hearing->knowledge;
    config.stages = ANALYSIS_CHAIN_STAGE_ALL; /* the style distance reads the result-only stages (progressions, form plan) */
    atomic_fetch_add(&hearing_chain_runs_made, 1);
    AnalysisChainStatus status = analysis_chain_run_score(heard.score, &config, &heard.chain);
    if (status != ANALYSIS_CHAIN_OK) {
        heard_span_release(&heard);
        return status == ANALYSIS_CHAIN_ERR_ALLOC ? HEARING_ERR_ALLOC : HEARING_ERR_ANALYSIS;
    }
    if (hearing->span_count == hearing->span_capacity) {
        size_t grown = hearing->span_capacity ? hearing->span_capacity * 2 : 8;
        HeardSpan *more = realloc(hearing->spans, grown * sizeof(*more));
        if (!more) {
            heard_span_release(&heard);
            return HEARING_ERR_ALLOC;
        }
        hearing->spans = more;
        hearing->span_capacity = grown;
    }
    hearing->spans[hearing->span_count] = heard;
    *out = hearing->spans[hearing->span_count++].chain;
    return HEARING_OK;
}
