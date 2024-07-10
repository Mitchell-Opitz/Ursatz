#include <math.h>
#include <string.h>

#include "analysis_test_util.h"
#include "meter_analysis_density.h"

static bool near(double a, double b) {
    return fabs(a - b) < 1e-12;
}

static void test_a_known_rhythm(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    piece_phrase(&piece, 0, 1, 2, 1, false, 0, 1, 1, 1, STANDARD_METER_EXPANSION_NONE, 0, false);
    /* line 0, bar 1: quarter, quarter, two eighths, quarter ... */
    piece_note(&piece, 0, 0, 1, 1, 4);
    piece_note(&piece, 0, 1, 4, 1, 2);
    piece_note(&piece, 0, 1, 2, 5, 8);
    piece_note(&piece, 0, 5, 8, 3, 4);
    piece_note(&piece, 0, 3, 4, 1, 1);
    /* ... bar 2: a half note and two quarters */
    piece_note(&piece, 0, 1, 1, 3, 2);
    piece_note(&piece, 0, 3, 2, 7, 4);
    piece_note(&piece, 0, 7, 4, 2, 1);
    /* line 1: a whole note in each bar */
    piece_note(&piece, 1, 0, 1, 1, 1);
    piece_note(&piece, 1, 1, 1, 2, 1);
    /* a note after the phrase belongs to no profile */
    piece_note(&piece, 0, 2, 1, 5, 2);

    StandardMeterDensities found = {NULL, 0};
    assert(standard_meter_find_densities(&piece.input, NULL, &found) == STANDARD_METER_ANALYSIS_OK);
    assert(found.count == 3);

    const StandardMeterDensityClaim *all = &found.items[0].claim;
    assert(all->phrase == 0 && all->line == -1 && all->notes == 10 && all->attacks == 8 && all->measures == 2);

    const StandardMeterDensityClaim *upper = &found.items[1].claim;
    assert(upper->line == 0 && upper->notes == 8 && upper->attacks == 8);
    assert(near(upper->attacks_per_beat_mean, 1.0) && near(upper->attacks_per_beat_max, 1.25));
    assert(near(upper->mean_inter_onset, 0.25));
    assert(near(upper->on_beat_share, 0.875) && near(upper->off_beat_share, 0.125));
    assert(upper->bin_count == 3 && upper->bins_truncated == 0);
    assert(upper->bins[0].numerator == 1 && upper->bins[0].denominator == 8 && upper->bins[0].count == 2);
    assert(upper->bins[1].numerator == 1 && upper->bins[1].denominator == 4 && upper->bins[1].count == 5);
    assert(upper->bins[2].numerator == 1 && upper->bins[2].denominator == 2 && upper->bins[2].count == 1);

    const StandardMeterDensityClaim *bass = &found.items[2].claim;
    assert(bass->line == 1 && bass->notes == 2 && bass->attacks == 2 && near(bass->mean_inter_onset, 1.0));
    assert(near(bass->attacks_per_beat_mean, 0.25) && near(bass->on_beat_share, 1.0) && bass->bin_count == 1 &&
           bass->bins[0].numerator == 1 && bass->bins[0].denominator == 1 && bass->bins[0].count == 2);
    assert(found.items[0].interpretation != NULL && found.items[2].interpretation != NULL);
    standard_meter_densities_destroy(&found);
    piece_destroy(&piece);
}

static void test_empty_phrases_and_bad_calls(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    piece_phrase(&piece, 0, 1, 1, 1, false, 0, 1, 1, 1, STANDARD_METER_EXPANSION_NONE, 0, false);
    StandardMeterDensities found = {NULL, 0};
    assert(standard_meter_find_densities(&piece.input, NULL, &found) == STANDARD_METER_ANALYSIS_OK);
    assert(found.count == 1 && found.items[0].claim.notes == 0 && found.items[0].claim.attacks == 0 &&
           near(found.items[0].claim.on_beat_share, 0.0) && found.items[0].claim.bin_count == 0);
    standard_meter_densities_destroy(&found);
    assert(standard_meter_find_densities(NULL, NULL, &found) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    assert(standard_meter_find_densities(&piece.input, NULL, NULL) == STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT);
    piece_destroy(&piece);
}

static void test_the_histogram_is_capped(void) {
    Piece piece;
    piece_init(&piece, 4, 2);
    piece_phrase(&piece, 0, 1, 40, 1, false, 0, 1, 1, 1, STANDARD_METER_EXPANSION_NONE, 0, false);
    /* twenty notes, each a different length: 1/2, 1/3, ... 1/21 of a whole note, one every two bars */
    for (long i = 0; i < 20; i++) {
        piece_note(&piece, 0, 2 * i, 1, 2 * i * (i + 2) + 1, (unsigned long)(i + 2));
    }
    StandardMeterDensities found = {NULL, 0};
    assert(standard_meter_find_densities(&piece.input, NULL, &found) == STANDARD_METER_ANALYSIS_OK);
    const StandardMeterDensityClaim *all = &found.items[0].claim;
    assert(all->notes == 20 && all->bin_count == STANDARD_METER_DENSITY_BINS && all->bins_truncated == 1);
    assert(all->bins[0].numerator == 1 && all->bins[0].denominator == 21 && all->bins[0].count == 1); /* the shortest first */
    standard_meter_densities_destroy(&found);
    piece_destroy(&piece);
}

int main(void) {
    test_a_known_rhythm();
    test_empty_phrases_and_bad_calls();
    test_the_histogram_is_capped();
    printf("test_analysis_density (standard-meter): ok\n");
    return 0;
}
