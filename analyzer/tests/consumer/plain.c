/* A consumer that links only ursatz_analyzer: every public header but result_chain.h, and nothing of the chain or the frameworks. */
#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/analysis_result.h>
#include <ursatz_analyzer/chord_sequence.h>
#include <ursatz_analyzer/composer_api.h>
#include <ursatz_analyzer/corpus.h>
#include <ursatz_analyzer/imported_notes.h>
#include <ursatz_analyzer/input_pipeline.h>
#include <ursatz_analyzer/markdown_report.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/motive_links.h>
#include <ursatz_analyzer/musicxml_pipeline.h>
#include <ursatz_analyzer/notation_reading.h>
#include <ursatz_analyzer/playback.h>
#include <ursatz_analyzer/report_writer.h>
#include <ursatz_analyzer/score_export.h>
#include <ursatz_analyzer/similar_material.h>
#include <ursatz_analyzer/staleness.h>
#include <ursatz_analyzer/style_labels.h>
#include <ursatz_analyzer/style_profile.h>

int analyzer_consumer_plain(void) { return 0; }
