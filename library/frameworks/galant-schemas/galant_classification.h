#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_CLASSIFICATION_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_CLASSIFICATION_H

#include <stdbool.h>

#include "galant_exemplar.h"
#include "galant_prototypes.h"

/*
 * Classifying an exemplar for galant-schemas-v1: which prototypes it resembles.
 * An exemplar resembles a prototype when it breaks none of the hard matching
 * rules against it (galant_matching_rules.h); soft rules are advice and do not
 * count. An exemplar can resemble more than one prototype, or none: the
 * Prinner and the modulating Prinner share their beat pattern and several of
 * their degrees.
 */

/* Sets bit (1u << id) in *schemas for each GalantSchemaId whose prototype the
   exemplar resembles; 0 when it resembles none. False (*schemas untouched) for
   an invalid exemplar or NULL. */
bool galant_classify(const GalantExemplar *exemplar, unsigned *schemas);

/* True when the exemplar resembles the prototype of schema `id`. False when it
   does not, and for an invalid exemplar or schema. */
bool galant_resembles(const GalantExemplar *exemplar, GalantSchemaId id);

#endif
