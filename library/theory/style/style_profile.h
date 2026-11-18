#ifndef URSATZ_THEORY_STYLE_PROFILE_H
#define URSATZ_THEORY_STYLE_PROFILE_H

#include <stdbool.h>
#include <stddef.h>

#include "theory/style/style_features.h"
#include "theory/style/style_labels.h"
#include "theory/style/style_piece.h"

/*
 * A style profile: counts of harmony, rhythm, melody and form labels (style_labels.h) over the pieces of a corpus, each a
 * StylePiece (R-617). It reads only what a piece holds, never a score (R-079). A profile is a count, not a rule: it says
 * how often something was seen.
 */

typedef struct StyleProfile StyleProfile;

typedef enum {
    STYLE_PROFILE_OK = 0,
    STYLE_PROFILE_ERR_ARGUMENT,
    STYLE_PROFILE_ERR_IO,      /* a file could not be read */
    STYLE_PROFILE_ERR_FORMAT,  /* not JSON, not an analysis document of the schema version the Analyzer reads, or not a corpus index */
    STYLE_PROFILE_ERR_ALLOC
} StyleProfileResult;

/* NULL on allocation failure. */
StyleProfile *style_profile_create(void);
void style_profile_destroy(StyleProfile *profile);

/* Counts one piece into the profile and counts the piece. STYLE_PROFILE_ERR_ALLOC leaves the profile partly counted. */
StyleProfileResult style_profile_add_piece(StyleProfile *profile, const StylePiece *piece);

/* Adds input_dir to the training corpus label (several corpora join with ';', one corpus twice counts once). */
StyleProfileResult style_profile_add_training_corpus(StyleProfile *profile, const char *input_dir);

/* How many pieces were added. */
size_t style_profile_piece_count(const StyleProfile *profile);

/* The training corpus label, "" when no corpus was added. Valid until the profile is destroyed. */
const char *style_profile_training_corpus(const StyleProfile *profile);

/*
 * Every (category, label) seen, sorted by category then label (byte order), each with its count and the sum of
 * the counts of its category. *out is a new array the caller frees with free(); its strings belong to the profile
 * and stay valid until it is destroyed. *out is NULL when *count is 0.
 */
StyleProfileResult style_profile_statistics(const StyleProfile *profile, StyleStatistic **out, size_t *count);

#endif
