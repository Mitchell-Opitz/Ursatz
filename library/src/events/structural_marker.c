#include "events/structural_marker.h"

#include <stdlib.h>
#include <string.h>

struct StructuralMarker {
    TemporalAnchor *temporal_anchor;
    char *kind;
    unsigned int parameter;
};

StructuralMarkerResult structural_marker_create(TemporalAnchor *temporal_anchor, const char *kind,
                                                unsigned int parameter, StructuralMarker **out) {
    if (temporal_anchor == NULL) {
        return STRUCTURAL_MARKER_ERR_NULL_TEMPORAL_ANCHOR;
    }
    if (kind == NULL || kind[0] == '\0') {
        return STRUCTURAL_MARKER_ERR_NULL_KIND;
    }

    StructuralMarker *marker = (StructuralMarker *)malloc(sizeof(StructuralMarker));
    if (marker == NULL) {
        return STRUCTURAL_MARKER_ERR_ALLOC;
    }

    size_t len = strlen(kind);
    marker->kind = (char *)malloc(len + 1);
    if (marker->kind == NULL) {
        free(marker);
        return STRUCTURAL_MARKER_ERR_ALLOC;
    }
    memcpy(marker->kind, kind, len + 1);

    marker->temporal_anchor = temporal_anchor;
    marker->parameter = parameter;

    *out = marker;
    return STRUCTURAL_MARKER_OK;
}

void structural_marker_destroy(StructuralMarker *marker) {
    if (marker == NULL) {
        return;
    }
    temporal_anchor_destroy(marker->temporal_anchor);
    free(marker->kind);
    free(marker);
}

const TemporalAnchor *structural_marker_temporal_anchor(const StructuralMarker *marker) {
    return marker->temporal_anchor;
}

const char *structural_marker_kind(const StructuralMarker *marker) {
    return marker->kind;
}

unsigned int structural_marker_parameter(const StructuralMarker *marker) {
    return marker->parameter;
}
