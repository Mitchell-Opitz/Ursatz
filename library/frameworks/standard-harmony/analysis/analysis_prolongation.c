#include "analysis_prolongation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "analysis_six_four.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "harmony_functions.h"
#include "harmony_prolongation.h"
#include "identity/entity_id.h"

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
    for (size_t i = 0; i < span->interior_count; i++) {
        if (span->relationships != NULL) {
            relationship_destroy(span->relationships[i]);
        }
        if (span->interpretations != NULL) {
            interpretation_destroy(span->interpretations[i]);
        }
    }
    free(span->interior_segments);
    free(span->reasons);
    free(span->relationships);
    free(span->interpretations);
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

static bool make_pair(const HarmonySegment *segments, const Run *first_run, const HarmonyProlongationSpan *span,
                      size_t k, double uncertainty, Relationship **relationship, Interpretation **interpretation) {
    const char *source = segments[span->first_segment].input->segment_id;
    const char *target = segments[span->interior_segments[k]].input->segment_id;
    (void)first_run;
    char relationship_id_value[512];
    snprintf(relationship_id_value, sizeof(relationship_id_value), "standard-harmony-prolongs-%s-%s", source, target);

    HarmonyProlongationScope scope;
    memset(&scope, 0, sizeof(scope));
    scope.function = (int)span->function;
    snprintf(scope.closing_segment, sizeof(scope.closing_segment), "%s",
             segments[span->closing_segment].input->segment_id);
    scope.interior_count = span->interior_count;
    HarmonyProlongationReasonValue reason_value;
    memset(&reason_value, 0, sizeof(reason_value));
    reason_value.reason = (int)span->reasons[k];

    EntityID *relationship_id = NULL;
    EntityID *source_id = NULL;
    EntityID *target_id = NULL;
    Value *scope_value = NULL;
    Value *qualifiers = NULL;
    ProvenanceReference *provenance = NULL;
    if (entity_id_create(relationship_id_value, &relationship_id) != ENTITY_ID_OK ||
        entity_id_create(source, &source_id) != ENTITY_ID_OK ||
        entity_id_create(target, &target_id) != ENTITY_ID_OK ||
        value_create(HARMONY_PROLONGATION_SCOPE_TYPE, &scope, sizeof(scope), &scope_value) != VALUE_OK ||
        value_create(HARMONY_PROLONGATION_REASON_TYPE, &reason_value, sizeof(reason_value), &qualifiers) !=
            VALUE_OK ||
        provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK ||
        relationship_create(relationship_id, source_id, HARMONY_PROLONGS, target_id, scope_value, NULL, qualifiers,
                            NULL, NULL, provenance, relationship) != RELATIONSHIP_OK) {
        entity_id_destroy(relationship_id);
        entity_id_destroy(source_id);
        entity_id_destroy(target_id);
        value_destroy(scope_value);
        value_destroy(qualifiers);
        provenance_reference_destroy(provenance);
        return false;
    }

    HarmonyProlongationClaim claim;
    memset(&claim, 0, sizeof(claim));
    claim.function = (int)span->function;
    claim.reason = (int)span->reasons[k];
    char interpretation_id[512];
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-harmony-prolongation-%s-%s", source, target);
    *interpretation = harmony_analysis_make_interpretation(interpretation_id, relationship_id_value,
                                                           HARMONY_PROLONGATION_CLAIM_TYPE, &claim, sizeof(claim),
                                                           uncertainty, "prolongation-adapter");
    if (*interpretation == NULL) {
        relationship_destroy(*relationship);
        *relationship = NULL;
        return false;
    }
    return true;
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
    double uncertainty = runs[i].uncertainty > runs[j].uncertainty ? runs[i].uncertainty : runs[j].uncertainty;
    bool ok = true;
    for (size_t m = i + 1; m < j && ok; m++) {
        /* the tonic line is a three-chord progression: it is the first run, one interior run, the closing run */
        ok = explained(m == i + 1 ? &runs[i] : NULL, &runs[m], &runs[j], m + 1 == j, function, &run_reasons[m - i]);
        if (runs[m].uncertainty > uncertainty) {
            uncertainty = runs[m].uncertainty;
        }
    }
    if (!ok) {
        free(run_reasons);
        return false;
    }
    span->interior_count = segment_total;
    span->interior_segments = (size_t *)calloc(segment_total, sizeof(size_t));
    span->reasons = (HarmonyProlongationReason *)calloc(segment_total, sizeof(HarmonyProlongationReason));
    span->relationships = (Relationship **)calloc(segment_total, sizeof(Relationship *));
    span->interpretations = (Interpretation **)calloc(segment_total, sizeof(Interpretation *));
    if (span->interior_segments == NULL || span->reasons == NULL || span->relationships == NULL ||
        span->interpretations == NULL) {
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
    for (k = 0; k < segment_total; k++) {
        if (!make_pair(segments, &runs[i], span, k, uncertainty, &span->relationships[k], &span->interpretations[k])) {
            span->interior_count = k; /* free only what was built */
            free_span(span);
            return false;
        }
    }
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

bool harmony_prolongation_claim_read(const Interpretation *interpretation, HarmonyProlongationClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), HARMONY_PROLONGATION_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
