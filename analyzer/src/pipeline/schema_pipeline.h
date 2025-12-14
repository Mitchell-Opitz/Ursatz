#ifndef URSATZ_ANALYZER_SCHEMA_PIPELINE_H
#define URSATZ_ANALYZER_SCHEMA_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/analysis_report.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "pipeline/roman_numeral_pipeline.h"
#include "pipeline/segmentation.h"

/*
 * The galant schemas of the piece, from Ursatz's galant-schemas adapter (analysis/galant_analysis_input.h and
 * galant_analysis_match.h), ported from the test glue (tests/integration/schema_util.h, R-053) as a rewrite with error
 * returns. The analyzer adds no music theory (R-018): it builds the library's neutral per-segment input and copies the
 * library's matches.
 *
 * Segment i of the input is chord segment i: its outer voices (the pitch classes of the entry's soprano and bass,
 * read as degrees of the local key by the offset above its tonic), its local key (the Roman numeral input's tonic and
 * mode, as a degree of the home key), its numeral (the library's claim, an applied chord's root from the tonicized
 * degree), the strength of its beat and the length of that beat. Consecutive chords with the same key, outer voice
 * degrees and numeral (a held chord) are one segment, with the uncertainty of the least certain. A chord outside every
 * key region, or a piece with no key region, is left out. A segment's uncertainty is its numeral's (0.5 with none).
 *
 * Fills report->schemas from report->entries, the segmentation and the Roman numerals the report was built from. A
 * piece with no chords or no key region has none; that is not an error. Returns false only on allocation or library
 * failure.
 */
bool schema_stage_collect(const ImportedNotes *notes, const Segmentation *segmentation, const RomanNumerals *romans,
                          AnalysisReport *report);

#endif
