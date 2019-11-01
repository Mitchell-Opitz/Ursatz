#ifndef MUSIC_TRANSFORM_INVERSION_H
#define MUSIC_TRANSFORM_INVERSION_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "transform/transform_result.h"
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

typedef enum {
    INVERSION_APPLY_OK = 0,
    INVERSION_APPLY_ERR_NULL_INVERSION,
    INVERSION_APPLY_ERR_NULL_EVENT,
    INVERSION_APPLY_ERR_NULL_PITCH_VALUE,
    INVERSION_APPLY_ERR_NULL_AXIS_VALUE,
    INVERSION_APPLY_ERR_TUNING_SYSTEM_MISMATCH,
    INVERSION_APPLY_ERR_ALLOC
} InversionApplyResult;

/*
 * Applies inversion to event (a borrowed NoteEvent): reflects pitch
 * about axis_value using PitchNumericValue arithmetic (pitch/
 * pitch_numeric_value.h) -- reflect_about_axis(event_pitch_value,
 * axis_value), then pitch_numeric_value_to_identity packages the result
 * into the output NoteEvent's pitch. Every other field (temporal
 * position, duration, spelling, voice) is copied unchanged.
 *
 * event_pitch_value and axis_value are CALLER-SUPPLIED numeric
 * resolutions: event_pitch_value of event's PitchIdentity, axis_value
 * of this Inversion's own axis (inversion_axis()), both under whatever
 * TuningSystem the caller is working in and both required to share that
 * same TuningSystem -- a mismatch fails with
 * INVERSION_APPLY_ERR_TUNING_SYSTEM_MISMATCH
 * (pitch_numeric_value_reflect_about_axis enforces this). This layer
 * cannot derive either one mechanically: PitchIdentity is opaque by
 * design, and pitch_numeric_value.h deliberately provides no generic
 * "PitchIdentity -> PitchNumericValue" decoder (see that file's
 * header). inversion_apply does not check that event_pitch_value/
 * axis_value actually correspond to note_event_pitch(event)/
 * inversion_axis(inversion); that correspondence is the caller's
 * responsibility to establish.
 *
 * has_tuning_semantics is always false on any successfully-constructed
 * Inversion (see inversion_create) -- there is nothing yet to check
 * against for tuning semantics, so this apply performs pure reflection
 * arithmetic only, exactly as the base contract's validation string
 * already states.
 *
 * event is never mutated; *out's event carries a real
 * ProvenanceReference pointing back to event.
 */
InversionApplyResult inversion_apply(const Inversion *inversion, const NoteEvent *event,
                                      const PitchNumericValue *event_pitch_value,
                                      const PitchNumericValue *axis_value, TransformResult **out);

#endif