#ifndef MUSIC_CONTAINERS_VOICE_H
#define MUSIC_CONTAINERS_VOICE_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * Voice (Container): identifies a single musical voice/line.
 *
 * Fields: id (EntityID) only. Domain Spec §10 explicitly excludes an
 * owned/mutable event list from Voice's required schema this pass --
 * do not add a "notes" or "events" field/list here. Membership
 * (which events/notes belong to a voice) is deferred to a later
 * Container/Structure spec, not yet available.
 *
 * Invariants:
 * - A Voice MUST NOT be assumed to map 1:1 to an Instrument, Staff, or
 *   Part.
 * - A Voice MAY appear on multiple staves; a Staff MAY contain
 *   multiple voices (semantic note only -- Staff does not exist yet
 *   this pass, so nothing enforces this in code).
 *
 * Voice has no mutator: a revision is a new Voice construction, not an
 * in-place field change (Law C2).
 *
 * Serialization: {id} only.
 */

typedef struct Voice Voice;

typedef enum {
    VOICE_OK = 0,
    VOICE_ERR_NULL_ID,
    VOICE_ERR_ALLOC
} VoiceResult;

/* Takes ownership of id on success (destroyed by voice_destroy). */
VoiceResult voice_create(EntityID *id, Voice **out);
void voice_destroy(Voice *voice);

const EntityID *voice_id(const Voice *voice);

/*
 * Identity equality: same EntityID, regardless of instance. Voice's
 * schema has no separate version field (Fields: id only), so this is
 * the same identity-equality pattern as Entity (core/entity.h),
 * reduced to the one field Voice actually has.
 */
bool voice_identity_equals(const Voice *a, const Voice *b);

#endif