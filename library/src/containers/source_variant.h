#ifndef MUSIC_CONTAINERS_SOURCE_VARIANT_H
#define MUSIC_CONTAINERS_SOURCE_VARIANT_H

#include <stdbool.h>

#include "identity/entity_id.h"

/*
 * SourceVariant (Value): a variant of a source (e.g. an autograph vs. a
 * first-print variant reading). The Registry marks SourceVariant
 * "shall eventually support" -- deferred normative, not yet given a
 * concrete schema. This is a minimal structural shape, built to that
 * status rather than guessing ahead of it: anything the Registry has
 * not yet resolved is flagged here, not silently decided.
 *
 * Fields: source_id (EntityID reference to the source this is a
 * variant of -- referenced, never embedded, same reference-only
 * pattern used throughout this codebase) and descriptor (opaque,
 * required, non-empty string describing the variant). descriptor is an
 * opaque string rather than a structured/enumerated field for the same
 * reason Layer's grouping field is (containers/layer.h) -- matching the
 * existing convention used for Articulation.kind, Dynamics.marking, and
 * Instrument's illustrative properties: an opaque string when no
 * dedicated schema exists yet, rather than a bare-EntityID placeholder
 * (contrast Measure's meter_map_region_ref, containers/measure.h, which
 * uses that other convention because it stands in for a same-layer
 * dependency -- MeterMap -- that will itself become a resolvable
 * entity; SourceVariant's descriptor has no such future target, it is
 * content, so the opaque-string convention fits instead).
 *
 * Value category, not Entity: identity derives from content
 * (source_id + descriptor), not from an assigned identifier -- two
 * SourceVariant instances built independently from equal content are
 * equal (core/value.h's model, hand-rolled here the same way concrete
 * Value-category types elsewhere in this codebase are -- e.g.
 * time/duration.h's Duration -- rather than wrapping the generic
 * byte-content Value type, since SourceVariant's fields do not fit
 * Value's single-blob shape).
 *
 * SourceVariant has no mutator: a revision is a new SourceVariant
 * construction, not an in-place field change (Law C2).
 *
 * Serialization: {source_id, descriptor}.
 */

typedef struct SourceVariant SourceVariant;

typedef enum {
    SOURCE_VARIANT_OK = 0,
    SOURCE_VARIANT_ERR_NULL_SOURCE_ID,
    SOURCE_VARIANT_ERR_NULL_DESCRIPTOR,
    SOURCE_VARIANT_ERR_EMPTY_DESCRIPTOR,
    SOURCE_VARIANT_ERR_ALLOC
} SourceVariantResult;

/*
 * Takes ownership of source_id on success (destroyed by
 * source_variant_destroy). descriptor is copied; it must be non-NULL
 * and non-empty.
 */
SourceVariantResult source_variant_create(EntityID *source_id, const char *descriptor, SourceVariant **out);
void source_variant_destroy(SourceVariant *variant);

const EntityID *source_variant_source_id(const SourceVariant *variant);
const char *source_variant_descriptor(const SourceVariant *variant);

/* Value equality: same source_id and same descriptor content, regardless of instance. */
bool source_variant_equals(const SourceVariant *a, const SourceVariant *b);

#endif