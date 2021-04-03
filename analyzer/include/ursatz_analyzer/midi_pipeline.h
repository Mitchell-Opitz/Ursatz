#ifndef URSATZ_ANALYZER_MIDI_PIPELINE_H
#define URSATZ_ANALYZER_MIDI_PIPELINE_H

#include <stddef.h>

#include "events/note_event.h"
#include "io/midi_importer.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_numeric_value.h"

typedef enum {
    MIDI_PIPELINE_OK = 0,
    MIDI_PIPELINE_ERR_CANNOT_READ_FILE,
    MIDI_PIPELINE_ERR_IMPORT_FAILED
} MidiPipelineResult;

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
    MidiKeySignature key_signature;
    MidiKeySignatureEvents key_signature_events;
    unsigned ticks_per_quarter; /* SMF header division (PPQN); 0 when unavailable */
    MidiMeterImport meter;
} ImportedNotes;

/*
 * Reads the file at path and imports it via Ursatz's MIDIImporter. On
 * MIDI_PIPELINE_OK, *out is populated and owned by the caller. On any
 * other result, a human-readable message describing the failure has
 * already been written to stderr and *out is left untouched.
 */
MidiPipelineResult midi_pipeline_import_file(const char *path, ImportedNotes *out);
void imported_notes_destroy(ImportedNotes *notes);

#endif
