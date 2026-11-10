#include "harmony_cadence_kinds.h"

#include <string.h>

static const HarmonyCadenceKind KINDS[] = {
    {"perfect-authentic", "perfect authentic", "PAC", CADENCE_TYPE_PERFECT_AUTHENTIC, 6, true},
    {"imperfect-authentic", "imperfect authentic", "IAC", CADENCE_TYPE_IMPERFECT_AUTHENTIC, 5, true},
    {"authentic-unspecified", "authentic", "AC", CADENCE_TYPE_AUTHENTIC_UNSPECIFIED, 4, true},
    {"plagal", "plagal", "PC", CADENCE_TYPE_PLAGAL, 3, true},
    {"deceptive", "deceptive", "DC", CADENCE_TYPE_DECEPTIVE, 2, false},
    {"half", "half", "HC", CADENCE_TYPE_HALF, 1, false},
    {"subverted", "subverted", "EV", CADENCE_TYPE_SUBVERTED, 0, false},
    {"evaded", "evaded", "EV", CADENCE_TYPE_SUBVERTED, 0, false},
    {"abandoned", "abandoned", "AB", CADENCE_TYPE_UNKNOWN, -1, false},
    {"medial-caesura", "medial caesura", "MC", CADENCE_TYPE_HALF, 1, false},
};

#define KIND_COUNT (sizeof KINDS / sizeof KINDS[0])

size_t harmony_cadence_kind_count(void) {
    return KIND_COUNT;
}

const HarmonyCadenceKind *harmony_cadence_kind_at(size_t index) {
    return index < KIND_COUNT ? &KINDS[index] : NULL;
}

const HarmonyCadenceKind *harmony_cadence_kind_find(const char *name) {
    if (name == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < KIND_COUNT; i++) {
        if (strcmp(KINDS[i].name, name) == 0) {
            return &KINDS[i];
        }
    }
    return NULL;
}

const HarmonyCadenceKind *harmony_cadence_kind_of(CadenceType core) {
    return harmony_cadence_kind_find(cadence_type_name(core));
}
