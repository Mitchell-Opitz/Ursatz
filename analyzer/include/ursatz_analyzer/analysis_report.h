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

/* One chord reading from the library's weighted identification: the best
   chord of a segment, or its runner-up. label is NULL when there is no
   such reading (no match, or no runner-up). All strings are owned. */
typedef struct {
    char *label;   /* the library's label for the chord, e.g. "V" or "dominant7" */
    char *root;    /* root pitch class name, e.g. "G" */
    char *quality; /* e.g. "major", "minor", "diminished", "dominant7" */
    char *bass;    /* bass pitch class name; the segment's, so the same for both readings */
    int inversion; /* 0 root position, 1 first, 2 second, 3 third; -1 when the bass is not a chord tone */
    double score;  /* the library's candidate score */
    AnalysisConfidence confidence;
} AnalysisChordReading;

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
    AnalysisChordReading chord;     /* best chord; label NULL when "no match" */
    AnalysisChordReading runner_up; /* label NULL when the library gave none */
} AnalysisReportEntry;

/* Frees a reading's strings and clears it. */
void analysis_chord_reading_destroy(AnalysisChordReading *reading);

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
