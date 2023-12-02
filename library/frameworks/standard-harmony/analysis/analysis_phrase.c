#include "analysis_phrase.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "analysis_six_four.h"
#include "core/value.h"
#include "harmony_phrase_labels.h"
#include "identity/entity_id.h"

static bool same_chord(const HarmonyChord *a, const HarmonyChord *b) {
    return a->degree == b->degree && a->inversion == b->inversion && a->seventh == b->seventh &&
           a->cadential == b->cadential && a->leading_tone == b->leading_tone &&
           a->half_diminished == b->half_diminished;
}

static bool spans_valid(size_t count, const HarmonyPhraseSpan *spans, size_t span_count) {
    for (size_t i = 0; i < span_count; i++) {
        if (spans[i].first_segment > spans[i].last_segment || spans[i].last_segment >= count ||
            (i > 0 && spans[i].first_segment <= spans[i - 1].last_segment)) {
            return false;
        }
    }
    return true;
}

typedef struct {
    HarmonyChord *chords;
    size_t *chord_of_segment; /* per segment of the span: the chord index, or (size_t)-1 when left out */
    size_t chord_count;
} Collapsed;

/* A 6/4 that prolongs the harmony around it (passing, neighbor or arpeggiating) is part of that harmony, not a
   chord of the phrase model. The cadential 6/4 stays: the rules treat it as part of the dominant unit. */
static bool prolonging_six_four(const HarmonySixFourResult *result) {
    return result->classified && result->type != SIX_FOUR_TYPE_CADENTIAL;
}

/* The span's chords, consecutive equal diatonic segments as one. */
static bool collapse(const HarmonySegment *segments, const HarmonySixFourResult *six_fours,
                     const HarmonyPhraseSpan *span, Collapsed *out) {
    size_t n = span->last_segment - span->first_segment + 1;
    out->chords = (HarmonyChord *)calloc(n, sizeof(HarmonyChord));
    out->chord_of_segment = (size_t *)calloc(n, sizeof(size_t));
    out->chord_count = 0;
    if (out->chords == NULL || out->chord_of_segment == NULL) {
        free(out->chords);
        free(out->chord_of_segment);
        return false;
    }
    for (size_t i = 0; i < n; i++) {
        const HarmonySegment *segment = &segments[span->first_segment + i];
        out->chord_of_segment[i] = (size_t)-1;
        if (!segment->valid || segment->applied || prolonging_six_four(&six_fours[span->first_segment + i])) {
            continue;
        }
        if (out->chord_count == 0 || !same_chord(&out->chords[out->chord_count - 1], &segment->chord)) {
            out->chords[out->chord_count++] = segment->chord;
        }
        out->chord_of_segment[i] = out->chord_count - 1;
    }
    return true;
}

static bool make_interpretation(const HarmonySegmentInput *input, PhraseModelLabel label, int phrase_index,
                                Interpretation **out) {
    HarmonyPhraseLabelClaim claim;
    memset(&claim, 0, sizeof(claim));
    claim.label = (int)label;
    claim.phrase_index = phrase_index;
    char id[256];
    snprintf(id, sizeof(id), "standard-harmony-phrase-model-%s", input->segment_id);
    *out = harmony_analysis_make_interpretation(id, input->segment_id, HARMONY_PHRASE_LABEL_CLAIM_TYPE, &claim,
                                                sizeof(claim), input->uncertainty, "phrase-model-adapter");
    return *out != NULL;
}

/* Labels one span. false only on allocation failure; a phrase the rules do not label is true and unlabeled. */
static bool label_span(const HarmonySegment *segments, const HarmonySixFourResult *six_fours,
                       const HarmonyPhraseSpan *span, int phrase_index, HarmonyPhraseModel *model) {
    Collapsed collapsed;
    if (!collapse(segments, six_fours, span, &collapsed)) {
        return false;
    }
    bool ok = true;
    if (collapsed.chord_count > 0) {
        HarmonyPhrase phrase = {segments[span->first_segment].input->mode, collapsed.chords, collapsed.chord_count};
        HarmonyPhraseLabels labels;
        if (harmony_phrase_model_label(&phrase, span->cadence, &labels)) {
            model->phrase_labeled[phrase_index] = true;
            size_t n = span->last_segment - span->first_segment + 1;
            for (size_t i = 0; ok && i < n; i++) {
                size_t chord = collapsed.chord_of_segment[i];
                if (chord == (size_t)-1 || !labels.has_label[chord]) {
                    continue;
                }
                HarmonyPhraseLabelResult *result = &model->segments[span->first_segment + i];
                result->has_label = true;
                result->label = labels.label[chord];
                ok = make_interpretation(segments[span->first_segment + i].input, result->label, phrase_index,
                                         &result->interpretation);
            }
        }
    }
    free(collapsed.chords);
    free(collapsed.chord_of_segment);
    return ok;
}

HarmonyPhraseModelStatus harmony_analysis_phrase_model_label(const HarmonySegment *segments, size_t count,
                                                             const HarmonyPhraseSpan *spans, size_t span_count,
                                                             HarmonyPhraseModel *out) {
    if (out == NULL || (segments == NULL && count > 0) || (spans == NULL && span_count > 0)) {
        return HARMONY_PHRASE_MODEL_ERR_NULL_ARGUMENT;
    }
    if (!spans_valid(count, spans, span_count)) {
        return HARMONY_PHRASE_MODEL_ERR_BAD_SPAN;
    }
    HarmonyPhraseModel model;
    memset(&model, 0, sizeof(model));
    model.segment_count = count;
    model.phrase_count = span_count;
    model.segments = (HarmonyPhraseLabelResult *)calloc(count > 0 ? count : 1, sizeof(HarmonyPhraseLabelResult));
    model.phrase_labeled = (bool *)calloc(span_count > 0 ? span_count : 1, sizeof(bool));
    if (model.segments == NULL || model.phrase_labeled == NULL) {
        harmony_phrase_model_destroy(&model);
        return HARMONY_PHRASE_MODEL_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        model.segments[i].segment_id = segments[i].input->segment_id;
        model.segments[i].phrase_index = -1;
    }
    HarmonySixFourResult *six_fours = NULL;
    if (harmony_analysis_classify_six_fours(segments, count, &six_fours) != HARMONY_SIX_FOUR_OK) {
        harmony_phrase_model_destroy(&model);
        return HARMONY_PHRASE_MODEL_ERR_ALLOC;
    }
    for (size_t p = 0; p < span_count; p++) {
        for (size_t s = spans[p].first_segment; s <= spans[p].last_segment; s++) {
            model.segments[s].phrase_index = (int)p;
        }
        if (!label_span(segments, six_fours, &spans[p], (int)p, &model)) {
            harmony_six_four_results_destroy(six_fours, count);
            harmony_phrase_model_destroy(&model);
            return HARMONY_PHRASE_MODEL_ERR_ALLOC;
        }
    }
    harmony_six_four_results_destroy(six_fours, count);
    *out = model;
    return HARMONY_PHRASE_MODEL_OK;
}

void harmony_phrase_model_destroy(HarmonyPhraseModel *model) {
    if (model == NULL) {
        return;
    }
    for (size_t i = 0; i < model->segment_count && model->segments != NULL; i++) {
        interpretation_destroy(model->segments[i].interpretation);
    }
    free(model->segments);
    free(model->phrase_labeled);
    model->segments = NULL;
    model->phrase_labeled = NULL;
    model->segment_count = 0;
    model->phrase_count = 0;
}

bool harmony_phrase_label_claim_read(const Interpretation *interpretation, HarmonyPhraseLabelClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), HARMONY_PHRASE_LABEL_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
