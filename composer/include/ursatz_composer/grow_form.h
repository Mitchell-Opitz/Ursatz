#ifndef URSATZ_COMPOSER_GROW_FORM_H
#define URSATZ_COMPOSER_GROW_FORM_H

#include <stdbool.h>

#include <ursatz_composer/fill_loop.h>
#include <ursatz_composer/fill_operator.h>
#include <ursatz_composer/grow.h>
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
 *
 * The first idea of a slot with a motive of its own (GrowRequest.slot_motives) restates that motive, and the slot's later
 * ideas are made from it as the basic motive's are (grow.h).
 *
 * A pool of saved motive-forms (R-166) is offered first for every idea this operator makes, before the registry's forms:
 * a pool form is offered for an idea when it derives from the motive the idea is made from (slot_motive_of: the slot
 * whose motive the form was made from, basic for m0, and the section's own motive), its functions include the function of
 * the idea's slot (GrowSlot, grow_slot_function) and it fits as a registry form does (a full motive in range, no wider a
 * leap, no longer than the idea's room, the cadence tone of an idea that ends in a cadence). It scores 1 plus the
 * connection plus 1/2 when approved, 1/4 when unflagged, so it outranks a registry form of the same connection and an
 * approved one an unflagged one. The candidate's label names it as "pool <item> <connection>%: <chain>".
 */

/* The functions a form suits (R-143, variant_functions), as bits. */
#define GROW_FUNCTION_BEGINNING 1u
#define GROW_FUNCTION_CONTINUATION 2u
#define GROW_FUNCTION_CONTRAST 4u
#define GROW_FUNCTION_CADENTIAL 8u

/* The bit of the function a slot's ideas have (grow_slot_function). */
unsigned grow_slot_function_bit(GrowSlot slot);

/* A saved motive-form the motives stage may use (R-166). The strings are copies; the motive must outlive the run. */
typedef struct GrowPoolForm {
    const Motive *motive;
    GrowSlot slot_motive_of; /* the slot whose motive the form derives from; basic for m0 */
    unsigned functions;      /* GROW_FUNCTION_* bits */
    bool approved;           /* approved forms are preferred over unflagged ones */
    char item[64];           /* the saved item's id, named in the record */
    char chain[256];         /* how it derives, as the item saved it (shown in the record and by --explain) */
} GrowPoolForm;

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
