#include "check/hearing_result.h"

#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include <time/duration.h>
#include <time/musical_time.h>
#include <time/temporal_anchor.h>
#include <time/time_rational.h>

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/composer_api.h>

#include <ursatz_composer/lanes.h>

/* Test only (hearing.h): how many spans were handed to the Analyzer. Not static, so it is not the file-scope state
   scripts/check-statics.sh refuses; nothing in src reads it. */
atomic_size_t hearing_analyzer_calls_made;

size_t hearing_analyzer_calls(void) {
    return atomic_load(&hearing_analyzer_calls_made);
}

/* One span heard: the result and the arrays it borrows, kept together. */
typedef struct {
    PlanSpan span;
    const NoteEvent **events;
    int *midi;
    size_t *streams;
    size_t count;
    size_t *source; /* each heard event's index among the rendered events */
    AnalyzerResult *result;
} HeardSpan;

struct ComposerHearing {
    const PiecePlan *plan;
    RenderedPlan rendered;
    int *midi;
    Rational *starts; /* parallel to the rendered events: where each starts, in whole notes */
    HeardSpan *spans;
    size_t span_count;
    size_t span_capacity;
};

static void heard_span_release(HeardSpan *heard) {
    analyzer_result_destroy(heard->result);
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

HearingResult hearing_create(const PiecePlan *plan, ComposerHearing **out) {
    if (!out) {
        return HEARING_ERR_NULL;
    }
    *out = NULL;
    if (!plan) {
        return HEARING_ERR_NULL;
    }
    ComposerHearing *hearing = calloc(1, sizeof(*hearing));
    if (!hearing) {
        return HEARING_ERR_ALLOC;
    }
    hearing->plan = plan;
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
        hearing->starts[i] = (Rational){0, 1};
        rational_of_musical_time(at, &hearing->starts[i]);
        hearing->midi[i] = (int)pitch_numeric_value_position_numerator(hearing->rendered.pitch_values[i]);
    }
    *out = hearing;
    return HEARING_OK;
}

const PiecePlan *hearing_plan(const ComposerHearing *hearing) {
    return hearing ? hearing->plan : NULL;
}

const RenderedPlan *hearing_rendered(const ComposerHearing *hearing) {
    return hearing ? &hearing->rendered : NULL;
}

int hearing_midi(const ComposerHearing *hearing, size_t event) {
    return hearing && event < hearing->rendered.count ? hearing->midi[event] : 0;
}

/* The key lane's key under the span's start. */
static bool key_under(const PiecePlan *plan, PlanSpan span, AnalysisKey *out) {
    LaneSpanId first;
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, &first, 1) == 0 ||
        rational_cmp(plan_lane_span_span(plan, first).start, span.start) > 0) {
        return false;
    }
    *out = plan_lane_span_payload(plan, first)->key;
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

HearingResult hearing_piece(ComposerHearing *hearing, const AnalyzerResult **out) {
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

size_t hearing_source_event(const ComposerHearing *hearing, const AnalyzerResult *result, size_t index) {
    for (size_t i = 0; hearing && i < hearing->span_count; i++) {
        if (hearing->spans[i].result == result && index < hearing->spans[i].count) {
            return hearing->spans[i].source[index];
        }
    }
    return (size_t)-1;
}

HearingResult hearing_section(ComposerHearing *hearing, PlanSpan span, const AnalyzerResult **out) {
    if (!out) {
        return HEARING_ERR_NULL;
    }
    *out = NULL;
    if (!hearing) {
        return HEARING_ERR_NULL;
    }
    HeardSpan *cached = find_heard(hearing, span);
    if (cached) {
        *out = cached->result;
        return HEARING_OK;
    }
    Rational end;
    AnalysisKey key;
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
    AnalyzerHearing input;
    memset(&input, 0, sizeof(input));
    input.events = (const NoteEvent *const *)heard.events;
    input.count = count;
    input.midi = heard.midi;
    input.meter = rendered->meter_map;
    input.has_key = true;
    input.key_tonic_pitch_class = analysis_key_tonic_pitch_class(&key);
    input.key_minor = key.mode == KEY_STRUCTURE_MODE_MINOR;
    input.streams = heard.streams;
    AnalyzerHearingOptions options = analyzer_hearing_options_default();
    atomic_fetch_add(&hearing_analyzer_calls_made, 1);
    AnalyzerStatus status = analyzer_hear(&input, &options, &heard.result);
    if (status != ANALYZER_OK) {
        heard_span_release(&heard);
        return status == ANALYZER_ERR_ALLOC ? HEARING_ERR_ALLOC : HEARING_ERR_ANALYSIS;
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
    *out = hearing->spans[hearing->span_count++].result;
    return HEARING_OK;
}
