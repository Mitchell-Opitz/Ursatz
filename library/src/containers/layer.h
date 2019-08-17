#ifndef MUSIC_CONTAINERS_LAYER_H
#define MUSIC_CONTAINERS_LAYER_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * Layer (Container): a notation-layer grouping (e.g. an ossia, a cue
 * staff, a chord-symbol layer). The Registry marks Layer non-normative
 * ("mentioned, no dedicated schema") -- this is deliberately a minimal
 * structural type, not a fleshed-out one.
 *
 * Fields: id (EntityID) and grouping (opaque, required, non-empty
 * string label for what this layer represents). grouping is an opaque
 * string rather than a structured/enumerated field for the same reason
 * events.md's Articulation.kind and Dynamics.marking, and
 * generation.md's framework_version, are opaque strings: no dedicated
 * schema exists yet to structure it against, and inventing one here
 * would be guessing ahead of the Registry. Do not add a member-list
 * field: Layer groups by being referenced (e.g. by Staff), it does not
 * itself hold references to what it groups -- same non-ownership
 * direction Voice keeps toward its own membership.
 *
 * Layer has no mutator: a revision is a new Layer construction, not an
 * in-place field change (Law C2).
 *
 * Serialization: {id, grouping}.
 */

typedef struct Layer Layer;

typedef enum {
    LAYER_OK = 0,
    LAYER_ERR_NULL_ID,
    LAYER_ERR_NULL_GROUPING,
    LAYER_ERR_EMPTY_GROUPING,
    LAYER_ERR_ALLOC
} LayerResult;

/*
 * Takes ownership of id on success (destroyed by layer_destroy).
 * grouping is copied; it must be non-NULL and non-empty.
 */
LayerResult layer_create(EntityID *id, const char *grouping, Layer **out);
void layer_destroy(Layer *layer);

const EntityID *layer_id(const Layer *layer);
const char *layer_grouping(const Layer *layer);

/*
 * Identity equality: same EntityID, regardless of instance. Layer's
 * schema has no separate version field, so this is the same
 * identity-equality pattern as Entity (core/entity.h), reduced to the
 * fields Layer actually has (grouping does not factor in, same as
 * Voice's identity equality excluding nothing further since it has no
 * other field -- here grouping is descriptive content, not identity).
 */
bool layer_identity_equals(const Layer *a, const Layer *b);

#endif