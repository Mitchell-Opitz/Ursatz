#include "analysis_cadence_claims.h"

#include <stdlib.h>
#include <string.h>

#include "analysis_cadence_build.h"
#include "time/duration.h"
#include "time/metrical_position.h"
#include "time/musical_time.h"
#include "time/time_span.h"

/* --- segments in measures --- */

static int measure_of(const HarmonySegment *segment) {
    return metrical_position_measure(segment->input->position);
}

static bool starts_measure(const HarmonySegment *segment) {
    return duration_numerator(metrical_position_offset(segment->input->position)) == 0;
}

/* The first segment that starts `measure` (offset 0), or count when none does. */
static size_t downbeat_segment(const HarmonySegment *segments, size_t count, int measure) {
    for (size_t i = 0; i < count; i++) {
        if (measure_of(&segments[i]) == measure && starts_measure(&segments[i])) {
            return i;
        }
    }
    return count;
}

static bool is_second_inversion_triad(const HarmonySegment *segment) {
    return segment->valid && segment->input->inversion == 2 && segment->input->interval_count == 3;
}

/* --- half readings --- */

typedef struct {
    HarmonyCadenceSpec *items;
    size_t count;
} SpecList;

static bool has_arrival(const SpecList *list, size_t arrival_first) {
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].arrival_first == arrival_first) {
            return true;
        }
    }
    return false;
}

/* Adds a HALF over [dominant_first, arrival_last] with the dominant's own segments; kept in order of arrival. */
static void add_half(SpecList *list, const HarmonySegment *segments, size_t first, size_t last,
                     const HarmonyCadenceResult *six_four_of) {
    HarmonyCadenceSpec spec;
    memset(&spec, 0, sizeof(spec));
    spec.type = CADENCE_TYPE_HALF;
    spec.dominant_first = first;
    spec.dominant_last = last;
    spec.arrival_first = first;
    spec.arrival_last = last;
    spec.soprano_degree = segments[first].input->soprano_degree;
    spec.share = 1000;
    if (six_four_of != NULL && six_four_of->has_six_four) {
        spec.has_six_four = true;
        spec.six_four_first = six_four_of->six_four_first;
        spec.six_four_last = six_four_of->six_four_last;
    }
    size_t at = list->count;
    while (at > 0 && list->items[at - 1].arrival_first > first) {
        list->items[at] = list->items[at - 1];
        at--;
    }
    list->items[at] = spec;
    list->count++;
}

static bool found_half_arrives_at(const HarmonyCadences *found, size_t segment) {
    for (size_t i = 0; found != NULL && i < found->count; i++) {
        if (found->items[i].type == CADENCE_TYPE_HALF && found->items[i].arrival_first == segment) {
            return true;
        }
    }
    return false;
}

static bool ends_on_v_type(CadenceType type) {
    return type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
           type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED || type == CADENCE_TYPE_SUBVERTED;
}

HarmonyCadenceAnalysisStatus harmony_analysis_half_readings(const HarmonySegment *segments, size_t count,
                                                            const HarmonyCadenceConfig *config,
                                                            const HarmonyCadences *found, HarmonyCadences *out) {
    if (out == NULL || (segments == NULL && count > 0)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    HarmonyCadenceConfig defaults = harmony_cadence_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (!harmony_cadence_config_is_valid(config)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG;
    }
    size_t found_count = found != NULL ? found->count : 0;
    SpecList list = {(HarmonyCadenceSpec *)calloc(count + found_count + 1, sizeof(HarmonyCadenceSpec)), 0};
    if (list.items == NULL) {
        return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    }

    /* END ON V */
    for (size_t k = 0; k < found_count; k++) {
        const HarmonyCadenceResult *f = &found->items[k];
        size_t d = f->dominant_first;
        size_t a = f->arrival_first;
        if (!ends_on_v_type(f->type) || a >= count || d >= a || !starts_measure(&segments[a]) ||
            measure_of(&segments[d]) != measure_of(&segments[a]) - 1 || found_half_arrives_at(found, d) ||
            has_arrival(&list, d)) {
            continue;
        }
        add_half(&list, segments, d, a - 1, f);
    }

    /* DOMINANT AGAIN */
    for (size_t i = 0; i < count; i++) {
        if (!segments[i].valid || !harmony_cadence_is_dominant_root(&segments[i]) || !starts_measure(&segments[i]) ||
            found_half_arrives_at(found, i) || has_arrival(&list, i)) {
            continue;
        }
        size_t j = i;
        while (j > 0 && (harmony_cadence_is_dominant_root(&segments[j - 1]) ||
                         is_second_inversion_triad(&segments[j - 1]))) {
            j--;
        }
        if (j == 0) {
            continue;
        }
        const HarmonySegment *before = &segments[j - 1];
        int degree = before->chord.degree;
        if (!before->valid || !(degree == 1 || degree == 2 || degree == 3 || degree == 4 || degree == 6)) {
            continue;
        }
        size_t next = downbeat_segment(segments, count, measure_of(&segments[i]) + 1);
        if (next < count && harmony_cadence_is_tonic_root(&segments[next])) {
            continue;
        }
        add_half(&list, segments, i, i, NULL);
    }

    HarmonyCadences result = {NULL, 0};
    if (list.count > 0) {
        result.items = (HarmonyCadenceResult *)calloc(list.count, sizeof(HarmonyCadenceResult));
        if (result.items == NULL) {
            free(list.items);
            return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i < list.count; i++) {
        HarmonyCadenceAnalysisStatus status = harmony_cadence_result_build(segments, &list.items[i], &result.items[i]);
        if (status != HARMONY_CADENCE_ANALYSIS_OK) {
            harmony_cadences_destroy(&result);
            free(list.items);
            return status;
        }
        result.count++;
    }
    free(list.items);
    *out = result;
    return HARMONY_CADENCE_ANALYSIS_OK;
}

/* --- claims --- */

typedef struct {
    HarmonyCadenceResult *result; /* in the working sets; NULL once taken */
    int measure;                  /* the point's measure */
} Candidate;

static bool is_authentic_family(CadenceType type) {
    return type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC ||
           type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED;
}

/* How strongly a type closes a phrase; 0 for a type that is no goal. */
static int strength_of(CadenceType type) {
    if (type == CADENCE_TYPE_PERFECT_AUTHENTIC) {
        return 3;
    }
    if (type == CADENCE_TYPE_IMPERFECT_AUTHENTIC || type == CADENCE_TYPE_AUTHENTIC_UNSPECIFIED) {
        return 2;
    }
    return type == CADENCE_TYPE_HALF ? 1 : 0;
}

static const HarmonyPhrasePoint *point_at(const HarmonyPhrasePoint *points, size_t point_count, int measure) {
    for (size_t i = 0; i < point_count; i++) {
        if (points[i].measure == measure) {
            return &points[i];
        }
    }
    return NULL;
}

/* The measure in which the window of segment `index` ends: the next segment's measure, or the one before it when
   that segment starts on a downbeat. */
static int window_end_measure(const HarmonySegment *segments, size_t count, size_t index) {
    if (index + 1 >= count) {
        return measure_of(&segments[index]);
    }
    const HarmonySegment *next = &segments[index + 1];
    return starts_measure(next) ? measure_of(next) - 1 : measure_of(next);
}

/* Step a: the point of a result; false when it has none. */
static bool point_of(const HarmonySegment *segments, size_t count, const HarmonyPhrasePoint *points,
                     size_t point_count, const HarmonyCadenceResult *result, int *measure) {
    if (result->arrival_first >= count) {
        return false;
    }
    int m = measure_of(&segments[result->arrival_first]);
    if (point_at(points, point_count, m) != NULL) {
        *measure = m;
        return true;
    }
    int end = window_end_measure(segments, count, result->arrival_first);
    if (end > m && point_at(points, point_count, end) != NULL) {
        *measure = end;
        return true;
    }
    return false;
}

static double start_in_whole_notes(const HarmonySegment *segment) {
    const MusicalTime *time = time_span_start(segment->input->span);
    return (double)musical_time_numerator(time) / (double)musical_time_denominator(time);
}

static HarmonyCadenceAnalysisStatus copy_cadences(const HarmonySegment *segments, const HarmonyCadenceConfig *config,
                                                  const HarmonyCadences *from, HarmonyCadences *to) {
    to->items = NULL;
    to->count = 0;
    if (from == NULL || from->count == 0) {
        return HARMONY_CADENCE_ANALYSIS_OK;
    }
    to->items = (HarmonyCadenceResult *)calloc(from->count, sizeof(HarmonyCadenceResult));
    if (to->items == NULL) {
        return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < from->count; i++) {
        HarmonyCadenceSpec spec;
        harmony_cadence_spec_of(&from->items[i], config, &spec);
        HarmonyCadenceAnalysisStatus status = harmony_cadence_result_build(segments, &spec, &to->items[i]);
        if (status != HARMONY_CADENCE_ANALYSIS_OK) {
            harmony_cadences_destroy(to);
            return status;
        }
        to->count++;
    }
    return HARMONY_CADENCE_ANALYSIS_OK;
}

/* Replaces *result by the result of `spec`. */
static HarmonyCadenceAnalysisStatus replace_result(const HarmonySegment *segments, const HarmonyCadenceSpec *spec,
                                                   HarmonyCadenceResult *result) {
    HarmonyCadenceResult built;
    HarmonyCadenceAnalysisStatus status = harmony_cadence_result_build(segments, spec, &built);
    if (status != HARMONY_CADENCE_ANALYSIS_OK) {
        return status;
    }
    harmony_cadence_result_destroy(result);
    *result = built;
    return HARMONY_CADENCE_ANALYSIS_OK;
}

/* A HALF over the one segment `index`. */
static void half_spec_at(const HarmonySegment *segments, size_t index, HarmonyCadenceSpec *spec) {
    memset(spec, 0, sizeof(*spec));
    spec->type = CADENCE_TYPE_HALF;
    spec->dominant_first = index;
    spec->dominant_last = index;
    spec->arrival_first = index;
    spec->arrival_last = index;
    spec->soprano_degree = segments[index].input->soprano_degree;
    spec->share = 1000;
}

/* Step b, for every HALF of `set`. */
static HarmonyCadenceAnalysisStatus move_halves(const HarmonySegment *segments, size_t count,
                                                const HarmonyPhrasePoint *points, size_t point_count,
                                                HarmonyCadences *set) {
    for (size_t i = 0; i < set->count; i++) {
        HarmonyCadenceResult *r = &set->items[i];
        int measure;
        if (r->type != CADENCE_TYPE_HALF || r->arrival_first >= count ||
            point_of(segments, count, points, point_count, r, &measure)) {
            continue;
        }
        int next = measure_of(&segments[r->arrival_first]) + 1;
        size_t s = downbeat_segment(segments, count, next);
        if (point_at(points, point_count, next) == NULL || s >= count || s <= r->arrival_last ||
            !harmony_cadence_is_dominant_root(&segments[s])) {
            continue;
        }
        bool between = true;
        for (size_t k = r->arrival_last + 1; k < s && between; k++) {
            between = harmony_cadence_is_dominant_root(&segments[k]) || is_second_inversion_triad(&segments[k]);
        }
        if (!between) {
            continue;
        }
        HarmonyCadenceSpec spec;
        half_spec_at(segments, s, &spec);
        HarmonyCadenceAnalysisStatus status = replace_result(segments, &spec, r);
        if (status != HARMONY_CADENCE_ANALYSIS_OK) {
            return status;
        }
    }
    return HARMONY_CADENCE_ANALYSIS_OK;
}

/* Step c, the boundaries: the section starts after the unit ends that have a section after them and after the half
   points, then the half points keep only their HALF reading and a SUBVERTED into the next measure becomes a HALF. */
static HarmonyCadenceAnalysisStatus add_boundaries(const HarmonySegment *segments, size_t count,
                                                   const HarmonyCadenceConfig *config,
                                                   const HarmonyPhrasePoint *points, size_t point_count,
                                                   HarmonyCadences *set) {
    double *unit_starts = (double *)calloc(point_count + 1, sizeof(double));
    double *half_starts = (double *)calloc(point_count + 1, sizeof(double));
    size_t *half_segments = (size_t *)calloc(point_count + 1, sizeof(size_t));
    if (unit_starts == NULL || half_starts == NULL || half_segments == NULL) {
        free(unit_starts);
        free(half_starts);
        free(half_segments);
        return HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
    }
    size_t unit_n = 0;
    size_t half_n = 0;
    for (size_t i = 0; i < point_count; i++) {
        size_t s = downbeat_segment(segments, count, points[i].measure + 1);
        if (s >= count) {
            continue;
        }
        if (!points[i].unit_end) {
            half_starts[half_n] = start_in_whole_notes(&segments[s]);
            half_segments[half_n++] = s;
        } else if (points[i].section_follows) {
            unit_starts[unit_n++] = start_in_whole_notes(&segments[s]);
        }
    }
    HarmonyCadenceAnalysisStatus status = HARMONY_CADENCE_ANALYSIS_OK;
    if (unit_n > 0) {
        status = harmony_analysis_add_boundary_cadences(segments, count, config, unit_starts, unit_n, set);
    }
    if (status == HARMONY_CADENCE_ANALYSIS_OK && half_n > 0) {
        status = harmony_analysis_add_boundary_cadences(segments, count, config, half_starts, half_n, set);
    }
    /* at a half point only the HALF reading stays: the authentic arrival opens the consequent */
    for (size_t h = 0; status == HARMONY_CADENCE_ANALYSIS_OK && h < half_n; h++) {
        for (size_t i = 0; status == HARMONY_CADENCE_ANALYSIS_OK && i < set->count; i++) {
            HarmonyCadenceResult *r = &set->items[i];
            const HarmonyCadenceAlternative *alt = &r->alternative;
            if (!alt->present || alt->reading != HARMONY_CADENCE_READING_CADENCE) {
                continue;
            }
            HarmonyCadenceSpec spec;
            harmony_cadence_spec_of(r, config, &spec);
            if (r->type == CADENCE_TYPE_HALF && is_authentic_family(alt->type) && alt->arrival_first == half_segments[h]) {
                /* the primary is the HALF reading already */
            } else if (is_authentic_family(r->type) && alt->type == CADENCE_TYPE_HALF &&
                       r->arrival_first == half_segments[h]) {
                spec.type = CADENCE_TYPE_HALF;
                spec.arrival_first = alt->arrival_first;
                spec.arrival_last = alt->arrival_last;
                spec.soprano_degree = alt->soprano_degree;
                spec.penalty = 0.0;
            } else {
                continue;
            }
            spec.has_alt = false;
            spec.share = 1000;
            status = replace_result(segments, &spec, r);
        }
    }
    free(unit_starts);
    free(half_starts);
    free(half_segments);
    return status;
}

/* A found SUBVERTED that arrives on the downbeat after a point whose dominant is in that point is replaced by the
   HALF reading of its dominant. */
static HarmonyCadenceAnalysisStatus replace_subverted(const HarmonySegment *segments, size_t count,
                                                      const HarmonyPhrasePoint *points, size_t point_count,
                                                      HarmonyCadences *set) {
    for (size_t i = 0; i < set->count; i++) {
        HarmonyCadenceResult *r = &set->items[i];
        size_t a = r->arrival_first;
        size_t d = r->dominant_first;
        if (r->type != CADENCE_TYPE_SUBVERTED || a >= count || d >= a || !starts_measure(&segments[a])) {
            continue;
        }
        int m = measure_of(&segments[a]) - 1;
        if (point_at(points, point_count, m) == NULL || measure_of(&segments[d]) != m) {
            continue;
        }
        HarmonyCadenceSpec spec;
        harmony_cadence_spec_of(r, NULL, &spec);
        spec.type = CADENCE_TYPE_HALF;
        spec.dominant_first = d;
        spec.dominant_last = a - 1;
        spec.arrival_first = d;
        spec.arrival_last = a - 1;
        spec.soprano_degree = segments[d].input->soprano_degree;
        spec.post_cadential = false;
        spec.penalty = 0.0;
        spec.share = 1000;
        spec.has_alt = false;
        HarmonyCadenceAnalysisStatus status = replace_result(segments, &spec, r);
        if (status != HARMONY_CADENCE_ANALYSIS_OK) {
            return status;
        }
    }
    return HARMONY_CADENCE_ANALYSIS_OK;
}

/* Whether candidate `b` beats candidate `a` at one point: stronger, or as strong and arriving later. */
static bool beats(const HarmonyCadenceResult *b, const HarmonyCadenceResult *a) {
    int sb = strength_of(b->type);
    int sa = strength_of(a->type);
    return sb > sa || (sb == sa && b->arrival_first > a->arrival_first);
}

/* The candidate that closes `point`, or NULL; a HALF is skipped when `skip_half`. */
static Candidate *best_at(Candidate *cands, size_t cand_count, int measure, bool skip_half) {
    Candidate *best = NULL;
    for (size_t i = 0; i < cand_count; i++) {
        if (cands[i].result == NULL || cands[i].measure != measure || strength_of(cands[i].result->type) == 0 ||
            (skip_half && cands[i].result->type == CADENCE_TYPE_HALF)) {
            continue;
        }
        if (best == NULL || beats(cands[i].result, best->result)) {
            best = &cands[i];
        }
    }
    return best;
}

static int compare_arrival(const void *x, const void *y) {
    const HarmonyCadenceResult *a = (const HarmonyCadenceResult *)x;
    const HarmonyCadenceResult *b = (const HarmonyCadenceResult *)y;
    return a->arrival_first < b->arrival_first ? -1 : (a->arrival_first > b->arrival_first ? 1 : 0);
}

HarmonyCadenceAnalysisStatus harmony_analysis_claim_cadences(const HarmonySegment *segments, size_t count,
                                                             const HarmonyCadenceConfig *config,
                                                             const HarmonyCadences *found,
                                                             const HarmonyCadences *readings,
                                                             const HarmonyPhrasePoint *points, size_t point_count,
                                                             HarmonyCadences *out) {
    if (out == NULL || (segments == NULL && count > 0) || (points == NULL && point_count > 0)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    HarmonyCadenceConfig defaults = harmony_cadence_config_default();
    if (config == NULL) {
        config = &defaults;
    }
    if (!harmony_cadence_config_is_valid(config)) {
        return HARMONY_CADENCE_ANALYSIS_ERR_BAD_CONFIG;
    }
    HarmonyCadences none = {NULL, 0};
    if (point_count == 0 || count == 0) {
        *out = none;
        return HARMONY_CADENCE_ANALYSIS_OK;
    }

    HarmonyCadences work = {NULL, 0};
    HarmonyCadences extra = {NULL, 0};
    Candidate *cands = NULL;
    HarmonyCadenceResult *kept = NULL;
    size_t kept_count = 0;
    HarmonyCadenceAnalysisStatus status = copy_cadences(segments, config, found, &work);
    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        status = copy_cadences(segments, config, readings, &extra);
    }
    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        status = move_halves(segments, count, points, point_count, &work);
    }
    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        status = move_halves(segments, count, points, point_count, &extra);
    }
    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        status = add_boundaries(segments, count, config, points, point_count, &work);
    }
    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        status = replace_subverted(segments, count, points, point_count, &work);
    }

    /* d: the candidates with their points */
    size_t cand_count = 0;
    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        cands = (Candidate *)calloc(work.count + extra.count + 1, sizeof(Candidate));
        kept = (HarmonyCadenceResult *)calloc(point_count + work.count + 1, sizeof(HarmonyCadenceResult));
        if (cands == NULL || kept == NULL) {
            status = HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
        }
    }
    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        HarmonyCadences *sets[2] = {&work, &extra};
        for (size_t k = 0; k < 2; k++) {
            for (size_t i = 0; i < sets[k]->count; i++) {
                int measure;
                if (strength_of(sets[k]->items[i].type) > 0 &&
                    point_of(segments, count, points, point_count, &sets[k]->items[i], &measure)) {
                    cands[cand_count].result = &sets[k]->items[i];
                    cands[cand_count].measure = measure;
                    cand_count++;
                }
            }
        }

        /* unit ends first, then half points; one result per point */
        bool *unit_has_half = (bool *)calloc(point_count, sizeof(bool));
        if (unit_has_half == NULL) {
            status = HARMONY_CADENCE_ANALYSIS_ERR_ALLOC;
        }
        for (int pass = 0; status == HARMONY_CADENCE_ANALYSIS_OK && pass < 2; pass++) {
            for (size_t i = 0; i < point_count; i++) {
                if (points[i].unit_end != (pass == 0)) {
                    continue;
                }
                bool skip_half = false;
                if (pass == 1) {
                    for (size_t u = 0; u < point_count; u++) {
                        if (points[u].unit_end && points[u].unit == points[i].unit && unit_has_half[u]) {
                            skip_half = true;
                        }
                    }
                }
                Candidate *best = best_at(cands, cand_count, points[i].measure, skip_half);
                if (best == NULL || best->result == NULL) {
                    continue;
                }
                if (pass == 0 && best->result->type == CADENCE_TYPE_HALF) {
                    unit_has_half[i] = true;
                }
                kept[kept_count++] = *best->result;
                memset(best->result, 0, sizeof(*best->result));
                /* the same measure is never claimed twice, even by a second point with that measure */
                for (size_t c = 0; c < cand_count; c++) {
                    if (cands[c].measure == points[i].measure) {
                        cands[c].result = NULL;
                    }
                }
            }
        }
        free(unit_has_half);
    }

    /* f: a post-cadential plagal directly after a kept PAC */
    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        size_t pac_count = kept_count;
        for (size_t i = 0; i < work.count; i++) {
            const HarmonyCadenceResult *r = &work.items[i];
            if (r->type != CADENCE_TYPE_PLAGAL || !r->post_cadential || r->cadence == NULL) {
                continue;
            }
            for (size_t k = 0; k < pac_count; k++) {
                if (kept[k].type == CADENCE_TYPE_PERFECT_AUTHENTIC && kept[k].arrival_last + 1 == r->dominant_first) {
                    kept[kept_count++] = work.items[i];
                    memset(&work.items[i], 0, sizeof(work.items[i]));
                    break;
                }
            }
        }
    }

    if (status == HARMONY_CADENCE_ANALYSIS_OK) {
        qsort(kept, kept_count, sizeof(HarmonyCadenceResult), compare_arrival);
        out->items = kept_count > 0 ? kept : NULL;
        out->count = kept_count;
        if (kept_count == 0) {
            free(kept);
        }
        kept = NULL;
    }
    for (size_t i = 0; kept != NULL && i < kept_count; i++) {
        harmony_cadence_result_destroy(&kept[i]);
    }
    free(kept);
    free(cands);
    harmony_cadences_destroy(&work);
    harmony_cadences_destroy(&extra);
    return status;
}
