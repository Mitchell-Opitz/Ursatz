#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_PROTOTYPES_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_PROTOTYPES_H

#include <stdbool.h>

#include "galant_prototype.h"

/*
 * The prototypes of galant-schemas-v1.
 * Meyer, Prinner, and the modulating Prinner are set out in the Galant schemas
 * chapter; the rest are from the summary chapter, taken from its itemized stage
 * tables (where the summary table disagrees, the itemized table is followed).
 * Each schema lists its stages as strength (S strong, W weak), melody degrees,
 * bass degrees, figures, and Roman numerals; a degree of di or fi is raised, te
 * is lowered, and la/le and mi/me accept either.
 *
 * Opening gambits:
 *   MEYER                 W S W S   do ti fa mi / do re ti do        5 6/4/3 6/5 5   I V V I
 *                         (the bass and harmony are less fixed than the melody;
 *                         it divides into an opening part, I to V, and a closing
 *                         part, V to I, which are not encoded)
 *   ROMANESCA             S W S W   do sol do do / do ti la mi       5 6 5 6         I V vi I
 *   DO_RE_MI              S W S     do re mi / do ti do              5 6 5           I V I
 *   DO_RE_MI_FOUR_STAGE   S W W S   do re re mi / do ti ti do        5 6 6 5         I V V I
 *                         (derived: the Do-Re and the Re-Mi parts put back
 *                         together, as the chapter describes; it has no table)
 *   SOL_FA_MI             W S W S   sol fa fa mi / do re ti do       5 5 6/5 5       I ii V I
 *                         (the harmonic variant I vii V I is not encoded)
 *   APRILE                S W S W   do ti re do / do re ti do        5 6/4/3 6/5 5   I V V I
 *   JUPITER               S W S W   do re fa mi / do ti sol do       5 6 5 5         I V V I
 *   PASTORELLA            S W S W   mi re fa mi / do sol sol do      5 5 7 5         I V V I
 *                         (Meyer, Aprile, Jupiter, and Pastorella have a bass and
 *                         harmony less fixed than the melody)
 *
 * Continuation (the answer, process, and transition schemas):
 *   PRINNER               S W S W   la sol fa mi / fa mi re do       5 6 7-6 5       IV I vii I
 *                         (in parallel tenths; the chapter's figures row ends in
 *                         "I", a numeral, where the other schemas end in 5)
 *   MODULATING_PRINNER    S W S W   mi re do ti / do ti la sol       5 6 7-#6 5      I V vii/V V
 *                         (in the dominant key: IV I vii I)
 *
 * Other schemas of the Galant style are not yet encoded. The Prinner schemas are
 * classified as continuation (SchemaFunction): the chapter calls the Prinner a
 * closing gesture, a response to an opening schema. The figures and numerals are
 * data and are not checked in an exemplar.
 */

/* True for a schema above; false for any other value. */
bool galant_schema_valid(GalantSchemaId id);

/* The name of a schema, in lowercase and hyphenated (GALANT_SCHEMA_MEYER is
   "meyer", GALANT_SCHEMA_DO_RE_MI is "do-re-mi"); NULL for an invalid schema. */
const char *galant_schema_name(GalantSchemaId id);

/* The prototype of a schema, owned by this module; NULL for an invalid
   schema. */
const GalantPrototype *galant_prototype(GalantSchemaId id);

#endif
