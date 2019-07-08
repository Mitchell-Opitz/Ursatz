#ifndef MUSIC_TRANSFORM_INVERSION_H
#define MUSIC_TRANSFORM_INVERSION_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "transform/transformation.h"

/*
 * Inversion (Transformation subtype): reflects pitch about a fixed
 * axis (TDD §142).
 *
 * Owns a Transformation (transform/transformation.h) holding the base
 * §139 contract fields (input_type="NoteEvent", output_type=
 * "NoteEvent", preserved_dimensions="rhythm, event order, articulation",
 * modified_dimensions="pitch", mapping_behavior="one-to-one, per-event
 * pitch reflection about axis", deterministic=true, and the supplied
 * provenance) built internally by inversion_create -- inversion_id
 * forwards to it rather than duplicating identity on Inversion.
 * parameters is left absent (NULL): axis is exposed directly as its
 * own typed field below rather than packed into the opaque parameters
 * Value.
 *
 * TDD §142 requires Inversion to explicitly define: axis, pitch space,
 * spelling policy, and tuning semantics -- and requires axis to be an
 * explicit pitch value, never implicitly "first note of input." axis
 * is therefore a required, owned PitchIdentity (pitch/pitch_identity.h)
 * supplied by the caller at construction, never derived from whatever
 * material this Inversion is later applied to.
 *
 * pitch_space and spelling_policy are required descriptor strings
 * (e.g. "chromatic pitch space" / "diatonic, preserve letter-name
 * distance from axis") -- no dedicated PitchSpace or SpellingPolicy
 * type exists yet, the same placeholder treatment Transposition gives
 * its own dimension fields (transform/transposition.h).
 *
 * Tuning semantics has no representation in this codebase yet
 * (Deliverable D) -- this constructor requires has_tuning_semantics to
 * be passed explicitly false (rejecting true, since there is nothing
 * yet to back a true value with), the same explicitly-absent-stub
 * treatment Transposition gives tuning/range/instrument.
 */

typedef struct Inversion Inversion;

typedef enum {
    INVERSION_OK = 0,
    INVERSION_ERR_NULL_ID,
    INVERSION_ERR_NULL_AXIS,
    INVERSION_ERR_NULL_PITCH_SPACE,
    INVERSION_ERR_NULL_SPELLING_POLICY,
    INVERSION_ERR_TUNING_SEMANTICS_NOT_SUPPORTED,
    INVERSION_ERR_NULL_PROVENANCE,
    INVERSION_ERR_ALLOC
} InversionResult;

/*
 * Takes ownership of id, axis, and provenance on success (all
 * destroyed, axis directly and id/provenance transitively via the
 * owned Transformation, by inversion_destroy). pitch_space and
 * spelling_policy are required (copied, non-empty). has_tuning_semantics
 * MUST be passed false -- passing true fails with
 * INVERSION_ERR_TUNING_SEMANTICS_NOT_SUPPORTED.
 */
InversionResult inversion_create(EntityID *id, PitchIdentity *axis, const char *pitch_space,
                                  const char *spelling_policy, bool has_tuning_semantics,
                                  ProvenanceReference *provenance, Inversion **out);
void inversion_destroy(Inversion *inversion);

const EntityID *inversion_id(const Inversion *inversion);
const Transformation *inversion_transformation(const Inversion *inversion);
const PitchIdentity *inversion_axis(const Inversion *inversion);
const char *inversion_pitch_space(const Inversion *inversion);
const char *inversion_spelling_policy(const Inversion *inversion);
bool inversion_has_tuning_semantics(const Inversion *inversion);

bool inversion_identity_equals(const Inversion *a, const Inversion *b);

#endif