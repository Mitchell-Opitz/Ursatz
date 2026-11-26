#ifndef URSATZ_COMPOSER_MOTIVE_BLUEPRINT_H
#define URSATZ_COMPOSER_MOTIVE_BLUEPRINT_H

#include <stdbool.h>
#include <stddef.h>

#include <plan/plan_constraint.h>
#include <ursatz_composer/harmony_chords.h>

/*
 * The melody of a blueprint (R-746, composer/docs/blueprints.md): where a chosen non-chord tone sounds between the notes
 * placed around it.
 */

/*
 * The pitch of an event (a chosen non-chord tone) between two placed notes, `previous` before it and `next` after it, by
 * R-746's rule, in the scale of the key (the natural scale, with the pitch classes of the two neighbours added, so a raised
 * leading tone steps as the scale does around it):
 *   appoggiatura   a scale step above next; below when previous is below next and next - previous is a step (1 or 2)
 *   suspension     previous
 *   retardation    previous
 *   passing        the scale step between when previous and next are a third apart (3 or 4 semitones), else the scale
 *                  tone nearest their middle (the lower on a tie)
 *   neighbor       a scale step above previous; below when next is above previous
 *   escape         a scale step from previous away from next (the direction of the line it leaves by a leap); up when
 *                  next is previous
 *   anticipation   next
 * A missing neighbour (-1) takes the other's pitch; -1 when both are missing. The chords are not read (the rule is the
 * scale's); NULL is allowed.
 */
int composer_event_pitch(const ComposerChords *chords, const SpelledKey *key, PlanEventType type, int previous, int next);

#endif
