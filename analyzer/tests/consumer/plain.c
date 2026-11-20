/* A consumer that links only ursatz_analyzer: the one API header, and nothing of the chain's internals. */
#include <ursatz_analyzer_internal/analysis_document.h>
#include <ursatz_analyzer_internal/analysis_result.h>
#include <ursatz_analyzer_internal/chord_sequence.h>
#include <ursatz_analyzer_internal/corpus.h>
#include <ursatz_analyzer_internal/input_pipeline.h>
#include <ursatz_analyzer_internal/motive_links.h>
#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer_internal/similar_material.h>
#include <ursatz_analyzer_internal/style_profile.h>

int analyzer_consumer_plain(void) { return 0; }
