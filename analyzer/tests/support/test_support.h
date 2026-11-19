#ifndef URSATZ_ANALYZER_TEST_SUPPORT_H
#define URSATZ_ANALYZER_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cJSON.h>

#include <chain/chain_result.h>
#include <io/imported_score.h>
#include <ursatz_analyzer/analysis_document.h>

/* A format 0 MIDI file with one track into buf; returns its size. */
size_t test_smf_type0(unsigned char *buf, uint16_t division, const unsigned char *track_body, size_t track_body_size);

/* The member of an object; asserts that it exists. */
const cJSON *test_json_require(const cJSON *object, const char *key);

/* The value of a {num, den} rational object. */
double test_rational_of(const cJSON *rational);

/* True when the {start, end} span of inner lies inside the span of outer. */
bool test_within(const cJSON *inner, const cJSON *outer);

/* The analysis document of the analysis a report came from (test_analyze_report), for an input path: analysis_document_build_from_result.
   NULL when the report is not one test_analyze_report made, or on allocation failure. */
AnalysisDocument *test_build_document(const char *input_path, const ChainResult *report);

/* test_build_document, then analysis_document_to_json. Returns a heap string the caller frees, or NULL. `notes` is the one the report
   was made from (unused: the result holds it). */
char *test_render_document(const char *input_path, const ImportedScore *notes, const ChainResult *report);

/* Imports a file (the format is detected), runs the pipeline and returns the parsed analysis document the
   caller deletes. Asserts on any failure. */
cJSON *test_analyze_midi(const char *path);

/* Analyzes imported notes with the test snapshot (analyzer_analyze, every stage) and gives *report a shallow copy of the report view of
   the result, which the test layer keeps until test_release_report: the copy's arrays are the chain's, so a test may read them and
   change them in place. false when the analysis fails. */
bool test_analyze_report(const ImportedScore *notes, ChainResult *report);

/* Releases the analysis behind a report test_analyze_report made, and zeroes the report. Any other report is chain_result_destroy's. */
void test_release_report(ChainResult *report);

#endif
