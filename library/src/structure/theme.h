#ifndef MUSIC_STRUCTURE_THEME_H
#define MUSIC_STRUCTURE_THEME_H

#include <stdbool.h>
#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Theme (Structure): a structural unit above phrase.
 *
 * Fields: id (EntityID) and phrase refs -- EntityID pointers at the L4
 * containers/phrase.h Phrases that make up the theme, referenced not
 * duplicated (TDD Sec56, Sec152). Theme does not embed the
 * Structure/Entity wrapper types directly -- it flattens to its own
 * field shape, the same approach Motif takes (structure/motif.h). No
 * hierarchy is assumed (TDD Sec57): referencing Phrase ids here does
 * not imply Theme must be built from Phrases, only that it may
 * reference them.
 *
 * THIS references containers/phrase.h's L4 container-layer Phrase --
 * not structure's own occurrence-based Phrase concept (referred to
 * elsewhere as PhraseOccurrence). See containers/phrase.h's header
 * comment for the same disambiguation from that side.
 *
 * Registry marks Theme "No (example only)": this is minimal,
 * non-normative scaffolding consistent with Motif's existing pattern,
 * not a finished authoritative design.
 *
 * Theme has no mutator: a revision is a new Theme construction, not an
 * in-place field change (Law C2).
 */

typedef struct Theme Theme;

typedef enum {
    THEME_OK = 0,
    THEME_ERR_NULL_ID,
    THEME_ERR_NULL_PHRASE_IDS,
    THEME_ERR_NULL_PHRASE_ID,
    THEME_ERR_ALLOC
} ThemeResult;

/*
 * Takes ownership of id and of phrase_ids (and each EntityID it points
 * to) on success -- all destroyed by theme_destroy. phrase_ids must be
 * non-NULL with phrase_id_count > 0; no entry may be NULL.
 */
ThemeResult theme_create(EntityID *id, EntityID **phrase_ids, size_t phrase_id_count, Theme **out);
void theme_destroy(Theme *theme);

const EntityID *theme_id(const Theme *theme);
size_t theme_phrase_id_count(const Theme *theme);
const EntityID *theme_phrase_id_at(const Theme *theme, size_t index);

/* Identity equality: same EntityID, regardless of instance. */
bool theme_identity_equals(const Theme *a, const Theme *b);

#endif