#ifndef URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_OUTER_SCALE_DEGREES_H
#define URSATZ_FRAMEWORKS_COMMON_PRACTICE_MINIMAL_OUTER_SCALE_DEGREES_H

#include <stdbool.h>

#include "theory/tonality/key_vocabulary.h"

/*
 * The scale degree label of a pitch class, under common-practice-minimal-v1. A degree is "1" to "7" with a "b" or "#" for a
 * chromatic alteration, by the pitch class above the tonic:
 *     major          1 b2 2 b3 3 4 #4 5 b6 6 b7 7
 *     natural minor  1 b2 2 3 #3 4 #4 5 6 #6 7 #7
 * so in minor the diatonic degrees are the natural minor scale's own (3, 6 and 7 are the flat forms), and the raised sixth and
 * seventh of the melodic and harmonic forms are altered (#6, #7). The tonic pitch class is 0 = C ... 11 = B; the mode may be
 * major or natural minor.
 */

/* The label of the pitch class `pitch_class` (0..11) in a key on tonic_pitch_class and mode, per the table
   above; NULL for an invalid argument or an unsupported mode. */
const char *common_practice_scale_degree_label(int pitch_class, int tonic_pitch_class, CommonPracticeKeyMode mode);

/* The numbers behind the label: *degree 1..7 and *alteration -1, 0 or 1 ("b" flat, "#" sharp), so no caller
   parses the label. False, outputs untouched, for NULL outputs, a pitch class or tonic outside 0..11 or an
   unsupported mode. */
bool common_practice_scale_degree_of(int pitch_class, int tonic_pitch_class, CommonPracticeKeyMode mode, int *degree,
                                     int *alteration);

#endif
