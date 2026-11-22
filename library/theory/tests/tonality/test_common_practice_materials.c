#include "theory/tonality/common_practice_materials.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "ursatz_test_knowledge.h"

/*
 * The common-practice materials resolved from a snapshot: the four ids, the refusals, the key mode a material is read as by its
 * third (all nine materials of the knowledge), and the mode table.
 */

static void test_resolve(void) {
    CommonPracticeMaterials materials;
    memset(&materials, 0, sizeof(materials));
    char message[200] = "not empty";
    assert(common_practice_materials_resolve(test_knowledge(), &materials, message, sizeof(message)));
    assert(message[0] == '\0');
    assert(strcmp(materials.major->id, "omt.material.major") == 0);
    assert(strcmp(materials.natural_minor->id, "omt.material.natural-minor") == 0);
    assert(strcmp(materials.harmonic_minor->id, "omt.material.harmonic-minor") == 0);
    assert(strcmp(materials.melodic_minor->id, "omt.material.melodic-minor") == 0);
    /* They are the snapshot's own entries, not copies. */
    assert(materials.major == knowledge_snapshot_material(test_knowledge(), "omt.material.major"));
    assert(materials.melodic_minor == knowledge_snapshot_material(test_knowledge(), "omt.material.melodic-minor"));
    /* No message wanted. */
    assert(common_practice_materials_resolve(test_knowledge(), &materials, NULL, 0));
    /* The test helper resolves the same ones. */
    assert(test_materials()->major == materials.major && test_materials()->natural_minor == materials.natural_minor);
}

static void test_refusals(void) {
    CommonPracticeMaterials materials;
    memset(&materials, 0, sizeof(materials));
    char message[200] = "";
    /* A snapshot always holds the four embedded ids, so a missing one is refused only through the arguments; the refusal leaves the
       output untouched and the message empty. */
    assert(!common_practice_materials_resolve(NULL, &materials, message, sizeof(message)));
    assert(!common_practice_materials_resolve(test_knowledge(), NULL, message, sizeof(message)));
    assert(message[0] == '\0');
    assert(materials.major == NULL && materials.natural_minor == NULL && materials.harmonic_minor == NULL &&
           materials.melodic_minor == NULL);
    assert(!common_practice_materials_resolve(NULL, NULL, NULL, 0));
    /* The snapshot lookup the resolve rests on: a missing id is NULL. */
    assert(knowledge_snapshot_material(test_knowledge(), "omt.material.no-such-material") == NULL);
}

static void test_material_by_mode(void) {
    const CommonPracticeMaterials *materials = test_materials();
    assert(common_practice_material(materials, COMMON_PRACTICE_KEY_MAJOR) == materials->major);
    assert(common_practice_material(materials, COMMON_PRACTICE_KEY_MINOR) == materials->natural_minor);
    assert(common_practice_material(materials, (CommonPracticeKeyMode)5) == NULL);
    assert(common_practice_material(NULL, COMMON_PRACTICE_KEY_MAJOR) == NULL);
}

/* The key mode of every material of the knowledge, by the size of its third. */
static void test_key_mode_by_third(void) {
    static const struct {
        const char *id;
        CommonPracticeKeyMode mode;
    } EXPECTED[] = {{"omt.material.major", COMMON_PRACTICE_KEY_MAJOR},          {"omt.material.natural-minor", COMMON_PRACTICE_KEY_MINOR},
                    {"omt.material.harmonic-minor", COMMON_PRACTICE_KEY_MINOR}, {"omt.material.melodic-minor", COMMON_PRACTICE_KEY_MINOR},
                    {"omt.material.lydian", COMMON_PRACTICE_KEY_MAJOR},         {"omt.material.mixolydian", COMMON_PRACTICE_KEY_MAJOR},
                    {"omt.material.dorian", COMMON_PRACTICE_KEY_MINOR},         {"omt.material.phrygian", COMMON_PRACTICE_KEY_MINOR},
                    {"omt.material.locrian", COMMON_PRACTICE_KEY_MINOR}};
    assert(sizeof(EXPECTED) / sizeof(EXPECTED[0]) == 9);
    for (size_t i = 0; i < sizeof(EXPECTED) / sizeof(EXPECTED[0]); i++) {
        const PitchMaterial *material = knowledge_snapshot_material(test_knowledge(), EXPECTED[i].id);
        assert(material != NULL);
        CommonPracticeKeyMode mode = (CommonPracticeKeyMode)99;
        assert(common_practice_key_mode_of(material, &mode));
        assert(mode == EXPECTED[i].mode);
        /* a major third is 4 semitones, a minor third 3 */
        assert(pitch_material_third(material) == (EXPECTED[i].mode == COMMON_PRACTICE_KEY_MAJOR ? 4 : 3));
    }
    /* A material with no third, or a third of another size, has no key mode; NULL arguments are refused. */
    PitchMaterial pair;
    memset(&pair, 0, sizeof(pair));
    pair.step_count = 2;
    pair.steps[1] = 7;
    CommonPracticeKeyMode mode = COMMON_PRACTICE_KEY_MAJOR;
    assert(!common_practice_key_mode_of(&pair, &mode) && mode == COMMON_PRACTICE_KEY_MAJOR);
    PitchMaterial odd = pair;
    odd.step_count = 3;
    odd.steps[1] = 2;
    odd.steps[2] = 5;
    assert(!common_practice_key_mode_of(&odd, &mode) && mode == COMMON_PRACTICE_KEY_MAJOR);
    assert(!common_practice_key_mode_of(NULL, &mode));
    assert(!common_practice_key_mode_of(test_materials()->major, NULL));
}

int main(void) {
    test_resolve();
    test_refusals();
    test_material_by_mode();
    test_key_mode_by_third();
    puts("common_practice_materials: ok");
    return 0;
}
