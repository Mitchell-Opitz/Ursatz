#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/feature_extraction.h"
#include "pitch/pitch_identity.h"
#include "semantics/interval.h"

/*
 * Structural test: FeatureExtraction exposes interval, contour, and
 * density as independent explicitly-named fields -- each with its own
 * has_* flag and accessor -- rather than one opaque feature blob.
 * Constructing with only a subset present (contour and density, no
 * interval) proves the fields are independently optional, not an
 * all-or-nothing bundle.
 *
 * Analysis-regression test: a fixed pitch pair (C4 up a major third to
 * E4, 4 semitones) and a fixed density (3 onsets per 1 reference
 * duration) always extract to the same feature values -- pinning
 * FeatureExtraction's field semantics against a future refactor.
 */

static PitchIdentity *make_pitch(const char *token) {
    PitchIdentity *pitch;
    assert(pitch_identity_create(token, strlen(token) + 1, &pitch) == PITCH_IDENTITY_OK);
    return pitch;
}

int main(void) {
    /* Structural: only contour and density present, interval absent. */
    FeatureExtraction *partial;
    assert(feature_extraction_create(false, NULL, true, "ascending", true, 3, 1, &partial) ==
           FEATURE_EXTRACTION_OK);
    assert(!feature_extraction_has_interval(partial));
    assert(feature_extraction_interval(partial) == NULL);
    assert(feature_extraction_has_contour(partial));
    assert(strcmp(feature_extraction_contour(partial), "ascending") == 0);
    assert(feature_extraction_has_density(partial));
    assert(feature_extraction_density_numerator(partial) == 3);
    assert(feature_extraction_density_denominator(partial) == 1);
    feature_extraction_destroy(partial);

    /* Structural: all features absent is a valid, empty extraction. */
    FeatureExtraction *empty;
    assert(feature_extraction_create(false, NULL, false, NULL, false, 0, 0, &empty) == FEATURE_EXTRACTION_OK);
    assert(!feature_extraction_has_interval(empty));
    assert(!feature_extraction_has_contour(empty));
    assert(!feature_extraction_has_density(empty));
    feature_extraction_destroy(empty);

    /* Construction rejects has_contour with an empty string. */
    FeatureExtraction *rejected;
    assert(feature_extraction_create(false, NULL, true, "", false, 0, 0, &rejected) ==
           FEATURE_EXTRACTION_ERR_NULL_CONTOUR);

    /* Construction rejects has_density with a zero denominator. */
    assert(feature_extraction_create(false, NULL, false, NULL, true, 1, 0, &rejected) ==
           FEATURE_EXTRACTION_ERR_NON_POSITIVE_DENSITY_DENOMINATOR);

    /* Analysis-regression: C4 up a major third to E4 (4 semitones). */
    PitchIdentity *c4 = make_pitch("C4");
    PitchIdentity *e4 = make_pitch("E4");
    Interval *interval;
    assert(interval_create(c4, e4, true, 4, true, 4, false, 0, &interval) == INTERVAL_OK);

    FeatureExtraction *full;
    assert(feature_extraction_create(true, interval, true, "ascending", true, 3, 1, &full) ==
           FEATURE_EXTRACTION_OK);
    assert(feature_extraction_has_interval(full));
    const Interval *extracted_interval = feature_extraction_interval(full);
    assert(interval_has_directed_semitones(extracted_interval));
    assert(interval_directed_semitones(extracted_interval) == 4);
    assert(strcmp(feature_extraction_contour(full), "ascending") == 0);
    assert(feature_extraction_density_numerator(full) == 3);
    assert(feature_extraction_density_denominator(full) == 1);
    feature_extraction_destroy(full);

    pitch_identity_destroy(c4);
    pitch_identity_destroy(e4);

    printf("test_feature_extraction: ok\n");
    return 0;
}