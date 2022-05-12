#ifndef MUSIC_SEMANTICS_SCHEMA_SENSE_H
#define MUSIC_SEMANTICS_SCHEMA_SENSE_H

#include <stdbool.h>

/*
 * SchemaSense (Value): one of the three ways the term "schema" is applied to
 * a stock musical phrase. A plain enum: it names a sense and carries no
 * EntityID, no pitches, and no framework claim.
 *
 *   PROTOTYPE   an idealized version of a common pattern
 *   EXEMPLAR    a single pattern that resembles the prototype
 *   THEORY      an explanation of a commonly occurring musical event: how an
 *               exemplar, understood as a version of a prototype, is
 *               functioning in a particular passage
 *
 * An exemplar is understood as a version of a prototype, and that
 * relationship is the theory.
 */

typedef enum {
    SCHEMA_SENSE_PROTOTYPE = 0,
    SCHEMA_SENSE_EXEMPLAR,
    SCHEMA_SENSE_THEORY,
    SCHEMA_SENSE_COUNT
} SchemaSense;

/* True for the three senses above; false for any other value. */
bool schema_sense_valid(SchemaSense sense);

/* "prototype" "exemplar" "theory"; NULL for an invalid sense. */
const char *schema_sense_name(SchemaSense sense);

#endif
