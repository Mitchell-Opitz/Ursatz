#ifndef MUSIC_SEMANTICS_SCHEMA_FUNCTION_H
#define MUSIC_SEMANTICS_SCHEMA_FUNCTION_H

#include <stdbool.h>

/*
 * SchemaFunction (Value): where a Galant schema typically appears in a piece,
 * the "when" of a schema (the "what" is its melodic, harmonic, and metric
 * stages). A plain enum: it names a place and carries no EntityID, no
 * pitches, and no framework claim.
 *
 *   OPENING_GAMBIT   a schema typically used to start a piece, such as the
 *                    Meyer
 *   CONTINUATION     a schema typically used after an opening, as a response to
 *                    it or to carry the music on, such as the Prinner
 *   CADENCE          a schema typically used to conclude
 *   POST_CADENTIAL   a schema typically used after the cadence
 *
 * Which schemas have which function, and what their stages are, is a
 * convention of a Framework (frameworks/galant-schemas), not of this type.
 */

typedef enum {
    SCHEMA_FUNCTION_OPENING_GAMBIT = 0,
    SCHEMA_FUNCTION_CONTINUATION,
    SCHEMA_FUNCTION_CADENCE,
    SCHEMA_FUNCTION_POST_CADENTIAL,
    SCHEMA_FUNCTION_COUNT
} SchemaFunction;

/* True for the four functions above; false for any other value. */
bool schema_function_valid(SchemaFunction function);

/* "opening-gambit" "continuation" "cadence" "post-cadential"; NULL for an
   invalid function. */
const char *schema_function_name(SchemaFunction function);

#endif
