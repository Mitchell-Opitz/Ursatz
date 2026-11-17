#include <assert.h>
#include <stdio.h>

#include "theory/knowledge/knowledge_snapshot.h"
#include "theory/tonality/tonal_key.h"

/*
 * The tonality materials are read from their knowledge entries: the four materials of the snapshot give the degrees and pitch
 * classes the key code uses. The entries are active because this code uses them (target architecture 5.1).
 */

static const char *MODE_IDS[4] = {"omt.material.major", "omt.material.natural-minor", "omt.material.harmonic-minor",
                                  "omt.material.melodic-minor"};

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
            assert(standard_tonality_material_degree_semitone(material, d) == pitch_material_step(material, d));
        }
        for (int tonic = 0; tonic < 12; tonic++) {
            unsigned mask = 0;
            for (int d = 1; d <= 7; d++) {
                mask |= 1u << ((tonic + pitch_material_step(material, d)) % 12);
            }
            assert(standard_tonality_material_pitch_classes(tonic, material) == mask);
        }
        /* every pitch class of the mode is found back at its degree */
        for (int d = 1; d <= 7; d++) {
            int degree = 0;
            int alteration = -1;
            assert(standard_tonality_material_degree_of_semitone(pitch_material_step(material, d), material, &degree, &alteration));
            assert(degree == d && alteration == 0);
        }
    }
    knowledge_snapshot_destroy(snapshot);
    puts("knowledge tonality modes: ok");
    return 0;
}
