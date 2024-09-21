#include "analysis_expansion.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "analysis_time.h"
#include "core/value.h"
#include "external_expansion_rules.h"
#include "internal_expansion_rules.h"
#include "length_rules.h"
#include "expansion_placement.h"
#include "external_expansion_kind.h"
#include "internal_expansion_technique.h"
#include "phrase_length_change.h"
#include "phrase_level_form.h"

static void destroy_expansion(StandardFormExpansion *expansion) {
    interpretation_destroy(expansion->interpretation);
    structure_destroy(expansion->shell);
    memset(expansion, 0, sizeof(*expansion));
}

void standard_form_expansions_destroy(StandardFormExpansions *expansions) {
    if (expansions == NULL) {
        return;
    }
    for (size_t i = 0; i < expansions->count; i++) {
        destroy_expansion(&expansions->items[i]);
    }
    free(expansions->items);
    expansions->items = NULL;
    expansions->count = 0;
}

bool standard_form_expansion_claim_read(const Interpretation *interpretation, StandardFormExpansionClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), STANDARD_FORM_EXPANSION_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}

static StandardFormAnalysisStatus emit(const StandardFormInput *input, StandardFormExpansions *list,
                                       StandardFormExpansionClaim *claim, double uncertainty, int note_first,
                                       int note_end) {
    StandardFormExpansion *item = &list->items[list->count];
    claim->index = (int)list->count;
    item->claim = *claim;
    item->uncertainty = uncertainty;
    if (note_end <= note_first) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    size_t count = (size_t)(note_end - note_first);
    const char **ids = (const char **)calloc(count, sizeof(char *));
    if (ids == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t k = 0; k < count; k++) {
        ids[k] = input->notes[(size_t)note_first + k].note_id;
    }
    char key[64];
    snprintf(key, sizeof(key), "e%d", claim->index);
    bool ok = standard_form_analysis_emit("length-change", key, ids, count, STANDARD_FORM_EXPANSION_CLAIM_TYPE, claim,
                                          sizeof(*claim), uncertainty, &item->shell, &item->interpretation);
    free(ids);
    return ok ? STANDARD_FORM_ANALYSIS_OK : STANDARD_FORM_ANALYSIS_ERR_ALLOC;
}

/* The ideas of the ending of a theme: [*first, *first + *count) in the segmentation's ideas. */
static bool ending_ideas(const StandardFormSegmentation *segmentation, const StandardFormThemeClaim *theme, size_t *first,
                         size_t *count) {
    size_t phrase_first = 0;
    size_t phrase_count = 0;
    if (theme->parts_are_phrases) {
        return standard_form_phrase_ideas(segmentation, theme->first_phrase + 1, first, count);
    }
    if (!standard_form_phrase_ideas(segmentation, theme->first_phrase, &phrase_first, &phrase_count)) {
        return false;
    }
    for (size_t i = 0; i < segmentation->subphrase_count; i++) {
        const StandardFormSubphraseClaim *sub = &segmentation->subphrases[i].claim;
        if (sub->phrase == theme->first_phrase && sub->index == 1) {
            *first = phrase_first + (size_t)sub->first_idea;
            *count = (size_t)sub->idea_count;
            return true;
        }
    }
    return false;
}

static unsigned techniques_of(const StandardFormInput *input, const StandardFormSegmentation *segmentation,
                              const StandardFormThemeClaim *theme, int ending_first_measure, int cadence_measure,
                              double similarity_min) {
    unsigned mask = 0;
    /* one more time: a cadence tried and evaded in the ending, and then retried (the goal cadence that ends it) */
    for (size_t c = 0; c < input->cadence_count; c++) {
        const StandardFormCadence *cadence = &input->cadences[c];
        StandardFormQ start;
        int measure = 0;
        if ((cadence->type != CADENCE_TYPE_SUBVERTED && cadence->type != CADENCE_TYPE_DECEPTIVE) ||
            !standard_form_span_q(cadence->arrival, &start, NULL) || !standard_form_measure_at(input->map, start, &measure)) {
            continue;
        }
        if (measure >= ending_first_measure && measure < cadence_measure && standard_form_is_one_more_time(true, true, true)) {
            mask |= standard_form_internal_technique_bit(INTERNAL_EXPANSION_TECHNIQUE_ONE_MORE_TIME);
        }
    }
    /* repetition: an ending idea repeats an earlier idea of the ending */
    size_t first = 0;
    size_t count = 0;
    if (ending_ideas(segmentation, theme, &first, &count)) {
        for (size_t j = 1; j < count; j++) {
            const StandardFormIdeaClaim *later = &segmentation->ideas[first + j].claim;
            for (size_t k = 0; k < j; k++) {
                const StandardFormIdeaClaim *earlier = &segmentation->ideas[first + k].claim;
                StandardFormSimilarity similarity;
                size_t earlier_count = (size_t)(earlier->note_end - earlier->note_first);
                size_t later_count = (size_t)(later->note_end - later->note_first);
                if (earlier_count > 0 && later_count > 0 &&
                    standard_form_idea_similarity(&input->notes[earlier->note_first], earlier_count,
                                                  &input->notes[later->note_first], later_count, similarity_min,
                                                  &similarity) &&
                    similarity.match && standard_form_repetition_creates_expansion(true, false)) {
                    mask |= standard_form_internal_technique_bit(INTERNAL_EXPANSION_TECHNIQUE_REPETITION);
                }
            }
        }
    }
    return mask;
}

StandardFormAnalysisStatus standard_form_detect_expansions(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                           const StandardFormSegmentation *segmentation,
                                                           const StandardFormThemes *themes,
                                                           const StandardFormConfig *config, StandardFormExpansions *out) {
    if (input == NULL || phrases == NULL || segmentation == NULL || themes == NULL || out == NULL || input->map == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardFormConfig defaults = standard_form_config_default();
    const StandardFormConfig *effective = config != NULL ? config : &defaults;
    if (!standard_form_config_valid(effective)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG;
    }
    StandardFormExpansions found = {NULL, 0};
    size_t capacity = themes->count + phrases->count;
    if (capacity > 0) {
        found.items = (StandardFormExpansion *)calloc(capacity, sizeof(StandardFormExpansion));
        if (found.items == NULL) {
            return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
    }
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_OK;
    for (size_t t = 0; t < themes->count && status == STANDARD_FORM_ANALYSIS_OK; t++) {
        const StandardFormTheme *theme = &themes->items[t];
        const StandardFormThemeClaim *tc = &theme->claim;
        if (tc->form == (int)PHRASE_LEVEL_FORM_REPEATED_PHRASE || tc->form == (int)PHRASE_LEVEL_FORM_COMPOUND_PERIOD) {
            continue; /* no expected length */
        }
        PhraseLengthChange change;
        if (!standard_form_ending_length_change((size_t)tc->first_part_length, (size_t)tc->second_part_length, &change)) {
            continue; /* balanced */
        }
        StandardFormExpansionClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.theme = tc->index;
        claim.phrase = tc->first_phrase + (tc->parts_are_phrases ? 1 : 0);
        claim.change = (int)change;
        claim.placement = (int)EXPANSION_PLACEMENT_INTERNAL;
        if (!standard_form_placement_allowed(change, EXPANSION_PLACEMENT_INTERNAL)) {
            continue;
        }
        claim.kind = change == PHRASE_LENGTH_CHANGE_EXPANSION ? (int)STANDARD_FORM_EXPANSION_INTERNAL
                                                              : (int)STANDARD_FORM_EXPANSION_CONTRACTION;
        claim.expected_length = (int)standard_form_expected_ending_length((size_t)tc->first_part_length);
        claim.actual_length = tc->second_part_length;
        claim.measures_changed = claim.actual_length > claim.expected_length ? claim.actual_length - claim.expected_length
                                                                              : claim.expected_length - claim.actual_length;
        claim.first_measure = tc->second_part_first_measure;
        claim.last_measure = tc->second_part_last_measure;
        claim.external_kind = -1;
        if (change == PHRASE_LENGTH_CHANGE_EXPANSION) {
            claim.techniques = (int)techniques_of(input, segmentation, tc, claim.first_measure, claim.last_measure,
                                                  effective->similarity_min);
        }
        size_t first = 0;
        size_t count = 0;
        if (!ending_ideas(segmentation, tc, &first, &count) || count == 0) {
            status = STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
            break;
        }
        status = emit(input, &found, &claim, theme->uncertainty, segmentation->ideas[first].claim.note_first,
                      segmentation->ideas[first + count - 1].claim.note_end);
        found.count++;
    }
    /* external expansions: the extension of a phrase */
    for (size_t p = 0; p < phrases->count && status == STANDARD_FORM_ANALYSIS_OK; p++) {
        const StandardFormPhraseClaim *pc = &phrases->items[p].claim;
        if (pc->extension_kind < 0 || pc->extension_measures <= 0) {
            continue;
        }
        StandardFormExpansionClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.theme = -1;
        for (size_t t = 0; t < themes->count; t++) {
            const StandardFormThemeClaim *tc = &themes->items[t].claim;
            if ((int)p >= tc->first_phrase && (int)p < tc->first_phrase + tc->phrase_count) {
                claim.theme = tc->index; /* the last theme that holds the phrase: a compound period over its sentences */
            }
        }
        claim.kind = (int)STANDARD_FORM_EXPANSION_EXTERNAL;
        claim.phrase = pc->index;
        claim.change = (int)PHRASE_LENGTH_CHANGE_EXPANSION;
        claim.placement = (int)EXPANSION_PLACEMENT_EXTERNAL;
        claim.expected_length = pc->length_measures;
        claim.actual_length = pc->length_measures + pc->extension_measures;
        claim.measures_changed = pc->extension_measures;
        claim.first_measure = pc->end_measure + 1;
        claim.last_measure = pc->last_measure;
        claim.external_kind = pc->extension_kind;
        claim.suffix_length_typical =
            standard_form_suffix_length_typical((ExternalExpansionKind)pc->extension_kind, (size_t)pc->extension_measures);
        StandardFormSuffixLocation location = pc->extension_kind == (int)EXTERNAL_EXPANSION_KIND_POST_CADENTIAL_EXTENSION
                                                  ? STANDARD_FORM_SUFFIX_LOCATION_WITHIN_SECTION
                                                  : STANDARD_FORM_SUFFIX_LOCATION_AFTER_SECTION;
        claim.suffix_agrees_with_location =
            standard_form_suffix_length_agrees_with_location(location, (size_t)pc->extension_measures);
        /* the extension's notes: those after the phrase proper */
        int from = -1;
        for (int i = pc->note_first; i < pc->note_end; i++) {
            StandardFormQ start;
            int measure = 0;
            if (standard_form_span_q(input->notes[i].span, &start, NULL) && standard_form_measure_at(input->map, start, &measure) &&
                measure > pc->end_measure) {
                from = i;
                break;
            }
        }
        if (from < 0) {
            continue;
        }
        status = emit(input, &found, &claim, phrases->items[p].uncertainty, from, pc->note_end);
        found.count++;
    }
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        standard_form_expansions_destroy(&found);
        return status;
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_FORM_ANALYSIS_OK;
}
