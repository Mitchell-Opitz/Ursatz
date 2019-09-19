#ifndef MUSIC_STRUCTURE_PHRASE_OCCURRENCE_H
#define MUSIC_STRUCTURE_PHRASE_OCCURRENCE_H

#include <stddef.h>

#include "core/occurrence.h"
#include "identity/entity_id.h"

/*
 * PhraseOccurrence: a situated instance of a (L4 containers/phrase.h)
 * Phrase.
 *
 * Composes a core/occurrence.h Occurrence (its own id, and the id of
 * the Phrase it is an occurrence of, as Occurrence's target_id) with
 * region_event_ids -- EntityID pointers at the concrete events present
 * at this occurrence, referenced not duplicated (TDD Sec56, Sec152).
 * Same pattern structure/motif_occurrence.h uses for Motif.
 *
 * THIS IS NOT containers/phrase.h's Phrase container concept -- this
 * is the L5 Structure-layer occurrence of it: a situated instance
 * pointing at the Phrase by id (Occurrence's target_id), the same
 * disambiguation containers/phrase.h's own header comment documents
 * from its side. This file does not include containers/phrase.h --
 * the caller supplies the Phrase's id into occurrence_create, the same
 * arms-length approach motif_occurrence.h takes toward motif.h.
 *
 * Key pattern (TDD Sec58): the Phrase is a container; occurrences
 * point at concrete regions of musical material without the Phrase
 * itself being duplicated per occurrence. A single Phrase may have
 * many PhraseOccurrences, each a distinct PhraseOccurrence instance
 * (distinct own id) sharing the same target Phrase id.
 *
 * PhraseOccurrence has no mutator: a revision is a new construction,
 * not an in-place field change (Law C2).
 */

typedef struct PhraseOccurrence PhraseOccurrence;

typedef enum {
    PHRASE_OCCURRENCE_OK = 0,
    PHRASE_OCCURRENCE_ERR_NULL_OCCURRENCE,
    PHRASE_OCCURRENCE_ERR_NULL_REGION_EVENT_IDS,
    PHRASE_OCCURRENCE_ERR_NULL_REGION_EVENT_ID,
    PHRASE_OCCURRENCE_ERR_ALLOC
} PhraseOccurrenceResult;

/*
 * Takes ownership of occurrence and of region_event_ids (and each
 * EntityID it points to) on success -- all destroyed by
 * phrase_occurrence_destroy. region_event_ids must be non-NULL with
 * region_event_id_count > 0; no entry may be NULL. occurrence's
 * target_id is the Phrase this is an occurrence of.
 */
PhraseOccurrenceResult phrase_occurrence_create(Occurrence *occurrence, EntityID **region_event_ids,
                                                 size_t region_event_id_count, PhraseOccurrence **out);
void phrase_occurrence_destroy(PhraseOccurrence *phrase_occurrence);

const EntityID *phrase_occurrence_id(const PhraseOccurrence *phrase_occurrence);
/* The id of the Phrase this is an occurrence of. */
const EntityID *phrase_occurrence_phrase_id(const PhraseOccurrence *phrase_occurrence);
size_t phrase_occurrence_region_event_id_count(const PhraseOccurrence *phrase_occurrence);
const EntityID *phrase_occurrence_region_event_id_at(const PhraseOccurrence *phrase_occurrence, size_t index);

#endif