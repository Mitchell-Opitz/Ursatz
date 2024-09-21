#include "analysis_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "beginning_rules.h"
#include "combination_rules.h"
#include "core/value.h"
#include "ending_rules.h"
#include "hybrid_rules.h"
#include "period_rules.h"
#include "phrase_rules.h"
#include "idea_label.h"
#include "phrase_beginning.h"
#include "phrase_ending.h"
#include "phrase_level_form.h"
#include "sentence_rules.h"

static double combine(double a, double b) {
    return 1.0 - (1.0 - a) * (1.0 - b);
}

static void destroy_theme(StandardFormTheme *theme) {
    interpretation_destroy(theme->interpretation);
    relationship_destroy(theme->relationship);
    structure_destroy(theme->shell);
    memset(theme, 0, sizeof(*theme));
}

void standard_form_themes_destroy(StandardFormThemes *themes) {
    if (themes == NULL) {
        return;
    }
    for (size_t i = 0; i < themes->count; i++) {
        destroy_theme(&themes->items[i]);
    }
    free(themes->items);
    themes->items = NULL;
    themes->count = 0;
}

bool standard_form_theme_claim_read(const Interpretation *interpretation, StandardFormThemeClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), STANDARD_FORM_THEME_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}

typedef struct {
    const StandardFormInput *input;
    const StandardFormPhrases *phrases;
    const StandardFormSegmentation *segmentation;
    const StandardFormConfig *config;
} ThemeContext;

typedef struct {
    PhraseBeginning beginning;
    PhraseEnding ending;
    PhraseLevelForm form;
    int begin_len;
    int end_len;
    int begin_first;
    int begin_last;
    int end_first;
    int end_last;
    bool fragmentation;
    StandardFormSimilarity similarity;
    const StandardFormSubphrase *begin_part; /* one-phrase themes only */
    const StandardFormSubphrase *end_part;
} Candidate;

static bool phrase_goal(const StandardFormPhrase *phrase) {
    return phrase->claim.signal == (int)STANDARD_FORM_SIGNAL_CADENCE && phrase->claim.cadence_type >= 0 &&
           standard_form_cadence_ends_phrase((CadenceType)phrase->claim.cadence_type);
}

static IdeaLabel as_basic_or_contrast(int label) {
    return label == (int)IDEA_LABEL_BASIC_IDEA ? IDEA_LABEL_BASIC_IDEA : IDEA_LABEL_CONTRASTING_IDEA;
}

static const StandardFormIdea *ideas_of(const ThemeContext *ctx, size_t phrase, size_t *count) {
    size_t first = 0;
    if (!standard_form_phrase_ideas(ctx->segmentation, (int)phrase, &first, count)) {
        *count = 0;
        return NULL;
    }
    return &ctx->segmentation->ideas[first];
}

static StandardFormSimilarity similarity_of_claim(const StandardFormIdeaClaim *claim) {
    StandardFormSimilarity similarity = {claim->compared != 0, 0, 0, claim->similarity, claim->match == 1, claim->margin};
    return similarity;
}

/* A one-phrase theme: a beginning subphrase and an ending subphrase, ending in a phrase-goal cadence. */
static bool one_phrase(const ThemeContext *ctx, size_t p, Candidate *out) {
    const StandardFormPhrase *phrase = &ctx->phrases->items[p];
    size_t count = 0;
    const StandardFormIdea *ideas = ideas_of(ctx, p, &count);
    if (!phrase_goal(phrase) || count < 3) {
        return false;
    }
    const StandardFormSubphrase *begin = NULL;
    const StandardFormSubphrase *end = NULL;
    for (size_t i = 0; i < ctx->segmentation->subphrase_count; i++) {
        const StandardFormSubphrase *sub = &ctx->segmentation->subphrases[i];
        if (sub->claim.phrase == (int)p) {
            if (sub->claim.index == 0) {
                begin = sub;
            } else {
                end = sub;
            }
        }
    }
    if (begin == NULL || end == NULL) {
        return false;
    }
    Candidate c;
    memset(&c, 0, sizeof(c));
    c.similarity = similarity_of_claim(&ideas[1].claim);
    if (!standard_form_classify_beginning(false, IDEA_LABEL_BASIC_IDEA, as_basic_or_contrast(ideas[1].claim.label),
                                          &c.beginning)) {
        return false;
    }
    size_t end_first = (size_t)end->claim.first_idea;
    bool basic_then_contrasting = end->claim.idea_count >= 2 &&
                                  ideas[end_first].claim.label == (int)IDEA_LABEL_BASIC_IDEA &&
                                  ideas[end_first + 1].claim.label != (int)IDEA_LABEL_BASIC_IDEA;
    for (int k = 0; k < end->claim.idea_count; k++) {
        c.fragmentation = c.fragmentation || standard_form_is_fragmentation((size_t)ideas[0].claim.length_measures,
                                                                              (size_t)ideas[end_first + (size_t)k].claim.length_measures);
    }
    if (!standard_form_classify_ending((CadenceType)phrase->claim.cadence_type, false, basic_then_contrasting,
                                       c.fragmentation, &c.ending) ||
        !standard_form_pairing_form(c.beginning, c.ending, &c.form)) {
        return false;
    }
    c.begin_len = begin->claim.length_measures;
    c.end_len = end->claim.length_measures;
    c.begin_first = begin->claim.first_measure;
    c.begin_last = begin->claim.last_measure;
    c.end_first = end->claim.first_measure;
    c.end_last = end->claim.last_measure;
    c.begin_part = begin;
    c.end_part = end;
    *out = c;
    return true;
}

/* A two-phrase theme: an antecedent phrase and a phrase with a stronger cadence. */
static bool two_phrase(const ThemeContext *ctx, size_t p, Candidate *out) {
    if (p + 1 >= ctx->phrases->count) {
        return false;
    }
    const StandardFormPhrase *first = &ctx->phrases->items[p];
    const StandardFormPhrase *second = &ctx->phrases->items[p + 1];
    size_t first_count = 0;
    size_t second_count = 0;
    const StandardFormIdea *first_ideas = ideas_of(ctx, p, &first_count);
    const StandardFormIdea *second_ideas = ideas_of(ctx, p + 1, &second_count);
    if (!phrase_goal(first) || !phrase_goal(second) || first_count != 2 || second_count < 1 ||
        second->claim.strength_rank <= first->claim.strength_rank) {
        return false;
    }
    Candidate c;
    memset(&c, 0, sizeof(c));
    if (!standard_form_classify_beginning(true, IDEA_LABEL_BASIC_IDEA, as_basic_or_contrast(first_ideas[1].claim.label),
                                          &c.beginning) ||
        c.beginning != PHRASE_BEGINNING_ANTECEDENT) {
        return false;
    }
    bool basic_then_contrasting = second_count >= 2 && second_ideas[1].claim.label != (int)IDEA_LABEL_BASIC_IDEA;
    for (size_t k = 0; k < second_count; k++) {
        c.fragmentation = c.fragmentation || standard_form_is_fragmentation((size_t)first_ideas[0].claim.length_measures,
                                                                              (size_t)second_ideas[k].claim.length_measures);
    }
    if (!standard_form_classify_ending((CadenceType)second->claim.cadence_type, false, basic_then_contrasting,
                                       c.fragmentation, &c.ending) ||
        !standard_form_pairing_form(c.beginning, c.ending, &c.form)) {
        return false;
    }
    if (c.form == PHRASE_LEVEL_FORM_PERIOD &&
        !standard_form_period_cadences_valid((CadenceType)first->claim.cadence_type,
                                             (CadenceType)second->claim.cadence_type)) {
        return false;
    }
    size_t a_count = (size_t)(first_ideas[0].claim.note_end - first_ideas[0].claim.note_first);
    size_t b_count = (size_t)(second_ideas[0].claim.note_end - second_ideas[0].claim.note_first);
    if (!standard_form_idea_similarity(a_count > 0 ? &ctx->input->notes[first_ideas[0].claim.note_first] : NULL, a_count,
                                       b_count > 0 ? &ctx->input->notes[second_ideas[0].claim.note_first] : NULL, b_count,
                                       ctx->config->similarity_min, &c.similarity)) {
        return false;
    }
    c.begin_len = first->claim.length_measures;
    c.end_len = second->claim.length_measures;
    c.begin_first = first->claim.first_measure;
    c.begin_last = first->claim.end_measure;
    c.end_first = second->claim.first_measure;
    c.end_last = second->claim.end_measure;
    *out = c;
    return true;
}

/* The notes of phrase p's ideas (the phrase proper). */
static bool phrase_notes(const ThemeContext *ctx, size_t p, const StandardFormNote **notes, size_t *count) {
    size_t n = 0;
    const StandardFormIdea *ideas = ideas_of(ctx, p, &n);
    if (n == 0) {
        return false;
    }
    int from = ideas[0].claim.note_first;
    int to = ideas[n - 1].claim.note_end;
    if (to <= from) {
        return false;
    }
    *notes = &ctx->input->notes[from];
    *count = (size_t)(to - from);
    return true;
}

static StandardFormAnalysisStatus emit_theme(const ThemeContext *ctx, StandardFormThemes *themes, StandardFormThemeClaim *claim,
                                             double uncertainty, int note_first, int note_end, const char *part_a,
                                             const char *part_b) {
    StandardFormTheme *theme = &themes->items[themes->count];
    claim->index = (int)themes->count;
    theme->claim = *claim;
    theme->uncertainty = uncertainty;
    if (note_end <= note_first || part_a == NULL || part_b == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    size_t count = (size_t)(note_end - note_first);
    const char **ids = (const char **)calloc(count, sizeof(char *));
    if (ids == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t k = 0; k < count; k++) {
        ids[k] = ctx->input->notes[(size_t)note_first + k].note_id;
    }
    char key[64];
    char shell_id[128];
    snprintf(key, sizeof(key), "t%d", claim->index);
    snprintf(shell_id, sizeof(shell_id), "standard-form-theme-notes-%s", key);
    theme->shell = standard_form_analysis_group(shell_id, ids, count);
    free(ids);
    if (theme->shell == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    theme->relationship = standard_form_analysis_relationship("theme", key, part_a, part_b,
                                                              phrase_level_form_name((PhraseLevelForm)claim->form));
    if (theme->relationship == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    char interpretation_id[128];
    snprintf(interpretation_id, sizeof(interpretation_id), "standard-form-theme-%s", key);
    theme->interpretation = standard_form_analysis_interpretation(
        interpretation_id, entity_id_value(relationship_id(theme->relationship)), STANDARD_FORM_THEME_CLAIM_TYPE, claim,
        sizeof(*claim), uncertainty, "theme-identification");
    return theme->interpretation != NULL ? STANDARD_FORM_ANALYSIS_OK : STANDARD_FORM_ANALYSIS_ERR_ALLOC;
}

static void fill_common(StandardFormThemeClaim *claim, const Candidate *c) {
    claim->form = (int)c->form;
    claim->beginning = (int)c->beginning;
    claim->ending = (int)c->ending;
    StandardFormCommonality commonality;
    claim->commonality = standard_form_pairing_commonality(c->beginning, c->ending, &commonality) ? (int)commonality : -1;
    claim->first_part_length = c->begin_len;
    claim->first_part_first_measure = c->begin_first;
    claim->first_part_last_measure = c->begin_last;
    claim->second_part_length = c->end_len;
    claim->second_part_first_measure = c->end_first;
    claim->second_part_last_measure = c->end_last;
    claim->has_fragmentation = c->fragmentation;
    claim->similarity_compared = c->similarity.comparable;
    claim->similarity = c->similarity.similarity;
    claim->similarity_match = c->similarity.match;
    claim->similarity_margin = c->similarity.margin;
    claim->first_cadence_type = -1;
    claim->second_cadence_type = -1;
}

static const char *phrase_part_id(const StandardFormPhrase *phrase) {
    return entity_id_value(phrase_occurrence_id(phrase->occurrence));
}

static const char *subphrase_part_id(const StandardFormSubphrase *sub) {
    return sub != NULL && sub->shell != NULL ? entity_id_value(entity_id(structure_entity(sub->shell))) : NULL;
}

static StandardFormAnalysisStatus emit_one_phrase(const ThemeContext *ctx, StandardFormThemes *themes, size_t p, const Candidate *c,
                                                  bool part_of_compound) {
    const StandardFormPhrase *phrase = &ctx->phrases->items[p];
    StandardFormThemeClaim claim;
    memset(&claim, 0, sizeof(claim));
    fill_common(&claim, c);
    claim.first_phrase = (int)p;
    claim.phrase_count = 1;
    claim.first_measure = phrase->claim.first_measure;
    claim.last_measure = phrase->claim.end_measure;
    claim.length_measures = phrase->claim.length_measures;
    claim.second_cadence_type = phrase->claim.cadence_type;
    claim.second_strength_rank = phrase->claim.strength_rank;
    claim.cadences_usual = 1;
    claim.lengths_usual = standard_form_presentation_length_usual((size_t)c->begin_len) &&
                          standard_form_continuation_length_usual((size_t)c->begin_len, (size_t)c->end_len);
    claim.archetypal = c->form == PHRASE_LEVEL_FORM_SENTENCE &&
                       standard_form_sentence_is_archetypal((size_t)claim.length_measures, (size_t)c->begin_len,
                                                            (size_t)c->end_len, c->fragmentation);
    claim.part_of_compound = part_of_compound;
    double uncertainty = combine(phrase->uncertainty, standard_form_similarity_uncertainty(&c->similarity));
    return emit_theme(ctx, themes, &claim, uncertainty, c->begin_part->claim.note_first, c->end_part->claim.note_end,
                      subphrase_part_id(c->begin_part), subphrase_part_id(c->end_part));
}

static StandardFormAnalysisStatus emit_two_phrases(const ThemeContext *ctx, StandardFormThemes *themes, size_t p,
                                                   PhraseLevelForm form, const Candidate *c) {
    const StandardFormPhrase *first = &ctx->phrases->items[p];
    const StandardFormPhrase *second = &ctx->phrases->items[p + 1];
    StandardFormThemeClaim claim;
    memset(&claim, 0, sizeof(claim));
    if (c != NULL) {
        fill_common(&claim, c);
    } else {
        claim.beginning = -1;
        claim.ending = -1;
        claim.commonality = -1;
        claim.first_cadence_type = -1;
        claim.second_cadence_type = -1;
    }
    claim.form = (int)form;
    if (form == PHRASE_LEVEL_FORM_REPEATED_PHRASE || form == PHRASE_LEVEL_FORM_COMPOUND_PERIOD) {
        claim.beginning = -1; /* no beginning or ending of its own: the parts are whole phrases */
        claim.ending = -1;
        claim.commonality = -1;
        claim.has_fragmentation = 0;
    }
    claim.parts_are_phrases = 1;
    claim.first_phrase = (int)p;
    claim.phrase_count = 2;
    claim.first_measure = first->claim.first_measure;
    claim.last_measure = second->claim.end_measure;
    claim.length_measures = first->claim.length_measures + second->claim.length_measures;
    claim.first_part_length = first->claim.length_measures;
    claim.first_part_first_measure = first->claim.first_measure;
    claim.first_part_last_measure = first->claim.end_measure;
    claim.second_part_length = second->claim.length_measures;
    claim.second_part_first_measure = second->claim.first_measure;
    claim.second_part_last_measure = second->claim.end_measure;
    claim.first_cadence_type = first->claim.cadence_type;
    claim.second_cadence_type = second->claim.cadence_type;
    claim.first_strength_rank = first->claim.strength_rank;
    claim.second_strength_rank = second->claim.strength_rank;
    if (form == PHRASE_LEVEL_FORM_PERIOD || form == PHRASE_LEVEL_FORM_HYBRID) {
        claim.cadences_usual = standard_form_antecedent_cadence_usual((CadenceType)first->claim.cadence_type) &&
                               standard_form_consequent_cadence_usual((CadenceType)second->claim.cadence_type);
        claim.lengths_usual = standard_form_antecedent_length_usual((size_t)first->claim.length_measures) &&
                              standard_form_consequent_length_usual((size_t)first->claim.length_measures,
                                                                    (size_t)second->claim.length_measures);
    }
    double u_similarity = c != NULL ? standard_form_similarity_uncertainty(&c->similarity) : 0.5;
    /* u_cadences is the greatest of the two phrases' */
    double u_cadences = first->uncertainty > second->uncertainty ? first->uncertainty : second->uncertainty;
    size_t first_count = 0;
    size_t second_count = 0;
    const StandardFormIdea *first_ideas = ideas_of(ctx, p, &first_count);
    const StandardFormIdea *second_ideas = ideas_of(ctx, p + 1, &second_count);
    if (first_count == 0 || second_count == 0) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    return emit_theme(ctx, themes, &claim, combine(u_cadences, u_similarity), first_ideas[0].claim.note_first,
                      second_ideas[second_count - 1].claim.note_end, phrase_part_id(first), phrase_part_id(second));
}

StandardFormAnalysisStatus standard_form_identify_themes(const StandardFormInput *input, const StandardFormPhrases *phrases,
                                                         const StandardFormSegmentation *segmentation,
                                                         const StandardFormConfig *config, StandardFormThemes *out) {
    if (input == NULL || phrases == NULL || segmentation == NULL || out == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardFormConfig defaults = standard_form_config_default();
    const StandardFormConfig *effective = config != NULL ? config : &defaults;
    if (!standard_form_config_valid(effective)) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_CONFIG;
    }
    ThemeContext ctx = {input, phrases, segmentation, effective};
    StandardFormThemes found = {NULL, 0};
    if (phrases->count > 0) {
        found.items = (StandardFormTheme *)calloc(phrases->count * 2, sizeof(StandardFormTheme));
        if (found.items == NULL) {
            return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
    }
    StandardFormAnalysisStatus status = STANDARD_FORM_ANALYSIS_OK;
    size_t p = 0;
    while (p < phrases->count && status == STANDARD_FORM_ANALYSIS_OK) {
        const StandardFormPhrase *phrase = &phrases->items[p];
        bool has_next = p + 1 < phrases->count;
        size_t consumed = 1;
        Candidate first_single;
        Candidate second_single;
        Candidate pair;
        bool single = one_phrase(&ctx, p, &first_single);
        if (has_next && phrase_goal(phrase) && phrase_goal(&phrases->items[p + 1])) {
            const StandardFormPhrase *next = &phrases->items[p + 1];
            PhraseLevelForm pair_form;
            const StandardFormNote *a = NULL;
            const StandardFormNote *b = NULL;
            size_t a_count = 0;
            size_t b_count = 0;
            StandardFormSimilarity phrase_similarity;
            bool repeats = phrase_notes(&ctx, p, &a, &a_count) && phrase_notes(&ctx, p + 1, &b, &b_count) &&
                           standard_form_idea_similarity(a, a_count, b, b_count, effective->similarity_min,
                                                         &phrase_similarity) &&
                           phrase_similarity.match;
            if (repeats && phrase->claim.cadence_type == next->claim.cadence_type &&
                standard_form_classify_phrase_pair((CadenceType)phrase->claim.cadence_type,
                                                   (CadenceType)next->claim.cadence_type, true, PHRASE_LEVEL_FORM_UNIQUE,
                                                   PHRASE_LEVEL_FORM_UNIQUE, &pair_form) &&
                pair_form == PHRASE_LEVEL_FORM_REPEATED_PHRASE) {
                Candidate repeated;
                memset(&repeated, 0, sizeof(repeated));
                repeated.form = PHRASE_LEVEL_FORM_REPEATED_PHRASE;
                repeated.similarity = phrase_similarity;
                status = emit_two_phrases(&ctx, &found, p, PHRASE_LEVEL_FORM_REPEATED_PHRASE, &repeated);
                found.count++;
                p += 2;
                continue;
            }
            if (single && first_single.form == PHRASE_LEVEL_FORM_SENTENCE && one_phrase(&ctx, p + 1, &second_single) &&
                second_single.form == PHRASE_LEVEL_FORM_SENTENCE && next->claim.strength_rank > phrase->claim.strength_rank &&
                standard_form_classify_phrase_pair((CadenceType)phrase->claim.cadence_type,
                                                   (CadenceType)next->claim.cadence_type, false, PHRASE_LEVEL_FORM_SENTENCE,
                                                   PHRASE_LEVEL_FORM_SENTENCE, &pair_form) &&
                pair_form == PHRASE_LEVEL_FORM_COMPOUND_PERIOD) {
                status = emit_one_phrase(&ctx, &found, p, &first_single, true);
                found.count++;
                if (status == STANDARD_FORM_ANALYSIS_OK) {
                    status = emit_one_phrase(&ctx, &found, p + 1, &second_single, true);
                    found.count++;
                }
                if (status == STANDARD_FORM_ANALYSIS_OK) {
                    Candidate compound;
                    memset(&compound, 0, sizeof(compound));
                    compound.form = PHRASE_LEVEL_FORM_COMPOUND_PERIOD;
                    size_t ca = 0;
                    size_t cb = 0;
                    size_t ia = 0;
                    size_t ib = 0;
                    const StandardFormIdea *first_ideas = ideas_of(&ctx, p, &ca);
                    const StandardFormIdea *second_ideas = ideas_of(&ctx, p + 1, &cb);
                    (void)ia;
                    (void)ib;
                    size_t na = (size_t)(first_ideas[0].claim.note_end - first_ideas[0].claim.note_first);
                    size_t nb = (size_t)(second_ideas[0].claim.note_end - second_ideas[0].claim.note_first);
                    standard_form_idea_similarity(&input->notes[first_ideas[0].claim.note_first], na,
                                                  &input->notes[second_ideas[0].claim.note_first], nb,
                                                  effective->similarity_min, &compound.similarity);
                    status = emit_two_phrases(&ctx, &found, p, PHRASE_LEVEL_FORM_COMPOUND_PERIOD, &compound);
                    found.count++;
                }
                p += 2;
                continue;
            }
            if (two_phrase(&ctx, p, &pair)) {
                status = emit_two_phrases(&ctx, &found, p, pair.form, &pair);
                found.count++;
                p += 2;
                continue;
            }
        }
        if (single) {
            status = emit_one_phrase(&ctx, &found, p, &first_single, false);
            found.count++;
        }
        p += consumed;
    }
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        standard_form_themes_destroy(&found);
        return status;
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_FORM_ANALYSIS_OK;
}
