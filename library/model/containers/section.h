#ifndef MUSIC_CONTAINERS_SECTION_H
#define MUSIC_CONTAINERS_SECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "model/identity/entity_id.h"

/*
 * Section (Container): groups phrases by reference (their ids), not by
 * embedding the referenced objects.
 *
 * Fields: id (EntityID), phrase_ids (owned array of EntityID references
 * to phrases). May be empty (count == 0, pass NULL) -- a Section with no
 * phrases assigned yet is still a valid Section, same "found nothing is
 * not an error" treatment CandidateSet gives its own arrays.
 *
 * Section does not itself resolve phrase_ids to actual Phrase instances
 * -- that lookup is a registry/repository concern (L9, out of scope
 * here).
 *
 * Section has no mutator: a revision is a new Section construction, not
 * an in-place field change (Law C2).
 *
 * Serialization: {id, phrase_ids}.
 */

typedef struct Section Section;

typedef enum {
    SECTION_OK = 0,
    SECTION_ERR_NULL_ID,
    SECTION_ERR_NULL_PHRASE_ID,
    SECTION_ERR_ALLOC
} SectionResult;

/*
 * Takes ownership of id and each EntityID in phrase_ids (if any) on
 * success (all destroyed by section_destroy). phrase_ids is optional --
 * pass NULL with count == 0 if not supplied. No entry of a non-empty
 * phrase_ids array may be NULL.
 */
SectionResult section_create(EntityID *id, EntityID **phrase_ids, size_t phrase_id_count, Section **out);
void section_destroy(Section *section);

const EntityID *section_id(const Section *section);
size_t section_phrase_id_count(const Section *section);
const EntityID *section_phrase_id_at(const Section *section, size_t index);

#endif