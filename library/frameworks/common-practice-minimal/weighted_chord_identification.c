#include "weighted_chord_identification.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "chord_vocabulary.h"
#include "chord_vocabulary_seventh.h"
#include "core/rational.h"
#include "identity/entity_id.h"
#include "pitch/midi_pitch.h"

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
    result->context = COMMON_PRACTICE_CONTEXT_NONE;
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

/* Re-wraps a built Chord with the structure the kernel carries: root, quality intervals, bass. */
static bool attach_structure(const CommonPracticeRankedChord *candidate, int bass_pitch_class, Chord **chord) {
    ChordStructure structure;
    structure.root_pitch_class = candidate->root_pitch_class;
    structure.bass_pitch_class = bass_pitch_class;
    int intervals[4];
    structure.interval_count = common_practice_ranked_chord_intervals(candidate, intervals);
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
        common_practice_weight_segment(segment->members, segment->member_count, config, &weights,
                                       segment->has_bass ? &segment->bass_pitch : NULL);
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

/* --- context --- */

/* The pitch classes a segment sounds, as a 12-bit mask, and its bass pitch class (-1 when empty). */
static unsigned segment_mask(const CommonPracticeSegment *segment, int *bass_pitch_class) {
    unsigned mask = 0;
    int bass = 0;
    for (size_t i = 0; i < segment->member_count; i++) {
        mask |= 1u << pitch_class_of_midi(segment->members[i].pitch);
        if (i == 0 || segment->members[i].pitch < bass) {
            bass = segment->members[i].pitch;
        }
    }
    if (segment->has_bass) {
        *bass_pitch_class = pitch_class_of_midi(segment->bass_pitch);
        return mask;
    }
    *bass_pitch_class = segment->member_count > 0 ? pitch_class_of_midi(bass) : -1;
    return mask;
}

static int bit_count(unsigned mask) {
    int count = 0;
    for (; mask != 0; mask &= mask - 1) {
        count++;
    }
    return count;
}

/* No match, or fewer than three of the best chord's pitch classes sound, unless two do with the root in the bass
   (a root in the bass is its own evidence, as for a triad with the fifth left out). A single note is incomplete. */
static bool incomplete(const CommonPracticeWeightedResult *result, unsigned chord_mask, unsigned sounding,
                       int bass_pitch_class) {
    int tones = bit_count(chord_mask & sounding);
    return !result->matched || tones < 2 || (tones < 3 && result->best.root_pitch_class != bass_pitch_class);
}

/* Replaces `into` with the reading of `from`, built over `segment`. */
static CommonPracticeWeightedStatus take_reading(const CommonPracticeWeightedResult *from,
                                                 const CommonPracticeSegment *segment, int bass_pitch_class,
                                                 CommonPracticeContextReading context,
                                                 CommonPracticeWeightedResult *into) {
    if (segment->id == NULL) {
        return COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT;
    }
    CommonPracticeWeightedResult result;
    clear_result(&result);
    result.matched = true;
    result.best = from->best;
    result.runner_up = from->runner_up;
    result.best_score = from->best_score;
    result.runner_up_score = from->runner_up_score;
    result.uncertainty = from->uncertainty;
    result.context = context;
    CommonPracticeWeightedStatus status = build_chord(&result.best, segment->id, bass_pitch_class,
                                                      1.0 - result.uncertainty, NULL, &result.best_chord);
    if (status == COMMON_PRACTICE_WEIGHTED_OK && from->runner_up_chord != NULL) {
        status = build_chord(&result.runner_up, segment->id, bass_pitch_class, result.uncertainty, "-runner-up",
                             &result.runner_up_chord);
    }
    if (status != COMMON_PRACTICE_WEIGHTED_OK) {
        common_practice_weighted_result_destroy(&result);
        return status;
    }
    common_practice_weighted_result_destroy(into);
    *into = result;
    return COMMON_PRACTICE_WEIGHTED_OK;
}

/* Reads each run of two or more segments inside one beat, none complete alone, as one arpeggiated chord when the
   union of their members is complete. masks[] is kept in step. */
static CommonPracticeWeightedStatus read_arpeggios(const CommonPracticeSegment *segments, size_t count,
                                                   int tonic_pitch_class, CommonPracticeKeyMode mode,
                                                   const CommonPracticeWeightedConfig *config,
                                                   CommonPracticeWeightedResults *results, unsigned *masks) {
    size_t run_start = 0;
    while (run_start < count) {
        size_t run_end = run_start + 1;
        while (run_end < count && segments[run_start].beat != 0 && segments[run_end].beat == segments[run_start].beat) {
            run_end++;
        }
        size_t first = run_start;
        run_start = run_end;
        if (run_end - first < 2) {
            continue;
        }

        size_t member_total = 0;
        bool any_complete = false;
        for (size_t i = first; i < run_end; i++) {
            int bass_pitch_class = -1;
            unsigned sounding = segment_mask(&segments[i], &bass_pitch_class);
            any_complete = any_complete || !incomplete(&results->items[i], masks[i], sounding, bass_pitch_class);
            member_total += segments[i].member_count;
        }
        if (any_complete || member_total == 0) {
            continue;
        }

        CommonPracticeMember *members = (CommonPracticeMember *)malloc(member_total * sizeof(CommonPracticeMember));
        if (members == NULL) {
            return COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
        }
        size_t next = 0;
        for (size_t i = first; i < run_end; i++) {
            for (size_t k = 0; k < segments[i].member_count; k++) {
                members[next++] = segments[i].members[k];
            }
        }
        CommonPracticeSegment joined = {members, member_total, segments[first].id, segments[first].beat,
                                        segments[first].has_bass, segments[first].bass_pitch};
        CommonPracticeWeightedResult arpeggio;
        CommonPracticeWeightedStatus status = identify_one(&joined, tonic_pitch_class, mode, config, &arpeggio);
        if (status != COMMON_PRACTICE_WEIGHTED_OK) {
            free(members);
            return status;
        }
        int joined_bass = -1;
        unsigned joined_sounding = segment_mask(&joined, &joined_bass);
        unsigned joined_mask = arpeggio.matched ? common_practice_ranked_chord_mask(&arpeggio.best, tonic_pitch_class) : 0;
        free(members);
        if (incomplete(&arpeggio, joined_mask, joined_sounding, joined_bass) ||
            bit_count(joined_mask & joined_sounding) < 3) {
            common_practice_weighted_result_destroy(&arpeggio);
            continue;
        }
        for (size_t i = first; i < run_end && status == COMMON_PRACTICE_WEIGHTED_OK; i++) {
            int bass_pitch_class = -1;
            (void)segment_mask(&segments[i], &bass_pitch_class);
            status = take_reading(&arpeggio, &segments[i], bass_pitch_class, COMMON_PRACTICE_CONTEXT_ARPEGGIO,
                                  &results->items[i]);
            masks[i] = joined_mask;
        }
        common_practice_weighted_result_destroy(&arpeggio);
        if (status != COMMON_PRACTICE_WEIGHTED_OK) {
            return status;
        }
    }
    return COMMON_PRACTICE_WEIGHTED_OK;
}

/* Gives each incomplete segment the chord of a neighbour that holds all its pitch classes, the previous first.
   masks[] (the chosen chords' pitch classes) is kept in step. */
static CommonPracticeWeightedStatus resolve_from_neighbours(const CommonPracticeSegment *segments, size_t count,
                                                            int tonic_pitch_class,
                                                            CommonPracticeWeightedResults *results,
                                                            unsigned *masks) {
    for (size_t i = 0; i < count; i++) {
        int bass_pitch_class = -1;
        unsigned sounding = segment_mask(&segments[i], &bass_pitch_class);
        if (sounding == 0 || results->items[i].context != COMMON_PRACTICE_CONTEXT_NONE ||
            !incomplete(&results->items[i], masks[i], sounding, bass_pitch_class)) {
            continue; /* empty, already read from context (an arpeggio), or complete */
        }
        const CommonPracticeWeightedResult *from = NULL;
        CommonPracticeContextReading context = COMMON_PRACTICE_CONTEXT_NONE;
        if (i > 0 && results->items[i - 1].matched && (sounding & ~masks[i - 1]) == 0) {
            int previous_bass = -1;
            unsigned previous = segment_mask(&segments[i - 1], &previous_bass);
            if (results->items[i - 1].context != COMMON_PRACTICE_CONTEXT_NONE ||
                !incomplete(&results->items[i - 1], masks[i - 1], previous, previous_bass)) {
                from = &results->items[i - 1];
                context = COMMON_PRACTICE_CONTEXT_PREVIOUS;
            }
        }
        if (from == NULL && i + 1 < count && results->items[i + 1].matched && (sounding & ~masks[i + 1]) == 0) {
            int next_bass = -1;
            unsigned next = segment_mask(&segments[i + 1], &next_bass);
            if (!incomplete(&results->items[i + 1], masks[i + 1], next, next_bass)) {
                from = &results->items[i + 1];
                context = COMMON_PRACTICE_CONTEXT_NEXT;
            }
        }
        if (from == NULL) {
            continue;
        }
        CommonPracticeWeightedStatus status =
            take_reading(from, &segments[i], bass_pitch_class, context, &results->items[i]);
        if (status != COMMON_PRACTICE_WEIGHTED_OK) {
            return status;
        }
        masks[i] = common_practice_ranked_chord_mask(&results->items[i].best, tonic_pitch_class);
    }
    return COMMON_PRACTICE_WEIGHTED_OK;
}

typedef struct {
    size_t segment;
    size_t member;
} LineRef;

static bool same_text(const char *a, const char *b) {
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static Rational grid_time(double whole_notes) {
    return rational_ratio((long long)llround(whole_notes * 960.0), 960);
}

/* Replaces the framework label of every leftover member of one voice with the injected classifier's. Silent
   when the classifier refuses the line or the voice has none: the framework label stays. */
static CommonPracticeWeightedStatus relabel_voice(const CommonPracticeSegment *segments, const unsigned *masks,
                                                  size_t segment_count, size_t index, const char *voice_id,
                                                  const CommonPracticeWeightedConfig *config,
                                                  CommonPracticeToneLabel *labels, size_t label_count) {
    size_t total = 0;
    for (size_t s = 0; s < segment_count; s++) {
        total += segments[s].member_count;
    }
    LineRef *refs = (LineRef *)malloc((total > 0 ? total : 1) * sizeof(LineRef));
    if (refs == NULL) {
        return COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
    }
    size_t n = 0;
    for (size_t s = 0; s < segment_count; s++) {
        for (size_t m = 0; m < segments[s].member_count; m++) {
            const CommonPracticeMember *member = &segments[s].members[m];
            if (same_text(member->voice_id, voice_id)) {
                refs[n].segment = s;
                refs[n].member = m;
                n++;
            }
        }
    }
    if (n == 0) {
        free(refs);
        return COMMON_PRACTICE_WEIGHTED_OK;
    }
    ToneLineNote *notes = (ToneLineNote *)malloc(n * sizeof(ToneLineNote));
    ToneLineLabel *out = (ToneLineLabel *)malloc(n * sizeof(ToneLineLabel));
    if (notes == NULL || out == NULL) {
        free(refs);
        free(notes);
        free(out);
        return COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
    }
    for (size_t k = 0; k < n; k++) {
        const CommonPracticeMember *member = &segments[refs[k].segment].members[refs[k].member];
        notes[k].midi = member->pitch;
        notes[k].onset = grid_time(member->onset);
        notes[k].duration = grid_time(member->duration);
        notes[k].chord_pitch_classes = masks[refs[k].segment];
    }
    if (config->classifier(notes, n, out, config->classifier_context)) {
        const CommonPracticeSegment *segment = &segments[index];
        for (size_t l = 0; l < label_count; l++) {
            const CommonPracticeMember *member = &segment->members[labels[l].member_index];
            if (!same_text(member->voice_id, voice_id)) {
                continue;
            }
            for (size_t k = 0; k < n; k++) {
                if (refs[k].segment == index && &segments[index].members[refs[k].member] == member) {
                    labels[l].classified = out[k].classified && !out[k].chord_tone;
                    labels[l].tone = out[k].type;
                    break;
                }
            }
        }
    }
    free(refs);
    free(notes);
    free(out);
    return COMMON_PRACTICE_WEIGHTED_OK;
}

/* The leftover labels of segments[index]: the framework's, with the injected classifier's replacing those of
   members that have a voice. */
static CommonPracticeWeightedStatus label_leftover(const CommonPracticeSegment *segments, const unsigned *masks,
                                                   size_t segment_count, size_t index,
                                                   const CommonPracticeWeightedConfig *config,
                                                   CommonPracticeToneLabel **labels, size_t *label_count) {
    if (common_practice_label_leftover_tones(segments, masks, segment_count, index, labels, label_count) !=
        COMMON_PRACTICE_TONES_OK) {
        return COMMON_PRACTICE_WEIGHTED_ERR_ALLOC;
    }
    if (config->classifier == NULL || *label_count == 0) {
        return COMMON_PRACTICE_WEIGHTED_OK;
    }
    const CommonPracticeSegment *segment = &segments[index];
    for (size_t l = 0; l < *label_count; l++) {
        const char *voice = segment->members[(*labels)[l].member_index].voice_id;
        bool seen = false;
        for (size_t p = 0; p < l && !seen; p++) {
            seen = same_text(segment->members[(*labels)[p].member_index].voice_id, voice);
        }
        if (voice == NULL || seen) {
            continue;
        }
        CommonPracticeWeightedStatus status =
            relabel_voice(segments, masks, segment_count, index, voice, config, *labels, *label_count);
        if (status != COMMON_PRACTICE_WEIGHTED_OK) {
            free(*labels);
            *labels = NULL;
            *label_count = 0;
            return status;
        }
    }
    return COMMON_PRACTICE_WEIGHTED_OK;
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

    /* Context: arpeggiated chords across a beat, then incomplete segments take a neighbouring chord. */
    if (status == COMMON_PRACTICE_WEIGHTED_OK) {
        status = read_arpeggios(segments, segment_count, tonic_pitch_class, mode, effective, &results, masks);
    }
    if (status == COMMON_PRACTICE_WEIGHTED_OK) {
        status = resolve_from_neighbours(segments, segment_count, tonic_pitch_class, &results, masks);
    }

    /* Pass 2: label each matched segment's leftover tones with the neighbors and the next chord in view. */
    for (size_t i = 0; i < segment_count && status == COMMON_PRACTICE_WEIGHTED_OK; i++) {
        if (!results.items[i].matched) {
            continue;
        }
        CommonPracticeToneLabel *labels = NULL;
        size_t label_count = 0;
        status = label_leftover(segments, masks, segment_count, i, effective, &labels, &label_count);
        if (status != COMMON_PRACTICE_WEIGHTED_OK) {
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
