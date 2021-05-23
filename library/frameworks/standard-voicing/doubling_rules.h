#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_DOUBLING_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_DOUBLING_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "rules/constraint.h"
#include "rules/rule.h"
#include "voicing_chord.h"

/*
 * Doubling rules (STANDARD_VOICING_RULE_DOUBLING). A chord has four
 * voices, so a triad has one note doubled:
 *
 *   BASS_DOUBLED      in a triad the note in the bass is usually doubled
 *                     in an upper voice -- a preference, not a
 *                     requirement (a soft constraint)
 *   LEADING_TONE      the leading tone is never doubled (hard)
 *   SEVENTH           a chordal seventh, and other tendency tones, is
 *                     never doubled (hard)
 *   SEVENTH_CHORD     a seventh chord has four notes, one for each voice,
 *                     so no note is doubled (hard)
 *
 * The chord's tones (root position order, 3 or 4 spellings) and the
 * key's leading tone are supplied by the caller, as in
 * construction_rules.h; tones are compared by letter and accidental,
 * octaves ignored. The leading tone is optional: NULL means the chord is
 * not in a key that has one in play, and the leading tone rule has
 * nothing to forbid.
 */

typedef enum {
    STANDARD_VOICING_DOUBLING_BASS_DOUBLED = 0, /* soft: a preference */
    STANDARD_VOICING_DOUBLING_LEADING_TONE,
    STANDARD_VOICING_DOUBLING_SEVENTH,
    STANDARD_VOICING_DOUBLING_SEVENTH_CHORD,
    STANDARD_VOICING_DOUBLING_RULE_COUNT
} StandardVoicingDoublingRule;

const Rule *standard_voicing_doubling_rule(StandardVoicingDoublingRule which);
const Constraint *standard_voicing_doubling_constraint(StandardVoicingDoublingRule which);

/* How many voices of the chord sound the pitch class of `note` (letter and
   accidental; octaves ignored). 0 for NULL. */
int standard_voicing_pitch_class_count(const StandardVoicingChord *chord, const PitchSpelling *note);

/* LEADING_TONE: true when at most one voice sounds the leading tone.
   False for a NULL chord or leading tone. */
bool standard_voicing_leading_tone_undoubled(const StandardVoicingChord *chord, const PitchSpelling *leading_tone);

/* SEVENTH: true when the chord is a triad (nothing to check), or a seventh
   chord whose seventh -- tones[3] -- sounds in at most one voice. False
   for invalid input (NULL, a tone count that is not 3 or 4). */
bool standard_voicing_seventh_undoubled(const StandardVoicingChord *chord, const PitchSpelling *const *tones,
                                        size_t tone_count);

/* SEVENTH_CHORD: true when the chord is a triad (nothing to check), or a
   seventh chord in which no two voices sound the same pitch class. False
   for invalid input. */
bool standard_voicing_seventh_chord_undoubled(const StandardVoicingChord *chord, const PitchSpelling *const *tones,
                                              size_t tone_count);

/* BASS_DOUBLED (preference): true when the bass of a triad is doubled in
   an upper voice, or the chord is a seventh chord (the preference does
   not apply). False when a triad's bass is not doubled, or for invalid
   input. Failing this never makes a voicing wrong, only less usual. */
bool standard_voicing_bass_doubled(const StandardVoicingChord *chord, const PitchSpelling *const *tones,
                                   size_t tone_count);

/* The hard rules together: LEADING_TONE, SEVENTH, and SEVENTH_CHORD
   (the bass preference is not part of validity). leading_tone may be
   NULL. False for invalid input. */
bool standard_voicing_doubling_valid(const StandardVoicingChord *chord, const PitchSpelling *const *tones,
                                     size_t tone_count, const PitchSpelling *leading_tone);

#endif
