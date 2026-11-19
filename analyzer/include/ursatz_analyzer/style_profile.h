#ifndef URSATZ_ANALYZER_STYLE_PROFILE_H
#define URSATZ_ANALYZER_STYLE_PROFILE_H

#include <stdbool.h>
#include <stddef.h>

#include "theory/style/style_compare.h"
#include "theory/style/style_features.h"
#include "theory/style/style_labels.h"
#include "theory/style/style_model.h"
#include "theory/style/style_model_file.h"
#include "theory/style/style_profile.h"

/*
 * The Analyzer's entries to a style profile (the profile, its counting, its comparison and its model file are the library's,
 * theory/style/style_profile.h, R-617): a profile counts analysis documents. It reads only the documents, never the scores
 * (R-079).
 */

/* Counts one analysis document (the text of an "analysis-document" with schema_version 13). */
StyleProfileResult style_profile_add_document_json(StyleProfile *profile, const char *json);

/*
 * Reads "<output_dir>/corpus-index.json" and adds every piece the index lists as analyzed, through the index's
 * document paths. The index's input_dir becomes the profile's training corpus (several corpora join with ';').
 */
StyleProfileResult style_profile_add_corpus(StyleProfile *profile, const char *output_dir);

#endif
