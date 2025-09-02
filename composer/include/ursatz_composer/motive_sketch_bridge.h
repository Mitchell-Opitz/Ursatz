#ifndef URSATZ_COMPOSER_MOTIVE_SKETCH_BRIDGE_H
#define URSATZ_COMPOSER_MOTIVE_SKETCH_BRIDGE_H

#include <stdbool.h>

#include "analysis/motive_sketch.h"

#include <ursatz_composer/motive.h>

/*
 * The Composer's Motive and the library's MotiveSketch (analysis/motive_sketch.h), converted both ways. The library's
 * motive relations and its variant recognizer work on sketches; the Composer's registry rows and records work on
 * motives.
 */

/*
 * The motive as a sketch: notes with exact onsets and durations, the scale of its key (none without one), the bar,
 * beat, pickup and length, its own harmony, features and role. A rhythm motive has no pitches (every note's pitch is
 * MOTIVE_SKETCH_PITCH_UNKNOWN); a contour motive's notes keep the pitches that sound it.
 *
 * A full motive with a key and no harmony of its own gets its best implied chord at 0 (the triad holding the most
 * notes, then the most time, the first of I V IV vi ii iii vii on a tie; the same choice the library makes), so a
 * relation that needs chords has one.
 *
 * False, with *out untouched, for NULL arguments or a motive that is not a valid sketch.
 */
bool motive_to_sketch(const Motive *motive, MotiveSketch *out);

/*
 * The sketch as a motive: the notes, harmony, features, role, pickup and length of the sketch, and the meter and key
 * of the template unless the sketch says another (a changed meter, a scale in another key). A pitch the sketch does not
 * know sounds on the template's first pitch, as a rhythm motive does. The template's form is kept.
 *
 * The implied chord motive_to_sketch added is not written back: a single chord at 0 that is the best implied chord of
 * the notes is left out when the template has no harmony of its own, so a motive and its round trip are equal.
 *
 * False, with *out untouched, for NULL arguments, a time the Composer cannot hold, a meter it does not have, or a
 * sketch that is not a valid motive. out may be the template.
 */
bool motive_from_sketch(const MotiveSketch *sketch, const Motive *template_motive, Motive *out);

#endif
