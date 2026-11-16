#include "semantics/non_chord_tone.h"

#include <stddef.h>

static const char *const NAMES[NON_CHORD_TONE_COUNT] = {"passing", "neighbor", "double-neighbor", "changing-tone",
                                                          "suspension", "anticipation", "appoggiatura",
                                                          "escape-tone", "retardation", "pedal"};

bool non_chord_tone_valid(NonChordTone type) {
    return type >= 0 && type < NON_CHORD_TONE_COUNT;
}

const char *non_chord_tone_name(NonChordTone type) {
    return non_chord_tone_valid(type) ? NAMES[type] : NULL;
}
