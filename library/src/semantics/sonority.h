#ifndef MUSIC_SEMANTICS_SONORITY_H
#define MUSIC_SEMANTICS_SONORITY_H

#include <stddef.h>

#include "identity/entity_id.h"
#include "time/time_span.h"

/*
 * Sonority (Value): a selected collection of simultaneous pitch-bearing
 * events.
 *
 * A Sonority is a SELECTION, and the selection semantics are explicit
 * fields, not implicit assumptions baked into how callers happen to
 * build one:
 *
 *   - time_window: the TimeSpan (time/time_span.h) the selection was
 *     drawn from. Owned -- sonority_destroy destroys it.
 *   - voice_ids: the voice set the selection was restricted to,
 *     referenced by EntityID (identity/entity_id.h), not embedded
 *     Voice objects. An empty voice set (voice_id_count == 0,
 *     voice_ids == NULL) is valid and means "no voice restriction",
 *     not "unknown" -- it is still an explicit choice, just an
 *     unrestricted one.
 *   - event_domain: whether the included events are symbolic (notated)
 *     or sounding (realized/performed) -- SONORITY_EVENT_DOMAIN_* below.
 *     There is no default; every Sonority states which domain it was
 *     drawn from.
 *
 * event_ids reference the pitch-bearing events actually selected
 * (EntityID pointers, referenced not embedded/duplicated -- the same
 * reference-only pattern music-structure's Structure/Cadence use for
 * their event refs). event_id_count MUST be > 0: an empty Sonority
 * (zero simultaneous events) is not a sonority.
 *
 * A Sonority is NOT automatically a Chord: this type carries no
 * chord-identification logic (root, quality, inversion, etc.) -- that
 * is Chord (Phase 8, depends on Framework/L7, not built here).
 */

typedef struct Sonority Sonority;

typedef enum { SONORITY_EVENT_DOMAIN_SYMBOLIC, SONORITY_EVENT_DOMAIN_SOUNDING } SonorityEventDomain;

typedef enum {
    SONORITY_OK = 0,
    SONORITY_ERR_NULL_TIME_WINDOW,
    SONORITY_ERR_NULL_EVENT_IDS,
    SONORITY_ERR_NULL_EVENT_ID,
    SONORITY_ERR_NULL_VOICE_ID,
    SONORITY_ERR_ALLOC
} SonorityResult;

/*
 * Takes ownership of time_window, event_ids (and each EntityID it
 * points to), and voice_ids (and each EntityID it points to) on
 * success -- all destroyed by sonority_destroy.
 *
 * event_ids must be non-NULL with event_id_count > 0; no entry may be
 * NULL. voice_ids may be NULL with voice_id_count == 0 (no voice
 * restriction); if voice_id_count > 0, voice_ids must be non-NULL and
 * no entry may be NULL.
 */
SonorityResult sonority_create(TimeSpan *time_window, EntityID **event_ids, size_t event_id_count,
                                EntityID **voice_ids, size_t voice_id_count, SonorityEventDomain event_domain,
                                Sonority **out);
void sonority_destroy(Sonority *sonority);

const TimeSpan *sonority_time_window(const Sonority *sonority);

size_t sonority_event_id_count(const Sonority *sonority);
const EntityID *sonority_event_id_at(const Sonority *sonority, size_t index);

size_t sonority_voice_id_count(const Sonority *sonority);
const EntityID *sonority_voice_id_at(const Sonority *sonority, size_t index);

SonorityEventDomain sonority_event_domain(const Sonority *sonority);

#endif