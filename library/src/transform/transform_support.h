#ifndef URSATZ_TRANSFORM_TRANSFORM_SUPPORT_H
#define URSATZ_TRANSFORM_TRANSFORM_SUPPORT_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "transform/augmentation.h"

/*
 * Internal to src/transform/: not part of the public API and not documented as such. Every function returns NULL (or false)
 * on allocation failure.
 */

/* The display name of an augmentation dimension; NULL for an unknown one. */
const char *transform_support_dimension_name(AugmentationDimension dimension);

PitchIdentity *transform_support_clone_pitch_identity(const PitchIdentity *source);

/* *out is a copy of source, or NULL when source is NULL (still true). */
bool transform_support_clone_pitch_spelling_or_null(const PitchSpelling *source, PitchSpelling **out);

/* *out is a copy of source, or NULL when source is NULL (still true). */
bool transform_support_clone_voice_reference_or_null(const VoiceReference *source, VoiceReference **out);

/* An EntityID with the value "<source id><suffix>". */
EntityID *transform_support_make_derived_id(const EntityID *source_id, const char *suffix);

/* Present provenance pointing at source_id. */
ProvenanceReference *transform_support_make_provenance_from(const EntityID *source_id);

/* A new NoteEvent identical to source (fresh copies of every field) except a new id and provenance. */
NoteEvent *transform_support_clone_note_event(const NoteEvent *source, const char *id_suffix);

/* A new NoteEvent identical to source except for pitch (ownership of new_pitch transfers in; it is destroyed on failure). */
NoteEvent *transform_support_clone_note_event_with_new_pitch(const NoteEvent *source, PitchIdentity *new_pitch,
                                                             const char *id_suffix);

/* A new NoteEvent identical to source except for duration (a fresh Duration at the given numerator/denominator). */
NoteEvent *transform_support_clone_note_event_with_new_duration(const NoteEvent *source, long new_duration_numerator,
                                                                unsigned long new_duration_denominator,
                                                                const char *id_suffix);

/* A new NoteEvent identical to source except for its onset (a fresh MusicalTime at the given numerator/denominator). */
NoteEvent *transform_support_clone_note_event_with_new_onset(const NoteEvent *source, long new_onset_numerator,
                                                             unsigned long new_onset_denominator, const char *id_suffix);

#endif
