#include "analysis_cadence.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "analysis_runs.h"
#include "core/value.h"
#include "harmony_cadence_rules.h"
#include "identity/entity_id.h"
#include "time/musical_time.h"

HarmonyCadenceConfig harmony_cadence_config_default(void) {
    HarmonyCadenceConfig config;
    config.min_arrival_strength = BEAT_STRENGTH_MEDIUM;
    config.arrival_min_beats = 1.0;
    config.soprano_rest_counts = true;
    config.beat_length = 0.25;
    config.soprano_absent_penalty = 0.3;
    return config;
}

static bool config_valid(const HarmonyCadenceConfig *config) {
    return beat_strength_valid(config->min_arrival_strength) && isfinite(config->arrival_min_beats) &&
           config->arrival_min_beats >= 0.0 && isfinite(config->beat_length) && config->beat_length > 0.0 &&
           isfinite(config->soprano_absent_penalty) && config->soprano_absent_penalty >= 0.0 &&
           config->soprano_absent_penalty <= 1.0;
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

/* --- cadence points --- */

/* Whether the run [first, last] is an arrival: on a strong enough beat, long enough or before a soprano rest. */
static bool arrival_ok(const HarmonySegment *segments, size_t count, size_t first, size_t last,
                       const HarmonyCadenceConfig *config) {
    const HarmonySegmentInput *start = segments[first].input;
    if (beat_strength_is_stronger(config->min_arrival_strength, start->beat_strength)) {
        return false; /* weaker than the minimum */
    }
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
            !arrival_ok(segments, count, arrival->first, arrival->last, config)) {
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
    out->dominant_first = dominant->first;
    out->dominant_last = dominant->last;
    if (arrival_run >= 2 && is_cadential_six_four(&segments[runs[arrival_run - 2].first])) {
        out->has_six_four = true;
        out->six_four_first = runs[arrival_run - 2].first;
        out->six_four_last = runs[arrival_run - 2].last;
        out->dominant_first = out->six_four_first;
    }
}

static bool classify_point(const HarmonySegment *segments, const HarmonyRun *runs, const InternalPoint *internal,
                           Classified *out) {
    const HarmonyCadencePoint *point = &internal->point;
    const HarmonySegment *arrival = &segments[point->arrival_first];
    const HarmonySegment *before = &segments[point->before_first];
    memset(out, 0, sizeof(*out));
    out->soprano_degree = arrival->input->soprano_degree;

    /* Root-position V(7) to root-position I, by the existing rule. The soprano is not read yet: it is passed as 1
       and only the authentic family of the answer is used. */
    if (is_dominant_root(before) && is_tonic_root(arrival)) {
        HarmonyCadence rule = {before->chord, arrival->chord, 1, false};
        CadenceType rule_type;
        if (harmony_cadence_classify(&rule, &rule_type) && cadence_type_is_authentic(rule_type)) {
            out->type = CADENCE_TYPE_AUTHENTIC_UNSPECIFIED;
            dominant_unit(segments, runs, internal->arrival_run, out);
            return true;
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
    for (size_t i = 0; i < point_count; i++) {
        Classified classified;
        if (!classify_point(segments, runs, &points[i], &classified)) {
            continue;
        }
        if (!build_result(segments, &points[i].point, &classified, &found.items[found.count])) {
            harmony_cadences_destroy(&found);
            free(runs);
            free(points);
            return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
        }
        found.count++;
    }
    (void)run_count;
    free(runs);
    free(points);
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
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
