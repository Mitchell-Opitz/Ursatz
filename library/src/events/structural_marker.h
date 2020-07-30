#ifndef MUSIC_EVENTS_STRUCTURAL_MARKER_H
#define MUSIC_EVENTS_STRUCTURAL_MARKER_H

#include "time/temporal_anchor.h"

/*
 * StructuralMarker: a symbol or direction that divides a work or movement
 * into sections or steers how it is traversed -- fermata, caesura, breath
 * mark, repeat signs, first/second/nth endings, one- and two-measure
 * repeats, segno, coda, fine, D.C./D.S. al coda/fine -- at a position.
 *
 * kind is an opaque, uninterpreted string, not a closed enum, the same
 * treatment events/articulation.h gives its kind: the vocabulary is open
 * (some styles add third and fourth endings, composers invent their own
 * markers). What a kind MEANS, and how a sequence of them is traversed,
 * is framework knowledge (frameworks/standard-markings), not this type's.
 *
 * parameter is a single optional non-negative integer qualifier, used
 * where a marker carries a number: the ending number of a numbered
 * ending (1, 2, 3, ...). 0 means none -- ending numbers start at 1, so
 * there is no clash.
 *
 * Positioned by a TemporalAnchor (events/control_event.h's pattern):
 * a fermata sits at its note's anchor, a repeat sign at a boundary.
 * Mapping an anchor to a measure belongs to Measure/MeterMap territory
 * and is not done here. No mutator: a revision is a new StructuralMarker.
 */

typedef struct StructuralMarker StructuralMarker;

typedef enum {
    STRUCTURAL_MARKER_OK = 0,
    STRUCTURAL_MARKER_ERR_NULL_TEMPORAL_ANCHOR,
    STRUCTURAL_MARKER_ERR_NULL_KIND,
    STRUCTURAL_MARKER_ERR_ALLOC
} StructuralMarkerResult;

/*
 * Takes ownership of temporal_anchor on success (destroyed by
 * structural_marker_destroy). kind is copied, required, non-empty.
 */
StructuralMarkerResult structural_marker_create(TemporalAnchor *temporal_anchor, const char *kind,
                                                unsigned int parameter, StructuralMarker **out);
void structural_marker_destroy(StructuralMarker *marker);

const TemporalAnchor *structural_marker_temporal_anchor(const StructuralMarker *marker);
const char *structural_marker_kind(const StructuralMarker *marker);
unsigned int structural_marker_parameter(const StructuralMarker *marker);

#endif
