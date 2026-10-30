#include "test_support.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/input_pipeline.h>

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

const cJSON *test_json_member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

const cJSON *test_json_require(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    assert(item != NULL);
    return item;
}

double test_json_number(const cJSON *object, const char *key) {
    const cJSON *item = test_json_member(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : -1e9;
}

const char *test_json_string(const cJSON *object, const char *key) {
    const cJSON *item = test_json_member(object, key);
    return cJSON_IsString(item) ? item->valuestring : "";
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

char *test_read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    char *data = malloc((size_t)size + 1);
    if (data && fread(data, 1, (size_t)size, file) != (size_t)size) {
        free(data);
        data = NULL;
    }
    fclose(file);
    if (data) {
        data[size] = '\0';
    }
    return data;
}

bool test_write_file(const char *path, const char *data) {
    FILE *file = fopen(path, "wb");
    if (!file) {
        return false;
    }
    bool ok = fputs(data, file) >= 0;
    return fclose(file) == 0 && ok;
}

cJSON *test_analyze_midi(const char *path) {
    ImportedNotes notes;
    assert(input_pipeline_import_file(path, INPUT_FORMAT_AUTO, &notes) == INPUT_PIPELINE_OK);
    AnalysisReport report;
    assert(run_analysis_pipeline(&notes, &report));
    char *json = analysis_document_render(path, &notes, &report);
    assert(json != NULL);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    cJSON *document = cJSON_Parse(json);
    assert(document != NULL);
    free(json);
    return document;
}
