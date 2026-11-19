#ifndef URSATZ_ANALYZER_INPUT_PIPELINE_H
#define URSATZ_ANALYZER_INPUT_PIPELINE_H

#include <stddef.h>

#include <common/status.h>
#include <io/imported_score.h>
#include <ursatz_analyzer/analysis_result.h>

/*
 * The Analyzer's import entries for the GUI (R-614): the imported score, the formats and the readings are the library's
 * (io/imported_score.h, included here), and these two calls are the thin way in. They read the file or the bytes into
 * *out, which the caller releases with imported_score_destroy, print nothing, and put the human-readable message of a
 * failure into error (NULL for none; its path is the file's).
 *
 * URSATZ_OK on success; URSATZ_ERR_REQUEST for a NULL path, NULL or empty bytes or a NULL out; URSATZ_ERR_ALLOC when memory runs
 * out; URSATZ_ERR_NOT_FOUND for a file that cannot be read; URSATZ_ERR_FORMAT for every other failure (a format that cannot be told,
 * a document that is not readable MIDI or MusicXML, a compressed or empty score). On a failure *out is untouched.
 */
UrsatzStatus analyzer_import_file(const char *path, ImportedFormat format, ImportedScore *out, UrsatzError *error);
UrsatzStatus analyzer_import_buffer(const unsigned char *bytes, size_t length, ImportedFormat format, ImportedScore *out,
                                    UrsatzError *error);

#endif
