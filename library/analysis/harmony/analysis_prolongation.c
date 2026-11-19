#include "analysis/harmony/analysis_prolongation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/harmony/analysis_six_four.h"
#include "model/core/provenance_reference.h"
#include "model/core/value.h"
#include "theory/harmony/harmony_functions.h"
#include "theory/harmony/harmony_prolongation.h"
#include "model/identity/entity_id.h"

static const char *const REASON_NAMES[HARMONY_PROLONGATION_REASON_COUNT] = {
    "passing-six-four", "neighbor-six-four", "arpeggiating-six-four", "tonic-line", "applied"};

const char *harmony_prolongation_reason_name(HarmonyProlongationReason reason) {
    return reason >= 0 && reason < HARMONY_PROLONGATION_REASON_COUNT ? REASON_NAMES[reason] : NULL;
}

/* A run: consecutive segments that make the same chord (or the same applied chord). */
typedef struct {
    size_t first;
    size_t last;
    bool valid;
    bool applied;
    HarmonyChord chord;
    HarmonyAppliedChord applied_chord;
    bool has_function;
    HarmonicFunction function;
    bool has_six_four;
    SixFourType six_four;
    double uncertainty;
} Run;

static bool same_run(const HarmonySegment *a, const HarmonySegment *b) {
    if (!a->valid || !b->valid || a->applied != b->applied) {
        return false;
    }
    if (a->applied) {
        return a->applied_chord.kind == b->applied_chord.kind && a->applied_chord.quality == b->applied_chord.quality &&
               a->applied_chord.target == b->applied_chord.target && a->input->inversion == b->input->inversion &&
               a->input->degree == b->input->degree;
    }
    return a->chord.degree == b->chord.degree && a->chord.inversion == b->chord.inversion &&
           a->chord.seventh == b->chord.seventh && a->chord.cadential == b->chord.cadential &&
           a->chord.leading_tone == b->chord.leading_tone && a->chord.half_diminished == b->chord.half_diminished;
}

static size_t build_runs(const HarmonySegment *segments, size_t count, const HarmonySixFourResult *six_fours,
                         Run *runs) {
    size_t n = 0;
    size_t i = 0;
    while (i < count) {
        Run run;
        memset(&run, 0, sizeof(run));
        run.first = i;
        run.last = i;
        run.valid = segments[i].valid;
        run.applied = segments[i].applied;
        run.chord = segments[i].chord;
        run.applied_chord = segments[i].applied_chord;
        run.uncertainty = segments[i].input->uncertainty;
        while (run.last + 1 < count &&
               (run.valid ? same_run(&segments[i], &segments[run.last + 1]) : !segments[run.last + 1].valid)) {
            run.last++;
            if (segments[run.last].input->uncertainty > run.uncertainty) {
                run.uncertainty = segments[run.last].input->uncertainty;
            }
        }
        if (run.valid && !run.applied) {
            run.has_function = harmony_function_of(segments[i].input->mode, &run.chord, &run.function);
            if (six_fours[i].classified) {
                run.has_six_four = true;
                run.six_four = six_fours[i].type;
            }
        }
        runs[n++] = run;
        i = run.last + 1;
    }
    return n;
}

static bool is_prolonged_function(const Run *run, HarmonicFunction function) {
    return run->valid && !run->applied && run->has_function && run->function == function;
}

/* Whether run `middle` between the runs `before` and `after` is explained, and why. */
static bool explained(const Run *before, const Run *middle, const Run *after, bool last_interior,
                      HarmonicFunction function, HarmonyProlongationReason *reason) {
    if (!middle->valid) {
        return false;
    }
    if (middle->applied) {
        if (last_interior && !after->applied && after->valid && after->chord.degree == middle->applied_chord.target) {
            *reason = HARMONY_PROLONGATION_REASON_APPLIED;
            return true;
        }
        return false;
    }
    if (middle->has_six_four) {
        switch (middle->six_four) {
        case SIX_FOUR_TYPE_PASSING:
            *reason = HARMONY_PROLONGATION_REASON_PASSING_SIX_FOUR;
            return true;
        case SIX_FOUR_TYPE_NEIGHBOR:
            *reason = HARMONY_PROLONGATION_REASON_NEIGHBOR_SIX_FOUR;
            return true;
        case SIX_FOUR_TYPE_ARPEGGIATING:
            *reason = HARMONY_PROLONGATION_REASON_ARPEGGIATING_SIX_FOUR;
            return true;
        default:
            break;
        }
    }
    if (function == HARMONIC_FUNCTION_TONIC && last_interior && before != NULL && !before->applied &&
        !after->applied) {
        HarmonyChord chords[3] = {before->chord, middle->chord, after->chord};
        HarmonyProlongationLine line;
        if (harmony_prolongation_of(chords, &line)) {
            *reason = HARMONY_PROLONGATION_REASON_TONIC_LINE;
            return true;
        }
    }
    return false;
}

static void free_span(HarmonyProlongationSpan *span) {
    free(span->interior_segments);
    free(span->reasons);
    memset(span, 0, sizeof(*span));
}

void harmony_prolongations_destroy(HarmonyProlongations *prolongations) {
    if (prolongations == NULL) {
        return;
    }
    for (size_t i = 0; i < prolongations->count; i++) {
        free_span(&prolongations->spans[i]);
    }
    free(prolongations->spans);
    prolongations->spans = NULL;
    prolongations->count = 0;
}

/* A span from run `i` to the next run of the same function `j`; false if the interior is not all explained. */
static bool try_span(const HarmonySegment *segments, const Run *runs, size_t i, size_t j, HarmonicFunction function,
                     HarmonyProlongationSpan *span) {
    memset(span, 0, sizeof(*span));
    span->function = function;
    span->first_segment = runs[i].first;
    span->closing_segment = runs[j].first;
    size_t segment_total = 0;
    for (size_t m = i + 1; m < j; m++) {
        segment_total += runs[m].last - runs[m].first + 1;
    }
    HarmonyProlongationReason *run_reasons =
        (HarmonyProlongationReason *)calloc(j - i, sizeof(HarmonyProlongationReason));
    if (run_reasons == NULL) {
        return false;
    }
    bool ok = true;
    for (size_t m = i + 1; m < j && ok; m++) {
        /* the tonic line is a three-chord progression: it is the first run, one interior run, the closing run */
        ok = explained(m == i + 1 ? &runs[i] : NULL, &runs[m], &runs[j], m + 1 == j, function, &run_reasons[m - i]);
    }
    if (!ok) {
        free(run_reasons);
        return false;
    }
    span->interior_count = segment_total;
    span->interior_segments = (size_t *)calloc(segment_total, sizeof(size_t));
    span->reasons = (HarmonyProlongationReason *)calloc(segment_total, sizeof(HarmonyProlongationReason));
    if (span->interior_segments == NULL || span->reasons == NULL) {
        free(run_reasons);
        free_span(span);
        return false;
    }
    size_t k = 0;
    for (size_t m = i + 1; m < j; m++) {
        for (size_t s = runs[m].first; s <= runs[m].last; s++) {
            span->interior_segments[k] = s;
            span->reasons[k] = run_reasons[m - i];
            k++;
        }
    }
    free(run_reasons);
    return true;
}

HarmonyProlongationStatus harmony_analysis_find_prolongations(const HarmonySegment *segments, size_t count,
                                                              HarmonyProlongations *out) {
    if (out == NULL || (segments == NULL && count > 0)) {
        return HARMONY_PROLONGATION_ERR_NULL_ARGUMENT;
    }
    HarmonySixFourResult *six_fours = NULL;
    if (harmony_analysis_classify_six_fours(segments, count, &six_fours) != HARMONY_SIX_FOUR_OK) {
        return HARMONY_PROLONGATION_ERR_ALLOC;
    }
    Run *runs = (Run *)calloc(count > 0 ? count : 1, sizeof(Run));
    HarmonyProlongationSpan *spans = (HarmonyProlongationSpan *)calloc(count > 0 ? count : 1, sizeof(*spans));
    if (runs == NULL || spans == NULL) {
        free(runs);
        free(spans);
        harmony_six_four_results_destroy(six_fours, count);
        return HARMONY_PROLONGATION_ERR_ALLOC;
    }
    size_t run_count = build_runs(segments, count, six_fours, runs);
    harmony_six_four_results_destroy(six_fours, count);

    size_t span_count = 0;
    for (size_t i = 0; i < run_count; i++) {
        for (int f = 0; f < 2; f++) {
            HarmonicFunction function = f == 0 ? HARMONIC_FUNCTION_TONIC : HARMONIC_FUNCTION_DOMINANT;
            if (!is_prolonged_function(&runs[i], function)) {
                continue;
            }
            size_t j = i + 1;
            while (j < run_count && !is_prolonged_function(&runs[j], function)) {
                j++;
            }
            if (j >= run_count || j == i + 1) {
                continue; /* no closing chord, or nothing between */
            }
            if (span_count >= count) {
                continue;
            }
            if (try_span(segments, runs, i, j, function, &spans[span_count])) {
                span_count++;
            }
        }
    }
    free(runs);
    if (span_count == 0) {
        free(spans);
        spans = NULL;
    }
    out->spans = spans;
    out->count = span_count;
    return HARMONY_PROLONGATION_OK;
}

