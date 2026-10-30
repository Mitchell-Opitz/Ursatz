#include "pipeline/chord_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "common-practice-minimal/chord_vocabulary_seventh.h"
#include "common-practice-minimal/weighted_chord_identification.h"
#include "core/rational.h"
#include "interpretation/chord.h"
#include "pipeline/interpretation_util.h"
#include "pipeline/pitch_class_util.h"
#include "semantics/non_chord_tone.h"

/* Root-relative semitone offsets of the chord's tones, ascending (the stack-of-thirds order for every chord in
   the library's vocabulary), as the library reports them. */
static size_t chord_offsets(const CommonPracticeRankedChord *candidate, int offsets[4]) {
    return common_practice_ranked_chord_intervals(candidate, offsets);
}

/* Quality of a triad from its offsets; NULL for a set the library does not build. */
static const char *triad_quality(const int offsets[4], size_t count) {
    if (count != 3) {
        return NULL;
    }
    if (offsets[1] == 4 && offsets[2] == 7) {
        return "major";
    }
    if (offsets[1] == 3 && offsets[2] == 7) {
        return "minor";
    }
    if (offsets[1] == 3 && offsets[2] == 6) {
        return "diminished";
    }
    if (offsets[1] == 4 && offsets[2] == 8) {
        return "augmented";
    }
    return NULL;
}

/* Position of the bass among the chord's tones: 0 for the root, 1 for the third, and so on; -1 when absent. */
static int inversion_of(const int offsets[4], size_t count, int root_pitch_class, int bass_pitch_class) {
    int bass_offset = ((bass_pitch_class - root_pitch_class) % 12 + 12) % 12;
    for (size_t i = 0; i < count; i++) {
        if (offsets[i] == bass_offset) {
            return (int)i;
        }
    }
    return -1;
}

static bool fill_reading(const CommonPracticeRankedChord *candidate, const Chord *chord, double score,
                         int bass_pitch_class, bool prefer_flats, AnalysisChordReading *out) {
    int offsets[4];
    size_t offset_count = chord_offsets(candidate, offsets);
    const char *quality = candidate->is_seventh ? common_practice_seventh_label(candidate->seventh)
                                                : triad_quality(offsets, offset_count);
    out->label = strdup(chord_label(chord));
    out->root = strdup(pitch_class_name(candidate->root_pitch_class, prefer_flats));
    out->quality = strdup(quality ? quality : "unknown");
    out->bass = strdup(pitch_class_name(bass_pitch_class, prefer_flats));
    out->inversion = inversion_of(offsets, offset_count, candidate->root_pitch_class, bass_pitch_class);
    out->score = score;
    out->confidence = interpretation_confidence(chord_interpretation(chord));
    if (!out->label || !out->root || !out->quality || !out->bass) {
        analysis_chord_reading_destroy(out);
        return false;
    }
    return true;
}

void chord_estimates_destroy(ChordEstimate *estimates, size_t count) {
    if (!estimates) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        analysis_chord_reading_destroy(&estimates[i].best);
        analysis_chord_reading_destroy(&estimates[i].runner_up);
        for (size_t t = 0; t < estimates[i].tone_count; t++) {
            free(estimates[i].tones[t].type);
        }
        free(estimates[i].tones);
        common_practice_weighted_result_destroy(&estimates[i].result);
    }
    free(estimates);
}

/* The segment's non-chord tones, from the library's leftover tones; false on allocation failure. */
static bool fill_tones(const Segment *segment, const CommonPracticeWeightedResult *result, ChordEstimate *estimate) {
    if (result->leftover_count == 0) {
        return true;
    }
    estimate->tones = calloc(result->leftover_count, sizeof(AnalysisNonChordTone));
    if (!estimate->tones) {
        return false;
    }
    for (size_t t = 0; t < result->leftover_count; t++) {
        const CommonPracticeLeftoverTone *leftover = &result->leftover[t];
        AnalysisNonChordTone *tone = &estimate->tones[estimate->tone_count++];
        for (size_t m = 0; m < segment->member_count; m++) {
            if (strcmp(segment->members[m].note_id, leftover->note_id) == 0) {
                tone->note_id = segment->note_indices[m];
                break;
            }
        }
        if (leftover->classified) {
            tone->type = strdup(non_chord_tone_name(leftover->tone));
            if (!tone->type) {
                return false;
            }
        }
    }
    return true;
}

/* Whether the region ends at or before the segment starts. */
static bool region_ends_by(const AnalysisKeyRegion *region, const Segment *segment) {
    return rational_cmp_unsigned((unsigned long long)region->end_numerator, region->end_denominator,
                                 (unsigned long long)segment->start_numerator, segment->start_denominator) <= 0;
}

/* Identifies segments [first, first + count) together, in the region's key, filling their estimates. False
   on allocation failure. */
static bool identify_in_region(const Segmentation *segmentation, size_t first, size_t count,
                               const AnalysisKeyRegion *region, ChordEstimate *estimates) {
    CommonPracticeKeyMode mode = region->is_major ? COMMON_PRACTICE_KEY_MAJOR : COMMON_PRACTICE_KEY_MINOR;
    CommonPracticeWeightedResults results = {0};
    if (common_practice_identify_weighted_sequence(segmentation->library_segments + first, count,
                                                   region->tonic_pitch_class, mode, NULL,
                                                   &results) != COMMON_PRACTICE_WEIGHTED_OK) {
        return true;
    }
    for (size_t k = 0; k < count; k++) {
        size_t i = first + k;
        const CommonPracticeWeightedResult *result = &results.items[k];
        if (!result->matched) {
            continue;
        }
        int bass = segment_bass_pitch_class(&segmentation->segments[i]);
        if (!fill_reading(&result->best, result->best_chord, result->best_score, bass, region->prefer_flats,
                          &estimates[i].best) ||
            (result->runner_up_chord &&
             !fill_reading(&result->runner_up, result->runner_up_chord, result->runner_up_score, bass,
                           region->prefer_flats, &estimates[i].runner_up)) ||
            !fill_tones(&segmentation->segments[i], result, &estimates[i])) {
            common_practice_weighted_results_destroy(&results);
            return false;
        }
        estimates[i].context = result->context;
        estimates[i].found = true;
    }
    for (size_t k = 0; k < count; k++) {
        estimates[first + k].result = results.items[k];
    }
    free(results.items);
    return true;
}

bool identify_chords(const Segmentation *segmentation, const AnalysisKeyRegion *regions, size_t region_count,
                     ChordEstimate **out) {
    size_t count = segmentation->segment_count;
    ChordEstimate *estimates = calloc(count > 0 ? count : 1, sizeof(ChordEstimate));
    if (!estimates) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        estimates[i].best.inversion = -1;
        estimates[i].runner_up.inversion = -1;
    }

    /* A segment belongs to the region it starts in; segments and regions are both in time order, so the
       segments of one region are contiguous. */
    size_t region = 0;
    size_t i = 0;
    while (i < count) {
        const Segment *segment = &segmentation->segments[i];
        while (region < region_count && region_ends_by(&regions[region], segment)) {
            region++;
        }
        if (region >= region_count) {
            break;
        }
        size_t last = i;
        while (last < count && !region_ends_by(&regions[region], &segmentation->segments[last])) {
            estimates[last].in_region = true;
            estimates[last].region = region;
            last++;
        }
        if (!identify_in_region(segmentation, i, last - i, &regions[region], estimates)) {
            chord_estimates_destroy(estimates, count);
            return false;
        }
        i = last;
    }

    *out = estimates;
    return true;
}

bool identify_pivot_chords(const Segmentation *segmentation, const AnalysisKeyRegion *regions,
                           AnalysisKeyChange *changes, size_t change_count) {
    for (size_t c = 0; c < change_count; c++) {
        AnalysisKeyChange *change = &changes[c];
        const AnalysisKeyRegion *old_region = &regions[change->from_region];
        if (!change->has_pivot || change->pivot_entry >= segmentation->segment_count) {
            continue;
        }
        /* The library reads the pivot in the old key and its own mode, as key_regions_estimate does when it finds
           the pivot. */
        CommonPracticeKeyMode mode = old_region->is_major ? COMMON_PRACTICE_KEY_MAJOR : COMMON_PRACTICE_KEY_MINOR;
        CommonPracticeWeightedResult result = {0};
        if (common_practice_identify_weighted(&segmentation->library_segments[change->pivot_entry],
                                              old_region->tonic_pitch_class, mode, NULL, &result) !=
            COMMON_PRACTICE_WEIGHTED_OK) {
            continue;
        }
        bool ok = true;
        if (result.matched) {
            int bass = segment_bass_pitch_class(&segmentation->segments[change->pivot_entry]);
            ok = fill_reading(&result.best, result.best_chord, result.best_score, bass, old_region->prefer_flats,
                              &change->pivot);
        }
        common_practice_weighted_result_destroy(&result);
        if (!ok) {
            return false;
        }
    }
    return true;
}
