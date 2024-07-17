#ifndef URSATZ_COMPOSER_MOTIVE_H
#define URSATZ_COMPOSER_MOTIVE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

/*
 * A motive: a short pitch-and-rhythm cell, typically one bar or less, supplied
 * by the user. It enters the plan as Given material on a motive node (see
 * motive_attach) and later operators never change it.
 *
 * A motive is a plain value. Its notes are one voice on a timeline that starts
 * at 0, the start of a bar: a note's offset is its onset in whole notes, a gap
 * between two notes is a rest, and a first note that does not start at 0 means
 * the motive opens with a rest. Notes never overlap. The motive ends where its
 * last note ends, so a rest at the end is not part of it. A tie is not kept: a
 * tied pair is one longer note.
 *
 * Loading:
 *   - MIDI and MusicXML go through Ursatz's importers (midi_importer.h and
 *     musicxml_importer.h). The motive is the one voice the file holds. A file
 *     with notes sounding together, or in more than one MusicXML part, is
 *     MOTIVE_ERR_POLYPHONIC: no voice is chosen for the user and none is
 *     dropped.
 *   - A note list is compact text, see motive_from_notes_text.
 *   - JSON uses the plan format's vocabulary, see motive_from_json.
 *
 * Meter. A file that declares a meter supplies it, and a meter in the options
 * that differs is MOTIVE_ERR_METER, since it would change where the bars fall.
 * A file with no meter is 4/4 as Ursatz reads it, unless the options give one.
 * A note list has no meter of its own and needs one in the options; JSON may
 * carry one.
 *
 * Key. The options come first, then a key the input states (a file's key
 * signature with a major or minor mode, or a JSON "key"), both marked
 * MOTIVE_KEY_GIVEN. Otherwise the key is inferred from the pitches with
 * Ursatz's common-practice key identification, which needs all seven degrees of
 * a major or natural minor scale, and is marked MOTIVE_KEY_INFERRED. When the
 * pitches do not settle on one key it stays MOTIVE_KEY_UNSPECIFIED.
 */

#define MOTIVE_MAX_NOTES 64
#define MOTIVE_ERROR_LOCATION_MAX 128
#define MOTIVE_ERROR_MESSAGE_MAX 160

typedef enum {
    MOTIVE_OK = 0,
    MOTIVE_ERR_NULL,
    MOTIVE_ERR_ALLOC,
    MOTIVE_ERR_IO,            /* the file could not be read */
    MOTIVE_ERR_FORMAT,        /* an unknown format name */
    MOTIVE_ERR_SYNTAX,        /* text that is not a note list or not JSON */
    MOTIVE_ERR_VALUE,         /* a value that is not allowed: a pitch, a duration, an overlap, an unknown name */
    MOTIVE_ERR_EMPTY,         /* no notes */
    MOTIVE_ERR_TOO_MANY_NOTES,
    MOTIVE_ERR_POLYPHONIC,    /* notes sounding together, or more than one part */
    MOTIVE_ERR_METER,         /* no meter, or a meter that conflicts with the file's */
    MOTIVE_ERR_IMPORT         /* the Ursatz importer refused the file */
} MotiveResult;

typedef enum {
    MOTIVE_KEY_UNSPECIFIED,
    MOTIVE_KEY_GIVEN,
    MOTIVE_KEY_INFERRED
} MotiveKeySource;

typedef struct {
    PlanMeter meter;
    /* Valid when key_source is not MOTIVE_KEY_UNSPECIFIED. */
    PlanKey key;
    MotiveKeySource key_source;
    PlanNote notes[MOTIVE_MAX_NOTES];
    size_t note_count;
    /* Where the last note ends. The span of the motive is this long. */
    PlanTime length;
} Motive;

/* What the caller says about the motive. NULL options mean nothing. */
typedef struct {
    bool has_meter;
    PlanMeter meter;
    bool has_key;
    PlanKey key;
    /* The most bars the notes may fill, or 0 for no limit. The readers themselves set none: a line of any length up to
       MOTIVE_MAX_NOTES notes is read, and the commands that take a one-bar motive check it after. */
    size_t max_bars;
} MotiveOptions;

/* Filled on failure. Pass NULL when the details are not wanted. */
typedef struct {
    /* Where the problem is: "token 3 (column 12)", "notes[2].duration.den", "at 3/8 (note 4)". Empty for the input as a whole. */
    char location[MOTIVE_ERROR_LOCATION_MAX];
    char message[MOTIVE_ERROR_MESSAGE_MAX];
} MotiveError;

typedef enum {
    MOTIVE_FORMAT_AUTO,
    MOTIVE_FORMAT_MIDI,
    MOTIVE_FORMAT_MUSICXML,
    MOTIVE_FORMAT_NOTES,
    MOTIVE_FORMAT_JSON
} MotiveFormat;

/* "ok", "polyphonic" and so on. */
const char *motive_result_name(MotiveResult result);

/* Reads "auto", "midi", "musicxml", "notes" or "json". False for any other text. */
bool motive_format_parse(const char *name, MotiveFormat *out);
/* "midi", "musicxml", "notes", "json" or "auto". */
const char *motive_format_name(MotiveFormat format);

/*
 * What the bytes look like, never MOTIVE_FORMAT_AUTO: "MThd" is MIDI, a first
 * character of '<' (or a UTF-16 byte order mark) is MusicXML, '{' is JSON, and
 * anything else is a note list. A UTF-8 byte order mark and white space before
 * the first character are skipped.
 */
MotiveFormat motive_detect_format(const void *data, size_t size);

/* "D minor", "Bb major", "F# major": a tonic letter A-G, an optional # or b, then major or minor. */
bool motive_parse_key(const char *text, PlanKey *out);
/* "3/4", "6/8": beats per bar from 1 to 64 over a beat unit of 1, 2, 4, 8 or 16. */
bool motive_parse_meter(const char *text, PlanMeter *out);

/*
 * A note list, as text:
 *
 *   D4:q F4:e A4:e D5:h
 *   r:e Bb3:e.~ Bb3:s C#4:q | D4:w
 *
 * Tokens are separated by white space. A token is a pitch, a colon and a
 * duration, with an optional tie mark at the end:
 *   - pitch: a letter A-G, up to two accidentals (# or b), and an octave, as
 *     in C4 (middle C, MIDI 60), F#3, Bb5 or C-1. "r" is a rest and has no
 *     octave.
 *   - duration: w (whole), h (half), q (quarter), e (eighth), s (sixteenth) or
 *     t (thirty-second), then up to two dots.
 *   - "~" ties the note to the next token, which must be the same pitch. The
 *     two are one note as long as both.
 * A "|" stands for a barline and is ignored. Notes follow one another from time
 * 0; there is no way to sound two at once. The text must end on a note.
 *
 * The meter comes from the options (a note list has none of its own).
 */
MotiveResult motive_from_notes_text(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error);

/*
 * JSON, in the plan format's vocabulary (docs/plan-format.md, "Motive documents"):
 *
 *   {"meter": {"beats_per_bar": 4, "beat_unit": 4},
 *    "key": {"tonic_pitch_class": 2, "mode": "minor"},
 *    "notes": [{"midi_pitch": 62, "offset": {"num": 0, "den": 1}, "duration": {"num": 1, "den": 4}}]}
 *
 * "meter" and "key" are optional. Unknown fields are ignored.
 */
MotiveResult motive_from_json(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error);

/* A Standard MIDI File (type 0 or 1) or an uncompressed score-partwise MusicXML document. */
MotiveResult motive_from_midi(const unsigned char *data, size_t size, const MotiveOptions *options, Motive *out,
                              MotiveError *error);
MotiveResult motive_from_musicxml(const char *data, size_t size, const MotiveOptions *options, Motive *out,
                                  MotiveError *error);

/* Any of the formats; MOTIVE_FORMAT_AUTO uses motive_detect_format. */
MotiveResult motive_load(const void *data, size_t size, MotiveFormat format, const MotiveOptions *options, Motive *out,
                         MotiveError *error);
MotiveResult motive_load_file(const char *path, MotiveFormat format, const MotiveOptions *options, Motive *out,
                              MotiveError *error);

/* The same meter, key and notes. */
bool motive_equal(const Motive *a, const Motive *b);

/* The motive as JSON in the form motive_from_json reads, 2-space indent, trailing newline. NULL on failure; caller frees. */
char *motive_to_json(const Motive *motive);

/*
 * Attaches the motive to the plan as Given material: a motive node under
 * parent (a subphrase or an idea) whose span starts at start and is as long as
 * the motive, with a melody lane span of the motive's notes. Both the node and
 * the span are Given. The meter and key are not written to the plan's lanes;
 * the plan's own meter and key lanes say what the music is in.
 *
 * Nothing is added on failure. Errors are the plan's: PLAN_ERR_NOT_FOUND for an
 * unknown parent, PLAN_ERR_BAD_KIND when parent cannot hold a motive,
 * PLAN_ERR_BAD_SPAN when the span is not inside the parent's,
 * PLAN_ERR_LANE_OVERLAP when the melody lane is taken there, and
 * PLAN_ERR_BAD_PAYLOAD for a motive that is not valid. name may be NULL.
 * out_node and out_span may be NULL.
 */
PlanResult motive_attach(PiecePlan *plan, NodeId parent, PlanTime start, const Motive *motive, const char *name,
                         NodeId *out_node, LaneSpanId *out_span);

#endif
