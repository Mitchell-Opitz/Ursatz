#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_CONSTRUCTION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_CONSTRUCTION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "voicing_chord.h"

/*
 * Chord construction rules (STANDARD_VOICING_RULE_CHORD_CONSTRUCTION).
 * Each chord is checked for correct notes and accidentals, with no note
 * missing. The bass voice always corresponds to the inversion the Roman
 * numeral's figures indicate; the order of the upper notes is free, so
 * there is more than one correct voicing of a chord.
 *
 * The chord is given as its tones in root position order (root, third,
 * fifth, and the seventh for a seventh chord), one spelling each; their
 * octaves are ignored. Where the tones come from -- standard-tonality's
 * chord rules, a Roman numeral analysis -- is the caller's concern, and
 * this module does not depend on it.
 *
 * Tones are compared by letter and accidental, so enharmonic spellings
 * are different notes (an erroneous accidental is caught).
 */

#define STANDARD_VOICING_TRIAD_TONES 3
#define STANDARD_VOICING_SEVENTH_TONES 4

typedef enum {
    STANDARD_VOICING_CONSTRUCTION_OK = 0,
    STANDARD_VOICING_CONSTRUCTION_INVALID_INPUT, /* NULL, a tone count that is not 3 or 4, an inversion out of range */
    STANDARD_VOICING_CONSTRUCTION_BASS_MISMATCH, /* the bass is not the chord tone the inversion calls for */
    STANDARD_VOICING_CONSTRUCTION_MISSING_TONE,  /* a chord tone sounds in no voice */
    STANDARD_VOICING_CONSTRUCTION_FOREIGN_NOTE   /* a voice sounds a note that is not a chord tone */
} StandardVoicingConstruction;

/* Checks the voicing against the chord. `tones` has `tone_count` (3 or 4)
   spellings in root position order; `inversion` is 0 for root position,
   1 for first inversion, 2 for second, 3 for third (seventh chords
   only). Reports the first problem found, checking the bass first, then
   foreign notes, then missing tones. */
StandardVoicingConstruction standard_voicing_check_construction(const StandardVoicingChord *chord,
                                                                const PitchSpelling *const *tones,
                                                                size_t tone_count, int inversion);

/* The inversion a Roman numeral figure indicates: a triad (chord_size 3)
   "" or "5" or "53" is 0, "6" or "63" 1, "64" 2; a seventh chord
   (chord_size 4) "7" is 0, "65" 1, "43" 2, "42" or "2" 3. False (output
   untouched) for NULL, a size other than 3 or 4, or a figure that does
   not belong to a chord of that size. */
bool standard_voicing_inversion_from_figure(const char *figure, size_t chord_size, int *inversion);

#endif
