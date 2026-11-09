#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_THEME_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_THEME_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_ideas.h"
#include "analysis_phrase_grid.h"
#include "relationship/relationship.h"

/*
 * Themes (the form adapter, step 3): the phrase-level form of each theme, from the segmented phrases, idea similarity and
 * the cadences, by standard-form's own rules. Nothing here defines a form: the beginning and the ending are classified with
 * standard_form_classify_beginning and standard_form_classify_ending (a beginning is told apart by whether it ends with a
 * cadence and by its two ideas, an ending by its cadence, bass, ideas and fragmentation), the form is the framework's pairing of
 * the two (standard_form_pairing_form: presentation + continuation is a sentence, antecedent + consequent a period, the other
 * seven pairings hybrids), and two phrases are combined with standard_form_classify_phrase_pair (repeated phrase, compound
 * period, period).
 *
 * GROUPING. Themes tile the input, in order, and do not overlap. Taking the phrases in order, at phrase i (with phrase i + 1
 * after it, when there is one):
 *
 *   1. REPEATED PHRASE   both end with phrase-goal cadences of the same type and the second repeats the first (the similarity of
 *                        their notes, analysis_similarity.h, is a match): a REPEATED_PHRASE of the two phrases
 *   2. COMPOUND PERIOD   each is a one-phrase SENTENCE (step 4) and the second's cadence is stronger by the supplied rank (and by
 *                        the framework's rule): a COMPOUND_PERIOD of the two phrases; each sentence is also a theme
 *                        (part_of_compound)
 *   3. TWO-PHRASE THEME  phrase i has exactly two ideas (a basic idea, then a contrasting one) and ends with a phrase-goal cadence,
 *                        phrase i + 1 ends with a stronger one: its beginning is an ANTECEDENT; the second phrase is the ending
 *                        (a CONSEQUENT when it is a basic idea then a contrasting one and its cadence is authentic, else a
 *                        CONTINUATION). A PERIOD also needs the framework's cadence rule (standard_form_period_cadences_valid)
 *                        to agree with the supplied rank; else there is no theme
 *   4. ONE-PHRASE THEME  phrase i has at least three ideas and a phrase-goal cadence: its first subphrase (two ideas, no cadence)
 *                        is the beginning (a PRESENTATION when the second idea repeats the first, a COMPOUND_BASIC_IDEA when it
 *                        contrasts), its second subphrase the ending as above. Presentation + continuation is a SENTENCE; the
 *                        other pairings are HYBRIDs
 *   otherwise    no theme is claimed (the phrase is unique or its cadence is no goal)
 *
 * ON THE PHRASE GRID (R-391, standard_form_identify_themes_on_grid). Steps 1 to 3, the themes of two phrases (a repeated phrase, a
 * compound period, a period or hybrid of two phrases), pair phrases i and i + 1 only when phrase i ends on a grid unit's half
 * measure (StandardFormGridUnit.half_measure, a unit of 8 or 16) and phrase i + 1 on that same unit's last measure: an
 * antecedent and consequent inside one unit. Two phrases across two units, or two phrases in a unit of 4, are not paired, and
 * each stays a candidate of step 4 alone. One-phrase themes (step 4) are unchanged. standard_form_identify_themes is this with
 * no grid.
 *
 * LIMITS. The cadential ending is not identified (no bass is in the input, so standard_form_classify_ending is given "not the
 * cadential pattern"); the only compound form is the compound period; fragmentation is judged on the nominal ideas
 * (analysis_ideas.h), so it is rarely found; no theme is claimed for a phrase of fewer than two ideas that no stronger cadence
 * follows.
 *
 * OUTPUT. A Structure shell per theme (the ids of its soprano notes) and a Relationship between its two parts (the subphrase shells of
 * a one-phrase theme, the phrases' PhraseOccurrences of a two-phrase theme), predicate = the form's name ("sentence", "period",
 * "hybrid", "compound-period", "repeated-phrase"), source the first part and target the second; the Interpretation targets the
 * RELATIONSHIP's id: FrameworkReference standard-form, claim = a StandardFormThemeClaim, and
 *
 *   uncertainty = 1 - (1 - u_cadences)(1 - u_similarity)
 *
 * u_cadences the greatest uncertainty of the phrases involved (each holds its cadence's), u_similarity the uncertainty of the
 * similarity that decided the form (analysis_similarity.h): the second idea against the first for a one-phrase theme, the second
 * phrase's first idea against the first phrase's for a two-phrase one, the two phrases for a repeated phrase.
 */

#define STANDARD_FORM_THEME_CLAIM_TYPE "standard-form/theme"

typedef struct {
    int index;
    int form;             /* PhraseLevelForm */
    int beginning;        /* PhraseBeginning, -1 for a repeated phrase or a compound period */
    int ending;           /* PhraseEnding, -1 likewise */
    int commonality;      /* StandardFormCommonality of the pairing, -1 when it has none */
    int first_phrase;
    int phrase_count;     /* 1 or 2 */
    int first_measure;
    int last_measure;     /* the last measure of the last phrase proper (an extension is not part of the theme) */
    int length_measures;
    int parts_are_phrases; /* the two parts are phrases, not subphrases */
    int first_part_length; /* measures; the first part's [first, last] follow */
    int first_part_first_measure;
    int first_part_last_measure;
    int second_part_length;
    int second_part_first_measure;
    int second_part_last_measure;
    int first_cadence_type;  /* CadenceType of the first part's phrase, -1 when the theme has no cadence of its own */
    int second_cadence_type;
    int first_strength_rank;
    int second_strength_rank;
    int cadences_usual;      /* a period: antecedent half cadence and consequent perfect authentic; a sentence: its cadence is a goal */
    int lengths_usual;       /* sentence: a four-measure presentation and a continuation at least as long; period: a four-measure
                                antecedent and a consequent at least as long */
    int archetypal;          /* a sentence of 4 or 8 measures whose continuation balances the presentation and has fragmentation */
    int has_fragmentation;
    int part_of_compound;    /* a sentence that is a part of a compound period */
    int similarity_compared;
    double similarity;
    int similarity_match;
    double similarity_margin;
} StandardFormThemeClaim;

typedef struct {
    StandardFormThemeClaim claim;
    double uncertainty;
    Structure *shell;               /* owned: the ids of the theme's soprano notes */
    Relationship *relationship;     /* owned: first part to second part */
    Interpretation *interpretation; /* owned; targets the relationship's id */
} StandardFormTheme;

typedef struct {
    StandardFormTheme *items; /* owned; in order of their first phrase (a compound period after its two sentences); NULL when none */
    size_t count;
} StandardFormThemes;

/* Identifies the themes. config may be NULL for the defaults. On OK, *out is owned by the caller (standard_form_themes_destroy). */
StandardFormAnalysisStatus standard_form_identify_themes(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                         const StandardFormSegmentation *segmentation,
                                                         const StandardFormConfig *config, StandardFormThemes *out);
/* Identifies the themes with two-phrase themes paired only inside one unit of `grid`, as described above. NULL_ARGUMENT for a NULL
   grid; otherwise as standard_form_identify_themes. */
StandardFormAnalysisStatus standard_form_identify_themes_on_grid(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                                 const StandardFormSegmentation *segmentation,
                                                                 const StandardFormPhraseGrid *grid,
                                                                 const StandardFormConfig *config, StandardFormThemes *out);
void standard_form_themes_destroy(StandardFormThemes *themes);
bool standard_form_theme_claim_read(const Interpretation *interpretation, StandardFormThemeClaim *out);

#endif
