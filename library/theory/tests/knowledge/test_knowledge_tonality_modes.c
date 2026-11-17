#include <assert.h>
#include <stdio.h>

#include "theory/knowledge/knowledge_modes.h"
#include "theory/knowledge/knowledge_snapshot.h"
#include "theory/tonality/tonal_key.h"

/*
 * The tonality modes are read from their knowledge entries: the four materials of the snapshot give the same degrees, pitch classes
 * and generated rows the key code uses. The entries are active because this code uses them (target architecture 5.1).
 */

static const char *MODE_IDS[4] = {"omt.material.major", "omt.material.natural-minor", "omt.material.harmonic-minor",
                                  "omt.material.melodic-minor"};
static const StandardTonalityMode MODES[4] = {STANDARD_TONALITY_MODE_MAJOR, STANDARD_TONALITY_MODE_NATURAL_MINOR,
                                              STANDARD_TONALITY_MODE_HARMONIC_MINOR, STANDARD_TONALITY_MODE_MELODIC_MINOR};

int main(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);

    for (int m = 0; m < 4; m++) {
        const KnowledgeEntry *entry = knowledge_snapshot_entry(snapshot, MODE_IDS[m]);
        assert(entry != NULL && entry->status == KNOWLEDGE_STATUS_ACTIVE);
        const PitchMaterial *material = knowledge_snapshot_material(snapshot, MODE_IDS[m]);
        assert(material != NULL && material->step_count == 7);

        for (int d = 1; d <= 7; d++) {
            assert(standard_tonality_degree_semitone(MODES[m], d) == pitch_material_step(material, d));
            assert(knowledge_tonality_mode_steps[m][d - 1] == material->steps[d - 1]);
        }
        for (int tonic = 0; tonic < 12; tonic++) {
            unsigned mask = 0;
            for (int d = 1; d <= 7; d++) {
                mask |= 1u << ((tonic + pitch_material_step(material, d)) % 12);
            }
            assert(standard_tonality_mode_pitch_classes(tonic, MODES[m]) == mask);
        }
        /* every pitch class of the mode is found back at its degree */
        for (int d = 1; d <= 7; d++) {
            int degree = 0;
            int alteration = -1;
            assert(standard_tonality_degree_of_semitone(pitch_material_step(material, d), MODES[m], &degree, &alteration));
            assert(degree == d && alteration == 0);
        }
    }
    knowledge_snapshot_destroy(snapshot);
    puts("knowledge tonality modes: ok");
    return 0;
}
