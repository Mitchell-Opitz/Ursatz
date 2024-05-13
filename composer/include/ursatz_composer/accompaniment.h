#ifndef URSATZ_COMPOSER_ACCOMPANIMENT_H
#define URSATZ_COMPOSER_ACCOMPANIMENT_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>

/*
 * The accompaniment operator: a Fill operator for the inner voice lanes (R-065). It reads a harmony lane, a bass lane
 * and a melody lane that are already written (Realized or Given, never open) and writes an accompaniment under the
 * melody and over the bass, in the inner voices 1 to 3.
 *
 * A target is a Constrained inner voice span, voice 1, with an ACCOMPANIMENT constraint (constraint.h) on the span or
 * on its owner, over the time of a harmony target: the chord spans under it tile it exactly, its bass and melody are
 * written, and it lies in one key. The other voices the pattern needs, 2 and 3, are Constrained spans over the same
 * time, or are not there at all; one candidate writes every voice (the voices the pattern does not use are written
 * empty, a span with no notes). Anything else, the operator does not apply to.
 *
 * The patterns sit on the meter's beat (PlanMeter.beat_unit; the dotted beat in a compound meter, 6/8):
 *   block        the chord on each beat, held to the next, one chord tone per voice (voices 1 to 3, top to bottom);
 *   arpeggiated  the chord's tones, rising then falling, one per eighth, in voice 1;
 *   Alberti      low, high, middle, high in eighths, in voice 1; simple meters only;
 *   oom-pah      a rest on the strong beats (the bass sounds them) and the chord on the weak ones, as the block does.
 * The tones come from the harmony lane's numerals in the key. A chord is voiced in the position closest to the one
 * before it (the fewest semitones moved), inside the constraint's low to high, above the bass note sounding with it
 * and below the lowest melody note sounding with it. ANY chooses among the patterns the meter allows, and the
 * texture asked for allows: a homophonic texture every pattern, homorhythm the block only. Every choice that is not
 * forced comes from the run's rng.
 */

#define ACCOMPANIMENT_OPERATOR_NAME "accompaniment"

#define ACCOMPANIMENT_DEFAULT_CANDIDATES 4

typedef struct {
    size_t candidates; /* the accompaniments proposed; 0 means ACCOMPANIMENT_DEFAULT_CANDIDATES */
} AccompanimentParams;

AccompanimentParams accompaniment_params_default(void);

/*
 * The checker for this operator's candidates; pass it as FillConfig.checker. Hard rules reject a candidate: every
 * note a chord tone of the harmony sounding at its start, inside the range, and above the bass and below the melody at
 * every moment it sounds. Soft, the score being the share that hold: the candidate moves fewer than 3 semitones per
 * voice per chord change on average. A candidate of another operator goes to harmony_phrase_checker.
 */
bool accompaniment_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                           const FillCandidate *candidate, void *context, FillCheck *out);

/*
 * Registers the operator with these parameters. The registry keeps the pointer, not a copy, so params must outlive
 * it (NULL means the defaults).
 */
FillResult accompaniment_register(FillOperatorRegistry *registry, const AccompanimentParams *params);

#endif
