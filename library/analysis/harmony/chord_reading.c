#include "analysis/harmony/chord_reading.h"

#include <stddef.h>

#define PITCH_CLASS_MASK 0xFFFu

/* The pitch classes of `mask` as semitones above `root`: bit n of the result is pitch class (root + n) % 12 of the mask. */
static unsigned mask_above_root(unsigned mask, int root) {
    unsigned above = 0;
    for (int n = 0; n < 12; n++) {
        if ((mask & (1u << ((root + n) % 12))) != 0) {
            above |= 1u << n;
        }
    }
    return above;
}

bool chord_reading_of_sonority(const KnowledgeSnapshot *snapshot, SonorityPitches sonority, int root, ChordReading *out) {
    if (snapshot == NULL || out == NULL || root > 11 || (sonority.pitch_classes & ~PITCH_CLASS_MASK) != 0 ||
        sonority.bass_pitch_class < -1 || sonority.bass_pitch_class > 11) {
        return false;
    }
    ChordReading reading = {0};
    reading.sonority = sonority;
    reading.bass_pitch_class = sonority.bass_pitch_class;
    reading.role = CHORD_ROLE_FUNCTIONAL;
    if (root >= 0) {
        reading.has_root = true;
        reading.root_pitch_class = root;
        reading.type = knowledge_snapshot_chord_type_of(snapshot, mask_above_root(sonority.pitch_classes, root));
        if (reading.type != NULL) {
            for (size_t i = 0; i < reading.type->member_count; i++) {
                int role = (int)reading.type->members[i].role;
                if (role >= 0 && role < CHORD_READING_ROLE_COUNT) {
                    int semitones = reading.type->members[i].semitones;
                    reading.members_by_role[role] |= 1u << (((root + semitones) % 12 + 12) % 12);
                }
            }
        }
    }
    *out = reading;
    return true;
}
