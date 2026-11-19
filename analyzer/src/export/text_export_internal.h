#ifndef URSATZ_ANALYZER_TEXT_EXPORT_INTERNAL_H
#define URSATZ_ANALYZER_TEXT_EXPORT_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "cJSON.h"

#include <common/strings.h>

#include <ursatz_analyzer/analyzer_api.h>

#include "io/score_layout.h"
#include "model/core/rational.h"

/*
 * The written score as the text export reads it (R-621): the measures of the library's score layout (io/score_layout.h), with the
 * times as exact Rationals in whole notes. A measure starts where the measures before it end; its length is the time
 * signature's, or the content's for a short first (pickup) measure. A chord is one event with several pitches.
 */

/* Most pitches kept in one chord event; further pitches of a larger chord are dropped. */
#define SCORE_TEXT_MAX_PITCHES 16

typedef struct {
    char step;  /* 'A' to 'G' */
    int alter;  /* semitones: -1 flat, 1 sharp */
    int octave; /* scientific pitch notation: middle C is C4 */
} ScoreTextPitch;

typedef struct {
    int part;  /* 1-based, in the order of the score's parts */
    int staff; /* 1-based; 1 in a part of one staff */
    int voice; /* the MusicXML voice */
    Rational onset;           /* whole notes from the start of the piece */
    Rational beat_in_measure; /* 1-based, in beats of the time signature (a pickup is aligned to the bar's end) */
    ScoreTextPitch pitches[SCORE_TEXT_MAX_PITCHES];
    size_t pitch_count; /* 0 for a rest */
    bool rest;
    char type;           /* notated value: b w h q e s t x y for breve, whole to 128th; '?' when it cannot be told */
    int dots;
    int tuplet_actual;   /* time-modification actual-notes; 0 when the note is not in a tuplet */
    int tuplet_normal;   /* time-modification normal-notes */
    bool tie_start;      /* ties to the next note */
    char xml_id[96];     /* the note's xml:id; empty for a rest */
    size_t order;        /* position in the layout among the part's events of the measure */
} ScoreTextEvent;

typedef struct {
    int part;   /* 1-based */
    int staff;  /* 1-based */
    bool bass;  /* an F clef on line 4; false for a G clef on line 2 (treble) */
} ScoreTextClef;

typedef struct {
    char number[24];  /* the measure number as written */
    bool pickup;      /* a first measure shorter than its time signature */
    Rational start;
    Rational length;
    int beats;        /* time signature in force */
    int beat_type;
    bool time_changed; /* a time signature that differs from the one before sits here (always on the first measure) */
    ScoreTextClef *clefs; /* the clefs written in the measure, by part: the opening ones in the first, a change in a later one */
    size_t clef_count;
    ScoreTextEvent *events; /* ordered by part, staff, voice, onset, layout order */
    size_t event_count;
} ScoreTextMeasure;

typedef struct {
    ScoreTextMeasure *measures;
    size_t measure_count;
    size_t part_count;
} ScoreTextScore;

/* The score text of a layout. NULL for a layout with no measure, a time that does not fit a Rational, or on allocation failure. */
ScoreTextScore *score_text_from_layout(const ScoreLayout *layout);

/* Frees a score. NULL is allowed. */
void score_text_destroy(ScoreTextScore *score);

/*
 * Private. The pieces of the text export that the piece export (text_export.c) and the material export
 * (material_export.c) share: the measure formatter and the header lines.
 */

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
void text_export_write_summary(CommonText *text, const cJSON *document, const ScoreTextScore *score, size_t first,
                               size_t last, bool whole);

/* The "analyzer:" and "notes:" legend lines. */
void text_export_write_legend(CommonText *text);

/* Measures first to last: measure line, notes (with the marks), then the claims that start in the measure. marks may
   be NULL. false on allocation failure. */
bool text_export_write_measures(CommonText *out, const cJSON *document, const ScoreTextScore *score, size_t first,
                                size_t last, const TextExportMarks *marks);

/* The text export of a piece from its analysis document tree (analyzer_text_export_piece parses the text and calls it; the material
   export builds the tree itself). The tree is read, not kept. */
UrsatzStatus analyzer_text_export_piece_tree(const ImportedScore *notes, const cJSON *document,
                                               const AnalyzerTextExportOptions *options, char **text);

#endif
