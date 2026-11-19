#ifndef URSATZ_COMPOSER_FORM_RECORD_H
#define URSATZ_COMPOSER_FORM_RECORD_H

#include <stddef.h>

#include <ursatz_composer/form_spec.h>
#include "structure/plan_util.h"
#include <plan/plan.h>

/* Private: not installed, not part of the API. */

/* The sections of the plan in order (not the material or the seam), at most FORM_SPEC_MAX_SECTIONS; how many. */
size_t form_record_sections(const Plan *plan, NodeId *sections);

/*
 * The size of the form whose sections the plan has: as many, in the same proportions (a size of the form at another
 * scale has the same sections in the same places). The first such size; NULL for none.
 */
const FormSpecSize *form_record_size_of_plan(const FormSpec *form, const Plan *plan, const NodeId *sections, size_t count);

/* Records the form a plan was built from in it (name, version, the hash of the built-in document, checks, closing). */
PieceResult piece_set_form_record(Plan *plan, const FormSpec *form);

/*
 * A plan of a format before 5 has no form record: this gives it the record of the built-in form whose plan name is its root node's
 * name, and each section's `closed` from that form's size (the first size with the plan's sections in its proportions). A plan of
 * no built-in form is left as it is. False only when out of memory; nothing is Given yet when the plan reader calls it.
 */
bool piece_form_record_migrate(Plan *plan);

#endif
