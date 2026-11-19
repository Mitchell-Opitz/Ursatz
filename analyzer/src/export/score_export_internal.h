#ifndef URSATZ_ANALYZER_SCORE_EXPORT_INTERNAL_H
#define URSATZ_ANALYZER_SCORE_EXPORT_INTERNAL_H

#include <ursatz_analyzer/score_export.h>

#include "io/score_layout.h"

/*
 * Private. The layout of an import's score (io/score_layout.h): what analyzer_score_export_musicxml writes, as measures and events, for
 * the text and material exports to read instead of reading the MusicXML back (R-621). *out is released with score_layout_destroy and is
 * filled only on ANALYZER_OK; the statuses are those of analyzer_score_export_musicxml.
 */
AnalyzerStatus analyzer_score_layout(const ImportedScore *notes, ScoreLayout *out);

#endif
