#ifndef URSATZ_ANALYZER_MIDI_PIPELINE_H
#define URSATZ_ANALYZER_MIDI_PIPELINE_H

#include <ursatz_analyzer/imported_notes.h>

typedef enum {
    MIDI_PIPELINE_OK = 0,
    MIDI_PIPELINE_ERR_CANNOT_READ_FILE,
    MIDI_PIPELINE_ERR_IMPORT_FAILED
} MidiPipelineResult;

/*
 * Reads the file at path and imports it via Ursatz's MIDIImporter. On
 * MIDI_PIPELINE_OK, *out is populated and owned by the caller. On any
 * other result, a human-readable message describing the failure has
 * already been written to stderr and *out is left untouched.
 */
MidiPipelineResult midi_pipeline_import_file(const char *path, ImportedNotes *out);

#endif
