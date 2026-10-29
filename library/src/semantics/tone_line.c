#include "semantics/tone_line.h"

void tone_line_label_clear(ToneLineLabel *label) {
    if (label == NULL) {
        return;
    }
    label->chord_tone = false;
    label->classified = false;
    label->type = NON_CHORD_TONE_PASSING;
    label->resolves_to = -1;
    label->unresolved = false;
}
