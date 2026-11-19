#include "test_support.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/input_pipeline.h>
#include "test_knowledge.h"
#include "result/analysis_result_internal.h"

static void append_bytes(unsigned char *buf, size_t *pos, const void *bytes, size_t len) {
    memcpy(buf + *pos, bytes, len);
    *pos += len;
}

static void append_u32(unsigned char *buf, size_t *pos, uint32_t v) {
    unsigned char b[4] = {(unsigned char)(v >> 24), (unsigned char)(v >> 16), (unsigned char)(v >> 8),
                          (unsigned char)v};
    append_bytes(buf, pos, b, 4);
}

static void append_u16(unsigned char *buf, size_t *pos, uint16_t v) {
    unsigned char b[2] = {(unsigned char)(v >> 8), (unsigned char)v};
    append_bytes(buf, pos, b, 2);
}

size_t test_smf_type0(unsigned char *buf, uint16_t division, const unsigned char *track_body,
                      size_t track_body_size) {
    size_t pos = 0;
    append_bytes(buf, &pos, "MThd", 4);
    append_u32(buf, &pos, 6);
    append_u16(buf, &pos, 0);
    append_u16(buf, &pos, 1);
    append_u16(buf, &pos, division);
    append_bytes(buf, &pos, "MTrk", 4);
    append_u32(buf, &pos, (uint32_t)track_body_size);
    append_bytes(buf, &pos, track_body, track_body_size);
    return pos;
}

const cJSON *test_json_require(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    assert(item != NULL);
    return item;
}

double test_rational_of(const cJSON *rational) {
    return test_json_require(rational, "num")->valuedouble / test_json_require(rational, "den")->valuedouble;
}

bool test_within(const cJSON *inner, const cJSON *outer) {
    return test_rational_of(test_json_require(inner, "start")) >=
               test_rational_of(test_json_require(outer, "start")) - 1e-12 &&
           test_rational_of(test_json_require(inner, "end")) <=
               test_rational_of(test_json_require(outer, "end")) + 1e-12;
}

char *test_render_document(const char *input_path, const ImportedScore *notes, const ChainResult *report) {
    AnalysisDocument *document = analysis_document_build(input_path, notes, report, test_knowledge());
    if (!document) {
        return NULL;
    }
    char *json = analysis_document_to_json(document);
    analysis_document_destroy(document);
    return json;
}

bool test_analyze_report(const ImportedScore *notes, ChainResult *report) {
    memset(report, 0, sizeof(*report));
    report->home_duration.den = 1;
    AnalyzerResult *result = NULL;
    if (analyzer_analyze(notes, test_analyzer_defaults(), &result) != ANALYZER_OK) {
        return false;
    }
    analyzer_result_take_report(result, report);
    analyzer_result_destroy(result);
    return true;
}

cJSON *test_analyze_midi(const char *path) {
    ImportedScore notes;
    assert(imported_score_import_file(path, IMPORTED_FORMAT_AUTO, &notes, NULL) == IMPORTED_SCORE_OK);
    ChainResult report;
    assert(test_analyze_report(&notes, &report));
    char *json = test_render_document(path, &notes, &report);
    assert(json != NULL);
    chain_result_destroy(&report);
    imported_score_destroy(&notes);
    cJSON *document = cJSON_Parse(json);
    assert(document != NULL);
    free(json);
    return document;
}
