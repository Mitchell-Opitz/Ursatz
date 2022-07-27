#include "harmony_embellishing.h"

#include <stddef.h>

bool harmony_gesture_valid(const HarmonyGesture *gesture) {
    if (gesture == NULL || (gesture->note_count != 2 && gesture->note_count != 3)) {
        return false;
    }
    for (int i = 0; i < gesture->note_count; i++) {
        if (!beat_strength_valid(gesture->beat[i])) {
            return false;
        }
    }
    return true;
}

static int magnitude(int n) {
    return n < 0 ? -n : n;
}

static int sign(int n) {
    return n > 0 ? 1 : (n < 0 ? -1 : 0);
}

bool harmony_embellishing_classify(const HarmonyGesture *gesture, NonChordTone *out) {
    if (out == NULL || !harmony_gesture_valid(gesture)) {
        return false;
    }
    if (gesture->note_count == 2) {
        if (gesture->steps[0] != gesture->steps[1]) {
            return false;
        }
        *out = NON_CHORD_TONE_ANTICIPATION;
        return true;
    }

    int approach = gesture->steps[1] - gesture->steps[0];
    int leave = gesture->steps[2] - gesture->steps[1];
    bool opposite = sign(approach) == -sign(leave);
    NonChordTone tone;

    if (approach == 0 && leave == 0) {
        tone = NON_CHORD_TONE_PEDAL;
    } else if (approach == 0 && leave == -1) {
        tone = NON_CHORD_TONE_SUSPENSION;
    } else if (approach == 0 && leave == 1) {
        tone = NON_CHORD_TONE_RETARDATION;
    } else if (magnitude(approach) == 1 && magnitude(leave) == 1) {
        tone = opposite ? NON_CHORD_TONE_NEIGHBOR : NON_CHORD_TONE_PASSING;
    } else if (magnitude(approach) > 1 && magnitude(leave) == 1 && opposite) {
        tone = NON_CHORD_TONE_APPOGGIATURA;
    } else if (magnitude(approach) == 1 && magnitude(leave) > 1 && opposite) {
        tone = NON_CHORD_TONE_ESCAPE_TONE;
    } else {
        return false;
    }
    *out = tone;
    return true;
}
