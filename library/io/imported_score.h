#ifndef URSATZ_IO_IMPORTED_SCORE_H
#define URSATZ_IO_IMPORTED_SCORE_H

#include <stdbool.h>
#include <stddef.h>

#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "model/core/rational.h"
#include "model/events/note_event.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/time/meter_map.h"
#include "model/time/tempo_map.h"

/*
 * The imported score (R-614): what a MIDI or MusicXML document says, read once into the model so that every later
 * reader (the Analyzer, the analysis chain, the Composer's hearing) is independent of the format.
 */

typedef enum {
    IMPORTED_FORMAT_AUTO = 0, /* choose by extension, then by content */
    IMPORTED_FORMAT_MIDI,
    IMPORTED_FORMAT_MUSICXML
} ImportedFormat;

/* A tempo marking of a notated score: its words and/or metronome mark, where it sits and the part it came from. */
typedef struct {
    char *text;           /* owned; NULL when the marking has only a metronome mark */
    bool has_metronome;
    Rational beat_unit;   /* the metronome's beat unit in whole notes, and its beats per minute */
    Rational bpm;
    Rational position;    /* where it sits, in whole notes from the start */
    size_t part;          /* index into ImportedSource.parts */
} ImportedTempoMarking;

/* A dynamics marking of a notated score: it holds from its position to the next one in its part. */
typedef struct {
    char *marking;        /* owned; the importer's text: "p", "mf", or an other-dynamics text */
    Rational start;       /* the span it holds over, in whole notes */
    Rational end;
    size_t part;          /* index into ImportedSource.parts */
    int staff;
} ImportedDynamic;

/* One part of a notated score. */
typedef struct {
    char *id;   /* owned; the score-part id */
    char *name; /* owned; the part name, "" when it has none */
    int staff_count;
    int midi_program; /* General MIDI program, 0-based; -1 when unknown (a MusicXML part, the percussion channel) */
} ImportedPart;

/*
 * A reading: how the notes of an input are to be read as notation, when the file's own meter or note values are not
 * the ones a score would write. It is applied to the imported score before analysis (imported_reading_apply); nothing
 * applies one by itself.
 *
 * has_meter: beats/beat_type is the meter to read the piece in (replacing the file's). has_pickup: the first measure
 * is a pickup of `pickup` whole notes, counted after scaling and shorter than one measure of the meter. scale: every
 * start and duration is multiplied by it; 1 leaves the notes unchanged. The allowed scales are 1/4, 1/2, 1, 2 and 4.
 */
typedef struct {
    bool has_meter;
    int beats;
    int beat_type;
    bool has_pickup;
    Rational pickup;
    Rational scale;
} ImportedReading;

/*
 * What the input says about itself, beyond the notes. For a MIDI file everything here is false/zero/NULL except
 * format and the parts (below).
 *
 * format is IMPORTED_FORMAT_MIDI or IMPORTED_FORMAT_MUSICXML, never AUTO. meter_notated and key_notated say the time
 * signatures and key signatures in ImportedScore are the notation's own (MusicXML <time> and <key>, with a defaulted
 * 4/4 map when the score gives no <time>); a MIDI file's are embedded events, and its key is inferred.
 * spelling_notated says the NoteEvents' PitchSpellings are the score's.
 *
 * parts, and the arrays parallel to ImportedScore.events: part_index[i] is the index into parts of the part note i
 * belongs to, staff[i] its staff number. A note's voice, where the score names one, is its NoteEvent's
 * VoiceReference. All parts are merged into the one texture the analysis reads; none is dropped. A MIDI file has one
 * part per track and channel that has notes, in order of first appearance, except that two keyboard-like streams the
 * library judges to be the hands of one piano (score_staves_pair_hands) are one part: its name from the track name
 * and the General MIDI instrument (the higher stream's for a pair), midi_program its program (0 when the channel
 * never changes it, -1 for the percussion channel). Its staff_count is 2 for a pair and for the keyboard-like
 * programs (score_program_uses_grand_staff), and staff[i] is then score_staves_assign_hinted's result, a paired
 * stream's staff as the hint.
 *
 * tempo_declared is true when the score gives a tempo; initial_quarters_per_minute is then the tempo at the start.
 * tempo_markings (tempo_marking_count of them) and dynamics (dynamics_count) are the markings the input carries, in
 * time order, owned; both are NULL for a MIDI file.
 *
 * warning_count is the number of importer warnings the input raised (0 for MIDI). warnings holds the lines to print
 * for the import's warnings, warning_line_count of them, owned, each the full text without a line end: a MusicXML
 * import gives the importer's messages and, when it kept fewer than it raised, one "and N more" line; a MIDI file
 * gives one line for each time signature conflict. The library prints nothing: the caller shows them.
 *
 * reading_applied is true when a reading (ImportedReading) was applied to the notes, and reading is that reading;
 * otherwise reading is zeroed.
 */
typedef struct {
    ImportedFormat format;
    bool meter_notated;
    bool key_notated;
    bool spelling_notated;
    ImportedPart *parts;
    size_t part_count;
    size_t *part_index;
    int *staff;
    bool tempo_declared;
    double initial_quarters_per_minute;
    size_t tempo_marking_count;
    size_t dynamics_count;
    size_t warning_count;
    char **warnings;
    size_t warning_line_count;
    ImportedTempoMarking *tempo_markings;
    ImportedDynamic *dynamics;
    bool reading_applied;
    ImportedReading reading;
} ImportedSource;

/*
 * events and pitch_values are index-paired, count entries each: for every i, pitch_values[i] is the numeric pitch the
 * importer computed for events[i] (see io/midi_importer.h). The score owns both arrays and every object in them;
 * release with imported_score_destroy. A score built by imported_score_view borrows its events and may borrow its
 * meter map (events_borrowed, meter_map_borrowed); imported_score_destroy then frees only what the view built.
 *
 * key_signature is the file's first Key Signature meta-event, if any (see io/midi_key_signature.h);
 * key_signature.found is false when the file carries none. key_signature_events is every Key Signature meta-event
 * with its position, in time order (count 0 when the file carries none). A malformed key signature meta-event does
 * not fail the import.
 *
 * meter is the file's time signatures as a MeterMap, plus the end time of the last note (see io/midi_importer.h). A
 * file with no time signature event gets a single 4/4 region marked defaulted (meter_map_is_defaulted), so
 * meter.meter_map is never NULL after a successful import.
 *
 * tempo_map is the file's set-tempo events as a TempoMap (time/tempo_map.h), from the one library parse of the file:
 * a MIDI import always has one (a single defaulted region when the file has no tempo event), a MusicXML import has
 * NULL. Owned; imported_score_destroy releases it. Note times do not depend on it.
 */
typedef struct ImportedScore {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t count;
    size_t *streams; /* owned; count entries, parallel to events: the stream the note belongs to: the MIDI track and
                        channel (not the part: source.part_index joins two paired hands), the MusicXML part; all 0 for
                        a one-part file */
    MidiKeySignature key_signature;
    MidiKeySignatureEvents key_signature_events;
    unsigned ticks_per_quarter; /* SMF header division (PPQN); 0 when unavailable */
    MidiMeterImport meter;
    ImportedSource source;
    TempoMap *tempo_map; /* owned; the file's tempo changes (MIDI); NULL for MusicXML */
    bool events_borrowed;    /* a view: the events belong to the caller */
    bool meter_map_borrowed; /* a view: meter.meter_map belongs to the caller */
} ImportedScore;

void imported_score_destroy(ImportedScore *score);

typedef enum {
    IMPORTED_SCORE_OK = 0,
    IMPORTED_SCORE_ERR_ARGUMENT,       /* a NULL argument, empty input or an invalid view */
    IMPORTED_SCORE_ERR_IO,             /* the file cannot be read */
    IMPORTED_SCORE_ERR_UNKNOWN_FORMAT, /* IMPORTED_FORMAT_AUTO and the bytes say nothing */
    IMPORTED_SCORE_ERR_FORMAT,         /* the document is not a readable MIDI file or MusicXML score */
    IMPORTED_SCORE_ERR_COMPRESSED,     /* a compressed MusicXML (.mxl), which the importer does not read */
    IMPORTED_SCORE_ERR_EMPTY_SCORE,    /* a score with no notes */
    IMPORTED_SCORE_ERR_ALLOC
} ImportedScoreResult;

/* The message of a failed import: the whole line the caller shows, without a line end ("Error: ..."). */
typedef struct {
    char message[256];
} ImportedScoreError;

/*
 * Parses the --input-format value ("midi" or "musicxml"). Returns false for anything else and leaves *out untouched.
 */
bool imported_format_parse(const char *name, ImportedFormat *out);

/*
 * The concrete format of the file at path: by extension (.mid, .midi; .musicxml, .xml, .mxl), and when the extension
 * says nothing, by the file's first bytes (an "MThd" header is MIDI; a zip signature or an XML document is MusicXML).
 * IMPORTED_FORMAT_AUTO is returned when neither identifies the file, and also when the file cannot be read.
 */
ImportedFormat imported_format_detect(const char *path);

/*
 * Reads the file at path in the given format (IMPORTED_FORMAT_AUTO detects it) and fills *out with the same
 * ImportedScore whatever the input. On IMPORTED_SCORE_OK, *out is owned by the caller (imported_score_destroy). On
 * any other result *out is left untouched and error (when not NULL) holds the message. Nothing is printed and
 * nothing is written to disk. The warnings are in out->source.warnings.
 */
ImportedScoreResult imported_score_import_file(const char *path, ImportedFormat format, ImportedScore *out,
                                               ImportedScoreError *error);

/*
 * Reads a MIDI or MusicXML document held in memory (IMPORTED_FORMAT_AUTO detects the format from the first bytes, as
 * imported_format_detect does for a file). The result is the same as for the file entry; no temporary file is made.
 * A NULL pointer or empty input is IMPORTED_SCORE_ERR_ARGUMENT.
 */
ImportedScoreResult imported_score_import_buffer(const unsigned char *bytes, size_t size, ImportedFormat format,
                                                 ImportedScore *out, ImportedScoreError *error);

/*
 * A view: the score the Composer's fragments and the hearing entries read, built from events somebody else owns.
 * events has count entries; midi has count parallel MIDI note numbers 0 to 127 (the library has no call that reads a
 * MIDI number from an event's pitch, a pitch identity is opaque by design), so midi is required for count > 0.
 * meter is the meter map (NULL or a defaulted one is 4/4, with a map the view owns). has_key gives a key signature
 * event at time 0 from key_fifths (-7 to 7) and key_minor. streams, when not NULL, has count entries: the stream
 * (voice line or part) each event belongs to; NULL puts every event in stream 0. streams is read, not kept.
 *
 * The events and a non-defaulted meter map are borrowed and must outlive the score; none of it is changed. The format
 * is MIDI, with no parts and no markings. IMPORTED_SCORE_ERR_ARGUMENT for a NULL argument, a NULL event, a MIDI
 * number outside 0 to 127, a key signature outside -7 to 7 or no midi for count > 0.
 */
typedef struct {
    const NoteEvent *const *events;
    size_t count;
    const int *midi;
    const MeterMap *meter;
    bool has_key;
    int key_fifths;
    bool key_minor;
    const size_t *streams;
} ImportedScoreView;

ImportedScoreResult imported_score_view(const ImportedScoreView *view, ImportedScore *out);

/* The reading that changes nothing: no meter, no pickup, scale 1. */
ImportedReading imported_reading_none(void);

/*
 * Each parser reads the whole text ("3/4", "2", "1/2", "1/4") and on success sets only its own fields of *r,
 * returning true. On false *r is untouched. The meter's beat type must be 1, 2, 4, 8 or 16 and its beats 1 to 64; the
 * scale one of the allowed scales; the pickup a positive fraction.
 */
bool imported_reading_parse_meter(const char *text, ImportedReading *r);
bool imported_reading_parse_scale(const char *text, ImportedReading *r);
bool imported_reading_parse_pickup(const char *text, ImportedReading *r);

typedef enum {
    IMPORTED_READING_OK = 0,
    IMPORTED_READING_ERR_NULL_ARGUMENT,
    IMPORTED_READING_ERR_INVALID, /* a scale that is not allowed, a meter out of range, a pickup not shorter than a measure */
    IMPORTED_READING_ERR_OVERFLOW, /* a scaled time does not fit */
    IMPORTED_READING_ERR_ALLOC
} ImportedReadingResult;

/*
 * Applies reading to the score in place. Every NoteEvent is rebuilt with its start and duration times the scale,
 * keeping its EntityID, pitch, spelling, voice and provenance, so note ids (and every list of them) stay valid;
 * pitch_values, streams and the parts are untouched. The key signature events, meter.end_time, the tempo markings and
 * the dynamics are scaled with the notes.
 *
 * Meter: with has_meter, meter.meter_map becomes one region of the reading's meter, preceded by a pickup region when
 * has_pickup. Without it the existing map is kept with its change points scaled; with has_pickup alone the pickup is
 * laid before the map's first full measure's signature, and its later changes are dropped. The reading is recorded in
 * score->source (reading_applied, reading).
 *
 * On any result other than IMPORTED_READING_OK the score is unchanged. A view's score cannot take a reading
 * (IMPORTED_READING_ERR_INVALID).
 */
ImportedReadingResult imported_reading_apply(ImportedScore *score, const ImportedReading *reading);

#endif
