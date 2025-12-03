#ifndef URSATZ_ANALYZER_VOICING_CHECK_H
#define URSATZ_ANALYZER_VOICING_CHECK_H

#include "pitch/pitch_spelling.h"

/*
 * Spacing of a four-part chord by Ursatz's standard-voicing rules (spacing_rules.h): no more than an octave between
 * the soprano and alto and between the alto and tenor, and no more than a twelfth between the tenor and the bass.
 * The analyzer adds no limit of its own (R-018). Kept in its own file because standard-voicing's headers share names
 * with other frameworks'.
 *
 * voices are the four sounding spellings, top to bottom (soprano, alto, tenor, bass). Returns VOICING_SPACING_VALID when
 * all three pairs are within the limits, the index (0 soprano, 1 alto, 2 tenor) of the highest voice of the first
 * pair that is too wide, or VOICING_SPACING_UNJUDGED when a voice has no spelling.
 */
#define VOICING_SPACING_VALID (-1)
#define VOICING_SPACING_UNJUDGED (-2)

int voicing_first_wide_spacing(const PitchSpelling *const voices[4]);

#endif
