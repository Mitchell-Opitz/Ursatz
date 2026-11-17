#ifndef URSATZ_GUI_TESTS_TEST_KNOWLEDGE_H
#define URSATZ_GUI_TESTS_TEST_KNOWLEDGE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "service/knowledge_service.h"

/*
 * The knowledge snapshot a test that calls the analysis services without starting the server needs (the server builds it itself):
 * the backend's own one, built on first use and freed at exit. Each test program has its own.
 */
static inline const KnowledgeSnapshot *test_knowledge(void) {
    static bool registered = false;
    if (!registered) {
        if (!ursatz_gui_knowledge_init()) {
            fprintf(stderr, "test_knowledge: the knowledge snapshot cannot be built\n");
            abort();
        }
        atexit(ursatz_gui_knowledge_shutdown);
        registered = true;
    }
    return ursatz_gui_knowledge();
}

#endif
