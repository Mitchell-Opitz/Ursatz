#ifndef URSATZ_IO_SCORE_LAYOUT_H
#define URSATZ_IO_SCORE_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>

#include "io/musicxml_export.h"

/*
 * The layout of a score as the MusicXML writer prints it (R-621): the measures of the meter map, and in each the notes and rests of
 * every part, cut at the barlines and tied across them, with their notated values, tuplets and clefs. musicxml_export_write_score
 * builds this layout and prints it, so the layout is exactly what the document holds; a caller that wants the written score reads
 * the layout instead of reading the document back (the Analyzer's text export did, from the XML).
 *
 * All times are ticks: 1/(4 * divisions) of a whole note, from the start of the piece, so a note is at `start / (4 * divisions)` whole
 * notes. Events are listed measure by measure, part by part, and within a part voice by voice in the order the document writes
 * them (the first voice of a part from the start of the measure, the others after a <backup>). A chord is several events that
 * share the first one's start and duration, the later ones with `chord` set. The directions (tempo, dynamics, pedals, key signature)
 * stay in the document and are not part of the layout.
 */

typedef struct {
    size_t part;        /* 0-based, in the order of the score's parts */
    int staff;          /* 1-based; 1 in a part of one staff */
    int voice;          /* 1-based within the part, as <voice> */
    long long start;    /* ticks from the start of the piece */
    long long duration; /* ticks */
    bool rest;
    bool measure_rest;  /* a rest that fills its measure, <rest measure="yes"/>; it has no notated value */
    bool chord;         /* written with <chord/>: it shares the previous event's start and duration */
    char step;          /* 'A' to 'G'; a note only */
    int alter;          /* semitones: -1 flat, 1 sharp; a note only */
    int octave;         /* scientific pitch notation: middle C is C4; a note only */
    int value;          /* the notated value: 1 whole, 2 half, 3 quarter, 4 eighth, 5 16th, 6 32nd, 7 64th, 8 128th; 0 for none */
    int dots;           /* 0 to 2 */
    unsigned tuplet_actual; /* <time-modification>: actual notes in the time of normal; 0 and 0 for none */
    unsigned tuplet_normal;
    int tuplet_mark;    /* 1 where a <tuplet> group opens, 2 where it closes, 0 otherwise */
    bool tie_start;     /* tied to the next piece */
    bool tie_stop;      /* tied from the previous piece */
    char xml_id[96];    /* the note's xml:id as written; empty for a rest */
} ScoreLayoutEvent;

/* The clef of one staff in force from a measure: a G clef on line 2 (treble) or an F clef on line 4 (bass). */
typedef struct {
    size_t part;
    int staff;
    bool bass;
} ScoreLayoutClef;

typedef struct {
    int number;            /* the measure number as written */
    bool implicit;         /* the first measure of a score with a pickup */
    long long start;       /* ticks from the start of the piece */
    long long end;
    int beats;             /* the time signature in force: its top number and its beat type */
    int beat_type;
    bool time_written;     /* a region of the meter map starts here, so the document writes a <time> */
    ScoreLayoutClef *clefs; /* the clefs written at this measure: the opening ones in the first, a change in a later one */
    size_t clef_count;
    ScoreLayoutEvent *events;
    size_t event_count;
} ScoreLayoutMeasure;

struct ScoreLayoutPrepared;

typedef struct {
    long long divisions;   /* <divisions>: ticks in a quarter note */
    size_t part_count;
    bool meter_defaulted;  /* the meter map is a default 4/4 nobody wrote */
    ScoreLayoutMeasure *measures;
    size_t measure_count;
    struct ScoreLayoutPrepared *prepared; /* what the writer prints from; private */
} ScoreLayout;

/*
 * Lays the score out. *out is filled only on MUSICXML_EXPORT_OK and released with score_layout_destroy; the layout does not
 * point into the score except through the writer's private state, so the score must outlive it. The results are those of
 * musicxml_export_write_score.
 */
MusicXmlExportResult score_layout_build(const MusicXmlExportScore *score, ScoreLayout *out);

/* Frees the layout and zeroes it. Safe on a zeroed layout and on NULL. */
void score_layout_destroy(ScoreLayout *layout);

#endif
