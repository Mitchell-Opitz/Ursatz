#include "analysis_cadence_claims.h"

#include <stdlib.h>
#include <string.h>

#include "analysis_cadence_build.h"
#include "time/duration.h"
#include "time/metrical_position.h"

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
