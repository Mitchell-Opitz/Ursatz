#ifndef URSATZ_TESTS_SUPPORT_TEST_KNOWLEDGE_H
#define URSATZ_TESTS_SUPPORT_TEST_KNOWLEDGE_H

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <ursatz_analyzer_internal/analysis_result.h>

#include "ursatz_test_knowledge.h"

/* The default analysis options with the test snapshot (the defaults carry none). */
static inline AnalyzerOptions test_analyzer_options(void) {
    AnalyzerOptions options = analyzer_options_default();
    options.knowledge = test_knowledge();
    return options;
}

/* The same as a pointer, for a call that passes `NULL` for the defaults. */
static inline const AnalyzerOptions *test_analyzer_defaults(void) {
    static AnalyzerOptions options;
    options = test_analyzer_options();
    return &options;
}

#endif
