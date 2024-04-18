#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_IDEAS_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_ANALYSIS_ANALYSIS_IDEAS_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_phrase.h"
#include "analysis_similarity.h"
#include "interpretation/interpretation.h"
#include "structure/structure.h"

/*
 * Ideas and subphrases (the form adapter, step 2), by standard-form's length conventions (segmentation_rules.h IDEA_LENGTH,
 * IDEA_DIVISION; sentence_rules.h PRESENTATION_LENGTH, BASIC_IDEA_LENGTH). Lengths are in measures of the MeterMap. The
 * rules say what an idea usually is, two measures, but give no way to find an idea's edge, so the adapter divides by the
 * nominal length:
 *
 *   IDEAS        a phrase of n measures (its length, a pickup not counted) is cut from its first measure into n / 2 ideas (at least
 *                one) of two measures, the last taking the remainder (two or three). The ideas fill the phrase exactly
 *                (standard_form_ideas_fill_phrase). The pickup measure belongs to the first idea and is not counted in its
 *                length. Notes after the phrase's cadence (an extension) belong to no idea
 *   SUBPHRASES   a phrase of at least three ideas has two subphrases: the first two ideas (a beginning: presentation,
 *                compound basic idea or antecedent, four measures) and the rest (an ending). A phrase of one or two ideas has no
 *                subphrases: it is itself a beginning or an ending
 *   LABELS       the first idea is the BASIC_IDEA. Each later idea is compared with it (analysis_similarity.h): a match is a
 *                repetition and counts as a BASIC_IDEA again (beginning_rules.h); otherwise it is a FRAGMENT when shorter than the
 *                basic idea (standard_form_is_fragmentation) and a CONTRASTING_IDEA when not
 *
 * Limits: the nominal cut finds no 1 + 1 + 2 division of a continuation, and no boundary the melody or the harmony suggests.
 *
 * Each idea and subphrase is a Structure shell (the ids of its soprano notes) and an Interpretation targeting the shell's id:
 * FrameworkReference standard-form, a StandardFormIdeaClaim or StandardFormSubphraseClaim, and
 *
 *   uncertainty = 1 - (1 - u_phrase)(1 - u_similarity)(1 - penalty)
 *
 * u_phrase the phrase's, u_similarity the similarity's (0 for the first idea; analysis_similarity.h), penalty 0.2 when the
 * phrase's length is odd (its last idea is not a nominal two).
 */

#define STANDARD_FORM_IDEA_CLAIM_TYPE "standard-form/idea"
#define STANDARD_FORM_SUBPHRASE_CLAIM_TYPE "standard-form/subphrase"
#define STANDARD_FORM_ODD_LENGTH_PENALTY 0.2

typedef struct {
    int phrase;
    int index;           /* within the phrase */
    int first_measure;   /* the first idea of a phrase starting with a pickup starts at the pickup */
    int last_measure;
    int length_measures; /* counted measures */
    int label;           /* IdeaLabel */
    int note_first;      /* [note_first, note_end) in the input's notes */
    int note_end;
    int compared;        /* the similarity to the phrase's first idea was computed (false for the first idea) */
    double similarity;
    int match;
    double margin;
} StandardFormIdeaClaim;

typedef struct {
    StandardFormIdeaClaim claim;
    double uncertainty;
    Structure *shell;               /* owned */
    Interpretation *interpretation; /* owned */
} StandardFormIdea;

typedef struct {
    int phrase;
    int index;       /* 0 or 1 */
    int first_idea;  /* into the phrase's ideas */
    int idea_count;
    int first_measure;
    int last_measure;
    int length_measures;
    int note_first;
    int note_end;
} StandardFormSubphraseClaim;

typedef struct {
    StandardFormSubphraseClaim claim;
    double uncertainty;
    Structure *shell;
    Interpretation *interpretation;
} StandardFormSubphrase;

typedef struct {
    StandardFormIdea *ideas;           /* owned; phrase by phrase, in order; NULL when none */
    size_t idea_count;
    StandardFormSubphrase *subphrases; /* owned; NULL when none */
    size_t subphrase_count;
} StandardFormSegmentation;

/* Divides every phrase. config may be NULL for the defaults. On OK, *out is owned by the caller. */
StandardFormAnalysisStatus standard_form_segment_ideas(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                       const StandardFormConfig *config, StandardFormSegmentation *out);
void standard_form_segmentation_destroy(StandardFormSegmentation *segmentation);

/* The ideas of phrase `phrase` as [*first, *first + *count) in segmentation->ideas. False when the phrase has none. */
bool standard_form_phrase_ideas(const StandardFormSegmentation *segmentation, int phrase, size_t *first, size_t *count);

bool standard_form_idea_claim_read(const Interpretation *interpretation, StandardFormIdeaClaim *out);
bool standard_form_subphrase_claim_read(const Interpretation *interpretation, StandardFormSubphraseClaim *out);

#endif
