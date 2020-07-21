#include "events/structural_marker.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static TemporalAnchor *anchor_at(long numerator, unsigned long denominator) {
    MusicalTime *time = NULL;
    assert(musical_time_create(numerator, denominator, &time) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(time, &anchor) == TEMPORAL_ANCHOR_OK);
    return anchor;
}

static void test_create_rejects_null_anchor(void) {
    StructuralMarker *marker = NULL;
    assert(structural_marker_create(NULL, "fermata", 0, &marker) == STRUCTURAL_MARKER_ERR_NULL_TEMPORAL_ANCHOR);
    assert(marker == NULL);
}

static void test_create_rejects_empty_kind(void) {
    TemporalAnchor *anchor = anchor_at(0, 1);
    StructuralMarker *marker = NULL;
    assert(structural_marker_create(anchor, NULL, 0, &marker) == STRUCTURAL_MARKER_ERR_NULL_KIND);
    assert(structural_marker_create(anchor, "", 0, &marker) == STRUCTURAL_MARKER_ERR_NULL_KIND);
    assert(marker == NULL);
    temporal_anchor_destroy(anchor);
}

static void test_fields(void) {
    StructuralMarker *marker = NULL;
    assert(structural_marker_create(anchor_at(7, 2), "fermata", 0, &marker) == STRUCTURAL_MARKER_OK);
    assert(strcmp(structural_marker_kind(marker), "fermata") == 0);
    assert(structural_marker_parameter(marker) == 0);
    const MusicalTime *position = temporal_anchor_position(structural_marker_temporal_anchor(marker));
    assert(musical_time_numerator(position) == 7);
    assert(musical_time_denominator(position) == 2);
    structural_marker_destroy(marker);
}

/* A numbered ending carries its number in the parameter. */
static void test_numbered_ending(void) {
    StructuralMarker *second = NULL;
    assert(structural_marker_create(anchor_at(8, 1), "ending", 2, &second) == STRUCTURAL_MARKER_OK);
    assert(structural_marker_parameter(second) == 2);
    structural_marker_destroy(second);
}

/* The kind is copied: mutating the caller's buffer does not change the marker. */
static void test_kind_is_copied(void) {
    char kind[] = "caesura";
    StructuralMarker *marker = NULL;
    assert(structural_marker_create(anchor_at(0, 1), kind, 0, &marker) == STRUCTURAL_MARKER_OK);
    kind[0] = 'X';
    assert(strcmp(structural_marker_kind(marker), "caesura") == 0);
    structural_marker_destroy(marker);
}

int main(void) {
    test_create_rejects_null_anchor();
    test_create_rejects_empty_kind();
    test_fields();
    test_numbered_ending();
    test_kind_is_copied();
    return 0;
}
