#ifndef URSATZ_WORKSHOP_ANALYSIS_CORPUS_TOOLS_H
#define URSATZ_WORKSHOP_ANALYSIS_CORPUS_TOOLS_H

#include <stdbool.h>

#include <cJSON.h>

#include <common/status.h>

/*
 * The tool "corpus" (R-656): analyzes every piece of the folder dir into out (NULL: "<dir>/.ursatz-corpus", the tool's one output). *result is
 *   {"pieces": [{"state": "analyzed"|"skipped"|"failed", "path", "error"?, "import_message"?, "warnings": [...]}], "analyzed", "skipped",
 *    "failed", "out"}
 * in run order. A run that fails answers the Analyzer's status and message.
 */
UrsatzStatus workshop_corpus_run(const char *dir, const char *out, bool resume, cJSON **result, UrsatzError *error);

/*
 * The tool "style-profile" (R-656): the style model of the corpus output folder dir, written to model_path (NULL: "<dir>/style-model.json") under
 * name. *result is {"path", "name", "pieces", "stale"}: stale counts the analyzed pieces whose document another version wrote.
 */
UrsatzStatus workshop_style_profile(const char *dir, const char *name, const char *model_path, cJSON **result, UrsatzError *error);

#endif
