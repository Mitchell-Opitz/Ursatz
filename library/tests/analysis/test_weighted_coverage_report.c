#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common-practice-minimal/triad_identification.h"
#include "weighted_pipeline.h"

/*
 * Coverage of weighted chord identification for the five framework-v1-corpus pieces, against the existing
 * subset-match baseline on the SAME adaptive segments. For each piece: the segments, how many resolve to a
 * chord under each method, and the mean uncertainty of the weighted matches. Printed for the PR; the only
 * assertion is that the weighted method resolves more segments than the baseline, for every piece.
 *
 * Each piece is run in the key below, whole-piece, with its own file's meter. The keys are the caller's:
 * Mozart 545 and Clementi 36 carry a C major key signature event and Field 5 a B-flat major one (two flats);
 * Bach 114 and Field 1 carry none, and are taken as G major (the Minuet in G) and E-flat major. These are not
 * verified against the scores. Bach 114's file declares 4/4 for a 3/4 piece, so its segments are cut on the
 * file's own, wrong, beats and barlines; the figures are what the file gives.
 *
 * The baseline is common_practice_identify_triad on the distinct scale degrees of every sounding note of the
 * segment (the subset-match function, unchanged): it ignores extra degrees, takes the first match, and has no
 * seventh chords.
 *
 * URSATZ_CORPUS_DIR is passed in by CMake.
 */

typedef struct {
    const char *file;
    int tonic_pitch_class;
    const char *key_name;
} Piece;

static const Piece PIECES[5] = {
    {"Bach 114.mid", 7, "G major"},   {"Mozart 545.mid", 0, "C major"}, {"Clementi 36.mid", 0, "C major"},
    {"Field 1.mid", 3, "E-flat major"}, {"Field 5.mid", 10, "B-flat major"},
};

static int degree_of_offset(int offset) {
    switch (offset) {
    case 0:
        return 1;
    case 2:
        return 2;
    case 4:
        return 3;
    case 5:
        return 4;
    case 7:
        return 5;
    case 9:
        return 6;
    case 11:
        return 7;
    default:
        return 0;
    }
}

static bool subset_match(const CommonPracticeSegment *segment, int tonic) {
    bool present[8] = {false};
    for (size_t i = 0; i < segment->member_count; i++) {
        int offset = (((segment->members[i].pitch - tonic) % 12) + 12) % 12;
        int degree = degree_of_offset(offset);
        if (degree > 0) {
            present[degree] = true;
        }
    }
    int degrees[7];
    size_t count = 0;
    for (int d = 1; d <= 7; d++) {
        if (present[d]) {
            degrees[count++] = d;
        }
    }
    CommonPracticeTriad triad;
    return count >= 3 && common_practice_identify_triad(degrees, count, &triad);
}

static void test_weighted_coverage_beats_the_subset_match_baseline(void) {
    printf("Weighted chord identification coverage, against subset-match on the same adaptive segments\n");
    printf("  %-16s %-13s %8s %9s %9s %12s %18s\n", "piece", "key", "segments", "weighted", "subset", "weighted %",
           "mean uncertainty");

    for (size_t p = 0; p < 5; p++) {
        WeightedPipeline pipeline = pipeline_run(URSATZ_CORPUS_DIR, PIECES[p].file, NULL);
        CommonPracticeWeightedResults results = {0};
        assert(common_practice_identify_weighted_sequence(pipeline.segments, pipeline.segment_count,
                                                          PIECES[p].tonic_pitch_class, COMMON_PRACTICE_KEY_MAJOR,
                                                          NULL, &results) == COMMON_PRACTICE_WEIGHTED_OK);

        size_t weighted = 0;
        size_t subset = 0;
        double uncertainty_sum = 0.0;
        for (size_t s = 0; s < pipeline.segment_count; s++) {
            if (results.items[s].matched) {
                weighted++;
                uncertainty_sum += results.items[s].uncertainty;
            }
            if (subset_match(&pipeline.segments[s], PIECES[p].tonic_pitch_class)) {
                subset++;
            }
        }
        double mean_uncertainty = weighted == 0 ? 0.0 : uncertainty_sum / (double)weighted;
        printf("  %-16s %-13s %8zu %4zu/%-4zu %4zu/%-4zu %11.1f%% %18.3f\n", PIECES[p].file, PIECES[p].key_name,
               pipeline.segment_count, weighted, pipeline.segment_count, subset, pipeline.segment_count,
               pipeline.segment_count == 0 ? 0.0 : 100.0 * (double)weighted / (double)pipeline.segment_count,
               mean_uncertainty);

        assert(pipeline.segment_count > 0);
        assert(weighted > subset);

        common_practice_weighted_results_destroy(&results);
        pipeline_destroy(&pipeline);
    }
}

int main(void) {
    test_weighted_coverage_beats_the_subset_match_baseline();
    printf("test_weighted_coverage_report: ok\n");
    return 0;
}
