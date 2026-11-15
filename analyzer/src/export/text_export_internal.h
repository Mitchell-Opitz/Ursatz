#ifndef URSATZ_ANALYZER_TEXT_EXPORT_INTERNAL_H
#define URSATZ_ANALYZER_TEXT_EXPORT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

#include "export/score_text_reader.h"

/*
 * Private. The pieces of the text export that the piece export (text_export.c) and the material export
 * (material_export.c) share: a growing string, the measure formatter and the header lines.
 */

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool failed; /* an allocation failed; the data is not to be used */
} Text;

void export_text_add(Text *text, const char *piece);
void export_text_addf(Text *text, const char *format, ...);

/* A segment mark printed after an event: "[label]" where the segment starts, "[/label]" where it ends. */
typedef struct {
    const ScoreTextEvent *event;
    const char *label;
    bool close;
} TextExportMark;

typedef struct {
    const TextExportMark *items;
    size_t count;
} TextExportMarks;

/* Finds the first and last measure (indexes) in the inclusive measure-number range; 0 and 0 is the whole score.
   false when no measure is in range. */
bool text_export_find_range(const ScoreTextScore *score, int first_measure, int last_measure, size_t *first,
                            size_t *last);

/* "key: .. | meter: .. | tempo: .. | measures: a-b" line, from the analysis document and the score. */
void text_export_write_summary(Text *text, const cJSON *document, const ScoreTextScore *score, size_t first,
                               size_t last, bool whole);

/* The "analyzer:" and "notes:" legend lines. */
void text_export_write_legend(Text *text);

/* Measures first to last: measure line, notes (with the marks), then the claims that start in the measure. marks may
   be NULL. false on allocation failure. */
bool text_export_write_measures(Text *out, const cJSON *document, const ScoreTextScore *score, size_t first,
                                size_t last, const TextExportMarks *marks);

#endif
