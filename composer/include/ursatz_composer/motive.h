#ifndef URSATZ_COMPOSER_MOTIVE_H
#define URSATZ_COMPOSER_MOTIVE_H

#include <stdbool.h>
#include <stddef.h>

#include <analysis/motive_tone_roles.h>

#include <ursatz_composer/constraint.h>
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
#define MOTIVE_MAX_CHORDS 8
#define MOTIVE_MAX_FEATURES 8
#define MOTIVE_FEATURE_LABEL_MAX 8

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

/*
 * What a motive holds. FULL is pitches and rhythm. RHYTHM keeps only the durations and onsets (every note sounds on
 * the same pitch). CONTOUR keeps only the heights of the notes in relation to each other, in even quarter notes.
 */
typedef enum {
    MOTIVE_FORM_FULL = 0,
    MOTIVE_FORM_RHYTHM,
    MOTIVE_FORM_CONTOUR
} MotiveForm;

/*
 * A chord of a motive's own harmony, in the motive's key: degree 1 to 7 of the scale, alteration -1 (flat), 0 or 1
 * (sharp), inversion 0 to 3 (3 needs a seventh), and the onset in whole notes from the start of the motive.
 */
typedef struct {
    Rational onset;
    int degree;
    int alteration;
    int inversion;
    bool seventh;
} MotiveChord;

/* A labelled run of the motive's notes: first and last are 0-based note indices, inclusive. */
typedef struct {
    char label[MOTIVE_FEATURE_LABEL_MAX + 1];
    size_t first;
    size_t last;
} MotiveFeature;

/* What part a motive plays: the tune, or what goes under it. */
typedef enum {
    MOTIVE_ROLE_MELODIC = 0,
    MOTIVE_ROLE_ACCOMPANIMENT
} MotiveRole;

typedef struct {
    PlanMeter meter;
    /* Valid when key_source is not MOTIVE_KEY_UNSPECIFIED. */
    PlanKey key;
    MotiveKeySource key_source;
    PlanNote notes[MOTIVE_MAX_NOTES];
    size_t note_count;
    /* Where the last note ends. The span of the motive is this long. */
    Rational length;
    /* Zero-initialized means a full motive with no pickup. */
    MotiveForm form;
    /* How much of the meter's bar comes before the first barline, in whole notes. The motive still starts at offset 0;
       bars and beats are counted from -pickup. */
    Rational pickup;
    /*
     * Its own harmony: up to MOTIVE_MAX_CHORDS chords with ascending onsets inside the length. Zero chords means none,
     * and wherever a relation needs chords the best implied chord at 0 stands in.
     */
    MotiveChord harmony[MOTIVE_MAX_CHORDS];
    size_t harmony_count;
    /* Labelled features, distinct labels. Zero-initialized means none. */
    MotiveFeature features[MOTIVE_MAX_FEATURES];
    size_t feature_count;
    /* Zero-initialized means melodic. */
    MotiveRole role;
    /*
     * The caller's own tone labels (0-based note indices, the later of two on one note wins), which replace the labels the
     * motive's base harmony gives its notes when it is attached to a plan (motive_tones.h). Zero means none.
     */
    MotiveToneOverride tone_overrides[MOTIVE_MAX_NOTES];
    size_t tone_override_count;
} Motive;

/* What the caller says about the motive. NULL options mean nothing. */
typedef struct {
    bool has_meter;
    PlanMeter meter;
    bool has_key;
    PlanKey key;
    bool has_pickup;
    /* In whole notes; the CLI converts beats. */
    Rational pickup;
    /* The most bars the notes may fill, or 0 for no limit. The readers themselves set none: a line of any length up to
       MOTIVE_MAX_NOTES notes is read, and the commands that take a one-bar motive check it after. */
    size_t max_bars;
    /* Own harmony, features and role; each replaces what the input says when its has_ flag is set. The chords are
       checked against the length, the features against the note count. */
    bool has_harmony;
    MotiveChord harmony[MOTIVE_MAX_CHORDS];
    size_t harmony_count;
    bool has_features;
    MotiveFeature features[MOTIVE_MAX_FEATURES];
    size_t feature_count;
    bool has_role;
    MotiveRole role;
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
    MOTIVE_FORMAT_JSON,
    /* The last three are never chosen by motive_detect_format: they have to be asked for. */
    MOTIVE_FORMAT_DEGREES,
    MOTIVE_FORMAT_RHYTHM,
    MOTIVE_FORMAT_CONTOUR
} MotiveFormat;

/* "ok", "polyphonic" and so on. */
const char *motive_result_name(MotiveResult result);

/* Reads "auto", "midi", "musicxml", "notes", "json", "degrees", "rhythm" or "contour". False for any other text. */
bool motive_format_parse(const char *name, MotiveFormat *out);
/* "midi", "musicxml", "notes", "json", "degrees", "rhythm", "contour" or "auto". */
const char *motive_format_name(MotiveFormat format);

/* Reads "full", "rhythm" or "contour". False for any other text. */
bool motive_form_parse(const char *name, MotiveForm *out);
/* "full", "rhythm" or "contour". */
const char *motive_form_name(MotiveForm form);

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
 * A degree list, as text: the same as a note list, but each pitch is a degree of the key's scale (natural minor in a
 * minor key):
 *
 *   1:q b3:e 5:e 1':h
 *
 * A pitch is an optional # or b, a degree 1 to 7, then octave marks, any number: ' up an octave, , down. Octave 0 is
 * the octave whose degree 1 is the tonic's pitch class in MIDI 60 to 71. "r:<duration>" is a rest, "~" ties and "|" is
 * ignored as in a note list. The key comes from the options; without one the pitches are C major's and the key stays
 * as the pitches settle it.
 */
MotiveResult motive_from_degrees_text(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error);

/*
 * A rhythm, as text: a note list with no pitches.
 *
 *   x:q x:e x:e x:h
 *
 * Each note is "x" and a duration; "r" rests, "~" ties, dots and "|" are as in a note list. The motive is read with
 * form MOTIVE_FORM_RHYTHM and sounds every note on degree 1 of the key (C major's when the options give none).
 */
MotiveResult motive_from_rhythm_text(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error);

/*
 * A contour, as text: ranks separated by white space, 0 the lowest, equal ranks the same height, no durations.
 *
 *   0 2 1 3
 *
 * The motive is read with form MOTIVE_FORM_CONTOUR and sounds rank r on degree r + 1 of the key (C major's when the
 * options give none), every note a quarter.
 */
MotiveResult motive_from_contour_text(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error);

/*
 * JSON, in the plan format's vocabulary (docs/plan-format.md, "Motive documents"):
 *
 *   {"meter": {"beats_per_bar": 4, "beat_unit": 4},
 *    "key": {"tonic_pitch_class": 2, "mode": "minor"},
 *    "notes": [{"midi_pitch": 62, "offset": {"num": 0, "den": 1}, "duration": {"num": 1, "den": 4}}]}
 *
 * "meter" and "key" are optional. Unknown fields are ignored. Own harmony, features and the role are optional too:
 *
 *   "harmony": "I V6@2 I@3",
 *   "features": [{"label": "a", "first": 0, "last": 2}],
 *   "role": "accompaniment"
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

/* The same meter, key, notes, harmony, features and role. */
bool motive_equal(const Motive *a, const Motive *b);

/*
 * The motive as text, each read back by its reader (motive_from_notes_text and so on) into the same notes. A rest
 * between notes, or before the first, is written as rests; a note whose length is not a plain or dotted value is
 * written as tied pieces. NULL on failure, and for a value that is not a sum of w to t; caller frees.
 *   notes:   "D4:q F4:e A4:e D5:h"
 *   degrees: "1:q b3:e 5:e 1':h", a raised degree as "#"; NULL when the motive has no key
 *   rhythm:  "x:q x:e x:e x:h"
 *   contour: "0 2 1 3", the rank of each note among the motive's distinct pitches
 */
char *motive_to_notes_text(const Motive *motive);
char *motive_to_degrees_text(const Motive *motive);
char *motive_to_rhythm_text(const Motive *motive);
char *motive_to_contour_text(const Motive *motive);

/*
 * The same degrees, rhythm and pickup in another key: each note keeps its step of the source key's scale and its
 * alteration, and sounds on that step of the new key's scale with degree 1 in octave 0 (MIDI 60 to 71 for the tonic's
 * pitch class). The result is marked MOTIVE_KEY_GIVEN. The mode may differ; the steps are counted in the new mode's
 * scale. Rhythm and contour forms sound as their readers place them (every note on degree 1; rank r on degree r + 1).
 * False, with *out untouched, for a source with no key, a note that leaves MIDI 0 to 127, or NULL arguments.
 * out may be the source.
 */
bool motive_in_key(const Motive *source, const PlanKey *key, Motive *out);

/*
 * Own harmony as text: chords separated by white space, each a Roman numeral and an optional onset.
 *
 *   I V6@2 I@3
 *
 * A chord is an optional b or # (a lowered or raised root), an upper case numeral I to VII, a figure (none for a triad
 * in root position, 6, 64; 7, 65, 43 or 42 for a seventh chord) and "@" with the onset in beats of the meter's beat
 * unit from the start of the motive, a whole number or a fraction as in @1/2. The first chord needs no onset (it is at
 * 0); the others do, and the onsets rise. At most MOTIVE_MAX_CHORDS chords; text with no chord is zero chords.
 * Whether the chords fall inside the motive is checked when the motive is read.
 */
MotiveResult motive_harmony_parse(const char *text, const PlanMeter *meter, MotiveChord *out, size_t *count,
                                  MotiveError *error);
/* The most characters of a chord's numeral with its NUL ("bVII65", "#IV43"). */
#define MOTIVE_CHORD_NUMERAL_MAX 16

/* A chord as the numeral motive_harmony_parse reads, without its onset ("V65", "bVII7"). False for a chord that has none. */
bool motive_chord_numeral(const MotiveChord *chord, char *out, size_t size);

/* The motive's own harmony as text, in the form motive_harmony_parse reads ("" when it has none); NULL on failure. */
char *motive_harmony_to_text(const Motive *motive);

/* A feature as "label:first-last" or "label:index", as in "a:0-2". The label is 1 to 8 characters. */
MotiveResult motive_feature_parse(const char *text, MotiveFeature *out, MotiveError *error);

/* Reads "melodic" or "accompaniment". False for any other text. */
bool motive_role_parse(const char *name, MotiveRole *out);
/* "melodic" or "accompaniment". */
const char *motive_role_name(MotiveRole role);

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
PlanResult motive_attach(PiecePlan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                         NodeId *out_node, LaneSpanId *out_span);

/*
 * motive_attach with a SOURCE constraint (constraint.h) on the motive node: where the motive was taken from in a
 * corpus (corpus_seed.h). It is added before the node becomes Given, which is the only time it can be. source may be
 * NULL, which is motive_attach.
 */
PlanResult motive_attach_sourced(PiecePlan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                                 const PlanSource *source, NodeId *out_node, LaneSpanId *out_span);

#endif
