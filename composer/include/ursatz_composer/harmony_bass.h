#ifndef URSATZ_COMPOSER_HARMONY_BASS_H
#define URSATZ_COMPOSER_HARMONY_BASS_H

#include <stdbool.h>

#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/lanes.h>

/*
 * The bass operator: a Fill operator for the bass lane. It reads a harmony lane
 * that is already written (Realized or Given, never open) and writes one bass
 * span for each chord, aligned with the chord's span, so the bass spans tile the
 * phrase as the chords do. Each note is the chord's bass member: the root of a
 * chord in root position, the third in first inversion, the fifth in second, the
 * seventh in third; an applied chord (root position only) sounds its root.
 *
 * A target is a Constrained bass lane span, which stands for the phrase and holds
 * one placeholder note (a bass span needs a note). It must cover exactly the
 * chord spans under it, lie inside one meter lane span and have a key lane span
 * or key constraint over it. Anything else, the operator does not apply to.
 * The loop never offers a Given span, so Given bass is never overwritten.
 *
 * A chord of two cells or more may be split on a cell boundary (a strong beat of
 * the meter) into up to four notes, each the chord's bass member, repeated or
 * moved by an octave. No other pitch is written, so every bass note matches its
 * chord.
 */

#define HARMONY_BASS_OPERATOR_NAME "harmony-bass"

/*
 * range_low and range_high are MIDI pitches, both included. subdivision is the
 * chance, in thousandths, that a chord of two cells or more is split into
 * several notes; stepwise is the chance, in thousandths, that a chord's first
 * note takes the octave nearest the note before it, instead of any octave in
 * range. Values above 1000 count as 1000.
 */
typedef struct {
    int range_low;
    int range_high;
    unsigned subdivision;
    unsigned stepwise;
} HarmonyBassParams;

/* The defaults: MIDI 40 to 60 (E2 to C4), subdivision 250, stepwise 750. */
HarmonyBassParams harmony_bass_params_default(void);

/*
 * Checks the bass lane over span, which must hold one bass span for each chord of the harmony lane,
 * aligned with it, against standard-counterpoint's melodic rules, reading only what is in the plan.
 * Every note must be in range (params, NULL for the defaults) and the bass member of its chord; the
 * line must break no hard melodic rule (augmented, diminished and seventh leaps, outlines of them), and
 * soft rules are not asked. True when the lane could be read, with *violation NULL if nothing is broken
 * and otherwise the rule that is.
 */
bool harmony_check_bass(const PiecePlan *plan, PlanSpan span, PlanKey key, const HarmonyBassParams *params,
                        const char **violation);

/*
 * Registers the operator with these parameters. The registry keeps the pointer,
 * not a copy, so params must outlive it (NULL means the defaults).
 */
FillResult harmony_bass_register(FillOperatorRegistry *registry, const HarmonyBassParams *params);

#endif
