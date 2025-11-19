#ifndef URSATZ_ANALYZER_IMPORTED_NOTES_H
#define URSATZ_ANALYZER_IMPORTED_NOTES_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_numeric_value.h"

typedef enum {
    INPUT_FORMAT_AUTO = 0, /* choose by extension, then by content */
    INPUT_FORMAT_MIDI,
    INPUT_FORMAT_MUSICXML
} InputFormat;

/* One part of a notated score. */
typedef struct {
    char *id;   /* owned; the score-part id */
    char *name; /* owned; the part name, "" when it has none */
    int staff_count;
} ImportedPart;

/*
 * What the input says about itself, beyond the notes. Everything here
 * is false/zero/NULL for a MIDI file except format.
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
 * texture the pipeline analyzes; none is dropped.
 *
 * tempo_declared is true when the score gives a tempo;
 * initial_quarters_per_minute is then the tempo at the start.
 * tempo_marking_count and dynamics_count count the markings the input
 * carries; they are carried through, not analyzed.
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
typedef struct {
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t count;
    size_t *streams; /* owned; count entries, parallel to events: the MIDI track chunk index the note was read from, the
                        MusicXML part index (source.part_index); all 0 for a one-track file */
    MidiKeySignature key_signature;
    MidiKeySignatureEvents key_signature_events;
    unsigned ticks_per_quarter; /* SMF header division (PPQN); 0 when unavailable */
    MidiMeterImport meter;
    ImportedSource source;
} ImportedNotes;

void imported_notes_destroy(ImportedNotes *notes);

#endif
