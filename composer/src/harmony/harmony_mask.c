#include "harmony/harmony_internal.h"

#include <theory/counterpoint/melodic_rules.h>
#include <theory/harmony/harmony_applied.h>

#include "util/util.h"

/* The chord masks and the melodic rule count the melody, the accompaniment and the counterpoint read through the harmony. */

bool melody_chord_mask(const ComposerChords *chords, const SpelledKey *key, const PhraseChord *chord, unsigned *mask, unsigned *seventh) {
    int intervals[4];
    size_t count;
    int root;
    if (chord->applied) {
        if (!harmony_applied_intervals(&chord->applied_chord, intervals, &count)) {
            return false;
        }
        root = harmony_applied_root_semitone(harmony_key_mode(key), &chord->applied_chord);
        if (root < 0) {
            return false;
        }
        root = (spelled_key_tonic_pitch_class(key) + root) % 12;
    } else if (!composer_chord_tones(chords, key, &chord->chord, &root, intervals, &count)) {
        return false;
    }
    *mask = 0;
    *seventh = 0;
    for (size_t k = 0; k < count; k++) {
        unsigned bit = 1u << ((root + intervals[k]) % 12);
        *mask |= bit;
        if (k == 3) {
            *seventh = bit;
        }
    }
    return true;
}

unsigned melody_melodic_soft_rules(void) {
    unsigned count = 0;
    for (int rule = 0; rule < STANDARD_COUNTERPOINT_MELODIC_RULE_COUNT; rule++) {
        count += standard_counterpoint_melodic_rule_soft((StandardCounterpointMelodicRule)rule);
    }
    return count;
}
