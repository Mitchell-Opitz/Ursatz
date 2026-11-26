#include <ursatz_composer/motive_blueprint.h>

#include <theory/tonality/tonal_key.h>

#include "util/util.h"

/* The major scale on C, bit pc: the scale of a key with no material (the zero key reads as C major). */
#define MAJOR_ON_C 0xAB5u

int composer_event_pitch(const ComposerChords *chords, const SpelledKey *key, PlanEventType type, int previous, int next) {
    (void)chords;
    if (previous < 0 && next < 0) {
        return -1;
    }
    previous = previous < 0 ? next : previous;
    next = next < 0 ? previous : next;
    /* The key's natural scale, with the pitch classes of the placed neighbours. */
    int tonic = key ? spelled_key_tonic_pitch_class(key) : 0;
    unsigned scale = key && key->material ? util_scale_mask(tonic, key->material) : 0;
    if (scale == 0) {
        scale = ((MAJOR_ON_C << tonic) | (MAJOR_ON_C >> (12 - tonic))) & 0xFFFu;
    }
    scale |= (1u << util_pitch_class(previous)) | (1u << util_pitch_class(next));
    int distance = next - previous;
    switch (type) {
    case PLAN_EVENT_APPOGGIATURA:
        return standard_tonality_mask_step(scale, next, distance >= 1 && distance <= 2 ? -1 : 1);
    case PLAN_EVENT_SUSPENSION:
    case PLAN_EVENT_RETARDATION:
        return previous;
    case PLAN_EVENT_PASSING: {
        int low = previous < next ? previous : next;
        int high = previous < next ? next : previous;
        if (high - low == 3 || high - low == 4) {
            return standard_tonality_mask_step(scale, low, 1);
        }
        /* The scale tone nearest the middle, the lower on a tie. */
        int best = -1;
        for (int pitch = low; pitch <= high; pitch++) {
            if (((scale >> util_pitch_class(pitch)) & 1u) &&
                (best < 0 || util_absolute(2 * pitch - (low + high)) < util_absolute(2 * best - (low + high)))) {
                best = pitch;
            }
        }
        return best;
    }
    case PLAN_EVENT_NEIGHBOR:
        return standard_tonality_mask_step(scale, previous, next > previous ? -1 : 1);
    case PLAN_EVENT_ESCAPE:
        return standard_tonality_mask_step(scale, previous, next > previous ? -1 : 1);
    case PLAN_EVENT_ANTICIPATION:
        return next;
    default:
        return -1;
    }
}
