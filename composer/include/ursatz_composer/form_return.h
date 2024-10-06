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
 *   - the variation the constraint names is EXACT (the others are offered by form_vary.c).
 * It applies once the returned section is Realized with every lane span under it written (the harmony, bass, melody
 * and inner voice lanes). Anything else, the operator does not apply to; grow-theme leaves a section the operator can
 * return alone (form_return_handles), and grows the others afresh from their motive.
 *
 * apply copies the subtree of the returned section (its phrases, subphrases and ideas, with their names, constraints
 * and states) and its lane spans (with their constraints, written, in the state they were in, Given or Realized
 * becoming Realized), shifted by the time between the two sections. The motive references point at the same material
 * motives. The key lane is not copied: the key plan decides it. An EXACT return is the copy, Realized.
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
 * Registers form-return. melody gives the range a re-registered melody keeps to (NULL for the defaults); the registry
 * keeps the pointer, so it must outlive the registry.
 */
FillResult form_return_register(FillOperatorRegistry *registry, const MelodyFillParams *melody);

/*
 * The checker for form-return's candidates; pass it as FillConfig.checker. A hard rule rejects a candidate whose
 * structure is not the returned section's, shifted, or whose melody, bass or harmony differ from it (an EXACT return
 * differs only in the transposition by the key difference). The score is 1/1 for a candidate that keeps to it. A
 * candidate of another operator is accepted at 0/1.
 */
bool form_return_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                         const FillCandidate *candidate, void *context, FillCheck *out);

#endif
