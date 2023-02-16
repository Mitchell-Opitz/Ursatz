#ifndef MUSIC_SEMANTICS_EXPECTED_LENGTH_BASIS_H
#define MUSIC_SEMANTICS_EXPECTED_LENGTH_BASIS_H

#include <stdbool.h>

/*
 * ExpectedLengthBasis (Value): what gives a phrase an expected length to compare against. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   ANTICIPATED_CADENCE   the cadential motion allows a cadence to be anticipated where it is evaded
 *   UNEXPANDED_MODEL      the piece states an unexpanded version of the phrase to compare against
 *
 * How the expected length is found is a judgment of an analysis; the conventions for using it
 * belong to a Framework (frameworks/standard-form), not to this type.
 */

typedef enum {
    EXPECTED_LENGTH_BASIS_ANTICIPATED_CADENCE = 0,
    EXPECTED_LENGTH_BASIS_UNEXPANDED_MODEL,
    EXPECTED_LENGTH_BASIS_COUNT
} ExpectedLengthBasis;

/* True for the 2 values above; false for any other value. */
bool expected_length_basis_valid(ExpectedLengthBasis value);

/* "anticipated-cadence" "unexpanded-model"; NULL for an invalid
   value. */
const char *expected_length_basis_name(ExpectedLengthBasis value);

#endif
