#ifndef URSATZ_CONTAINERS_ID_ARRAY_H
#define URSATZ_CONTAINERS_ID_ARRAY_H

#include <stddef.h>

#include "identity/entity_id.h"

/*
 * Internal to src/containers/ and src/analysis/: not part of the public API. The id-array helpers every container shares, and
 * the one construction path of the same-shape shells (Idea, Movement, Part, Phrase, Section, Subphrase, Work): an owner (an
 * EntityID or Entity) plus an owned array of EntityIDs.
 */

/* A new array holding the same EntityID pointers as `ids` (ownership of the ids moves to the copy's owner); NULL when `count`
   is 0 or on allocation failure. */
EntityID **id_array_copy(EntityID **ids, size_t count);

/* Destroys every id and the array itself. NULL is a no-op. */
void id_array_destroy(EntityID **ids, size_t count);

typedef enum {
    CONTAINER_SHELL_OK = 0,
    CONTAINER_SHELL_ERR_NULL_OWNER,
    CONTAINER_SHELL_ERR_NULL_ID,
    CONTAINER_SHELL_ERR_ALLOC
} ContainerShellStatus;

/* Validates the shell arguments and, on CONTAINER_SHELL_OK, sets *copy to id_array_copy(ids, count) (NULL for an empty array).
   The owner is not touched; the caller stores it and *copy in its own struct and frees *copy with id_array_destroy. */
ContainerShellStatus container_shell_create(const void *owner, EntityID **ids, size_t count, EntityID ***copy);

#endif
