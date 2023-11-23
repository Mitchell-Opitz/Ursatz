#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_PHRASE_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_ANALYSIS_ANALYSIS_PHRASE_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis_input.h"
#include "interpretation/interpretation.h"
#include "phrase_model_label.h"
#include "semantics/cadence_type.h"

/*
 * Phrase model labels per segment (the standard-harmony analysis adapter, step 2). Phrases and cadences do not
 * exist as analysis yet (L3.6, L3.3), so the caller SUPPLIES them: each HarmonyPhraseSpan names the segments of
 * one phrase and the cadence that ends it. Real phrases and cadences replace them when those land; nothing
 * here listens for either.
 *
 *   first_segment, last_segment   the phrase's segments, as indices into the prepared segments, inclusive
 *   cadence                       the cadence that ends the phrase: an authentic one (perfect, imperfect or
 *                                 unspecified) or HALF. Anything else leaves the phrase unlabeled
 *
 * Spans must be in order, must not overlap, and must lie inside the segments, or the call is BAD_SPAN.
 *
 * Within one span the labels are the existing rules' (harmony_phrase_model_label, harmony_phrase_labels.h), applied
 * to the phrase's CHORDS: consecutive valid diatonic segments that make the same chord (the same degree,
 * inversion, seventh and cadential mark) are one chord, so a chord sounded over several segments is labeled once
 * and every one of its segments carries the label. Invalid segments and applied chords are left out of the chord
 * list: an applied chord is not one of the phrase model's chords, and takes no label. The labels, by the rules:
 *
 *   Tb   the first chord, when it is a tonic
 *   PD   the first strong predominant of the run of predominants before the cadential dominant
 *   D    the cadential dominant, V or V7 (a cadential 6/4 before it is part of it and has no label of its own)
 *   Te   the tonic of an authentic cadence; a half cadence has none
 *
 * A phrase that does not end as its cadence says, has an invalid chord list, or has more than
 * HARMONY_PHRASE_LABELS_MAX chords gets no labels (labeled false for it). Each labeled segment gets one
 * Interpretation: target = the segment's Sonority id, FrameworkReference standard-harmony, claim = a
 * HarmonyPhraseLabelClaim, the segment's uncertainty.
 */

#define HARMONY_PHRASE_LABEL_CLAIM_TYPE "standard-harmony/phrase-model-label"

typedef struct {
    size_t first_segment;
    size_t last_segment;
    CadenceType cadence;
} HarmonyPhraseSpan;

typedef struct {
    int label;           /* PhraseModelLabel */
    int phrase_index;    /* the index of the span in the caller's list */
} HarmonyPhraseLabelClaim;

typedef struct {
    const char *segment_id;       /* borrowed from the input */
    bool has_label;               /* false: no Interpretation */
    PhraseModelLabel label;
    int phrase_index;             /* the span the segment is in, or -1 outside every span */
    Interpretation *interpretation; /* owned; NULL when !has_label */
} HarmonyPhraseLabelResult;

typedef struct {
    HarmonyPhraseLabelResult *segments; /* owned; one per prepared segment */
    size_t segment_count;
    bool *phrase_labeled;               /* owned; one per span: the phrase got labels */
    size_t phrase_count;
} HarmonyPhraseModel;

typedef enum {
    HARMONY_PHRASE_MODEL_OK = 0,
    HARMONY_PHRASE_MODEL_ERR_NULL_ARGUMENT,
    HARMONY_PHRASE_MODEL_ERR_BAD_SPAN,
    HARMONY_PHRASE_MODEL_ERR_ALLOC
} HarmonyPhraseModelStatus;

/* On OK, *out is owned by the caller (harmony_phrase_model_destroy); otherwise untouched. span_count may be 0. */
HarmonyPhraseModelStatus harmony_analysis_phrase_model_label(const HarmonySegment *segments, size_t count,
                                                             const HarmonyPhraseSpan *spans, size_t span_count,
                                                             HarmonyPhraseModel *out);

void harmony_phrase_model_destroy(HarmonyPhraseModel *model);

/* Reads the claim back out of a phrase model Interpretation. False if it is not one. */
bool harmony_phrase_label_claim_read(const Interpretation *interpretation, HarmonyPhraseLabelClaim *out);

#endif
