#ifndef URSATZ_COMPOSER_MOTIVE_SKETCH_BRIDGE_H
#define URSATZ_COMPOSER_MOTIVE_SKETCH_BRIDGE_H

#include <stdbool.h>

#include <analysis/motive_relation.h>
#include <analysis/motive_sketch.h>
#include <analysis/motive_tone_roles.h>

#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>

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
 * The motive as a sketch whose chords are the given ones, in place of the motive's own harmony and of the implied
 * chord. count may be 0 (no chords: the tonic triad of the scale stands in). False, with *out untouched, as
 * motive_to_sketch, or when a chord is not valid in the sketch.
 */
bool motive_to_sketch_with_chords(const Motive *motive, const MotiveChord *chords, size_t count, MotiveSketch *out);

/*
 * The harmonic role of every note of the motive against the given chords (the library's motive_tone_roles): out has
 * motive->note_count entries. overrides are 0-based note indices with their own role. Returns the library's status:
 * MOTIVE_TONE_NO_SCALE for a motive with no key or no pitches, MOTIVE_TONE_INVALID when the motive is not a valid
 * sketch.
 */
MotiveToneStatus motive_tone_labels(const Motive *motive, const MotiveChord *chords, size_t chord_count,
                                    const MotiveToneOverride *overrides, size_t override_count, MotiveToneLabel *out);

/*
 * The sketch as a motive: the notes, harmony, features, role, pickup and length of the sketch, and the meter and key
 * of the template unless the sketch says another (a changed meter, a scale in another key). A pitch the sketch does not
 * know sounds on the template's first pitch, as a rhythm motive does. The template's form is kept.
 *
 * The implied chord motive_to_sketch added is not written back: a single chord at 0 that is the best implied chord of
 * the notes is left out when the template has no harmony of its own, so a motive and its round trip are equal.
 *
 * False, with *out untouched, for NULL arguments, a time the Composer cannot hold (a pickup must be a multiple of a thirty-second), a meter it does not have, or a
 * sketch that is not a valid motive. out may be the template.
 */
bool motive_from_sketch(const MotiveSketch *sketch, const Motive *template_motive, Motive *out);

/*
 * The development result of a library relation result: OK, UNCHANGED, RANGE and NOT_EXPRESSIBLE keep their meaning,
 * NO_SCALE is NO_KEY, INVALID is INVALID, and every other refusal is NOT_APPLICABLE.
 */
MotiveDevResult motive_sketch_result(MotiveRelationResult result);

#endif
