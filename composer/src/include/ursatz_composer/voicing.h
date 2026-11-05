#ifndef URSATZ_COMPOSER_VOICING_H
#define URSATZ_COMPOSER_VOICING_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>

/*
 * The four-part voicing operator: a Fill operator that voices a written progression in soprano, alto, tenor and bass
 * (R-066). It reads a harmony lane and a bass lane that are already written (Realized or Given, never open) and writes
 * the alto, the tenor and, when asked, the soprano.
 *
 * A target is a Constrained inner voice span, voice 1 (the alto), with a VOICING constraint (constraint.h) on the span or
 * on its owner, over the time of a harmony target: the chord spans under it tile it exactly, its bass is written, and it
 * lies in one key. The tenor is the inner voice 2: a Constrained span over the same time, or no span at all. The
 * soprano is the written melody, or, with write_soprano, a Constrained melody span over the same time that the voicing
 * writes too (one note a chord). Anything else, the operator does not apply to.
 *
 * Each chord is voiced in one note a voice, held for the chord. A chord is kept only when standard-voicing holds for it:
 * every voice in its range, no more than an octave between the upper voices and a twelfth between tenor and bass, no
 * voice crossing, no leading tone or chordal seventh doubled, the bass the chord member its inversion calls for, no chord
 * tone missing. The range of the soprano and the bass are asked only of what the voicing writes: a written melody or bass
 * outside its range does not stop the chords being voiced. Without write_soprano the soprano of a chord is the written
 * melody note sounding at the start of the chord, or, when that note is not a tone of the chord, the first chord tone
 * sounding in it.
 *
 * The chords are joined by a beam search (voicing_search.c): the cost of a path is the semitones the upper three voices
 * move, and 12 more for each of their leaps wider than a fifth. A connection is refused when standard-counterpoint finds
 * parallel fifths or octaves between any two of the four voices (and direct fifths or octaves between the outer two when
 * the soprano is written), or when standard-harmony's resolution rules find a chordal seventh or the leading tone of a V
 * unresolved in the same voice. The cheapest complete paths are the candidates; ties are ordered by the run's rng.
 */

#define VOICING_OPERATOR_NAME "voicing-satb"

#define VOICING_DEFAULT_CANDIDATES 4
#define VOICING_DEFAULT_BEAM 64

typedef struct {
    size_t candidates; /* the voicings proposed; 0 means VOICING_DEFAULT_CANDIDATES */
    size_t beam;       /* the paths kept at each chord; 0 means VOICING_DEFAULT_BEAM */
} VoicingParams;

VoicingParams voicing_params_default(void);

/*
 * The checker for this operator's candidates; pass it as FillConfig.checker. Hard rules reject a candidate: a chord that
 * standard-voicing does not hold for, parallel fifths or octaves (standard-counterpoint), an unresolved chordal seventh
 * or leading tone (standard-harmony). Soft, the score being the share that hold: every upper voice moves by a third or
 * less in a connection. A candidate of another operator goes to harmony_phrase_checker.
 */
bool voicing_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                     const FillCandidate *candidate, void *context, FillCheck *out);

/*
 * Registers the operator with these parameters. The registry keeps the pointer, not a copy, so params must outlive
 * it (NULL means the defaults).
 */
FillResult voicing_register(FillOperatorRegistry *registry, const VoicingParams *params);

#endif
