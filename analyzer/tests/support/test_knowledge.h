#ifndef URSATZ_TESTS_SUPPORT_TEST_KNOWLEDGE_H
#define URSATZ_TESTS_SUPPORT_TEST_KNOWLEDGE_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/composer_api.h>

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

/* The four common-practice materials of test_knowledge(). */
static inline const CommonPracticeMaterials *test_materials(void) {
    (void)test_knowledge();
    return &test_knowledge_materials_value;
}

/* The default analysis options and hearing options with the test snapshot (the defaults carry none). */
static inline AnalyzerOptions test_analyzer_options(void) {
    AnalyzerOptions options = analyzer_options_default();
    options.knowledge = test_knowledge();
    return options;
}

static inline AnalyzerHearingOptions test_hearing_options(void) {
    AnalyzerHearingOptions options = analyzer_hearing_options_default();
    options.analysis.knowledge = test_knowledge();
    return options;
}

/* The same as pointers, for a call that passes `NULL` for the defaults. */
static inline const AnalyzerOptions *test_analyzer_defaults(void) {
    static AnalyzerOptions options;
    options = test_analyzer_options();
    return &options;
}

static inline const AnalyzerHearingOptions *test_hearing_defaults(void) {
    static AnalyzerHearingOptions options;
    options = test_hearing_options();
    return &options;
}

#endif
