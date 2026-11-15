#ifndef URSATZ_ANALYZER_SCORE_TEXT_READER_H
#define URSATZ_ANALYZER_SCORE_TEXT_READER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * Private. Reads the score MusicXML the library's writer emits (analyzer_score_export_musicxml, or the Composer's
 * rendered MusicXML) back into measures and events for the text export (text_export.h). It reads only what that writer
 * emits and skips every other element; the list is in docs/text-export.md.
 *
 * All times are exact rationals in whole notes (no floating point). A measure starts where the measures before it end;
 * a measure's length is the time signature's length, or the content's length for a short first (pickup) measure.
 */

/* Most pitches kept in one chord event; further pitches of a larger chord are dropped. */
#define SCORE_TEXT_MAX_PITCHES 16

/* A fraction in lowest terms with den > 0. */
typedef struct {
    int64_t num;
    int64_t den;
} TextRational;

TextRational text_rational(int64_t num, int64_t den);
TextRational text_rational_add(TextRational left, TextRational right);
TextRational text_rational_sub(TextRational left, TextRational right);
TextRational text_rational_mul(TextRational left, TextRational right);
/* -1, 0 or 1 as left is less than, equal to or greater than right. */
int text_rational_compare(TextRational left, TextRational right);

typedef struct {
    char step;  /* 'A' to 'G' */
    int alter;  /* semitones: -1 flat, 1 sharp */
    int octave; /* scientific pitch notation: middle C is C4 */
} ScoreTextPitch;

typedef struct {
    int part;  /* 1-based, in document order */
    int staff; /* 1-based; 1 when the note names none */
    int voice; /* the MusicXML voice; 1 when the note names none */
    TextRational onset;           /* whole notes from the start of the piece */
    TextRational beat_in_measure; /* 1-based, in beats of the time signature (a pickup is aligned to the bar's end) */
    ScoreTextPitch pitches[SCORE_TEXT_MAX_PITCHES];
    size_t pitch_count; /* 0 for a rest */
    bool rest;
    char type;           /* notated value: b w h q e s t x y for breve, whole to 128th; '?' when it cannot be told */
    int dots;
    int tuplet_actual;   /* time-modification actual-notes; 0 when the note is not in a tuplet */
    int tuplet_normal;   /* time-modification normal-notes */
    bool tie_start;      /* ties to the next note */
    char xml_id[64];     /* the note's xml:id; empty when it has none */
    size_t order;        /* position in the file among the measure's notes */
} ScoreTextEvent;

typedef struct {
    int part;   /* 1-based, in document order */
    int staff;  /* 1-based; 1 in a part that names no <clef number> */
    bool bass;  /* an F clef on line 4; false for a G clef on line 2 (treble). Other clefs are not read. */
} ScoreTextClef;

typedef struct {
    char number[24];  /* <measure number> as written */
    bool pickup;      /* a first measure shorter than its time signature */
    TextRational start;
    TextRational length;
    int beats;        /* time signature in force */
    int beat_type;
    bool time_changed; /* a <time> that differs from the one before sits here (always on the first measure) */
    ScoreTextClef *clefs; /* the <clef> elements of the measure's <attributes>, by part then document order; on the
                             first measure these are the opening clefs, on a later one each is a change */
    size_t clef_count;
    ScoreTextEvent *events; /* ordered by part, staff, voice, onset, file order */
    size_t event_count;
} ScoreTextMeasure;

typedef struct {
    ScoreTextMeasure *measures;
    size_t measure_count;
    size_t part_count;
} ScoreTextScore;

/*
 * Reads musicxml. Returns the score the caller frees with score_text_destroy, or NULL with *error (when error is not
 * NULL) set to a malloc'd message the caller frees. NULL for a NULL musicxml.
 */
ScoreTextScore *score_text_read(const char *musicxml, char **error);

/* Frees a score. NULL is allowed. */
void score_text_destroy(ScoreTextScore *score);

#endif
