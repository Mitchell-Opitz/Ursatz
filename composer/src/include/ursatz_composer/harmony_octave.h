#ifndef URSATZ_COMPOSER_HARMONY_OCTAVE_H
#define URSATZ_COMPOSER_HARMONY_OCTAVE_H

#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/lanes.h>

/*
 * The harmony-from-bass operator: a Fill operator for the harmony lane that works
 * from a bass written first, in the manner of the rule of the octave. It is the
 * other direction of the bass operator (harmony_bass.h).
 *
 * A target is a Constrained harmony lane span, not marked for embellishment, with
 * a Given bass under it that:
 *   - tiles the span exactly, with every note starting on a cell boundary (a
 *     strong beat of the meter, harmony_phrase.h);
 *   - stays in the key's scale (in minor, the raised seventh degree is allowed)
 *     and moves by step from one pitch class to the next (a repeated pitch class
 *     is one chord);
 *   - starts and ends on the tonic, as the rule's scale does.
 * Anything else, the operator does not apply to. (The phrase-model operator
 * refuses a span with a Given bass, so the two never compete.)
 *
 * It writes one chord for each run of the same bass pitch class, with the bass
 * pitch class as the chord's bass member, root or first inversion (V7 in any
 * inversion): the first and last chords are the tonic in root position and the
 * others are drawn with the seeded stream. Ursatz's scale-harmonization states
 * the rule's structure but not its chord for each degree, so the chords are found
 * by search and judged by standard-harmony (no cadence is asked for, an inverted
 * V to I is not one): the phrase model must hold, and a candidate that breaks it
 * is never proposed. A bass no such chords fit gets no candidates, so the loop
 * leaves the span open as FILL_STUCK_ALL_REJECTED.
 */

#define HARMONY_OCTAVE_OPERATOR_NAME "harmony-octave"

/* Registers the operator. FILL_OK, or the registry's error. */
FillResult harmony_octave_register(FillOperatorRegistry *registry);

#endif
