#ifndef URSATZ_COMPOSER_TEST_SUPPORT_H
#define URSATZ_COMPOSER_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>

#include <io/midi_importer.h>

/*
 * Writes the text to a file at the path; aborts with the path when it cannot. The checked form of test_write_file (the
 * shared support, ursatz_test.h), which reports a failure and leaves the test to check it.
 */
void test_write_text(const char *path, const char *text);

/*
 * The notes of a MIDI file through Ursatz's complete import (midi_importer_import_complete): the NoteEvents and their
 * index-paired PitchNumericValues, newly allocated arrays the caller owns (destroy each, then free both arrays), in
 * time order. The rest of the import is destroyed. On failure nothing is written.
 */
MidiImportResult test_midi_import_notes(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                        PitchNumericValue ***out_pitch_values, size_t *out_count);

#endif
