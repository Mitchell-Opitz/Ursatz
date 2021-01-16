#include "pipeline/chord_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "chord_vocabulary_seventh.h"
#include "interpretation/chord.h"
#include "pipeline/interpretation_util.h"
#include "pipeline/pitch_class_util.h"
#include "weighted_chord_identification.h"

/* Root-relative semitone offsets of the chord's tones, ascending (which is the stack-of-thirds order for
   every chord in the library's vocabulary), from the pitch-class mask the library reports for it. */
static size_t chord_offsets(const CommonPracticeRankedChord *candidate, int tonic_pitch_class, int offsets[4]) {
    unsigned mask = common_practice_ranked_chord_mask(candidate, tonic_pitch_class);
    size_t count = 0;
    for (int offset = 0; offset < 12 && count < 4; offset++) {
        if (mask & (1u << ((candidate->root_pitch_class + offset) % 12))) {
            offsets[count++] = offset;
        }
    }
    return count;
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
                         int tonic_pitch_class, int bass_pitch_class, bool prefer_flats,
                         AnalysisChordReading *out) {
    int offsets[4];
    size_t offset_count = chord_offsets(candidate, tonic_pitch_class, offsets);
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
    }
    free(estimates);
}

bool identify_chords(const Segmentation *segmentation, const KeyEstimate *key, ChordEstimate **out) {
    size_t count = segmentation->segment_count;
    ChordEstimate *estimates = calloc(count > 0 ? count : 1, sizeof(ChordEstimate));
    if (!estimates) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        estimates[i].best.inversion = -1;
        estimates[i].runner_up.inversion = -1;
    }

    if (count > 0 && key && key->found && key->is_major) {
        CommonPracticeWeightedResults results = {0};
        if (common_practice_identify_weighted_sequence(segmentation->library_segments, count,
                                                       key->tonic_pitch_class, COMMON_PRACTICE_KEY_MAJOR, NULL,
                                                       &results) == COMMON_PRACTICE_WEIGHTED_OK) {
            for (size_t i = 0; i < count; i++) {
                const CommonPracticeWeightedResult *result = &results.items[i];
                if (!result->matched) {
                    continue;
                }
                int tonic = key->tonic_pitch_class;
                int bass = segment_bass_pitch_class(&segmentation->segments[i]);
                if (!fill_reading(&result->best, result->best_chord, result->best_score, tonic, bass,
                                  key->prefer_flats, &estimates[i].best) ||
                    (result->runner_up_chord &&
                     !fill_reading(&result->runner_up, result->runner_up_chord, result->runner_up_score, tonic,
                                   bass, key->prefer_flats, &estimates[i].runner_up))) {
                    common_practice_weighted_results_destroy(&results);
                    chord_estimates_destroy(estimates, count);
                    return false;
                }
                estimates[i].found = true;
            }
            common_practice_weighted_results_destroy(&results);
        }
    }

    *out = estimates;
    return true;
}
