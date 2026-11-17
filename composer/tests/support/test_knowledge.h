#ifndef URSATZ_TESTS_SUPPORT_TEST_KNOWLEDGE_H
#define URSATZ_TESTS_SUPPORT_TEST_KNOWLEDGE_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <analysis/notes/analysis_key.h>
#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/lanes.h>

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

/*
 * A key written as a literal with a mode and no material, given the material of test_knowledge() for its mode (the major or the
 * natural minor): what the Composer reads a key's mode from.
 */
static inline AnalysisKey test_key_with_material(AnalysisKey key) {
    key.material = key.mode == KEY_STRUCTURE_MODE_MINOR ? test_materials()->natural_minor : test_materials()->major;
    return key;
}

/* The major (mode 0) or the natural minor (KEY_STRUCTURE_MODE_MINOR or HARMONY_MODE_MINOR) of test_knowledge(). */
static inline const PitchMaterial *test_material_mode(int mode) {
    return mode == KEY_STRUCTURE_MODE_MINOR ? test_materials()->natural_minor : test_materials()->major;
}

/*
 * The key on a pitch class (0 to 11) in the major or the natural minor of test_knowledge(), spelled as plan_key_of_pitch_class
 * spells it. TRANSITIONAL: the mode argument goes with the mode types (P58 B13 commit 6, test keys from words in commit 7).
 */
static inline AnalysisKey test_key_mode(int pitch_class, int mode) {
    return plan_key_of_pitch_class(pitch_class, test_material_mode(mode));
}

#endif
