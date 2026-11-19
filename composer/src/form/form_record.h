#ifndef URSATZ_COMPOSER_FORM_RECORD_H
#define URSATZ_COMPOSER_FORM_RECORD_H

#include <stddef.h>

#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/piece.h>
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

#endif
