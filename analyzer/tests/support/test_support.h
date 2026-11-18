#ifndef URSATZ_ANALYZER_TEST_SUPPORT_H
#define URSATZ_ANALYZER_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

#include <ursatz_analyzer/analysis_report.h>
#include <io/imported_score.h>

/* A format 0 MIDI file with one track into buf; returns its size. */
size_t test_smf_type0(unsigned char *buf, uint16_t division, const unsigned char *track_body, size_t track_body_size);

/* The member of an object; asserts that it exists. */
const cJSON *test_json_require(const cJSON *object, const char *key);

/* The value of a {num, den} rational object. */
double test_rational_of(const cJSON *rational);

/* True when the {start, end} span of inner lies inside the span of outer. */
bool test_within(const cJSON *inner, const cJSON *outer);

/* analysis_document_build, then analysis_document_to_json, for an input path, its notes and report. Returns a heap
   string the caller frees, or NULL on allocation failure. */
char *test_render_document(const char *input_path, const ImportedScore *notes, const AnalysisReport *report);

/* Imports a file (the format is detected), runs the pipeline and returns the parsed analysis document the
   caller deletes. Asserts on any failure. */
cJSON *test_analyze_midi(const char *path);

#endif
