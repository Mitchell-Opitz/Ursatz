#include "analysis_ideas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "core/value.h"
#include "segmentation_rules.h"
#include "idea_label.h"
#include "core/rational.h"
#include "time/meter_arithmetic.h"
#include "time/meter_map.h"
#include "time/score_time.h"
#include "sentence_rules.h"

static double combine(double a, double b) {
    return 1.0 - (1.0 - a) * (1.0 - b);
}

static void destroy_idea(StandardFormIdea *idea) {
    interpretation_destroy(idea->interpretation);
    structure_destroy(idea->shell);
    memset(idea, 0, sizeof(*idea));
}

static void destroy_subphrase(StandardFormSubphrase *subphrase) {
    interpretation_destroy(subphrase->interpretation);
    structure_destroy(subphrase->shell);
    memset(subphrase, 0, sizeof(*subphrase));
}

void standard_form_segmentation_destroy(StandardFormSegmentation *segmentation) {
    if (segmentation == NULL) {
        return;
    }
    for (size_t i = 0; i < segmentation->idea_count; i++) {
        destroy_idea(&segmentation->ideas[i]);
    }
    for (size_t i = 0; i < segmentation->subphrase_count; i++) {
        destroy_subphrase(&segmentation->subphrases[i]);
    }
    free(segmentation->ideas);
    free(segmentation->subphrases);
    memset(segmentation, 0, sizeof(*segmentation));
}

bool standard_form_phrase_ideas(const StandardFormSegmentation *segmentation, int phrase, size_t *first, size_t *count) {
    if (segmentation == NULL || first == NULL || count == NULL) {
        return false;
    }
    bool found = false;
    for (size_t i = 0; i < segmentation->idea_count; i++) {
        if (segmentation->ideas[i].claim.phrase == phrase) {
            if (!found) {
                *first = i;
                *count = 0;
                found = true;
            }
            (*count)++;
        }
    }
    return found;
}

static bool read_claim(const Interpretation *interpretation, const char *type, void *out, size_t size) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t content_size = 0;
    if (claim == NULL || strcmp(value_type(claim), type) != 0) {
        return false;
    }
    const void *content = value_content(claim, &content_size);
    if (content == NULL || content_size != size) {
        return false;
    }
    memcpy(out, content, size);
    return true;
}

bool standard_form_idea_claim_read(const Interpretation *interpretation, StandardFormIdeaClaim *out) {
    return read_claim(interpretation, STANDARD_FORM_IDEA_CLAIM_TYPE, out, sizeof(*out));
}

bool standard_form_subphrase_claim_read(const Interpretation *interpretation, StandardFormSubphraseClaim *out) {
    return read_claim(interpretation, STANDARD_FORM_SUBPHRASE_CLAIM_TYPE, out, sizeof(*out));
}

/* The notes of [note_first, note_end) whose first measure lies in [first, last]: the contiguous range, empty when none. */
static bool notes_in_measures(const StandardFormInput *input, int note_first, int note_end, int first, int last,
                              int *from, int *to) {
    *from = -1;
    *to = 0;
    for (int i = note_first; i < note_end; i++) {
        Rational start;
        int measure = 0;
        if (!score_time_span(input->notes[i].span, &start, NULL) || meter_map_measure_of(input->map, start, &measure) != METER_MAP_OK) {
            return false;
        }
        if (measure >= first && measure <= last) {
            if (*from < 0) {
                *from = i;
            }
            *to = i + 1;
        }
    }
    if (*from < 0) {
        *from = *to;
    }
    return true;
}

static StandardFormAnalysisStatus emit_idea(const StandardFormInput *input, StandardFormIdea *idea, double uncertainty) {
    idea->uncertainty = uncertainty;
    if (idea->claim.note_end <= idea->claim.note_first) {
        return STANDARD_FORM_ANALYSIS_OK; /* no notes: a claim without a shell */
    }
    size_t count = (size_t)(idea->claim.note_end - idea->claim.note_first);
    const char **ids = (const char **)calloc(count, sizeof(char *));
    if (ids == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t k = 0; k < count; k++) {
        ids[k] = input->notes[(size_t)idea->claim.note_first + k].note_id;
    }
    char key[64];
    snprintf(key, sizeof(key), "p%d-i%d", idea->claim.phrase, idea->claim.index);
    bool ok = standard_form_analysis_emit("idea", key, ids, count, STANDARD_FORM_IDEA_CLAIM_TYPE, &idea->claim,
                                          sizeof(idea->claim), uncertainty, &idea->shell, &idea->interpretation);
    free(ids);
    return ok ? STANDARD_FORM_ANALYSIS_OK : STANDARD_FORM_ANALYSIS_ERR_ALLOC;
}

static StandardFormAnalysisStatus emit_subphrase(const StandardFormInput *input, StandardFormSubphrase *subphrase,
                                                 double uncertainty) {
    subphrase->uncertainty = uncertainty;
    if (subphrase->claim.note_end <= subphrase->claim.note_first) {
        return STANDARD_FORM_ANALYSIS_OK;
    }
    size_t count = (size_t)(subphrase->claim.note_end - subphrase->claim.note_first);
    const char **ids = (const char **)calloc(count, sizeof(char *));
    if (ids == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t k = 0; k < count; k++) {
        ids[k] = input->notes[(size_t)subphrase->claim.note_first + k].note_id;
    }
    char key[64];
    snprintf(key, sizeof(key), "p%d-s%d", subphrase->claim.phrase, subphrase->claim.index);
    bool ok = standard_form_analysis_emit("subphrase", key, ids, count, STANDARD_FORM_SUBPHRASE_CLAIM_TYPE,
                                          &subphrase->claim, sizeof(subphrase->claim), uncertainty, &subphrase->shell,
                                          &subphrase->interpretation);
    free(ids);
    return ok ? STANDARD_FORM_ANALYSIS_OK : STANDARD_FORM_ANALYSIS_ERR_ALLOC;
}

StandardFormAnalysisStatus standard_form_segment_ideas(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                       const StandardFormConfig *config, StandardFormSegmentation *out) {
    if (input == NULL || phrases == NULL || out == NULL || input->map == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardFormConfig defaults = standard_form_config_default();
    const StandardFormConfig *effective = config != NULL ? config : &defaults;
    if (!standard_form_config_valid(effective)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG;
    }
    StandardFormSegmentation found = {NULL, 0, NULL, 0};
    size_t idea_capacity = 0;
    size_t subphrase_capacity = 0;
    for (size_t p = 0; p < phrases->count; p++) {
        int n = phrases->items[p].claim.length_measures;
        if (n > 0) {
            size_t k = n / 2 < 1 ? 1 : (size_t)(n / 2);
            idea_capacity += k;
            subphrase_capacity += k >= 3 ? 2 : 0;
        }
    }
    if (idea_capacity > 0) {
        found.ideas = (StandardFormIdea *)calloc(idea_capacity, sizeof(StandardFormIdea));
    }
    if (subphrase_capacity > 0) {
        found.subphrases = (StandardFormSubphrase *)calloc(subphrase_capacity, sizeof(StandardFormSubphrase));
    }
    if ((idea_capacity > 0 && found.ideas == NULL) || (subphrase_capacity > 0 && found.subphrases == NULL)) {
        free(found.ideas);
        free(found.subphrases);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_OK;
    for (size_t p = 0; p < phrases->count && status == STANDARD_FORM_ANALYSIS_OK; p++) {
        const StandardFormPhrase *phrase = &phrases->items[p];
        int n = phrase->claim.length_measures;
        if (n <= 0) {
            continue;
        }
        size_t k = n / 2 < 1 ? 1 : (size_t)(n / 2);
        size_t lengths[64];
        if (k > 64) {
            status = STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
            break;
        }
        for (size_t j = 0; j < k; j++) {
            lengths[j] = j + 1 < k ? 2 : (size_t)n - 2 * (k - 1);
        }
        if (!standard_form_ideas_fill_phrase(lengths, k, (size_t)n)) {
            status = STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
            break;
        }
        double penalty = n % 2 != 0 ? STANDARD_FORM_ODD_LENGTH_PENALTY : 0.0;
        size_t first_idea = found.idea_count;
        int measure = phrase->claim.counted_first;
        for (size_t j = 0; j < k && status == STANDARD_FORM_ANALYSIS_OK; j++) {
            StandardFormIdea *idea = &found.ideas[found.idea_count];
            StandardFormIdeaClaim *claim = &idea->claim;
            claim->phrase = phrase->claim.index;
            claim->index = (int)j;
            claim->first_measure = j == 0 ? phrase->claim.first_measure : measure;
            claim->last_measure = measure + (int)lengths[j] - 1;
            claim->length_measures = (int)lengths[j];
            measure += (int)lengths[j];
            if (!notes_in_measures(input, phrase->claim.note_first, phrase->claim.note_end, claim->first_measure,
                                   claim->last_measure, &claim->note_first, &claim->note_end)) {
                status = STANDARD_FORM_ANALYSIS_ERR_METER;
                break;
            }
            double u_similarity = 0.0;
            claim->match = -1;
            if (j == 0) {
                claim->label = (int)IDEA_LABEL_BASIC_IDEA;
            } else {
                const StandardFormIdeaClaim *basic = &found.ideas[first_idea].claim;
                StandardFormSimilarity similarity;
                size_t basic_count = (size_t)(basic->note_end - basic->note_first);
                size_t count = (size_t)(claim->note_end - claim->note_first);
                if (!standard_form_idea_similarity(basic_count > 0 ? &input->notes[basic->note_first] : NULL, basic_count,
                                                   count > 0 ? &input->notes[claim->note_first] : NULL, count,
                                                   effective->similarity_min, &similarity)) {
                    status = STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG;
                    break;
                }
                claim->compared = similarity.comparable ? 1 : 0;
                claim->similarity = similarity.similarity;
                claim->match = similarity.match ? 1 : 0;
                claim->margin = similarity.margin;
                u_similarity = standard_form_similarity_uncertainty(&similarity);
                if (similarity.match) {
                    claim->label = (int)IDEA_LABEL_BASIC_IDEA;
                } else if (standard_form_is_fragmentation((size_t)basic->length_measures, lengths[j])) {
                    claim->label = (int)IDEA_LABEL_FRAGMENT;
                } else {
                    claim->label = (int)IDEA_LABEL_CONTRASTING_IDEA;
                }
            }
            status = emit_idea(input, idea, combine(combine(phrase->uncertainty, u_similarity), penalty));
            found.idea_count++;
        }
        if (status != STANDARD_FORM_ANALYSIS_OK) {
            break;
        }
        if (k >= 3) {
            const size_t starts[2] = {0, 2};
            const size_t counts[2] = {2, k - 2};
            for (size_t s = 0; s < 2 && status == STANDARD_FORM_ANALYSIS_OK; s++) {
                StandardFormSubphrase *subphrase = &found.subphrases[found.subphrase_count];
                StandardFormSubphraseClaim *claim = &subphrase->claim;
                const StandardFormIdeaClaim *first = &found.ideas[first_idea + starts[s]].claim;
                const StandardFormIdeaClaim *last = &found.ideas[first_idea + starts[s] + counts[s] - 1].claim;
                claim->phrase = phrase->claim.index;
                claim->index = (int)s;
                claim->first_idea = (int)starts[s];
                claim->idea_count = (int)counts[s];
                claim->first_measure = first->first_measure;
                claim->last_measure = last->last_measure;
                claim->note_first = first->note_first;
                claim->note_end = last->note_end;
                for (size_t j = 0; j < counts[s]; j++) {
                    claim->length_measures += found.ideas[first_idea + starts[s] + j].claim.length_measures;
                }
                status = emit_subphrase(input, subphrase, combine(phrase->uncertainty, penalty));
                found.subphrase_count++;
            }
        }
    }
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        standard_form_segmentation_destroy(&found);
        return status;
    }
    if (found.idea_count == 0) {
        free(found.ideas);
        found.ideas = NULL;
    }
    if (found.subphrase_count == 0) {
        free(found.subphrases);
        found.subphrases = NULL;
    }
    *out = found;
    return STANDARD_FORM_ANALYSIS_OK;
}
