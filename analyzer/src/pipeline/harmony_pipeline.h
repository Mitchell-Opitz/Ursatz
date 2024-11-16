#ifndef URSATZ_ANALYZER_HARMONY_PIPELINE_H
#define URSATZ_ANALYZER_HARMONY_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "analysis_cadence.h"
#include "key_regions.h"
#include "analysis_input.h"
#include "pipeline/roman_numeral_pipeline.h"
#include "pipeline/segmentation.h"

/*
 * Ursatz's standard-harmony analysis over the chain's chord segments (analysis/analysis_input.h and
 * analysis_cadence.h), ported from the test glue (tests/integration/test_cadences_reference_pieces.c make_inputs,
 * intervals_of, set_intervals, and framework_v1_chain.h, R-053) as a rewrite with error returns. The analyzer adds no
 * music theory: every input is read from the library's own results (the Roman numeral claims, the outer voices'
 * degree labels, the meter), and every cadence, type and verdict is the library's.
 *
 * A HarmonyStage owns the neutral inputs, the prepared segments the later stages (cadences here; functions,
 * six-fours and prolongations after) read, and the cadences found. A piece the library cannot prepare (a segment it
 * cannot place on the meter) leaves the stage empty, as a piece with no cadences; that is not an error.
 */
typedef struct {
    size_t count;                 /* chord segments prepared; 0 when there are none or the library declined */
    HarmonySegmentInput *inputs;  /* owned; one per chord segment */
    MetricalPosition **positions; /* owned; inputs[i].position */
    Duration **beat_lengths;      /* owned; inputs[i].beat_length (NULL where the meter gives none) */
    HarmonySegment *segments;     /* owned; prepared, pointing at inputs */
    HarmonyCadences cadences;     /* owned; in order of arrival */
    bool *boundary;               /* owned; parallel to cadences.items: a half cadence the boundary step added;
                                     NULL until harmony_stage_add_boundary_cadences */
} HarmonyStage;

/* Builds the inputs of every chord segment and prepares them. A segment with no Roman numeral gets degree 0.
   entries supply each segment's key label, soprano and bass degree labels (never the outer voice lines indexed by
   chord segment: those are per voice segment). Returns false only on allocation failure. */
bool harmony_stage_prepare(const ImportedNotes *notes, const Segmentation *segmentation, const RomanNumerals *romans,
                           const AnalysisReportEntry *entries, HarmonyStage *out);

/* Finds the cadences with the library's default config. Returns false only on allocation failure. */
bool harmony_stage_find_cadences(HarmonyStage *stage);

/*
 * Dominants before a section start are half cadences of the earlier section (R-045, R-054), ported from the
 * Framework v1 chain (fv1_section_starts, fv1_add_boundary_cadences). A section starts at each key region that
 * returns to the home key after leaving it; regions are the library's regions of the pass and home_key_index the key
 * index of the report's home region. Sections from the form structure (P3 B7) do not feed back. Marks the cadences the
 * step added or replaced. Returns false only on allocation failure.
 */
bool harmony_stage_add_boundary_cadences(HarmonyStage *stage, const CommonPracticeKeyRegions *regions,
                                         int home_key_index);

/* Copies the stage's cadences into report->cadences, each with its entry ranges, type, soprano degree (the entry's
   label), beat strength, key region and uncertainty. entries must be the ones the stage was prepared from. Returns
   false only on allocation failure. */
bool harmony_stage_collect_cadences(const HarmonyStage *stage, AnalysisReport *report);

void harmony_stage_destroy(HarmonyStage *stage);

#endif
