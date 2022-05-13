#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_PROTOTYPES_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_PROTOTYPES_H

#include <stdbool.h>

#include "galant_prototype.h"

/*
 * The prototypes of galant-schemas-v1. Each is as the schema is set out in the
 * Galant schemas chapter of Open Music Theory (Gotham and Shaffer); other
 * schemas (the Pastorella, Jupiter, and Aprile, and the continuation, cadence,
 * and post-cadential schemas of the summary chapter) are not yet encoded.
 *
 *   MEYER             an opening gambit of four stages, the first and last
 *                     tonic chords and the two in the middle not tonic:
 *                       strength   W    S    W    S
 *                       melody     do   ti   fa   mi
 *                       bass       do   re   ti   do
 *                       numeral    I    V    V    I
 *                     The bass and the harmony are less fixed than the melody.
 *                     It divides into an opening part, I to V, and a closing
 *                     part, V to I (not encoded).
 *   PRINNER           a continuation, the typical response to an opening
 *                     schema, four stages on four bass notes with the melody
 *                     accompanying them in parallel tenths:
 *                       strength   S    W    S    W
 *                       melody     la/le sol  fa   mi/me
 *                       bass       fa   mi/me re   do
 *                       figures    5    6    7-6  5   (the chapter's row ends in
 *                                                       "I", a numeral; the
 *                                                       other schemas end in 5)
 *                       numeral    IV   I    vii  I
 *   MODULATING_PRINNER  the Prinner as a modulation from the tonic to the
 *                     dominant:
 *                       strength   S    W    S    W
 *                       melody     mi   re   do   ti
 *                       bass       do   ti   la   sol
 *                       figures    5    6    7-#6  5
 *                       numeral    I    V    vii/V  V   (tonic key)
 *                       numeral    IV   I    vii  I      (dominant key)
 *
 * The Prinner and the modulating Prinner are classified as continuation
 * (SchemaFunction): the chapter calls the Prinner a closing gesture, a response
 * to an opening schema.
 */

/* True for the three schemas above; false for any other value. */
bool galant_schema_valid(GalantSchemaId id);

/* "meyer" "prinner" "modulating-prinner"; NULL for an invalid schema. */
const char *galant_schema_name(GalantSchemaId id);

/* The prototype of a schema, owned by this module; NULL for an invalid
   schema. */
const GalantPrototype *galant_prototype(GalantSchemaId id);

#endif
