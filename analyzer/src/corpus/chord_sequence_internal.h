#ifndef URSATZ_ANALYZER_CHORD_SEQUENCE_INTERNAL_H
#define URSATZ_ANALYZER_CHORD_SEQUENCE_INTERNAL_H

#include <cJSON.h>

#include <ursatz_analyzer/chord_sequence.h>

/* analyzer_chord_sequence_from_document over a parsed document, for a caller that has parsed it and checked its schema
   version already. The statuses are the same. */
AnalyzerStatus chord_sequence_from_tree(const cJSON *document, AnalyzerChord **out, size_t *count);

#endif
