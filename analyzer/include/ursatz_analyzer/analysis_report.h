#ifndef URSATZ_ANALYZER_ANALYSIS_REPORT_H
#define URSATZ_ANALYZER_ANALYSIS_REPORT_H

#include <stdbool.h>
#include <stddef.h>

/* Which key-estimation step found the key, in the order they are tried. */
typedef enum {
    ANALYSIS_KEY_METHOD_NONE = 0,
    ANALYSIS_KEY_METHOD_KEY_SIGNATURE,
    ANALYSIS_KEY_METHOD_BASS_FALLBACK,
    ANALYSIS_KEY_METHOD_PITCH_CLASS_SCAN
} AnalysisKeyMethod;

/* A library Interpretation's scalar confidence (numerator/denominator in
   [0, 1]); present is false when the library supplies none. */
typedef struct {
    bool present;
    unsigned long numerator;
    unsigned long denominator;
} AnalysisConfidence;

typedef struct {
    long time_numerator; /* segment start, whole notes */
    unsigned long time_denominator;
    long end_numerator; /* segment end (exclusive), whole notes */
    unsigned long end_denominator;
    char *chord_label; /* owned; "no match" when nothing was identified */
    char *key_label;   /* owned; "no match" when nothing was identified */
    size_t *note_ids;  /* owned; ImportedNotes indices (Ursatz "midi-note-N") sounding in this segment, attacked or held */
    size_t note_id_count;
    AnalysisConfidence chord_confidence;
} AnalysisReportEntry;

typedef struct {
    AnalysisReportEntry *entries;
    size_t entry_count;
    bool key_found;
    AnalysisKeyMethod key_method;        /* ANALYSIS_KEY_METHOD_NONE unless key_found */
    AnalysisConfidence key_confidence;
} AnalysisReport;

/* Frees every entry's labels and note ids, and the entries array. */
void analysis_report_destroy(AnalysisReport *report);

#endif
