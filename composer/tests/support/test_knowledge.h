#ifndef URSATZ_TESTS_SUPPORT_TEST_KNOWLEDGE_H
#define URSATZ_TESTS_SUPPORT_TEST_KNOWLEDGE_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <theory/tonality/spelled_key.h>
#include <ursatz_composer/composer_api.h>
#include <theory/harmony/harmony_chord.h>
#include <ursatz_composer/harmony_phrase.h>
#include <ursatz_composer/lanes.h>
#include <ursatz_composer/motive.h>

#include "theory/knowledge/knowledge_snapshot.h"
#include "theory/tonality/common_practice_materials.h"

/*
 * The knowledge snapshot a test passes where code wants one: the embedded entries (no user files), built on first use and freed at
 * exit. Each test program that includes this header has its own, so a test never shares a snapshot with another program. The
 * snapshot outlives every key, sketch and chain a test makes from it.
 */
static KnowledgeSnapshot *test_knowledge_snapshot_value;
static CommonPracticeMaterials test_knowledge_materials_value;

static inline void test_knowledge_release(void) {
    knowledge_snapshot_destroy(test_knowledge_snapshot_value);
    test_knowledge_snapshot_value = NULL;
}

static inline const KnowledgeSnapshot *test_knowledge(void) {
    if (test_knowledge_snapshot_value == NULL) {
        char message[256];
        KnowledgeStatus status = knowledge_snapshot_build(NULL, 0, &test_knowledge_snapshot_value, message, sizeof(message));
        if (status != KNOWLEDGE_OK) {
            fprintf(stderr, "test_knowledge: %s\n", message);
            abort();
        }
        if (!common_practice_materials_resolve(test_knowledge_snapshot_value, &test_knowledge_materials_value, message,
                                               sizeof(message))) {
            fprintf(stderr, "test_knowledge: %s\n", message);
            abort();
        }
        atexit(test_knowledge_release);
    }
    return test_knowledge_snapshot_value;
}

/* The call context of test_knowledge(): what every composing and reading entry of composer_api.h takes first. */
static inline const ComposerContext *test_context(void) {
    static ComposerContext context;
    context.knowledge = test_knowledge();
    return &context;
}

/* The four common-practice materials of test_knowledge(). */
static inline const CommonPracticeMaterials *test_materials(void) {
    (void)test_knowledge();
    return &test_knowledge_materials_value;
}

/* The material of test_knowledge() a word names (its key word, name, id or alias: "major", "minor", "dorian"); the test stops for a word it does not know. */
static inline const PitchMaterial *test_material(const char *word) {
    const PitchMaterial *material = knowledge_snapshot_material_named(test_knowledge(), word);
    assert(material != NULL);
    return material;
}

/* The word of a mode's key: "major" or "minor". */
static inline const char *test_mode_word(HarmonyMode mode) {
    return mode == HARMONY_MODE_MINOR ? "minor" : "major";
}

/* The major (HARMONY_MODE_MAJOR) or the natural minor (HARMONY_MODE_MINOR) of test_knowledge(), found by its word. */
static inline const PitchMaterial *test_material_mode(HarmonyMode mode) {
    return test_material(test_mode_word(mode));
}

/*
 * The key on a pitch class (0 to 11) in the material the word names ("major", "minor", ...), spelled as plan_key_of_pitch_class
 * spells it: a test builds its keys from materials as a request does, by the word.
 */
static inline SpelledKey test_key(int pitch_class, const char *word) {
    return plan_key_of_pitch_class(pitch_class, test_material(word));
}

/* test_key for a mode held in a variable. */
static inline SpelledKey test_key_mode(int pitch_class, HarmonyMode mode) {
    return test_key(pitch_class, test_mode_word(mode));
}

/* The motive options a read with nothing else to say takes: no meter, no key, the snapshot of test_knowledge(). */
static inline const MotiveOptions *test_motive_options(void) {
    static MotiveOptions options;
    options.knowledge = test_knowledge();
    return &options;
}

/* A key as a table row writes it: a spelled tonic and a mode; test_key_of makes the SpelledKey of the mode's material. */
typedef struct {
    PitchLetter tonic_letter;
    int tonic_accidental;
    HarmonyMode mode;
} TestKeySpec;

static inline SpelledKey test_key_of(TestKeySpec spec) {
    SpelledKey key = {NULL, spec.tonic_letter, spec.tonic_accidental, test_material_mode(spec.mode)};
    return key;
}

#endif
