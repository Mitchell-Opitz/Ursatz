#ifndef URSATZ_IO_MUSICXML_SCORE_INTERNAL_H
#define URSATZ_IO_MUSICXML_SCORE_INTERNAL_H

/* What musicxml_score.c (preparation), musicxml_score_segments.c (segments, tuplets and beams) and musicxml_score_write.c (the writer
   and the layout build) share: private to io, not staged. */

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "model/events/note_event.h"
#include "io/musicxml_export.h"
#include "io/score_layout.h"
#include "io/score_clefs.h"
#include "io/score_quantize.h"
#include "model/identity/entity_id.h"
#include "model/pitch/pitch_spelling.h"
#include "model/events/dynamics.h"
#include "model/time/duration.h"
#include "model/core/rational.h"
#include "model/time/metronome_marking.h"
#include "model/time/tempo_map.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_signature.h"
#include "model/time/time_span.h"

/* --- output buffer --- */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    bool failed;
} XmlBuf;

typedef enum {
    TIME_OK = 0,
    TIME_NOT_REPRESENTABLE,
    TIME_DIVISIONS_OVERFLOW
} TimeStatus;

/* --- the prepared score --- */

typedef struct {
    const char *id;
    const char *name;
    int staff_count;
    int midi_program; /* 0 to 127, or -1 for none */
} PartInfo;

typedef struct {
    size_t index;  /* the input index of the event */
    size_t part;
    int staff;
    size_t group;
    long long start; /* ticks */
    long long end;
    long start_n;    /* the exact start and duration, kept until divisions are final */
    unsigned long start_d;
    long dur_n;
    unsigned long dur_d;
    unsigned actual; /* the notated tuplet of the event: actual notes in the time of normal, 0 and 0 for none */
    unsigned normal;
} NoteRec;

/* Something written between notes at a position: a key signature now, markings next. Sorted by part, tick, kind. */
typedef enum {
    DIR_KEY = 0,
    DIR_TEMPO,   /* source: the TempoMap region index */
    DIR_MARKING, /* source: the TempoMarking index */
    DIR_DYNAMIC, /* source: the Dynamics index */
    DIR_PEDAL_STOP,  /* source: the MusicXmlExportPedal index; a stop sorts before a start at one tick */
    DIR_PEDAL_START
} DirKind;

typedef struct {
    DirKind kind;
    size_t part;
    int staff;
    long raw_n; /* the exact position, kept until divisions are final */
    unsigned long raw_d;
    long long tick;
    size_t source; /* the index into the input array of its kind */
    bool written;  /* a pedal stop already written at the end of the last measure */
} Dir;

/* One voice of one staff of one part: the notes that share a VoiceReference (or have none). */
typedef struct {
    size_t part;
    int staff;
    const char *voice; /* the VoiceReference's target id, NULL for none */
    int voice_number;  /* 1-based within the part */
} Group;

typedef struct {
    const MusicXmlExportScore *score;

    PartInfo *parts;
    size_t part_count;

    NoteRec *notes;
    size_t note_count;
    Group *groups;
    size_t group_count;

    MeterMap *own_meter; /* the defaulted map made when none was supplied */
    const MeterMap *meter;
    bool meter_defaulted;

    long long divisions;
    long long extent; /* ticks: the end of the last note */
    size_t measure_count;
    int first_measure_number;
    long long *measure_start; /* ticks, measure_count entries */
    long long *measure_end;
    long long *measure_beat; /* ticks in one beat of the measure: its bottom unit, or three of them in a compound meter */
    bool *measure_four_four; /* a full 4/4 measure, whose second half does not take a note across beat 3 */

    Dir *dirs;
    size_t dir_count;
    size_t *dir_first;     /* part_count + 1 entries: part p's dirs are dirs[dir_first[p]] up to dirs[dir_first[p+1]] */
    long long dir_max_tick; /* -1 when there are no dirs besides pedal stops */
    long long dir_stop_tick; /* the last pedal stop, -1 when there is none: a stop on a barline adds no measure after it */

    long long **cuts;   /* per part: every barline, ascending, where notes and rests are split */
    size_t *cut_count;

    ScoreQuantized quantized; /* the notated times of every event when score->quantize is set, else zeroed */
    ScoreClefChanges clefs;   /* the clef of every staff at measure 1 and each change, from score_clefs_infer */
} Prep;

/* --- segments: the notes and rests of one voice, cut at the part's cuts --- */

#define REST_SEGMENT SIZE_MAX

#define MAX_BEAM_LEVELS 5 /* an eighth has one beam, a 128th five */

enum { BEAM_NONE, BEAM_BEGIN, BEAM_CONTINUE, BEAM_END, BEAM_FORWARD_HOOK, BEAM_BACKWARD_HOOK };

typedef struct {
    long long start;
    long long end;
    size_t note;      /* an index into prep->notes, or REST_SEGMENT */
    int fragment;     /* 0 for the first piece of a note */
    bool tie_start;
    bool tie_stop;
    bool chord;       /* written with <chord/>, sharing the previous note's start and duration */
    bool measure_rest; /* a rest that fills its measure, written as <rest measure="yes"/> */
    int tuplet;        /* 1 opens a triplet group and 2 closes it (with <tuplet>), 0 neither */
    int beam_group;    /* 0 when not beamed, else the number of the beat group in the measure (from 1) */
    int beam_levels;   /* beams a beamed note carries: 1 for an eighth, 2 for a 16th, and so on */
    unsigned char beam[MAX_BEAM_LEVELS]; /* the state of beam level n + 1 on this note: a BEAM_* value */
} Segment;

typedef struct {
    Segment *items;
    size_t count;
    size_t cap;
} SegmentList;

/* A length written as a note: type is 1 (whole) to 8 (128th), dots 0 to 2, tuplet when it is actual/normal of the plain
   value (the inferred triplet is 3:2, a hinted one is the note's own ratio). */
typedef struct {
    int type;
    int dots;
    bool tuplet;
    unsigned actual;
    unsigned normal;
} NotatedValue;

bool musicxml_score_xmlbuf_init(XmlBuf *buf);
void musicxml_score_xmlbuf_free(XmlBuf *buf);
void musicxml_score_xmlbuf_append(XmlBuf *buf, const char *fmt, ...);
void musicxml_score_xmlbuf_append_escaped(XmlBuf *buf, const char *text);
TimeStatus musicxml_score_time_ticks(long n, unsigned long d, long long divisions, long long *out);
void musicxml_score_prep_free(Prep *prep);
bool musicxml_score_beat_unit_name(unsigned long n, unsigned long d, const char **name, int *dots);
bool musicxml_score_ratio_to_decimal(unsigned long long num, unsigned long long den, char *out, size_t size, bool *exact);
MusicXmlExportResult musicxml_score_from_meter_status(MeterMapResult status);
MusicXmlExportResult musicxml_score_prepare(Prep *prep);
bool musicxml_score_segment_push(SegmentList *list, Segment segment);
long long musicxml_score_next_cut(const long long *cuts, size_t count, long long tick);
bool musicxml_score_classify_piece(const Prep *prep, size_t note, long long length, bool allow_triplet, NotatedValue *out);
bool musicxml_score_add_run(const Prep *prep, size_t part, SegmentList *list, long long from, long long to, size_t note, size_t members);
void musicxml_score_segments_free(SegmentList *lists, size_t count);
MusicXmlExportResult musicxml_score_build_segments(const Prep *prep, SegmentList **out);
extern const char *const musicxml_score_PITCH_STEP_NAMES[7];
bool musicxml_score_format_note_xml_id(const char *entity_value, char *out, size_t out_size, bool cut);
void musicxml_score_mark_tuplets(const Prep *prep, SegmentList *list);
void musicxml_score_mark_beams(const Prep *prep, SegmentList *list);
void musicxml_score_write_segment(const Prep *prep, XmlBuf *buf, const Segment *segment, size_t part, int staff, int voice_number);

#endif
