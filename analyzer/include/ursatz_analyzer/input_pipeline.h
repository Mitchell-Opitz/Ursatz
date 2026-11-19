#ifndef URSATZ_ANALYZER_INPUT_PIPELINE_H
#define URSATZ_ANALYZER_INPUT_PIPELINE_H

#include <stddef.h>

#include <io/imported_score.h>
#include <ursatz_analyzer/analysis_result.h>

/*
 * The Analyzer's import entries for the GUI (R-614): the imported score, the formats and the readings are the library's
 * (io/imported_score.h, included here), and these two calls are the thin way in. They read the file or the bytes into
 * *out, which the caller releases with imported_score_destroy, print nothing, and write the human-readable message of a
 * failure into message (size bytes; NULL or 0 for none).
 *
 * ANALYZER_OK on success; ANALYZER_ERR_ARGUMENT for a NULL path, NULL or empty bytes or a NULL out; ANALYZER_ERR_ALLOC when
 * memory runs out; ANALYZER_ERR_LIBRARY for every other failure (a file that cannot be read, a format that cannot be told,
 * a document that is not readable MIDI or MusicXML, a compressed or empty score). On a failure *out is untouched.
 */
AnalyzerStatus analyzer_import_file(const char *path, ImportedFormat format, ImportedScore *out, char *message,
                                    size_t size);
AnalyzerStatus analyzer_import_buffer(const unsigned char *bytes, size_t length, ImportedFormat format,
                                      ImportedScore *out, char *message, size_t size);

#endif
