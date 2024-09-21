#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_SENTENCE_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_SENTENCE_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"
#include "continuation_trait.h"
#include "semantics/subphrase_role.h"

/*
 * Sentence rules for standard-form-v1. A sentence is a special kind of phrase made of two
 * subphrases, a presentation and a continuation:
 *
 *   STRUCTURE                  a presentation comes first, then a continuation
 *   PRESENTATION_LENGTH        a presentation is often four measures
 *   BASIC_IDEA_LENGTH          a basic idea is often two measures, with one and four
 *                              also occurring
 *   CONTINUATION_LENGTH        a continuation is often as long as the presentation, just as
 *                              often longer, and uncommonly shorter
 *   FRAGMENTATION              fragmentation makes unit sizes shorter than the units
 *                              before it, and says nothing about their melodic content
 *   CONTINUATION_TRAITS        a continuation is marked by fragmentation, faster durations,
 *                              sequence, and faster harmonic rhythm; it is easier to
 *                              identify with more of them, and fragmentation alone usually
 *                              signals it
 *   REPETITION_CONTOUR         a second idea with the same contour as the first is heard as
 *                              its varied repetition; a different contour is heard as
 *                              contrast
 *   BASIC_IDEA_TRANSFORMATIONS how a basic idea is commonly varied when repeated
 *   SENTENTIAL                 at a minimum a sentential phrase has a presentation and a
 *                              continuation
 *   ARCHETYPAL                 the archetypal sentence is 4 or 8 measures with a continuation
 *                              that balances the presentation and has fragmentation
 *
 * Lengths are in a single consistent unit chosen by the caller (measures in the examples).
 */

typedef enum {
    STANDARD_FORM_SENTENCE_RULE_STRUCTURE = 0,
    STANDARD_FORM_SENTENCE_RULE_PRESENTATION_LENGTH,
    STANDARD_FORM_SENTENCE_RULE_BASIC_IDEA_LENGTH,
    STANDARD_FORM_SENTENCE_RULE_CONTINUATION_LENGTH,
    STANDARD_FORM_SENTENCE_RULE_FRAGMENTATION,
    STANDARD_FORM_SENTENCE_RULE_CONTINUATION_TRAITS,
    STANDARD_FORM_SENTENCE_RULE_REPETITION_CONTOUR,
    STANDARD_FORM_SENTENCE_RULE_BASIC_IDEA_TRANSFORMATIONS,
    STANDARD_FORM_SENTENCE_RULE_SENTENTIAL,
    STANDARD_FORM_SENTENCE_RULE_ARCHETYPAL,
    STANDARD_FORM_SENTENCE_RULE_COUNT
} StandardFormSentenceRule;

const Rule *standard_form_sentence_rule(StandardFormSentenceRule which);
const Constraint *standard_form_sentence_constraint(StandardFormSentenceRule which);

/* STRUCTURE: true only for a presentation followed by a continuation. */
bool standard_form_subphrase_roles_in_order(SubphraseRole first, SubphraseRole second);

/* PRESENTATION_LENGTH: true for the usual length of four. Any other length is unusual. */
bool standard_form_presentation_length_usual(size_t measures);

/* BASIC_IDEA_LENGTH: true for the lengths 1, 2, and 4. */
bool standard_form_basic_idea_length_valid(size_t measures);

/* BASIC_IDEA_LENGTH: true for the most usual basic-idea length, two. */
bool standard_form_basic_idea_length_usual(size_t measures);

/* CONTINUATION_LENGTH: true when the continuation is at least as long as the presentation (a
   zero-length presentation is not valid). A shorter continuation is uncommon, not wrong. */
bool standard_form_continuation_length_usual(size_t presentation, size_t continuation);

/* CONTINUATION_LENGTH: true when the two lengths are equal and non-zero, the balance of the
   archetypal sentence. */
bool standard_form_continuation_balanced(size_t presentation, size_t continuation);

/* FRAGMENTATION: true when unit_size is shorter than established_unit_size and not zero. Both
   are in the same unit. Only length counts, not melodic content. */
bool standard_form_is_fragmentation(size_t established_unit_size, size_t unit_size);

/* CONTINUATION_TRAITS: the bit for a trait in a trait mask. Zero for an invalid trait. */
unsigned standard_form_continuation_trait_bit(ContinuationTrait trait);

/* CONTINUATION_TRAITS: how many distinct traits a mask holds. */
size_t standard_form_continuation_trait_count(unsigned traits);

/* CONTINUATION_TRAITS: true when the mask holds fragmentation, which usually signals a
   continuation even with no other trait. A continuation without it is still possible. */
bool standard_form_fragmentation_signals_continuation(unsigned traits);

/* REPETITION_CONTOUR: true when a second idea reads as a varied repetition of the first
   (same contour); false when it reads as contrast. */
bool standard_form_idea_reads_as_repetition(bool same_contour);

typedef enum {
    STANDARD_FORM_BASIC_IDEA_TRANSFORMATION_EMBELLISHMENT = 0,
    STANDARD_FORM_BASIC_IDEA_TRANSFORMATION_TRANSPOSITION,
    STANDARD_FORM_BASIC_IDEA_TRANSFORMATION_CHANGE_OF_HARMONIZATION,
    STANDARD_FORM_BASIC_IDEA_TRANSFORMATION_CHANGE_OF_INTERVAL,
    STANDARD_FORM_BASIC_IDEA_TRANSFORMATION_COUNT
} StandardFormBasicIdeaTransformation;

/* True for the four transformations above; false for any other value. */
bool standard_form_basic_idea_transformation_valid(StandardFormBasicIdeaTransformation transformation);

/* "embellishment" "transposition" "change-of-harmonization" "change-of-interval"; NULL for an
   invalid value. */
const char *standard_form_basic_idea_transformation_name(StandardFormBasicIdeaTransformation transformation);

/*
 * The Transformation subtype that carries it out, by its src/transform/ file name:
 * "ornamentation" "transposition" NULL "intervallic_manipulation". Change of harmonization
 * has no NoteEvent transformation, since it changes the chords and not the notes of the idea,
 * so it has no type. NULL for an invalid value too.
 */
const char *standard_form_basic_idea_transformation_type(StandardFormBasicIdeaTransformation transformation);

/* SENTENTIAL: true when both subphrases are present. */
bool standard_form_phrase_is_sentential(bool has_presentation, bool has_continuation);

/* ARCHETYPAL: true when the sentence is 4 or 8 measures, the presentation and continuation
   fill it and balance, and the continuation has fragmentation. */
bool standard_form_sentence_is_archetypal(size_t total_measures, size_t presentation_measures,
                                          size_t continuation_measures, bool has_fragmentation);

#endif
