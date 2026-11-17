#include "model/pitch/region.h"

#include <string.h>

#define SEMITONES_PER_OCTAVE 12

static bool same_material(const PitchMaterial *a, const PitchMaterial *b) {
    if (a == b) {
        return true;
    }
    return a != NULL && b != NULL && a->id != NULL && b->id != NULL && strcmp(a->id, b->id) == 0;
}

Region region_tonal(int center, const PitchMaterial *material) {
    Region region;
    region.kind = REGION_TONAL_MODAL;
    region.center = ((center % SEMITONES_PER_OCTAVE) + SEMITONES_PER_OCTAVE) % SEMITONES_PER_OCTAVE;
    region.material = material;
    region.strength = REGION_STRENGTH_ESTABLISHED;
    return region;
}

bool region_equals(const Region *a, const Region *b) {
    if (a == NULL || b == NULL) {
        return a == b;
    }
    return a->kind == b->kind && a->center == b->center && a->strength == b->strength && same_material(a->material, b->material);
}
