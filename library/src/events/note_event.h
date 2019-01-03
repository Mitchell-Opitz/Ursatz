#ifndef MUSIC_EVENTS_NOTE_EVENT_H
#define MUSIC_EVENTS_NOTE_EVENT_H

#include <stdbool.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/temporal_anchor.h"

/*
 * NoteEvent: a sounding pitch at a temporal position with a duration.
 *
 * STATUS FLAG (carry forward, do not silently resolve): NoteEvent's
 * category (Entity vs Event vs pure Occurrence) is UNRESOLVED upstream
 * (Domain Spec §9, §16.1). This type is implemented under the current
 * provisional "Event" treatment. Do not write code or tests here that
 * assume the boundary is settled.
 *
 * id and provenance are the closest thing to an Entity-shaped identity
 * NoteEvent carries, but NoteEvent is deliberately NOT built on top of
 * core/entity.h's Entity type -- doing so would itself be an assumption
 * about the unresolved category boundary.
 *
 * optional_pitch_spelling and optional_voice are nullable; every other
 * field is required. optional_voice, when present, is typed as
 * VoiceReference* -- the same structural-gap enforcement PitchIdentity
 * uses for its token (pitch/pitch_identity.h): there is no C type
 * representing a resolved Voice (L4) in this codebase for a caller to
 * pass instead, so the L3/L4 non-dependency cannot be violated through
 * this constructor. A resolved Voice object or a music-containers
 * include would fail to compile, and the architecture-gate test
 * (tests/events/test_architecture_events.c) independently confirms no
 * containers/ include exists anywhere under src/events/.
 *
 * There is no mutator. A revision is a new NoteEvent with a new id
 * and/or provenance -- constructing that from an old one is out of scope
 * for this layer (no Activity/Transformation exists yet to drive it).
 *
 * Serialization (left to callers, no serializer exists yet in this
 * codebase): id MUST serialize as an EntityID, never a positional index.
 * optional_voice MUST serialize as an opaque VoiceReference identifier
 * only, never inlined Voice fields -- note_event_optional_voice already
 * returns nothing but the opaque reference, so there are no Voice fields
 * available to inline by construction. provenance MUST serialize with an
 * explicit present/absent marker, never omitted.
 */

typedef struct NoteEvent NoteEvent;

typedef enum {
    NOTE_EVENT_OK = 0,
    NOTE_EVENT_ERR_NULL_ID,
    NOTE_EVENT_ERR_NULL_TEMPORAL_ANCHOR,
    NOTE_EVENT_ERR_NULL_DURATION,
    NOTE_EVENT_ERR_NULL_PITCH,
    NOTE_EVENT_ERR_NULL_PROVENANCE,
    NOTE_EVENT_ERR_ALLOC
} NoteEventResult;

/*
 * Takes ownership of id, temporal_anchor, duration, pitch, provenance,
 * and (when non-NULL) optional_pitch_spelling and optional_voice, on
 * success (destroyed by note_event_destroy). optional_pitch_spelling and
 * optional_voice may be NULL; every other parameter is required.
 *
 * duration's own invariant (>= 0) is enforced by Duration itself
 * (time/duration.h rejects a negative numerator at duration_create) --
 * a caller cannot construct a negative Duration to pass here, so this
 * constructor does not re-check it.
 */
NoteEventResult note_event_create(EntityID *id, TemporalAnchor *temporal_anchor, Duration *duration,
                                   PitchIdentity *pitch, PitchSpelling *optional_pitch_spelling,
                                   VoiceReference *optional_voice, ProvenanceReference *provenance,
                                   NoteEvent **out);
void note_event_destroy(NoteEvent *note_event);

const EntityID *note_event_id(const NoteEvent *note_event);
const TemporalAnchor *note_event_temporal_anchor(const NoteEvent *note_event);
const Duration *note_event_duration(const NoteEvent *note_event);
const PitchIdentity *note_event_pitch(const NoteEvent *note_event);
/* NULL if not present. */
const PitchSpelling *note_event_optional_pitch_spelling(const NoteEvent *note_event);
/* NULL if not present. */
const VoiceReference *note_event_optional_voice(const NoteEvent *note_event);
const ProvenanceReference *note_event_provenance(const NoteEvent *note_event);

/* IdentityEquality (primary): same id, regardless of every other field. */
bool note_event_equals_by_id(const NoteEvent *a, const NoteEvent *b);

/*
 * StructuralEquality: same temporal_anchor, duration, pitch,
 * optional_pitch_spelling, optional_voice, and provenance, independent
 * of id (mirrors core/equality.h's StructuralEquality, which is also
 * independent of EntityID). Value-equal does NOT imply the same event --
 * two independently constructed NoteEvents can carry identical content
 * under different ids.
 */
bool note_event_equals_by_value(const NoteEvent *a, const NoteEvent *b);

#endif