#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_PROTOTYPE_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_PROTOTYPE_H

#include <stdbool.h>
#include <stddef.h>

#include "galant_degree.h"
#include "galant_exemplar.h"
#include "semantics/beat_strength.h"
#include "semantics/schema_function.h"

/*
 * A prototype for galant-schemas-v1 (SchemaSense): the idealized version of a
 * common pattern. A schema is defined by what happens in it, two or more
 * stages, and by where it typically appears (SchemaFunction). Each stage has:
 *
 *   strength          whether it falls on a strong ("S") or weak ("W") beat
 *   melody, bass      the scale degrees of the top and bottom voices
 *   melody_next       for a stage whose melody has two notes (the Monte's do-te
 *                     and re-do), the degree of the second; a degree of 0 when
 *                     the stage has one melody note
 *   figures           the figured bass above the bass note, or NULL when there
 *                     is none
 *   numeral           the Roman numeral in the tonic key
 *   numeral_dominant  for a schema that modulates to the dominant, the Roman
 *                     numeral in the dominant key; NULL otherwise
 *
 * The figures and numerals are the schema's harmony, kept as data. They are not
 * checked in an exemplar: that needs chord analysis, which this framework does
 * not do.
 *
 * Two flags say how an exemplar is matched (galant_matching_rules.h):
 *
 *   bass_defining      the bass is what defines the schema (the Prinner is four
 *                      bass notes), so an exemplar must have it; otherwise the
 *                      bass is less fixed than the melody and is advice only
 *   melody_in_tenths   the melody accompanies the bass in parallel tenths
 */

typedef enum {
    GALANT_SCHEMA_MEYER = 0,
    GALANT_SCHEMA_PRINNER,
    GALANT_SCHEMA_MODULATING_PRINNER,
    GALANT_SCHEMA_COUNT
} GalantSchemaId;

typedef struct {
    BeatStrength strength;
    GalantDegree melody;
    GalantDegree bass;
    const char *figures;
    const char *numeral;
    const char *numeral_dominant;
    GalantDegree melody_next;
} GalantStage;

typedef struct {
    GalantSchemaId id;
    const char *name;
    SchemaFunction function;
    size_t stage_count;
    GalantStage stages[GALANT_MAX_STAGES];
    bool bass_defining;
    bool melody_in_tenths;
} GalantPrototype;

#endif
