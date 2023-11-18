#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_SIX_FOUR_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_SIX_FOUR_H

#include <stdbool.h>

#include "harmony_chord.h"
#include "semantics/beat_strength.h"
#include "six_four_type.h"
#include "semantics/voice_part.h"

/*
 * 6/4 chords for standard-harmony-v1 (6/4 Chords as Forms of Prolongation). A 6/4 chord is
 * a triad in second inversion: the fifth of the chord is in the bass, with the root a
 * fourth above and the third a sixth above. The fourth is a dissonance with the bass, so
 * composers treat these chords in special ways, and in analysis the Roman numeral is not
 * a sufficient label: the kind (SixFourType, six_four_type.h) is part of it.
 * Besides the cadential 6/4 (harmony_cadential_rules.h), three more are labeled:
 *
 *   PASSING   built on a passing tone in the bass: three notes in stepwise motion, the
 *             first and last belonging to the same functional area (tonic or
 *             predominant), the middle one the passing tone. The chords on both sides
 *             have the same function, never different functions. It works backward too
 *   NEIGHBOR  a static bass (typically do do do or sol sol sol) with two upper voices
 *             making upper-neighbor motion over the middle note; the chords on both sides
 *             are the same and in root position, and it prolongs I or V. Sometimes called
 *             the pedal 6/4
 *   ARPEGGIATING
 *             the bass leaps to the fifth of a chord while the upper voices sustain it,
 *             as in an ending bass arpeggiation or a waltz accompaniment. It may prolong
 *             any harmony and is typically not labeled: each measure is simply the chord
 *
 * The subject is three chords in order (the one before the 6/4, the 6/4, and the one
 * after), their notes as diatonic steps for each voice part from the soprano to the bass
 * (harmony_steps.h; do is 0, re is 1, an octave is 7), and a beat strength for each. A step
 * is one diatonic step, and a leap is more than one.
 *
 * The 6/4 chord over a bass note has its root a fourth above that note: a V6/4 over re,
 * a IV6/4 over do, an I6/4 over sol.
 */

typedef struct {
    HarmonyMode mode;
    HarmonyChord chords[3];
    int steps[3][VOICE_PART_COUNT];
    BeatStrength beat[3];
} HarmonySixFour;

/* True for a valid mode, three valid chords, and a valid beat strength for each. */
bool harmony_six_four_valid(const HarmonySixFour *six_four);

/* True when the middle chord is a triad in second inversion, with its root a fourth above
   the bass note it sounds over. False for any other chord and for an invalid subject. */
bool harmony_six_four_middle_is_six_four(const HarmonySixFour *six_four);

/* The kind of 6/4 the middle chord is, by what the bass does:
     a chord marked cadential: CADENTIAL;
     a static bass, with the same root position chord on both sides: NEIGHBOR;
     a bass that goes up or down by step three times in one direction, with the same
       function on both sides: PASSING;
     a bass that leaps to the fifth of the same chord, the upper voices sustaining it
       (every upper voice on the same note in all three chords): ARPEGGIATING.
   False (*out untouched) for anything else, when the middle chord is not a 6/4, and for an
   invalid subject. */
bool harmony_six_four_classify(const HarmonySixFour *six_four, SixFourType *out);

#endif
