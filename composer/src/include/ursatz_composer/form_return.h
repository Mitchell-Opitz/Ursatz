#ifndef URSATZ_COMPOSER_FORM_RETURN_H
#define URSATZ_COMPOSER_FORM_RETURN_H

#include <stdbool.h>

#include <ursatz_composer/constraint.h>
#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/melody_fill.h>
#include <ursatz_composer/piece_plan.h>

/*
 * The return operator (R-070): a Grow operator for a theme section that returns an earlier one. A returning section
 * reuses the realized material of the section it returns instead of being grown afresh from its motive.
 *
 * A target is a Constrained theme section with no children and a RETURN constraint (constraint.h) naming a section node
 * of the plan, when
 *   - the returned section is a section of the piece (not the material), another section, and as long as the target;
 *   - the meter lane is the same over both, and the key lane has the same shape over both: the same number of key
 *     spans, cut at the same bars, each in the same mode, the key of the target's differing from the other's by one
 *     amount throughout (the target is then transposed by it);
 *   - the variation the constraint names is one the operator makes: EXACT, ORNAMENT, REHARMONIZE or REREGISTER.
 * It applies once the returned section is Realized with every lane span under it written (the harmony, bass, melody
 * and inner voice lanes). Anything else, the operator does not apply to; grow-theme leaves a section the operator can
 * return alone (form_return_handles), and grows the others afresh from their motive.
 *
 * apply copies the subtree of the returned section (its phrases, subphrases and ideas, with their names, constraints
 * and states) and its lane spans (with their constraints, written, in the state they were in, Given or Realized
 * becoming Realized), shifted by the time between the two sections. The motive references point at the same material
 * motives. The key lane is not copied: the key plan decides it. The copy is Realized, except for what a variation
 * reopens (form_vary.c):
 *
 *   EXACT        nothing is changed.
 *   ORNAMENT     the melody keeps every note and gains passing and neighbor tones between them, off the strong beats, at
 *                most one added note a beat; candidates differ in which notes are ornamented.
 *   REHARMONIZE  the harmony spans are reopened as the targets they were written from (the HARMONY_SHAPE and the cadence
 *                kept), and the bass and the inner voices with them, each as an open span with the constraints it had;
 *                the melody is the returned one. compose_run runs the harmony, bass, melody and texture stages again
 *                after the returns, writes the chords the harmony operators write, and keeps the result when every
 *                strong-beat melody note is a tone of the chord under it (form_return_melody_fits), else tries again.
 *   REREGISTER   the melody, and the inner voices with it, an octave up or down; one candidate for each that keeps the
 *                melody inside its range, none when neither does, and the section stays open.
 *
 * The checker (form_return_checker) holds each candidate to its variation's rules.
 */

#define FORM_RETURN_OPERATOR_NAME "form-return"

/*
 * Whether the operator can return the section, whatever state the returned section is in: the section is a Constrained
 * theme section with no children and a RETURN that satisfies the list above. grow-theme does not apply to such a
 * section, and compose_run leaves it for the RETURNS stage.
 */
bool form_return_handles(const PiecePlan *plan, NodeId section);

/* The section the target returns and how, when form_return_handles holds. */
bool form_return_source(const PiecePlan *plan, NodeId section, NodeId *source, PlanReturnVariation *variation);

/*
 * Whether every melody note that starts on a strong beat (the start of a cell of the meter, where a chord may start) in
 * the section is a tone of the chord under it. A section whose key lane has more than one span is not judged: true.
 * False when the harmony or the melody cannot be read.
 */
bool form_return_melody_fits(const PiecePlan *plan, NodeId section);

/*
 * How many of the chords of a section that returns another (a RETURN constraint) are not the chords of the other at the
 * same place: another numeral, or the same one over another length. *total is the number of chords of the section. A
 * reharmonized return changes some. False when the section has no RETURN or either harmony lane cannot be read.
 */
bool form_return_chords_changed(const PiecePlan *plan, NodeId section, size_t *changed, size_t *total);

/*
 * Whether the cadence of a section that returns another is changed: a chord that starts in the last quarter of the
 * section (the last 2 bars of 8, the last bar of 4) is not the chord of the other at the same place, as
 * form_return_chords_changed compares them. False when the section has no RETURN or either harmony lane cannot be
 * read.
 */
bool form_return_cadence_changed(const PiecePlan *plan, NodeId section, bool *changed);

/*
 * Registers form-return. melody gives the range a re-registered melody keeps to (NULL for the defaults); the registry
 * keeps the pointer, so it must outlive the registry.
 */
FillResult form_return_register(FillOperatorRegistry *registry, const MelodyFillParams *melody);

/*
 * The checker for form-return's candidates; pass it as FillConfig.checker. Hard rules reject a candidate that does not
 * keep to its variation: the structure is the returned section's, shifted by the time between the two and transposed by
 * the key difference; an ornamented melody keeps every note and adds only passing and neighbor tones, off the strong
 * beats, one a beat; a reharmonization reopens the harmony, bass and inner voices, covers the returned harmony and keeps
 * the melody; a re-registered melody is an
 * octave away and inside the range. The score is 1/1 for a candidate that keeps to it and changes what its variation
 * changes (an ornament with a note added), else 0/1. A candidate of another operator is accepted at 0/1.
 */
bool form_return_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out);

#endif
