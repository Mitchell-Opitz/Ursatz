#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_FRAMEWORK_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_FRAMEWORK_H

#include "theory/framework.h"

/*
 * galant-schemas-v1: Theory Framework for the schemas of the Galant style: the
 * stock musical phrases that composers and improvisers of the 18th century
 * learned as the melodic, harmonic, and metric skeletons for new music. A
 * schema is defined by its stages (melody and bass scale degrees, figures,
 * Roman numerals, and the strength of the beat each falls on) and by where it
 * typically appears (SchemaFunction). This framework holds the prototypes
 * (SchemaSense) and decides whether an exemplar resembles one. Built as a
 * Framework extension module physically outside src/theory/, the same shape
 * as standard-counterpoint; this module never modifies src/theory/ or
 * src/rules/.
 *
 * The one dependency on another framework is one way: this module reads the
 * pitch arithmetic of frameworks/standard-counterpoint, and that framework
 * knows nothing of this one. See README.md for the full scope statement.
 *
 * Dependency ceiling: canonical L0-L6 interfaces, theory/framework.h,
 * rules/rule.h, rules/constraint.h, and standard-counterpoint/. Enforced by
 * test_architecture_galant_schemas.c.
 */

/*
 * Returns the single shared galant-schemas-v1 Framework instance, lazily
 * constructed on first call. Owned by this module; callers must not destroy
 * it. Returns NULL only if the one-time construction failed (allocation
 * failure).
 */
const Framework *galant_schemas_framework(void);

#endif
