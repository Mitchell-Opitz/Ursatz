#ifndef URSATZ_MODEL_PITCH_REGION_H
#define URSATZ_MODEL_PITCH_REGION_H

#include <stdbool.h>

#include "model/pitch/pitch_material.h"

/*
 * A region (target architecture 4.5): where a stretch of music sits in pitch, as a kind, a center, a material and a strength. A
 * tonal or modal region has a center (0 to 11) and a material; a pitch field is a collection with no center (center -1); a serial
 * region holds nothing until serial material exists (P80). The strength says how firmly the music establishes it (OMT VII ch. 13):
 * a region a plan names is ESTABLISHED, one an analysis finds may be weaker. The material is borrowed from the knowledge snapshot
 * (or a table that outlives the region).
 */
typedef enum {
    REGION_TONAL_MODAL,
    REGION_PITCH_FIELD,
    REGION_SERIAL,
    REGION_NONE
} RegionKind;

typedef enum {
    REGION_STRENGTH_ESTABLISHED,
    REGION_STRENGTH_TONICIZED,
    REGION_STRENGTH_IMPLIED,
    REGION_STRENGTH_FRAGILE,
    REGION_STRENGTH_ABSENT
} RegionStrength;

typedef struct {
    RegionKind kind;
    int center; /* 0 to 11 for a tonal or modal region, -1 for a pitch field */
    const PitchMaterial *material;
    RegionStrength strength;
} Region;

/* An established tonal or modal region on `center` (any integer, reduced to 0 to 11) with `material`. */
Region region_tonal(int center, const PitchMaterial *material);

/* True when both have the same kind, center, strength and material (the same entry id, or both none). */
bool region_equals(const Region *a, const Region *b);

#endif
