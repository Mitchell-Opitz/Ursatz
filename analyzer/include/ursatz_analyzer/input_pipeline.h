#ifndef URSATZ_ANALYZER_INPUT_PIPELINE_H
#define URSATZ_ANALYZER_INPUT_PIPELINE_H

#include <stdbool.h>

#include <ursatz_analyzer/imported_notes.h>

typedef enum {
    INPUT_PIPELINE_OK = 0,
    INPUT_PIPELINE_ERR_CANNOT_READ_FILE,
    INPUT_PIPELINE_ERR_UNKNOWN_FORMAT,
    INPUT_PIPELINE_ERR_IMPORT_FAILED
} InputPipelineResult;

/*
 * Parses the --input-format value ("midi" or "musicxml"). Returns false
 * for anything else and leaves *out untouched.
 */
bool input_format_parse(const char *name, InputFormat *out);

/* "midi" or "musicxml"; "auto" for INPUT_FORMAT_AUTO. */
const char *input_format_name(InputFormat format);

/*
 * The concrete format of the file at path: by extension (.mid, .midi;
 * .musicxml, .xml, .mxl), and when the extension says nothing, by the
 * file's first bytes (an "MThd" header is MIDI; a zip signature or an
 * XML document is MusicXML). INPUT_FORMAT_AUTO is returned when neither
 * identifies the file, and also when the file cannot be read.
 */
InputFormat input_format_detect(const char *path);

/*
 * Reads the file at path in the given format (INPUT_FORMAT_AUTO detects
 * it) and fills *out with the same ImportedNotes whatever the input, so
 * every later stage is independent of the format. On INPUT_PIPELINE_OK,
 * *out is owned by the caller (imported_notes_destroy). On any other
 * result, a human-readable message has already been written to stderr,
 * nothing was written to disk and *out is left untouched.
 */
InputPipelineResult input_pipeline_import_file(const char *path, InputFormat format, ImportedNotes *out);

#endif
