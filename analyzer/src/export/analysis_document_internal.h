#ifndef URSATZ_ANALYZER_ANALYSIS_DOCUMENT_INTERNAL_H
#define URSATZ_ANALYZER_ANALYSIS_DOCUMENT_INTERNAL_H

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/version.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "model/pitch/pitch_arithmetic.h"
#include "theory/tonality/common_practice_framework.h"
#include "theory/tonality/common_practice_materials.h"
#include "model/core/rational.h"
#include "result/analysis_result_internal.h"
#include <chain/analysis_chain.h>
#include <reading/reading.h>
#include "store/path_util.h"
#include "theory/tonality/tonal_key.h"
#include "theory/form/framework.h"
#include "theory/harmony/harmony_framework.h"
#include "theory/meter/beat_grid.h"
#include "theory/meter/metric_position_rules.h"
#include "theory/framework/framework.h"
#include "model/time/duration.h"
#include "model/time/meter_map.h"
#include "model/time/metrical_position.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_signature.h"
#include "util/json_util.h"
#include "util/misc_util.h"

/* Library-private view of AnalysisDocument: renderers read the cJSON tree directly. */
struct AnalysisDocument {
    cJSON *root;
};

/*
 * What the files of the analysis document share (R-631). analysis_document.c assembles the document from the areas below;
 * document_shared.c holds the helpers more than one area uses; every other document_*.c builds the areas named in its comment.
 * DocBuild is the state of one build: it holds the reading the document is exported from.
 */

/* The state of one analysis_document_build_from_reading. overflowed is set when a time value does not fit a Rational; the build then
   fails instead of printing a wrapped time. */
typedef struct {
    bool overflowed;
    CommonPracticeMaterials materials; /* the major and natural minor a notated key signature's labels are spelled with */
    const Reading *reading;            /* the Analyzer's reading the document is exported from (R-631) */
} DocBuild;

/* The text being printed (document_print.c). */
typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} Buffer;

/* ---- shared helpers (document_shared.c) ---- */

Rational doc_rational(DocBuild *build, long long num, unsigned long long den);
Rational doc_add(DocBuild *build, Rational a, Rational b);
Rational doc_sub(DocBuild *build, Rational a, Rational b);
Rational doc_in_beats(DocBuild *build, Rational length, const Duration *beat);
bool doc_time_before(Rational a, Rational b);
Rational doc_note_onset(const NoteEvent *event);
Rational doc_note_end(DocBuild *build, const NoteEvent *event);
void doc_notes_span(DocBuild *build, const ImportedScore *notes, const size_t *ids, size_t id_count, Rational *start, Rational *end);
cJSON *doc_rational_json(Rational value);
cJSON *doc_span_json(Rational start, Rational end);
cJSON *doc_note_ids_json(const size_t *ids, size_t id_count);
cJSON *doc_uncertainty_json(AnalysisConfidence confidence);
cJSON *doc_position_json(DocBuild *build, const MeterMap *map, Rational time);
cJSON *doc_location_json(DocBuild *build, const MeterMap *map, Rational start, Rational end);
cJSON *doc_claim_json_with(DocBuild *build, const char *framework, const ImportedScore *notes, const size_t *ids, size_t id_count, Rational start, Rational end, const char *label, cJSON *uncertainty);
cJSON *doc_claim_json_at(DocBuild *build, const char *framework, const ImportedScore *notes, const size_t *ids, size_t id_count, Rational start, Rational end, const char *label, AnalysisConfidence confidence);
cJSON *doc_meter_json(DocBuild *build, const ImportedScore *notes);
const char *doc_library_framework_name(void);
cJSON *doc_metadata_json(DocBuild *build, const char *input_path, const ImportedScore *notes, const ChainResult *report);
Rational doc_region_start(DocBuild *build, const AnalysisKeyRegion *region);
Rational doc_region_end(DocBuild *build, const AnalysisKeyRegion *region);
cJSON *doc_measures_json(int first, int last);
bool doc_change_is_tonicization(const AnalysisKeyChange *change);
cJSON *doc_span_beats_json(DocBuild *build, const MeterMap *map, Rational start, Rational end);
cJSON *doc_optional_string_json(const char *value);
const char *doc_outer_status_name(AnalysisOuterStatus status);
cJSON *doc_outer_uncertainty_json(const ChainResult *report, const ChainEntry *entry, const AnalysisOuterNote *note);
size_t *doc_entry_range_note_ids(const ChainResult *report, size_t first, size_t last, size_t *count);
Rational doc_entry_start_time(DocBuild *build, const ChainEntry *entry);
Rational doc_entry_end_time(DocBuild *build, const ChainEntry *entry);
cJSON *doc_string_or_null_json(const char *text);

/* ---- the areas (one document_<name>.c each), called by the assembly in analysis_document.c ---- */

cJSON *doc_keys_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_tonicizations_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_modulations_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_pivot_chords_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_notes_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_chords_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_non_chord_tones_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_cadences_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_prolongations_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_phrases_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_themes_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_form_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_repeats_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_progressions_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_harmonic_rhythm_json(DocBuild *build, const ChainResult *report);
cJSON *doc_outer_voices_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_chord_vocabulary_json(DocBuild *build, const ChainResult *report, const cJSON *chords);
cJSON *doc_motives_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_melody_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_rhythm_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_texture_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_markings_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_voice_leading_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
cJSON *doc_schemas_json(DocBuild *build, const ImportedScore *notes, const ChainResult *report);
bool doc_buffer_append(Buffer *buffer, const char *text);
bool doc_print_value(Buffer *buffer, const cJSON *item, int depth);

#endif
