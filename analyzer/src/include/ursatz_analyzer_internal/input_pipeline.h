#ifndef URSATZ_ANALYZER_INTERNAL_INPUT_PIPELINE_H
#define URSATZ_ANALYZER_INTERNAL_INPUT_PIPELINE_H

#include <ursatz_analyzer/analyzer_api.h>

/* analyzer_import_file's sibling for bytes in memory (the Analyzer's command line and tests); the rules are the same. */
UrsatzStatus analyzer_import_buffer(const unsigned char *bytes, size_t length, ImportedFormat format, ImportedScore *out,
                                    UrsatzError *error);

#endif
