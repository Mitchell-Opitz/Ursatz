#ifndef URSATZ_ANALYZER_REPORT_WRITER_H
#define URSATZ_ANALYZER_REPORT_WRITER_H

#include <stdbool.h>

/*
 * Derives the output report path from input_path: strips a trailing
 * extension (the substring after the last '.' in the last path
 * segment, if any) and appends ".report.md". E.g. "song.mid" ->
 * "song.report.md"; "song" (no extension) -> "song.report.md".
 * Returns a newly allocated, NUL-terminated string the caller owns
 * (release with free()), or NULL on allocation failure.
 */
char *derive_report_path(const char *input_path);

/*
 * Writes markdown (a NUL-terminated string) to path, overwriting any
 * existing file. Returns true on success; on failure, writes a
 * human-readable message to stderr and returns false.
 */
bool write_report_file(const char *path, const char *markdown);

#endif
