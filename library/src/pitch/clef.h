#ifndef MUSIC_PITCH_CLEF_H
#define MUSIC_PITCH_CLEF_H

#include <stdbool.h>

#include "pitch/pitch_spelling.h"

/*
 * Clef: indicates which pitches are assigned to the lines and spaces of
 * a staff. A Clef is a sign (G, F, or C) together with the staff line it
 * is anchored on; the sign names the reference letter and octave that
 * sits on that line:
 *
 *   G sign -> G4,  F sign -> F3,  C sign -> C4
 *
 * line counts staff lines from the bottom, 1 (bottom) through 5 (top).
 * The four clefs in common use are provided as presets:
 *
 *   treble = G sign on line 2    bass  = F sign on line 4
 *   alto   = C sign on line 3    tenor = C sign on line 4
 *
 * octave_shift is the number of octaves the music SOUNDS relative to the
 * pitch written under the clef: 0 for an ordinary clef, -1 for an octave
 * (or double) treble clef with an "8" below -- sounds an octave lower than
 * written -- and +1 for one with an "8" above. Only -1, 0, and +1 are
 * accepted. A double treble clef is the same as the octave-down treble:
 * the glyph difference is presentation, not meaning, so it is not
 * modeled separately.
 *
 * Clef is a value type: immutable, value equality on (sign, line, octave_shift). It is
 * deliberately not attached to Staff or Instrument -- a clef may change
 * mid-staff, so placing a Clef within a staff is a later concern.
 * Mapping a staff position to a pitch under a Clef is
 * pitch/clef_resolution.h.
 */

typedef enum {
    CLEF_SIGN_G,
    CLEF_SIGN_F,
    CLEF_SIGN_C
} ClefSign;

typedef struct Clef Clef;

typedef enum {
    CLEF_OK = 0,
    CLEF_ERR_INVALID_SIGN,
    CLEF_ERR_INVALID_LINE,
    CLEF_ERR_INVALID_OCTAVE_SHIFT,
    CLEF_ERR_ALLOC
} ClefResult;

#define CLEF_MIN_LINE 1
#define CLEF_MAX_LINE 5
#define CLEF_MIN_OCTAVE_SHIFT (-1)
#define CLEF_MAX_OCTAVE_SHIFT 1

/* line: 1 (bottom) through 5 (top). */
ClefResult clef_create(ClefSign sign, int line, Clef **out);
/* As clef_create, with a sounding octave shift of -1, 0, or +1. */
ClefResult clef_create_shifted(ClefSign sign, int line, int octave_shift, Clef **out);
ClefResult clef_create_treble(Clef **out);
ClefResult clef_create_bass(Clef **out);
ClefResult clef_create_alto(Clef **out);
ClefResult clef_create_tenor(Clef **out);
void clef_destroy(Clef *clef);

ClefSign clef_sign(const Clef *clef);
int clef_line(const Clef *clef);
int clef_octave_shift(const Clef *clef);

/* Letter and octave of the pitch the sign names (G4, F3, or C4). */
PitchLetter clef_reference_letter(const Clef *clef);
int clef_reference_octave(const Clef *clef);

/* Value equality on (sign, line, octave_shift). */
bool clef_equals(const Clef *a, const Clef *b);

#endif
