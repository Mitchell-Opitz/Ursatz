#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_EXPANSION_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_EXPANSION_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_theme.h"

/*
 * Phrase expansion and contraction (the form adapter, step 4), by standard-form's length, internal and external expansion
 * rules (length_rules.h, internal_expansion_rules.h, external_expansion_rules.h). The framework judges a length against an
 * expected one that the caller supplies; here it comes from the theme's own structure, not from a guessed nominal:
 *
 *   BALANCE      an ending is as long as its beginning (standard_form_expected_ending_length). A one-phrase theme's beginning and
 *                ending are its two subphrases, a two-phrase theme's its two phrases. An ending longer than its beginning is an
 *                EXPANSION, a shorter one a CONTRACTION (standard_form_ending_length_change), by the measures of the difference.
 *                A lone phrase, a repeated phrase and a compound period have no expected length and get none
 *   PLACEMENT    an expansion of the ending is INTERNAL: it lies after the ending's beginning and before its cadence
 *                (standard_form_placement_allowed); a contraction is always internal. This is the framework's only "cadential
 *                expansion": there is no separate rule for one
 *   TECHNIQUE    named only from evidence, as a mask of InternalExpansionTechnique bits (the framework allows combinations):
 *                  ONE_MORE_TIME  a subverted or deceptive cadence arrives in the ending before its goal cadence (a cadence tried,
 *                                 evaded and retried: standard_form_is_one_more_time)
 *                  REPETITION     an idea of the ending repeats an earlier idea of the ending (idea similarity; the repeat of a
 *                                 presentation's basic idea is not in the ending, standard_form_repetition_creates_expansion)
 *                with no evidence the mask is empty (the technique is unspecified). Alternative path and stretching are not detected
 *   EXTERNAL     the extension of a phrase (a post-cadential extension or a codetta, analysis_phrase.h) is an EXTERNAL expansion by
 *                its measures. Whether its length is typical for its kind (standard_form_suffix_length_typical) and agrees with its
 *                location (standard_form_suffix_length_agrees_with_location) is recorded. A prefix (an introduction) is not detected
 *
 * Each expansion or contraction is a Structure shell (the soprano notes it concerns) and an Interpretation targeting it:
 * FrameworkReference standard-form, claim = a StandardFormExpansionClaim, and uncertainty = the theme's (internal and contraction)
 * or the phrase's (external).
 */

#define STANDARD_FORM_EXPANSION_CLAIM_TYPE "standard-form/length-change"

typedef enum {
    STANDARD_FORM_EXPANSION_INTERNAL = 0,
    STANDARD_FORM_EXPANSION_EXTERNAL,
    STANDARD_FORM_EXPANSION_CONTRACTION
} StandardFormExpansionKind;

typedef struct {
    int index;
    int kind;             /* StandardFormExpansionKind */
    int theme;            /* the theme it concerns, -1 for an external expansion of a phrase that is in no theme */
    int phrase;           /* the phrase of the ending (internal, contraction) or the extended phrase (external) */
    int change;           /* PhraseLengthChange */
    int placement;        /* ExpansionPlacement */
    int expected_length;  /* measures: the beginning's (internal, contraction) or the phrase's own (external) */
    int actual_length;    /* measures: the ending's, or the phrase's with its extension */
    int measures_changed; /* added (expansion) or removed (contraction), at least 1 */
    int first_measure;    /* the ending's first measure, or the extension's */
    int last_measure;
    int techniques;       /* mask of InternalExpansionTechnique bits; 0 when unspecified or not internal */
    int external_kind;    /* ExternalExpansionKind, -1 when not external */
    int suffix_length_typical;
    int suffix_agrees_with_location;
} StandardFormExpansionClaim;

typedef struct {
    StandardFormExpansionClaim claim;
    double uncertainty;
    Structure *shell;
    Interpretation *interpretation;
} StandardFormExpansion;

typedef struct {
    StandardFormExpansion *items; /* owned; in order of theme and then phrase; NULL when none */
    size_t count;
} StandardFormExpansions;

StandardFormAnalysisStatus standard_form_detect_expansions(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                           const StandardFormSegmentation *segmentation,
                                                           const StandardFormThemes *themes,
                                                           const StandardFormConfig *config, StandardFormExpansions *out);
void standard_form_expansions_destroy(StandardFormExpansions *expansions);
bool standard_form_expansion_claim_read(const Interpretation *interpretation, StandardFormExpansionClaim *out);

#endif
