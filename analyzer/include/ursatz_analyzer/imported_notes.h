#ifndef URSATZ_ANALYZER_IMPORTED_NOTES_H
#define URSATZ_ANALYZER_IMPORTED_NOTES_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_numeric_value.h"
#include "time/tempo_map.h"

#include <ursatz_analyzer/notation_reading.h>

typedef enum {
    INPUT_FORMAT_AUTO = 0, /* choose by extension, then by content */
    INPUT_FORMAT_MIDI,
    INPUT_FORMAT_MUSICXML
} InputFormat;

/* A tempo marking of a notated score: its words and/or metronome mark, where it sits and the part it came from. */
typedef struct {
    char *text;                           /* owned; NULL when the marking has only a metronome mark */
    bool has_metronome;
    unsigned long beat_unit_numerator;    /* the metronome's beat unit in whole notes, and its beats per minute */
    unsigned long beat_unit_denominator;
    unsigned long bpm_numerator;
    unsigned long bpm_denominator;
    long position_numerator;              /* where it sits, in whole notes from the start */
    unsigned long position_denominator;
    size_t part;                          /* index into ImportedSource.parts */
} ImportedTempoMarking;

/* A dynamics marking of a notated score: it holds from its position to the next one in its part. */
typedef struct {
    char *marking;                        /* owned; the importer's text: "p", "mf", or an other-dynamics text */
    long start_numerator;                 /* the span it holds over, in whole notes */
    unsigned long start_denominator;
    long end_numerator;
    unsigned long end_denominator;
    size_t part;                          /* index into ImportedSource.parts */
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
 * What the input says about itself, beyond the notes. For a MIDI file
 * everything here is false/zero/NULL except format and the parts (below).
 *
 * format is INPUT_FORMAT_MIDI or INPUT_FORMAT_MUSICXML, never AUTO.
 * meter_notated and key_notated say the time signatures and key
 * signatures in ImportedNotes are the notation's own (MusicXML <time>
 * and <key>, with a defaulted 4/4 map when the score gives no <time>);
 * a MIDI file's are embedded events, and its key is inferred.
 * spelling_notated says the NoteEvents' PitchSpellings are the score's.
 *
 * parts, and the arrays parallel to ImportedNotes.events: part_index[i]
 * is the index into parts of the part note i belongs to, staff[i] its
 * staff number. A note's voice, where the score names one, is its
 * NoteEvent's VoiceReference. All parts are merged into the one
 * texture the pipeline analyzes; none is dropped. A MIDI file has one
 * part per track and channel that has notes, in order of first
 * appearance: its name from the track name and the General MIDI
 * instrument, midi_program its program (0 when the channel never
 * changes it, -1 for the percussion channel). Its staff_count is 2 for
 * the keyboard-like programs (score_program_uses_grand_staff), and
 * staff[i] is then score_staves_assign's result.
 *
 * tempo_declared is true when the score gives a tempo;
 * initial_quarters_per_minute is then the tempo at the start.
 * tempo_markings (tempo_marking_count of them) and dynamics
 * (dynamics_count) are the markings the input carries, in time order,
 * owned; both are NULL for a MIDI file. Reading them is the markings
 * stage's job (markings_pipeline.h).
 *
 * reading_applied is true when a notation reading (notation_reading.h)
 * was applied to the notes, and reading is that reading; otherwise
 * reading is zeroed.
 */
typedef struct {
    InputFormat format;
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
    ImportedTempoMarking *tempo_markings;
    ImportedDynamic *dynamics;
    bool reading_applied;
    NotationReading reading;
} ImportedSource;

/*
 * events and pitch_values are index-paired, count entries each: for
 * every i, pitch_values[i] is the numeric pitch importer computed for
 * events[i] (see io/midi_importer.h). Caller owns both arrays and
 * every object in them; release with imported_notes_destroy.
 *
 * key_signature is the file's first Key Signature meta-event, if any
 * (see io/midi_key_signature.h); key_signature.found is false when the
 * file carries none. key_signature_events is every Key Signature
 * meta-event with its position, in time order (count 0 when the file
 * carries none). A malformed key signature meta-event does not
 * fail the import -- see midi_pipeline_import_file.
 *
 * meter is the file's time signatures as a MeterMap, plus the end time
 * of the last note (see io/midi_importer.h). A file with no time
 * signature event gets a single 4/4 region marked defaulted
 * (meter_map_is_defaulted), so meter.meter_map is never NULL after a
 * successful import.
 */
typedef struct ImportedNotes {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t count;
    size_t *streams; /* owned; count entries, parallel to events: the part index the note belongs to (source.part_index):
                        the MIDI track and channel, the MusicXML part; all 0 for a one-part file */
    MidiKeySignature key_signature;
    MidiKeySignatureEvents key_signature_events;
    unsigned ticks_per_quarter; /* SMF header division (PPQN); 0 when unavailable */
    MidiMeterImport meter;
    ImportedSource source;
    TempoMap *tempo_map; /* owned; the file's tempo changes (MIDI); NULL for MusicXML */
} ImportedNotes;

void imported_notes_destroy(ImportedNotes *notes);

#endif
