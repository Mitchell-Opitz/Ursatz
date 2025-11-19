#ifndef URSATZ_COMPOSER_GROW_FORM_H
#define URSATZ_COMPOSER_GROW_FORM_H

#include <stdbool.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>

/*
 * The motive-forms of a grown theme's ideas (R-143, R-145). grow-theme gives each idea after the basic idea a motive
 * node m<slot> with the P13 motive_ref it was planned with; in the motives stage this operator asks the variant
 * registry for forms of the basic idea that suit the idea's place in the theme, and ranks them by how well they connect
 * (motive_connection):
 *
 *   repetition               the exact restatement, or a transposition; see grow.md for the dominant form
 *   consequent basic idea    the exact restatement
 *   contrasting ideas        a form with the function `contrast` whose contour differs from the basic idea's
 *   cadential ideas          a form with the function `cadential`
 *
 * The slot is the name of the idea whose melody asks for the motive node. Candidates are one-step chains of registry
 * rows with the open parameters drawn from fixed seeds, so the same plan always gives the same candidates; only a tie in
 * the connection score is settled by the run's seed. A form must be a full motive, keep every pitch within C#4 to C6
 * (MIDI 58 to 84) and fit the idea. A slot with no such form is left to the P13 motive operators.
 *
 * Applies to a Constrained motive node of the material whose melody is asked for by an idea of that name and whose
 * motive_ref names a motive node with a melody; it writes the Realized melody lane span of the node like the P13
 * operators do. The motive operators' MotiveDevelopContext.basic gives m0 its harmony, features and role.
 */

#define GROW_FORM_OPERATOR_NAME "motive-form"

/* context may be NULL for the defaults and must outlive the registry otherwise. */
FillResult grow_form_register(FillOperatorRegistry *registry, const MotiveDevelopContext *context);

/*
 * The checker of the grow motives stage. A motive-form candidate scores 1 plus its connection score (so a form always
 * outranks the P13 operators, whose recognizability is at most 1, and the part after the 1 is the connection); a
 * candidate of the P13 operators is checked by motive_develop_checker.
 */
bool grow_motives_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                          const FillCandidate *candidate, void *context, FillCheck *out);

/* The motive a motive-form candidate proposes, NULL for any other candidate. */
const Motive *grow_form_candidate_motive(const FillCandidate *candidate);

#endif
