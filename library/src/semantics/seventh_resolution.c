#include "semantics/seventh_resolution.h"

#include <stddef.h>

static const char *const NAMES[SEVENTH_RESOLUTION_COUNT] = {"default", "incomplete", "leading-tone-drop"};

bool seventh_resolution_valid(SeventhResolution resolution) {
    return resolution >= 0 && resolution < SEVENTH_RESOLUTION_COUNT;
}

const char *seventh_resolution_name(SeventhResolution resolution) {
    return seventh_resolution_valid(resolution) ? NAMES[resolution] : NULL;
}

bool seventh_resolution_gives_complete_tonic(SeventhResolution resolution) {
    return resolution == SEVENTH_RESOLUTION_INCOMPLETE || resolution == SEVENTH_RESOLUTION_LEADING_TONE_DROP;
}
