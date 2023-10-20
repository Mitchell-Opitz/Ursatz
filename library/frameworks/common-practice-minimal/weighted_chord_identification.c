#include "weighted_chord_identification.h"

#include <stdio.h>
#include <stdlib.h>

#include "chord_vocabulary.h"
#include "chord_vocabulary_seventh.h"
#include "identity/entity_id.h"

#define CONFIDENCE_DENOMINATOR 1000000UL

static void clear_result(CommonPracticeWeightedResult *result) {
    result->matched = false;
    result->best_score = 0.0;
    result->runner_up_score = 0.0;
    result->uncertainty = 1.0;
    result->best_chord = NULL;
    result->runner_up_chord = NULL;
    result->leftover = NULL;
    result->leftover_count = 0;
    result->best.degree = 0;
    result->runner_up.degree = 0;
}

void common_practice_weighted_result_destroy(CommonPracticeWeightedResult *result) {
    if (result == NULL) {
        return;
    }
    chord_destroy(result->best_chord);
    chord_destroy(result->runner_up_chord);
    free(result->leftover);
    result->best_chord = NULL;
    result->runner_up_chord = NULL;
    result->leftover = NULL;
    result->leftover_count = 0;
}

void common_practice_weighted_results_destroy(CommonPracticeWeightedResults *results) {
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < results->count; i++) {
        common_practice_weighted_result_destroy(&results->items[i]);
    }
    free(results->items);
    results->items = NULL;
    results->count = 0;
}

/* A confidence in [0, 1] as millionths. No libm: round half up by hand. */
static unsigned long millionths(double confidence) {
    if (!(confidence > 0.0)) {
        return 0;
    }
    if (confidence >= 1.0) {
        return CONFIDENCE_DENOMINATOR;
    }
    return (unsigned long)(confidence * (double)CONFIDENCE_DENOMINATOR + 0.5);
}

/* The candidate's quality as root-relative semitones. In a major key the triads on degrees 1 4 5 are major,
   2 3 6 minor, 7 diminished; the four seventh qualities are fixed by their name. */
static size_t candidate_intervals(const CommonPracticeRankedChord *candidate, int intervals[4]) {
    static const int MAJOR_TRIAD[3] = {0, 4, 7};
    static const int MINOR_TRIAD[3] = {0, 3, 7};
    static const int DIMINISHED_TRIAD[3] = {0, 3, 6};
    static const int SEVENTHS[COMMON_PRACTICE_SEVENTH_COUNT][4] = {
        {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10}, {0, 3, 6, 10}};
    if (candidate->is_seventh) {
        for (int i = 0; i < 4; i++) {
            intervals[i] = SEVENTHS[candidate->seventh][i];
        }
        return 4;
    }
    const int *triad = MAJOR_TRIAD;
    switch (candidate->triad) {
    case COMMON_PRACTICE_TRIAD_II:
    case COMMON_PRACTICE_TRIAD_III:
    case COMMON_PRACTICE_TRIAD_VI:
        triad = MINOR_TRIAD;
        break;
    case COMMON_PRACTICE_TRIAD_VII_DIM:
        triad = DIMINISHED_TRIAD;
        break;
    default:
        break;
    }
    for (int i = 0; i < 3; i++) {
        intervals[i] = triad[i];
    }
    return 3;
}

/* Re-wraps a built Chord with the structure the kernel carries: root, quality intervals, bass. */
static bool attach_structure(const CommonPracticeRankedChord *candidate, int bass_pitch_class, Chord **chord) {
    ChordStructure structure;
    structure.root_pitch_class = candidate->root_pitch_class;
    structure.bass_pitch_class = bass_pitch_class;
    int intervals[4];
    structure.interval_count = candidate_intervals(candidate, intervals);
    for (size_t i = 0; i < structure.interval_count; i++) {
        structure.root_relative_intervals[i] = intervals[i];
    }
    char label[256];
    snprintf(label, sizeof(label), "%s", chord_label(*chord));
    Interpretation *interpretation = chord_take_interpretation(*chord);
    Chord *wrapped = NULL;
    if (chord_create_with_structure(interpretation, label, &structure, &wrapped) != CHORD_OK) {
        interpretation_destroy(interpretation);
        *chord = NULL;
        return false;
    }
    *chord = wrapped;
    return true;
}

static CommonPracticeWeightedStatus build_chord(const CommonPracticeRankedChord *candidate, const char *segment_id,
                                                int bass_pitch_class, double confidence, const char *suffix,
                                                Chord **out) {
    EntityID *target = NULL;
    if (entity_id_create(segment_id, &target) != ENTITY_ID_OK) {
        return COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
    }
    ChordResult built = candidate->is_seventh
                            ? common_practice_build_seventh_chord_with_confidence(
                                  candidate->seventh, target, millionths(confidence), CONFIDENCE_DENOMINATOR,
                                  suffix, out)
                            : common_practice_build_triad_chord_with_confidence(
                                  candidate->triad, target, millionths(confidence), CONFIDENCE_DENOMINATOR, suffix,
                                  out);
    if (built != CHORD_OK) {
        entity_id_destroy(target); /* the builders take the id on success only */
        return COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
    }
    if (!attach_structure(candidate, bass_pitch_class, out)) {
        *out = NULL;
        return COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
    }
    return COMMON_PRACTICE_WEIGHTED_OK;
}

/* Pass 1 for one segment: weigh, rank, and (when it matches) build the two Chords. */
static CommonPracticeWeightedStatus identify_one(const CommonPracticeSegment *segment, int tonic_pitch_class,
                                                 CommonPracticeKeyMode mode,
                                                 const CommonPracticeWeightedConfig *config,
                                                 CommonPracticeWeightedResult *out) {
    CommonPracticeWeightedResult result;
    clear_result(&result);
    if (segment->member_count == 0) {
        *out = result;
        return COMMON_PRACTICE_WEIGHTED_OK;
    }

    CommonPracticeWeights weights;
    CommonPracticeWeightedStatus status =
        common_practice_weight_segment(segment->members, segment->member_count, config, &weights);
    if (status != COMMON_PRACTICE_WEIGHTED_OK) {
        return status;
    }
    CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
    size_t ranked_count = 0;
    status = common_practice_rank_candidates(&weights, tonic_pitch_class, mode, config, ranked, &ranked_count);
    if (status != COMMON_PRACTICE_WEIGHTED_OK) {
        return status;
    }
    if (ranked_count == 0) {
        *out = result; /* no root sounds: no candidate, and no match */
        return COMMON_PRACTICE_WEIGHTED_OK;
    }

    /* The runner-up is the best candidate on a DIFFERENT root: a triad and the seventh built on it are one
       harmony, not rivals. With none, there is no rival and the uncertainty is 0. */
    size_t runner_index = 1;
    while (runner_index < ranked_count && ranked[runner_index].root_pitch_class == ranked[0].root_pitch_class) {
        runner_index++;
    }
    bool has_runner_up = runner_index < ranked_count;
    result.best = ranked[0];
    result.best_score = ranked[0].score;
    if (has_runner_up) {
        result.runner_up = ranked[runner_index];
        result.runner_up_score = ranked[runner_index].score;
    }
    if (result.best_score > 0.0) {
        double ratio = result.runner_up_score / result.best_score;
        result.uncertainty = ratio < 0.0 ? 0.0 : (ratio > 1.0 ? 1.0 : ratio);
    } else {
        result.uncertainty = 1.0;
    }
    result.matched = result.best_score >= config->min_coverage;

    if (result.matched) {
        if (segment->id == NULL) {
            return COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT;
        }
        status = build_chord(&result.best, segment->id, weights.bass_pitch_class, 1.0 - result.uncertainty, NULL, &result.best_chord);
        if (status == COMMON_PRACTICE_WEIGHTED_OK && has_runner_up) {
            status = build_chord(&result.runner_up, segment->id, weights.bass_pitch_class, result.uncertainty, "-runner-up",
                                 &result.runner_up_chord);
        }
        if (status != COMMON_PRACTICE_WEIGHTED_OK) {
            common_practice_weighted_result_destroy(&result);
            return status;
        }
    }
    *out = result;
    return COMMON_PRACTICE_WEIGHTED_OK;
}

CommonPracticeWeightedStatus common_practice_identify_weighted(const CommonPracticeSegment *segment,
                                                                int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                                const CommonPracticeWeightedConfig *config,
                                                                CommonPracticeWeightedResult *out) {
    if (segment == NULL || out == NULL || (segment->members == NULL && segment->member_count > 0)) {
        return COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT;
    }
    CommonPracticeWeightedConfig defaults = common_practice_weighted_config_default();
    return identify_one(segment, tonic_pitch_class, mode, config != NULL ? config : &defaults, out);
}

CommonPracticeWeightedStatus common_practice_identify_weighted_sequence(const CommonPracticeSegment *segments,
                                                                         size_t segment_count,
                                                                         int tonic_pitch_class,
                                                                         CommonPracticeKeyMode mode,
                                                                         const CommonPracticeWeightedConfig *config,
                                                                         CommonPracticeWeightedResults *out) {
    if ((segments == NULL && segment_count > 0) || out == NULL) {
        return COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT;
    }
    CommonPracticeWeightedConfig defaults = common_practice_weighted_config_default();
    const CommonPracticeWeightedConfig *effective = config != NULL ? config : &defaults;

    CommonPracticeWeightedResults results = {NULL, 0};
    unsigned *masks = NULL;
    if (segment_count > 0) {
        results.items = (CommonPracticeWeightedResult *)calloc(segment_count, sizeof(CommonPracticeWeightedResult));
        masks = (unsigned *)calloc(segment_count, sizeof(unsigned));
        if (results.items == NULL || masks == NULL) {
            free(results.items);
            free(masks);
            return COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
        }
    }

    /* Pass 1: choose a chord for every segment. */
    CommonPracticeWeightedStatus status = COMMON_PRACTICE_WEIGHTED_OK;
    for (size_t i = 0; i < segment_count && status == COMMON_PRACTICE_WEIGHTED_OK; i++) {
        status = identify_one(&segments[i], tonic_pitch_class, mode, effective, &results.items[i]);
        if (status == COMMON_PRACTICE_WEIGHTED_OK) {
            results.count = i + 1;
            if (results.items[i].matched) {
                masks[i] = common_practice_ranked_chord_mask(&results.items[i].best, tonic_pitch_class);
            }
        }
    }

    /* Pass 2: label each matched segment's leftover tones with the neighbors and the next chord in view. */
    for (size_t i = 0; i < segment_count && status == COMMON_PRACTICE_WEIGHTED_OK; i++) {
        if (!results.items[i].matched) {
            continue;
        }
        CommonPracticeToneLabel *labels = NULL;
        size_t label_count = 0;
        if (common_practice_label_leftover_tones(segments, masks, segment_count, i, &labels, &label_count) !=
            COMMON_PRACTICE_TONES_OK) {
            status = COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
            break;
        }
        if (label_count > 0) {
            results.items[i].leftover =
                (CommonPracticeLeftoverTone *)malloc(label_count * sizeof(CommonPracticeLeftoverTone));
            if (results.items[i].leftover == NULL) {
                free(labels);
                status = COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
                break;
            }
            for (size_t k = 0; k < label_count; k++) {
                results.items[i].leftover[k].note_id = segments[i].members[labels[k].member_index].note_id;
                results.items[i].leftover[k].classified = labels[k].classified;
                results.items[i].leftover[k].tone = labels[k].tone;
            }
            results.items[i].leftover_count = label_count;
        }
        free(labels);
    }
    free(masks);

    if (status != COMMON_PRACTICE_WEIGHTED_OK) {
        common_practice_weighted_results_destroy(&results);
        return status;
    }
    results.count = segment_count;
    *out = results;
    return COMMON_PRACTICE_WEIGHTED_OK;
}
