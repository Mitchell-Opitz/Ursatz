#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_MOTIVE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_MOTIVE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "semantics/motive_kind.h"

/*
 * Motive rules for standard-form-v1. A motive is a short, regularly recurring snippet of
 * music, typically smaller than an idea:
 *
 *   MOTIVE_SHORT            a motive is shorter than the idea that contains it; a whole
 *                           theme or melody is not a motive
 *   MOTIVE_DEFAULT_KIND     "motive" by itself means a pitch-based motive; a motive known
 *                           mainly by its rhythm, contour or timbre is named for that
 *   MOTIVE_TRANSFORMATIONS  the transformations a recurring motive commonly undergoes, each
 *                           mapped to the Transformation subtype that carries it out
 *
 * Lengths are event counts, so the rule needs no tempo or meter.
 */

typedef enum {
    STANDARD_FORM_MOTIVE_RULE_SHORT = 0,
    STANDARD_FORM_MOTIVE_RULE_DEFAULT_KIND,
    STANDARD_FORM_MOTIVE_RULE_TRANSFORMATIONS,
    STANDARD_FORM_MOTIVE_RULE_COUNT
} StandardFormMotiveRule;

const Rule *standard_form_motive_rule(StandardFormMotiveRule which);
const Constraint *standard_form_motive_constraint(StandardFormMotiveRule which);

/* MOTIVE_SHORT: true when 0 < motive_event_count < idea_event_count. */
bool standard_form_motive_fits_idea(size_t motive_event_count, size_t idea_event_count);

/* MOTIVE_DEFAULT_KIND: the kind a bare "motive" means (MOTIVE_KIND_PITCH). */
MotiveKind standard_form_motive_default_kind(void);

typedef enum {
    STANDARD_FORM_MOTIVE_TRANSFORMATION_ENLARGEMENT = 0,
    STANDARD_FORM_MOTIVE_TRANSFORMATION_CONTRACTION,
    STANDARD_FORM_MOTIVE_TRANSFORMATION_INVERSION,
    STANDARD_FORM_MOTIVE_TRANSFORMATION_DISPLACEMENT,
    STANDARD_FORM_MOTIVE_TRANSFORMATION_RETROGRADE,
    STANDARD_FORM_MOTIVE_TRANSFORMATION_INTERVALLIC_MANIPULATION,
    STANDARD_FORM_MOTIVE_TRANSFORMATION_EMBELLISHMENT,
    STANDARD_FORM_MOTIVE_TRANSFORMATION_COUNT
} StandardFormMotiveTransformation;

/* True for the seven transformations above; false for any other value. */
bool standard_form_motive_transformation_valid(StandardFormMotiveTransformation transformation);

/* "enlargement" "contraction" "inversion" "displacement" "retrograde"
   "intervallic-manipulation" "embellishment"; NULL for an invalid value. */
const char *standard_form_motive_transformation_name(StandardFormMotiveTransformation transformation);

/*
 * The Transformation subtype that carries the transformation out, by its src/transform/
 * file name: "augmentation" "diminution" "inversion" "displacement" "retrograde"
 * "intervallic_manipulation" "ornamentation". Enlargement lengthens durations and
 * contraction shortens them, so contraction here is diminution, not the sub-range removal
 * of transform/contraction.h. NULL for an invalid value.
 */
const char *standard_form_motive_transformation_type(StandardFormMotiveTransformation transformation);

#endif
