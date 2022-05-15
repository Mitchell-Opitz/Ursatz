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
 *   PRINNER_WITH_DOMINANT S W S W S la sol fa fa mi / fa mi re sol do  5 6 7-6 7 5   IV I vii V I
 *   MODULATING_PRINNER_WITH_DOMINANT
 *                         S W S W S mi re do do ti / do ti la re sol   5 6 7-#6 7/# 5   I V vii/V V/V V
 *                         (the variants with an added dominant are not in
 *                         parallel tenths at that stage)
 *   FONTE                 W S W S   sol fa fa mi / di re ti do       6/5 5 6/5 5     V/ii ii V I
 *   MONTE                 W S W S   do-te la re-do ti / mi fa fi sol 6/5 5 6/5 5     V/IV V V/V V
 *                         (the first and third stages have two melody notes)
 *   PONTE                 S W S     sol ti re / sol sol sol          5 7 7           V V V
 *
 * Pre-cadential (incomplete cadences):
 *   FENAROLI              S W S W   fa mi ti do / ti do re mi        6/5 5 6/5 6     V I V I
 *   INDUGIO               S W S W S re fa la do ti / fa fa fa fi sol 6/5 6/5 6/5 6/5 5  IV IV IV V/V V
 *   DECEPTIVE_CADENCE     W S W S   do re re do / mi fa sol la       6 6/5 5 5       I ii V vi
 *   EVADED_CADENCE        W S W S   do re re do / mi fa sol mi       6 6/5 5 6       I ii V I
 *   PASSO_INDIETRO        S W       ti do / fa mi                    6/4/2 6         V I
 *                         (essentially the first two stages of a Prinner)
 *   COMMA                 W S       fa mi / ti do                    6/5 5           V I
 *   CONVERGING_CADENCE    W S W S   mi re do ti / mi fa fi sol       6 6/5 6/5 5     I ii V/V V
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
