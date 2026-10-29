#include "sentence_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_SENTENCE_RULE_COUNT] = {
    "sentence-structure",
    "sentence-presentation-length",
    "sentence-basic-idea-length",
    "sentence-continuation-length",
    "sentence-fragmentation",
    "sentence-continuation-traits",
    "sentence-repetition-contour",
    "sentence-basic-idea-transformations",
    "sentence-sentential",
    "sentence-archetypal"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_SENTENCE_RULE_COUNT);

static bool valid_rule(StandardFormSentenceRule which) {
    return which >= 0 && which < STANDARD_FORM_SENTENCE_RULE_COUNT;
}

const Rule *standard_form_sentence_rule(StandardFormSentenceRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->rule : NULL;
}

const Constraint *standard_form_sentence_constraint(StandardFormSentenceRule which) {
    const RulePair *pair = valid_rule(which) ? rule_pair_table_get(&g_table, (size_t)which) : NULL;
    return pair != NULL ? pair->constraint : NULL;
}

bool standard_form_subphrase_roles_in_order(SubphraseRole first, SubphraseRole second) {
    return first == SUBPHRASE_ROLE_PRESENTATION && second == SUBPHRASE_ROLE_CONTINUATION;
}

bool standard_form_presentation_length_usual(size_t measures) {
    return measures == 4;
}

bool standard_form_basic_idea_length_valid(size_t measures) {
    return measures == 1 || measures == 2 || measures == 4;
}

bool standard_form_basic_idea_length_usual(size_t measures) {
    return measures == 2;
}

bool standard_form_continuation_length_usual(size_t presentation, size_t continuation) {
    return presentation > 0 && continuation >= presentation;
}

bool standard_form_continuation_balanced(size_t presentation, size_t continuation) {
    return presentation > 0 && presentation == continuation;
}

bool standard_form_is_fragmentation(size_t established_unit_size, size_t unit_size) {
    return unit_size > 0 && unit_size < established_unit_size;
}

unsigned standard_form_continuation_trait_bit(ContinuationTrait trait) {
    return continuation_trait_valid(trait) ? 1u << (unsigned)trait : 0u;
}

size_t standard_form_continuation_trait_count(unsigned traits) {
    size_t count = 0;
    for (int i = 0; i < CONTINUATION_TRAIT_COUNT; i++) {
        if (traits & standard_form_continuation_trait_bit((ContinuationTrait)i)) {
            count++;
        }
    }
    return count;
}

bool standard_form_fragmentation_signals_continuation(unsigned traits) {
    return (traits & standard_form_continuation_trait_bit(CONTINUATION_TRAIT_FRAGMENTATION)) != 0;
}

bool standard_form_idea_reads_as_repetition(bool same_contour) {
    return same_contour;
}

static const char *const TRANSFORMATION_NAMES[STANDARD_FORM_BASIC_IDEA_TRANSFORMATION_COUNT] = {
    "embellishment", "transposition", "change-of-harmonization", "change-of-interval"};

static const char *const TRANSFORMATION_TYPES[STANDARD_FORM_BASIC_IDEA_TRANSFORMATION_COUNT] = {
    "ornamentation", "transposition", NULL, "intervallic_manipulation"};

bool standard_form_basic_idea_transformation_valid(StandardFormBasicIdeaTransformation transformation) {
    return transformation >= 0 && transformation < STANDARD_FORM_BASIC_IDEA_TRANSFORMATION_COUNT;
}

const char *standard_form_basic_idea_transformation_name(StandardFormBasicIdeaTransformation transformation) {
    return standard_form_basic_idea_transformation_valid(transformation) ? TRANSFORMATION_NAMES[transformation]
                                                                          : NULL;
}

const char *standard_form_basic_idea_transformation_type(StandardFormBasicIdeaTransformation transformation) {
    return standard_form_basic_idea_transformation_valid(transformation) ? TRANSFORMATION_TYPES[transformation]
                                                                          : NULL;
}

bool standard_form_phrase_is_sentential(bool has_presentation, bool has_continuation) {
    return has_presentation && has_continuation;
}

bool standard_form_sentence_is_archetypal(size_t total_measures, size_t presentation_measures,
                                          size_t continuation_measures, bool has_fragmentation) {
    return (total_measures == 4 || total_measures == 8) &&
           presentation_measures + continuation_measures == total_measures &&
           standard_form_continuation_balanced(presentation_measures, continuation_measures) && has_fragmentation;
}
