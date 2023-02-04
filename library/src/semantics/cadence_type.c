#include "semantics/cadence_type.h"

#include <stddef.h>

static const char *const NAMES[CADENCE_TYPE_COUNT] = {
    "perfect-authentic", "imperfect-authentic", "half", "subverted", "plagal", "deceptive",
    "authentic-unspecified", "unknown"};

static const char *const LABELS[CADENCE_TYPE_COUNT] = {
    "authentic (PAC)", "authentic (IAC)", "half", "subverted", "plagal", "deceptive",
    "authentic (unspecified)", "unknown"};

bool cadence_type_valid(CadenceType type) {
    return type >= 0 && type < CADENCE_TYPE_COUNT;
}

const char *cadence_type_name(CadenceType type) {
    return cadence_type_valid(type) ? NAMES[type] : NULL;
}

const char *cadence_type_label(CadenceType type) {
    return cadence_type_valid(type) ? LABELS[type] : NULL;
}

bool cadence_type_is_authentic(CadenceType type) {
    return type == CADENCE_TYPE_PERFECT_AUTHENTIC || type == CADENCE_TYPE_IMPERFECT_AUTHENTIC;
}
