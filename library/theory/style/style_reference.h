#ifndef URSATZ_THEORY_STYLE_REFERENCE_H
#define URSATZ_THEORY_STYLE_REFERENCE_H

#include <stddef.h>

#include "theory/style/style_compare.h"
#include "theory/style/style_features.h"
#include "theory/style/style_model.h"

/*
 * A style the fragments are measured against, prepared once: the model's statistics with the named categories dropped. One
 * reference serves any number of fragments (R-617).
 */
typedef struct StyleReference StyleReference;

typedef enum {
    STYLE_REFERENCE_OK = 0,
    STYLE_REFERENCE_ERR_ARGUMENT,
    STYLE_REFERENCE_ERR_ALLOC,
    STYLE_REFERENCE_ERR_COMPARE /* the comparison failed (a category it cannot read, or an overflow) */
} StyleReferenceResult;

/*
 * Prepares model for distances. without names the categories left out (the list the model was read without,
 * style_model_file_parse); they appear nowhere in a distance. model_version is the version the reference's own
 * models carry (what produced the statistics). STYLE_REFERENCE_ERR_ARGUMENT for a NULL model or out, an unknown category
 * name, or count > 0 with NULL without. *out is NULL on failure. The reference copies what it needs: model need not outlive it.
 */
StyleReferenceResult style_reference_create(const StyleModel *model, const char *const *without, size_t without_count,
                                            const char *model_version, StyleReference **out);

/* NULL is a no-op. */
void style_reference_destroy(StyleReference *reference);

/*
 * How far a fragment is from the reference: its statistics (sorted, as a profile gives them; the entries of the dropped
 * categories are moved out of the array in place, so the array is the caller's to free but not to reuse) compared with the
 * reference's, with the categories the fragment has no data for left out of the means and kept in out->categories.
 * Release out with style_comparison_destroy.
 */
StyleReferenceResult style_reference_distance(const StyleReference *reference, StyleStatistic *fragment_statistics,
                                              size_t fragment_count, StyleComparison *out);

#endif
