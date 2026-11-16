#ifndef URSATZ_COMPOSER_TEST_SUPPORT_H
#define URSATZ_COMPOSER_TEST_SUPPORT_H

#include <stdbool.h>
#include <stddef.h>

#include <io/midi_importer.h>

/*
 * Helpers the tests share. The whole file at `path` as a NUL-terminated string the caller frees; asserts when the file
 * cannot be read. When size is not NULL it receives the length without the terminator.
 */
char *test_read_file(const char *path, size_t *size);

/* Whether a file exists at the path. */
bool test_exists(const char *path);

/* Writes the text to a new file at the path; asserts when it cannot. */
void test_write_file(const char *path, const char *text);

/*
 * The notes of a MIDI file through Ursatz's complete import (midi_importer_import_complete): the NoteEvents and their
 * index-paired PitchNumericValues, newly allocated arrays the caller owns (destroy each, then free both arrays), in
 * time order. The rest of the import is destroyed. On failure nothing is written.
 */
MidiImportResult test_midi_import_notes(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                        PitchNumericValue ***out_pitch_values, size_t *out_count);

#endif
