#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_result.h>

#include "test_support.h"
#include "ursatz_test.h"
#include "test_knowledge.h"

/*
 * The bytes of the analysis document of Bach 114 (R-631): analysis_document_to_json of the document built from the result, with the
 * values of metadata.analyzer_version and metadata.producer_key replaced by "*" (they change with the build), compared byte for byte
 * with tests/snapshots/bach-114.analysis.bytes.json. test_document_snapshots compares the parsed documents of five pieces; this one
 * pins the text of one, so a change of how the document is printed (its member order, its spacing, its numbers) is seen.
 * With URSATZ_UPDATE_GOLDENS=1 (the run then exits 2) the golden is rewritten.
 */

/* Replaces the value of the string member `"<name>": "<value>"` by "*". Only the first one, which is in the metadata. */
static char *mask_member(char *text, const char *name) {
    char key[64];
    snprintf(key, sizeof(key), "\"%s\": \"", name);
    char *found = strstr(text, key);
    if (!found) {
        CHECKF(false, "no member %s in the document", name);
        return text;
    }
    char *value = found + strlen(key);
    char *end = strchr(value, '"');
    if (!end) {
        CHECKF(false, "member %s is not closed", name);
        return text;
    }
    size_t head = (size_t)(value - text);
    size_t tail = strlen(end);
    char *masked = malloc(head + 1 + tail + 1);
    if (!masked) {
        CHECKF(false, "out of memory");
        return text;
    }
    memcpy(masked, text, head);
    masked[head] = '*';
    memcpy(masked + head + 1, end, tail + 1);
    free(text);
    return masked;
}

static char *render_bach_114(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/Bach 114.mid", URSATZ_FIXTURES_DIR);
    ImportedScore notes;
    if (imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &notes, NULL) != IMPORTED_SCORE_OK) {
        CHECKF(false, "cannot import %s", path);
        return NULL;
    }
    char *text = NULL;
    AnalyzerResult *result = NULL;
    AnalyzerOptions options = test_analyzer_options();
    if (analyzer_analyze(&notes, &options, &result) == URSATZ_OK) {
        AnalysisDocument *document = analysis_document_build_from_result("Bach 114.mid", result);
        if (document) {
            text = analysis_document_to_json(document);
            analysis_document_destroy(document);
        }
    }
    analyzer_result_destroy(result);
    imported_score_destroy(&notes);
    if (!text) {
        CHECKF(false, "no document for Bach 114");
        return NULL;
    }
    text = mask_member(text, "analyzer_version");
    return mask_member(text, "producer_key");
}

int main(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/bach-114.analysis.bytes.json", URSATZ_SNAPSHOT_DIR);
    char *text = render_bach_114();
    if (!text) {
        return test_finish("test_document_bytes");
    }
    if (test_update_goldens()) {
        CHECKF(test_write_file(path, text, strlen(text)), "cannot write %s", path);
        free(text);
        return test_goldens_updated("test_document_bytes");
    }
    char *expected = test_read_file(path, NULL);
    CHECKF(expected != NULL, "cannot read %s (record it with update-goldens)", path);
    if (expected) {
        bool same = strcmp(expected, text) == 0;
        CHECKF(same, "the document of Bach 114 differs from %s byte for byte", path);
        if (!same) {
            size_t i = 0;
            while (expected[i] && expected[i] == text[i]) {
                i++;
            }
            fprintf(stderr, "first difference at byte %zu\n", i);
        }
    }
    free(expected);
    free(text);
    return test_finish("test_document_bytes");
}
