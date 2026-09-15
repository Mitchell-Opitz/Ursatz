#ifndef URSATZ_COMPOSER_MOTIVE_TONES_H
#define URSATZ_COMPOSER_MOTIVE_TONES_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/motive_tone_roles.h"

#include <ursatz_composer/motive.h>

/*
 * A motive's base harmony: the chords every note is heard against (at most 2 to a bar). It is the motive's own harmony
 * when it has one (given), otherwise it is proposed from the notes.
 */
typedef struct {
    MotiveChord chords[MOTIVE_MAX_CHORDS];
    size_t count;
    bool given;
} MotiveBaseHarmony;

/*
 * The base harmony of a motive. False, *out untouched, when it has none: a motive with no key, or one whose notes do
 * not sound a known pitch (a rhythm or a contour).
 *
 * An own harmony is the base harmony as it stands. Without one it is proposed bar by bar from the triads of the key (I
 * V IV vi ii iii vii): the triad with the most strong-beat notes as chord tones, ties to I and then to the earlier in
 * that order. A strong beat is the downbeat, and in 4/4 the third beat, in a compound meter every third beat. A bar
 * with no note keeps the chord before it. A second chord comes in a bar only when the best single triad leaves two or
 * more strong-beat notes as non-chord tones and a triad starting on a beat inside the bar makes at least two of them
 * chord tones (the one that makes the most, the earliest beat and then the earliest triad on a tie). A chord equal to
 * the one before it is not repeated.
 */
bool motive_base_harmony(const Motive *motive, MotiveBaseHarmony *out);

/* The most chords the base harmony may put in one bar. */
#define MOTIVE_MAX_CHORDS_PER_BAR 2

/* True when no bar of the motive's own harmony holds more than MOTIVE_MAX_CHORDS_PER_BAR chords. */
bool motive_harmony_bar_limit_ok(const Motive *motive);

/* The name of a non-chord tone type as the motive reports it: "passing", "neighbor", "appoggiatura", "suspension",
   "retardation", "escape", "anticipation", "double-neighbor", "changing-tone" or "pedal". NULL for an invalid type. */
const char *motive_tone_type_name(NonChordTone type);

#endif
