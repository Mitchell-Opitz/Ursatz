#include "analysis_cadence.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "analysis_runs.h"
#include "core/value.h"
#include "harmony_cadence_rules.h"
#include "harmony_deceptive.h"
#include "harmony_functions.h"
#include "harmony_plagal.h"
#include "identity/entity_id.h"
#include "time/metrical_position.h"
#include "time/musical_time.h"

HarmonyCadenceConfig harmony_cadence_config_default(void) {
    HarmonyCadenceConfig config;
    config.min_arrival_strength = BEAT_STRENGTH_MEDIUM;
    config.arrival_min_beats = 1.0;
    config.soprano_rest_counts = true;
    config.beat_length = 0.25;
    config.soprano_absent_penalty = 0.3;
    config.allow_weak_arrival = true;
    config.reading_strong_arrival = 150;
    config.reading_cadential_approach = 150;
    config.reading_followup_cadence = 150;
    config.reading_tonic_continues = 100;
    config.reading_long_dominant = 150;
    config.reading_break_after_dominant = 150;
    config.reading_pac_shape = 150;
    config.reading_short_dominant = 100;
    config.reading_followup_beats = 8.0;
    config.reading_long_dominant_beats = 4.0;
    return config;
}

static bool config_valid(const HarmonyCadenceConfig *config) {
    return beat_strength_valid(config->min_arrival_strength) && isfinite(config->arrival_min_beats) &&
           config->arrival_min_beats >= 0.0 && isfinite(config->beat_length) && config->beat_length > 0.0 &&
           isfinite(config->soprano_absent_penalty) && config->soprano_absent_penalty >= 0.0 &&
           config->soprano_absent_penalty <= 1.0 && config->reading_strong_arrival >= 0 &&
           config->reading_cadential_approach >= 0 && config->reading_followup_cadence >= 0 &&
           config->reading_tonic_continues >= 0 && config->reading_long_dominant >= 0 &&
           config->reading_break_after_dominant >= 0 && config->reading_pac_shape >= 0 &&
           config->reading_short_dominant >= 0 && isfinite(config->reading_followup_beats) &&
           config->reading_followup_beats >= 0.0 && isfinite(config->reading_long_dominant_beats) &&
           config->reading_long_dominant_beats >= 0.0;
}

static double whole_notes(const MusicalTime *time) {
    return (double)musical_time_numerator(time) / (double)musical_time_denominator(time);
}

/* --- the chords the rules read --- */

static bool diatonic(const HarmonySegment *segment) {
    return segment->valid && !segment->applied;
}

/* V or V7 in root position, with the major third a dominant has (a minor v is not one). */
static bool is_dominant_root(const HarmonySegment *segment) {
    return diatonic(segment) && segment->chord.degree == 5 && segment->chord.inversion == 0 &&
           !segment->chord.cadential && segment->input->intervals[1] == 4;
}

static bool is_tonic_root(const HarmonySegment *segment) {
    return diatonic(segment) && segment->chord.degree == 1 && segment->chord.inversion == 0 &&
           !segment->chord.seventh && !segment->chord.cadential;
}

static bool is_cadential_six_four(const HarmonySegment *segment) {
    return diatonic(segment) && segment->chord.cadential;
}

bool harmony_cadence_is_dominant_root(const HarmonySegment *segment) {
    return is_dominant_root(segment);
}

bool harmony_cadence_is_tonic_root(const HarmonySegment *segment) {
    return is_tonic_root(segment);
}

bool harmony_cadence_is_cadential_six_four(const HarmonySegment *segment) {
    return is_cadential_six_four(segment);
}

/* --- cadence points --- */

/* Whether the run [first, last] is long enough, or the soprano rests right after it, or it ends the piece. */
static bool arrival_closes(const HarmonySegment *segments, size_t count, size_t first, size_t last,
                           const HarmonyCadenceConfig *config) {
    const HarmonySegmentInput *start = segments[first].input;
    double beat = config->beat_length;
    if (start->beat_length != NULL) {
        beat = (double)duration_numerator(start->beat_length) / (double)duration_denominator(start->beat_length);
    }
    double length = whole_notes(time_span_end(segments[last].input->span)) -
                    whole_notes(time_span_start(segments[first].input->span));
    if (length + 1e-9 >= config->arrival_min_beats * beat) {
        return true;
    }
    return config->soprano_rest_counts && last + 1 < count && segments[last + 1].input->soprano_degree == 0;
}

/* Whether the segment starts on the beat grid (its offset in the measure is a whole number of beats). */
static bool starts_on_beat(const HarmonySegment *segment, const HarmonyCadenceConfig *config) {
    const HarmonySegmentInput *input = segment->input;
    double beat = config->beat_length;
    if (input->beat_length != NULL) {
        beat = (double)duration_numerator(input->beat_length) / (double)duration_denominator(input->beat_length);
    }
    const Duration *offset = metrical_position_offset(input->position);
    double beats = ((double)duration_numerator(offset) / (double)duration_denominator(offset)) / beat;
    return fabs(beats - floor(beats + 0.5)) < 1e-9;
}

/* Whether the dominant starts on a stronger beat than the tonic: a stronger BeatStrength, or, at equal strength, on
   the beat while the tonic starts off it (a weak beat against the weak half beat after it). */
static bool dominant_on_stronger_beat(const HarmonySegment *dominant, const HarmonySegment *tonic,
                                      const HarmonyCadenceConfig *config) {
    BeatStrength d = dominant->input->beat_strength;
    BeatStrength t = tonic->input->beat_strength;
    return beat_strength_is_stronger(d, t) ||
           (d == t && starts_on_beat(dominant, config) && !starts_on_beat(tonic, config));
}

/* Whether the run [first, last] is an arrival: on a strong enough beat, long enough or before a soprano rest. A
   tonic below the minimum beat strength is still one when the dominant before it (starting at before_first) falls on
   a stronger beat and the tonic is long, followed by a soprano rest, or the end of the piece. */
static bool arrival_ok(const HarmonySegment *segments, size_t count, size_t before_first, size_t first, size_t last,
                       const HarmonyCadenceConfig *config) {
    const HarmonySegmentInput *start = segments[first].input;
    if (!beat_strength_is_stronger(config->min_arrival_strength, start->beat_strength)) {
        return arrival_closes(segments, count, first, last, config);
    }
    return config->allow_weak_arrival && is_dominant_root(&segments[before_first]) &&
           is_tonic_root(&segments[first]) &&
           dominant_on_stronger_beat(&segments[before_first], &segments[first], config) &&
           (last + 1 == count || arrival_closes(segments, count, first, last, config));
}

bool harmony_cadence_is_arrival(const HarmonySegment *segments, size_t count, size_t before_first, size_t first,
                                size_t last, const HarmonyCadenceConfig *config) {
    return arrival_ok(segments, count, before_first, first, last, config);
}

/* A candidate with the index of its arrival run, so the classifier can look at the runs around it. */
typedef struct {
    HarmonyCadencePoint point;
    size_t arrival_run;
} InternalPoint;

/* The runs and the candidates among them. On OK the caller frees both arrays. */
static HarmonyCadenceAnalysisStatus collect(const HarmonySegment *segments, size_t count,
                                            const HarmonyCadenceConfig *config, HarmonyRun **out_runs,
                                            size_t *out_run_count, InternalPoint **out_points, size_t *out_count) {
    if (!config_valid(config)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG;
    }
    HarmonyRun *runs = (HarmonyRun *)calloc(count > 0 ? count : 1, sizeof(HarmonyRun));
    InternalPoint *points = (InternalPoint *)calloc(count > 0 ? count : 1, sizeof(InternalPoint));
    if (runs == NULL || points == NULL) {
        free(runs);
        free(points);
        return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    }
    size_t run_count = harmony_analysis_build_runs(segments, count, runs);
    size_t n = 0;
    for (size_t r = 1; r < run_count; r++) {
        const HarmonyRun *arrival = &runs[r];
        const HarmonyRun *before = &runs[r - 1];
        if (!diatonic(&segments[arrival->first]) || !diatonic(&segments[before->first]) ||
            !arrival_ok(segments, count, before->first, arrival->first, arrival->last, config)) {
            continue;
        }
        points[n].point.arrival_first = arrival->first;
        points[n].point.arrival_last = arrival->last;
        points[n].point.before_first = before->first;
        points[n].point.before_last = before->last;
        points[n].arrival_run = r;
        n++;
    }
    *out_runs = runs;
    *out_run_count = run_count;
    *out_points = points;
    *out_count = n;
    return HARMONY_CADENCE_ANALYSIS_OK;
}

HarmonyCadenceAnalysisStatus harmony_analysis_cadence_points(const HarmonySegment *segments, size_t count,
                                                             const HarmonyCadenceConfig *config,
                                                             HarmonyCadencePoint **out, size_t *out_count) {
    HarmonyCadenceConfig defaults = harmony_cadence_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (out == NULL || out_count == NULL || (segments == NULL && count > 0)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    HarmonyRun *runs = NULL;
    size_t run_count = 0;
    InternalPoint *internal = NULL;
    size_t n = 0;
    HarmonyCadenceAnalysisStatus status = collect(segments, count, config, &runs, &run_count, &internal, &n);
    if (status != HARMONY_CADENCE_ANALYSIS_OK) {
        return status;
    }
    HarmonyCadencePoint *points = NULL;
    if (n > 0) {
        points = (HarmonyCadencePoint *)calloc(n, sizeof(HarmonyCadencePoint));
        if (points == NULL) {
            free(runs);
            free(internal);
            return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
        }
        for (size_t i = 0; i < n; i++) {
            points[i] = internal[i].point;
        }
    }
    free(runs);
    free(internal);
    *out = points;
    *out_count = n;
    return HARMONY_CADENCE_ANALYSIS_OK;
}

/* --- classification --- */

typedef struct {
    CadenceType type;
    size_t dominant_first_run; /* the run index where the dominant unit starts (the 6/4's, when it has one) */
    size_t dominant_first;
    size_t dominant_last;
    bool has_six_four;
    size_t six_four_first;
    size_t six_four_last;
    int soprano_degree;
    bool post_cadential;
    double penalty;
} Classified;

/* The dominant unit before the arrival: the V run, with a cadential 6/4 run directly before it when there is one. */
static void dominant_unit(const HarmonySegment *segments, const HarmonyRun *runs, size_t arrival_run,
                          Classified *out) {
    const HarmonyRun *dominant = &runs[arrival_run - 1];
    out->dominant_first_run = arrival_run - 1;
    out->dominant_first = dominant->first;
    out->dominant_last = dominant->last;
    if (arrival_run >= 2 && is_cadential_six_four(&segments[runs[arrival_run - 2].first])) {
        out->has_six_four = true;
        out->six_four_first = runs[arrival_run - 2].first;
        out->six_four_last = runs[arrival_run - 2].last;
        out->dominant_first = out->six_four_first;
        out->dominant_first_run = arrival_run - 2;
    }
}

/* Whether the function of the chord at segment `index` is `function`. */
static bool has_function(const HarmonySegment *segment, HarmonicFunction function) {
    HarmonicFunction found;
    return diatonic(segment) && harmony_function_of(segment->input->mode, &segment->chord, &found) &&
           found == function;
}

/* The soprano degree to hand the rules: the real one, or 1 when absent (a type that does not read it). */
static int soprano_or_one(int degree) {
    return degree >= 1 && degree <= 7 ? degree : 1;
}

static bool classify_point(const HarmonySegment *segments, const HarmonyRun *runs, const InternalPoint *internal,
                           const bool *pac_arrival_run, const HarmonyCadenceConfig *config, Classified *out) {
    const HarmonyCadencePoint *point = &internal->point;
    const HarmonySegment *arrival = &segments[point->arrival_first];
    const HarmonySegment *before = &segments[point->before_first];
    size_t r = internal->arrival_run;
    memset(out, 0, sizeof(*out));
    out->soprano_degree = arrival->input->soprano_degree;
    bool soprano = out->soprano_degree >= 1 && out->soprano_degree <= 7;

    if (is_dominant_root(before)) {
        /* The dominant before the arrival: V(7) in root position, with a cadential 6/4 before it when it has one. */
        dominant_unit(segments, runs, r, out);
        const HarmonySegment *prior =
            out->dominant_first_run > 0 ? &segments[runs[out->dominant_first_run - 1].first] : NULL;

        /* AUTHENTIC: V(7) to root-position I, by the existing rule: perfect when the soprano is on 1 over the tonic,
           imperfect otherwise; with the soprano absent it cannot be asked and the penalty applies. */
        if (is_tonic_root(arrival)) {
            HarmonyCadence rule = {before->chord, arrival->chord, soprano_or_one(out->soprano_degree), false};
            CadenceType rule_type;
            if (harmony_cadence_classify(&rule, &rule_type) && cadence_type_is_authentic(rule_type)) {
                out->type = soprano ? rule_type : CADENCE_TYPE_AUTHENTIC_UNSPECIFIED;
                out->penalty = soprano ? 0.0 : config->soprano_absent_penalty;
                return true;
            }
        }

        /* DECEPTIVE: V(7) to the chord the existing rule names (vi, or IV6). */
        HarmonyChord pair[2] = {before->chord, arrival->chord};
        HarmonyDeceptiveKind kind;
        if (diatonic(arrival) && harmony_deceptive_of(pair, &kind)) {
            out->type = CADENCE_TYPE_DECEPTIVE;
            return true;
        }

        /* SUBVERTED: a cadential dominant (after a predominant, or with a cadential 6/4) that resolves to I6 or
           another chord that is neither a tonic, the V again, a deceptive substitute nor a 6/4. */
        bool cadential = out->has_six_four || (prior != NULL && has_function(prior, HARMONIC_FUNCTION_PREDOMINANT));
        if (cadential && diatonic(arrival) && !arrival->chord.cadential && arrival->chord.degree != 5 &&
            !is_tonic_root(arrival)) {
            HarmonyCadence rule = {before->chord, arrival->chord, soprano_or_one(out->soprano_degree), true};
            CadenceType rule_type;
            out->type = CADENCE_TYPE_SUBVERTED;
            if (arrival->chord.degree != 1 || harmony_cadence_classify(&rule, &rule_type)) {
                return true;
            }
        }
        memset(out, 0, sizeof(*out));
        out->soprano_degree = arrival->input->soprano_degree;
    }

    /* PLAGAL: IV to I directly after a perfect authentic cadence (post-cadential), by the existing rule. */
    if (is_tonic_root(arrival) && r >= 2 && pac_arrival_run[r - 2]) {
        HarmonyChord pair[2] = {before->chord, arrival->chord};
        if (diatonic(before) && harmony_plagal_progression_is(pair, HARMONY_PLAGAL_AFTER_CADENCE)) {
            out->type = CADENCE_TYPE_PLAGAL;
            out->post_cadential = true;
            out->dominant_first_run = r - 1;
            out->dominant_first = point->before_first;
            out->dominant_last = point->before_last;
            return true;
        }
    }

    /* HALF: arrival on a root-position V (or V7) at a cadence point, preceded by a tonic or a predominant; a
       cadential 6/4 directly before the V is part of the dominant and the chord before it is the one that counts. */
    if (is_dominant_root(arrival)) {
        bool six_four = is_cadential_six_four(before);
        size_t unit_run = six_four ? r - 1 : r;
        if (unit_run >= 1) {
            const HarmonySegment *from = &segments[runs[unit_run - 1].first];
            HarmonyCadence rule = {from->chord, arrival->chord, soprano_or_one(out->soprano_degree), false};
            CadenceType rule_type;
            /* A 6/4 is never the chord that sets up a half cadence: a neighbor 6/4 between two V is part of the V. */
            if (diatonic(from) && !is_dominant_root(from) && !from->chord.cadential && from->chord.inversion != 2 &&
                (has_function(from, HARMONIC_FUNCTION_TONIC) || has_function(from, HARMONIC_FUNCTION_PREDOMINANT)) &&
                harmony_cadence_classify(&rule, &rule_type) && rule_type == CADENCE_TYPE_HALF) {
                out->type = CADENCE_TYPE_HALF;
                out->dominant_first_run = r;
                out->dominant_first = point->arrival_first;
                out->dominant_last = point->arrival_last;
                if (six_four) {
                    out->has_six_four = true;
                    out->six_four_first = point->before_first;
                    out->six_four_last = point->before_last;
                    out->dominant_first = point->before_first;
                    out->dominant_first_run = r - 1;
                }
                return true;
            }
        }
    }
    return false;
}

/* --- output --- */

static double max_uncertainty(const HarmonySegment *segments, size_t first, size_t last, double u) {
    for (size_t i = first; i <= last; i++) {
        if (segments[i].input->uncertainty > u) {
            u = segments[i].input->uncertainty;
        }
    }
    return u;
}

static bool build_result(const HarmonySegment *segments, const HarmonyCadencePoint *point, const Classified *c,
                         HarmonyCadenceResult *result) {
    const HarmonySegmentInput *arrival = segments[point->arrival_first].input;
    memset(result, 0, sizeof(*result));
    result->type = c->type;
    result->dominant_first = c->dominant_first;
    result->dominant_last = c->dominant_last;
    result->has_six_four = c->has_six_four;
    result->six_four_first = c->six_four_first;
    result->six_four_last = c->six_four_last;
    result->arrival_first = point->arrival_first;
    result->arrival_last = point->arrival_last;
    result->soprano_degree = c->soprano_degree;
    result->post_cadential = c->post_cadential;
    result->arrival_beat_strength = arrival->beat_strength;
    result->key_ref = arrival->key_ref;

    double u = max_uncertainty(segments, c->dominant_first, c->dominant_last, 0.0);
    u = max_uncertainty(segments, point->arrival_first, point->arrival_last, u);
    result->uncertainty = 1.0 - (1.0 - u) * (1.0 - c->penalty);

    if (time_span_create(time_span_start(segments[c->dominant_first].input->span),
                         time_span_end(segments[point->arrival_last].input->span), TIME_SPAN_BOUNDARY_LEFT_CLOSED,
                         &result->span) != TIME_SPAN_OK) {
        return false;
    }

    /* the Cadence shell: the segment ids from the dominant's first to the arrival's last */
    size_t event_count = point->arrival_last - c->dominant_first + 1;
    EntityID **event_ids = (EntityID **)calloc(event_count, sizeof(EntityID *));
    char cadence_id_value[256];
    snprintf(cadence_id_value, sizeof(cadence_id_value), "standard-harmony-cadence-%s", arrival->segment_id);
    EntityID *cadence_id = NULL;
    bool ok = event_ids != NULL && entity_id_create(cadence_id_value, &cadence_id) == ENTITY_ID_OK;
    for (size_t i = 0; ok && i < event_count; i++) {
        ok = entity_id_create(segments[c->dominant_first + i].input->segment_id, &event_ids[i]) == ENTITY_ID_OK;
    }
    if (!ok || cadence_create(cadence_id, event_ids, event_count, NULL, &result->cadence) != CADENCE_OK) {
        for (size_t i = 0; event_ids != NULL && i < event_count; i++) {
            entity_id_destroy(event_ids[i]);
        }
        free(event_ids);
        entity_id_destroy(cadence_id);
        time_span_destroy(result->span);
        result->span = NULL;
        return false;
    }
    /* cadence_create took ownership of the ids; the array itself is ours */
    free(event_ids);

    HarmonyCadenceClaim claim;
    memset(&claim, 0, sizeof(claim));
    claim.type = (int)c->type;
    claim.soprano_degree = c->soprano_degree;
    claim.post_cadential = c->post_cadential;
    claim.has_six_four = c->has_six_four;
    claim.arrival_beat_strength = (int)arrival->beat_strength;
    snprintf(claim.arrival_segment, sizeof(claim.arrival_segment), "%s", arrival->segment_id);
    snprintf(claim.dominant_segment, sizeof(claim.dominant_segment), "%s",
             segments[c->dominant_first].input->segment_id);
    snprintf(claim.key_ref, sizeof(claim.key_ref), "%s", arrival->key_ref);
    char interpretation_id[256];
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-harmony-cadence-interpretation-%s",
             arrival->segment_id);
    result->interpretation = harmony_analysis_make_interpretation(interpretation_id, cadence_id_value,
                                                                  HARMONY_CADENCE_CLAIM_TYPE, &claim, sizeof(claim),
                                                                  result->uncertainty, "cadence-adapter");
    if (result->interpretation == NULL) {
        cadence_destroy(result->cadence);
        result->cadence = NULL;
        time_span_destroy(result->span);
        result->span = NULL;
        return false;
    }
    return true;
}

static void destroy_result(HarmonyCadenceResult *result) {
    interpretation_destroy(result->interpretation);
    cadence_destroy(result->cadence);
    time_span_destroy(result->span);
    memset(result, 0, sizeof(*result));
}

void harmony_cadences_destroy(HarmonyCadences *cadences) {
    if (cadences == NULL) {
        return;
    }
    for (size_t i = 0; i < cadences->count; i++) {
        destroy_result(&cadences->items[i]);
    }
    free(cadences->items);
    cadences->items = NULL;
    cadences->count = 0;
}

HarmonyCadenceAnalysisStatus harmony_analysis_find_cadences(const HarmonySegment *segments, size_t count,
                                                            const HarmonyCadenceConfig *config,
                                                            HarmonyCadences *out) {
    if (out == NULL || (segments == NULL && count > 0)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    HarmonyCadenceConfig defaults = harmony_cadence_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    HarmonyRun *runs = NULL;
    size_t run_count = 0;
    InternalPoint *points = NULL;
    size_t point_count = 0;
    HarmonyCadenceAnalysisStatus status = collect(segments, count, config, &runs, &run_count, &points, &point_count);
    if (status != HARMONY_CADENCE_ANALYSIS_OK) {
        return status;
    }
    HarmonyCadences found = {NULL, 0};
    if (point_count > 0) {
        found.items = (HarmonyCadenceResult *)calloc(point_count, sizeof(HarmonyCadenceResult));
        if (found.items == NULL) {
            free(runs);
            free(points);
            return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
        }
    }
    bool *pac_arrival_run = (bool *)calloc(run_count > 0 ? run_count : 1, sizeof(bool));
    Classified *classified = (Classified *)calloc(point_count > 0 ? point_count : 1, sizeof(Classified));
    bool *is_cadence = (bool *)calloc(point_count > 0 ? point_count : 1, sizeof(bool));
    if (pac_arrival_run == NULL || classified == NULL || is_cadence == NULL) {
        free(pac_arrival_run);
        free(classified);
        free(is_cadence);
        free(found.items);
        free(runs);
        free(points);
        return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < point_count; i++) {
        is_cadence[i] = classify_point(segments, runs, &points[i], pac_arrival_run, config, &classified[i]);
        if (is_cadence[i] && classified[i].type == CADENCE_TYPE_PERFECT_AUTHENTIC) {
            pac_arrival_run[points[i].arrival_run] = true;
        }
    }
    /* A half cadence arrival is the END of a phrase on V. A V that the very next cadence resolves (an authentic,
       deceptive or subverted cadence whose dominant is this V) is the dominant of that cadence, not a half cadence
       of its own, so the half cadence is dropped. */
    for (size_t i = 0; i < point_count; i++) {
        if (!is_cadence[i] || classified[i].type != CADENCE_TYPE_HALF) {
            continue;
        }
        for (size_t j = 0; j < point_count; j++) {
            if (is_cadence[j] && j != i && classified[j].dominant_last == points[i].point.arrival_last &&
                (cadence_type_is_authentic(classified[j].type) ||
                 classified[j].type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED ||
                 classified[j].type == CADENCE_TYPE_DECEPTIVE || classified[j].type == CADENCE_TYPE_SUBVERTED)) {
                is_cadence[i] = false;
                break;
            }
        }
    }
    for (size_t i = 0; i < point_count; i++) {
        if (!is_cadence[i]) {
            continue;
        }
        if (!build_result(segments, &points[i].point, &classified[i], &found.items[found.count])) {
            harmony_cadences_destroy(&found);
            free(pac_arrival_run);
            free(classified);
            free(is_cadence);
            free(runs);
            free(points);
            return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
        }
        found.count++;
    }
    free(pac_arrival_run);
    free(classified);
    free(is_cadence);
    free(runs);
    free(points);
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return HARMONY_CADENCE_ANALYSIS_OK;
}

/* --- cadences at formal boundaries --- */

/* The index of the segment that starts at `start` whole notes, from 1 on; false when none does. */
static bool section_start_segment(const HarmonySegment *segments, size_t count, double start, size_t *out) {
    for (size_t i = 1; i < count; i++) {
        if (fabs(whole_notes(time_span_start(segments[i].input->span)) - start) < 1e-9) {
            *out = i;
            return true;
        }
    }
    return false;
}

static bool is_authentic_family(CadenceType type) {
    return cadence_type_is_authentic(type) || type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED;
}

HarmonyCadenceAnalysisStatus harmony_analysis_add_boundary_cadences(const HarmonySegment *segments, size_t count,
                                                                    const HarmonyCadenceConfig *config,
                                                                    const double *section_starts, size_t section_count,
                                                                    HarmonyCadences *cadences) {
    if (cadences == NULL || (segments == NULL && count > 0) || (section_starts == NULL && section_count > 0)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    HarmonyCadenceConfig defaults = harmony_cadence_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (!config_valid(config)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG;
    }
    for (size_t k = 0; k < section_count; k++) {
        if (!isfinite(section_starts[k])) {
            return HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG;
        }
    }
    if (section_count == 0 || count < 2) {
        return HARMONY_CADENCE_ANALYSIS_OK;
    }
    HarmonyRun *runs = (HarmonyRun *)calloc(count, sizeof(HarmonyRun));
    HarmonyCadenceResult *merged =
        (HarmonyCadenceResult *)calloc(cadences->count + section_count, sizeof(HarmonyCadenceResult));
    bool *removed = (bool *)calloc(cadences->count > 0 ? cadences->count : 1, sizeof(bool));
    if (runs == NULL || merged == NULL || removed == NULL) {
        free(runs);
        free(merged);
        free(removed);
        return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    }
    size_t run_count = harmony_analysis_build_runs(segments, count, runs);
    bool *pac_arrival_run = (bool *)calloc(run_count > 0 ? run_count : 1, sizeof(bool));
    if (pac_arrival_run == NULL) {
        free(runs);
        free(merged);
        free(removed);
        return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    }

    size_t added = 0;
    HarmonyCadenceAnalysisStatus status = HARMONY_CADENCE_ANALYSIS_OK;
    for (size_t k = 0; k < section_count && status == HARMONY_CADENCE_ANALYSIS_OK; k++) {
        size_t start;
        if (!section_start_segment(segments, count, section_starts[k], &start)) {
            continue;
        }
        size_t r = 0;
        while (r < run_count && runs[r].last != start - 1) {
            r++;
        }
        if (r == 0 || r >= run_count || !is_dominant_root(&segments[runs[r].first]) ||
            !diatonic(&segments[runs[r - 1].first])) {
            continue;
        }
        bool recorded = false;
        for (size_t i = 0; i < cadences->count; i++) {
            const HarmonyCadenceResult *old = &cadences->items[i];
            recorded = recorded || (old->type == CADENCE_TYPE_HALF && old->arrival_last == start - 1);
        }
        InternalPoint point;
        memset(&point, 0, sizeof(point));
        point.point.arrival_first = runs[r].first;
        point.point.arrival_last = runs[r].last;
        point.point.before_first = runs[r - 1].first;
        point.point.before_last = runs[r - 1].last;
        point.arrival_run = r;
        Classified classified;
        if (!recorded && (!classify_point(segments, runs, &point, pac_arrival_run, config, &classified) ||
                          classified.type != CADENCE_TYPE_HALF)) {
            continue;
        }
        /* the tonic after the dominant starts the new section: an authentic cadence read into it is replaced */
        for (size_t i = 0; i < cadences->count; i++) {
            const HarmonyCadenceResult *old = &cadences->items[i];
            if (is_authentic_family(old->type) && old->arrival_first == start && old->dominant_last == start - 1) {
                removed[i] = true;
            }
        }
        if (!recorded) {
            if (!build_result(segments, &point.point, &classified, &merged[added])) {
                status = HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
                break;
            }
            added++;
        }
    }
    free(pac_arrival_run);
    free(runs);
    if (status != HARMONY_CADENCE_ANALYSIS_OK) {
        for (size_t i = 0; i < added; i++) {
            destroy_result(&merged[i]);
        }
        free(merged);
        free(removed);
        return status;
    }

    /* the kept cadences and the added ones, in order of arrival */
    HarmonyCadenceResult *items = (HarmonyCadenceResult *)calloc(cadences->count + added > 0 ? cadences->count + added : 1,
                                                                 sizeof(HarmonyCadenceResult));
    if (items == NULL) {
        for (size_t i = 0; i < added; i++) {
            destroy_result(&merged[i]);
        }
        free(merged);
        free(removed);
        return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    }
    size_t n = 0;
    size_t next_added = 0;
    for (size_t i = 0; i < cadences->count; i++) {
        if (removed[i]) {
            destroy_result(&cadences->items[i]);
            continue;
        }
        while (next_added < added && merged[next_added].arrival_first < cadences->items[i].arrival_first) {
            items[n++] = merged[next_added++];
        }
        items[n++] = cadences->items[i];
    }
    while (next_added < added) {
        items[n++] = merged[next_added++];
    }
    free(cadences->items);
    free(merged);
    free(removed);
    cadences->items = n > 0 ? items : NULL;
    cadences->count = n;
    if (n == 0) {
        free(items);
    }
    return HARMONY_CADENCE_ANALYSIS_OK;
}

bool harmony_cadence_claim_read(const Interpretation *interpretation, HarmonyCadenceClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), HARMONY_CADENCE_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
