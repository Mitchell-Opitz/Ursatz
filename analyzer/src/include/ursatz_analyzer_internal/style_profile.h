#ifndef URSATZ_ANALYZER_INTERNAL_STYLE_PROFILE_H
#define URSATZ_ANALYZER_INTERNAL_STYLE_PROFILE_H

#include <ursatz_analyzer/analyzer_api.h>

#include "theory/style/style_compare.h"
#include "theory/style/style_features.h"
#include "theory/style/style_labels.h"
#include "theory/style/style_model.h"
#include "theory/style/style_model_file.h"
#include "theory/style/style_profile.h"

/*
 * Reads "<output_dir>/corpus-index.json" and adds every piece the index lists as analyzed, through the index's
 * document paths. The index's input_dir becomes the profile's training corpus (several corpora join with ';').
 */
StyleProfileResult style_profile_add_corpus(StyleProfile *profile, const char *output_dir);

#endif
