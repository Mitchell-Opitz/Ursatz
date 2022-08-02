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

bool non_chord_tone_involves_leap(NonChordTone type) {
    return type == NON_CHORD_TONE_DOUBLE_NEIGHBOR || type == NON_CHORD_TONE_CHANGING_TONE ||
           type == NON_CHORD_TONE_APPOGGIATURA || type == NON_CHORD_TONE_ESCAPE_TONE;
}

bool non_chord_tone_involves_static_note(NonChordTone type) {
    return type == NON_CHORD_TONE_SUSPENSION || type == NON_CHORD_TONE_ANTICIPATION ||
           type == NON_CHORD_TONE_RETARDATION || type == NON_CHORD_TONE_PEDAL;
}
