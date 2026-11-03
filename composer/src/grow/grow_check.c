#include <ursatz_composer/grow.h>

#include <stdio.h>
#include <string.h>

#include <standard-harmony/harmony_cadence_rules.h>
#include <standard-form/hybrid_rules.h>
#include <standard-form/idea_label.h>
#include <standard-form/period_rules.h>
#include <standard-form/phrase_beginning.h>
#include <standard-form/phrase_ending.h>
#include <standard-form/phrase_level_form.h>
#include <standard-form/phrase_role.h>
#include <semantics/subphrase_role.h>
#include <standard-form/sentence_rules.h>

#include <ursatz_composer/harmony_phrase.h>

#include "grow/grow_internal.h"

#define TICKS_PER_WHOLE 128L

/* A length as a whole number of ticks, the unit the rules below are asked in; 0 when it is not a whole number. */
static size_t ticks_of(Rational time) {
    long scaled = time.num * TICKS_PER_WHOLE;
    return time.den != 0 && scaled > 0 && scaled % (long)time.den == 0 ? (size_t)(scaled / (long)time.den) : 0;
}

/* The soft rules that hold, out of those asked. */
typedef struct {
    unsigned held;
    unsigned total;
} Soft;

static void ask(Soft *soft, bool holds) {
    soft->held += holds ? 1u : 0u;
    soft->total++;
}

static CadenceType cadence_of(PlanCadenceType cadence) {
    return harmony_cadence_type_of(cadence);
}

/* The rules of a continuation: it fragments the basic idea, the fragments in their span and in their notes. */
static const char *judge_continuation(const GrowThemeSpec *spec) {
    Rational bar = grow_bar_length(&spec->meter);
    Rational unit = {0, 1};
    if (rational_make(bar.num * (long)spec->bars, bar.den * 8, &unit) != RATIONAL_OK) {
        return "theme not measurable";
    }
    Rational basic = {unit.num * 2, unit.den};
    size_t basic_ticks = ticks_of(basic);
    size_t fragment_ticks = ticks_of(unit);
    if (basic_ticks == 0 || fragment_ticks == 0 || !standard_form_is_fragmentation(basic_ticks, fragment_ticks)) {
        return "continuation does not fragment";
    }
    for (GrowPlace idea = GROW_PLACE_FRAGMENT_1; idea <= GROW_PLACE_FRAGMENT_2; idea++) {
        /* A slot's own motive begins its slot as it is: only a fragment of it is shorter than its source. */
        bool own = grow_idea_own(spec, idea);
        if (!spec->has_developed[idea] ||
            (!own && !standard_form_is_fragmentation(ticks_of(grow_idea_source(spec, idea)->length),
                                                     ticks_of(spec->developed[idea].length)))) {
            return "fragment not shorter than the basic idea";
        }
    }
    return NULL;
}

/* The verdict on a planned sentence: standard-form's rule functions, called on the lengths the candidate plans. */
static const char *judge_sentence(const GrowThemeSpec *spec, Soft *soft) {
    PhraseLevelForm form;
    if (!standard_form_pairing_form(grow_phrase_beginning(spec->pairing.beginning),
                                    grow_phrase_ending(spec->pairing.ending), &form) ||
        form != PHRASE_LEVEL_FORM_SENTENCE) {
        return "pairing is not a sentence";
    }
    if (!standard_form_subphrase_roles_in_order(SUBPHRASE_ROLE_PRESENTATION, SUBPHRASE_ROLE_CONTINUATION)) {
        return "subphrase roles out of order";
    }
    if (!standard_form_phrase_is_sentential(true, true)) {
        return "phrase not sentential";
    }
    if (!standard_form_basic_idea_length_valid(spec->bars / 4)) {
        return "basic idea length not valid";
    }
    const char *hard = judge_continuation(spec);
    if (hard) {
        return hard;
    }
    size_t half = spec->bars / 2;
    ask(soft, standard_form_sentence_is_archetypal(spec->bars, half, half, true));
    ask(soft, standard_form_presentation_length_usual(half));
    ask(soft, standard_form_continuation_balanced(half, half));
    ask(soft, spec->has_developed[GROW_PLACE_REPETITION] &&
                  standard_form_idea_reads_as_repetition(grow_same_contour(&spec->source, &spec->developed[GROW_PLACE_REPETITION])));
    return NULL;
}

/* Whether the contrasting idea of a motive keeps the contour of m0 and so would read as its repetition. */
static bool reads_as_repetition(const GrowThemeSpec *spec, GrowPlace idea) {
    return standard_form_idea_reads_as_repetition(grow_same_contour(&spec->source, &spec->developed[idea]));
}

static bool motives_differ(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return true;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].midi_pitch != b->notes[i].midi_pitch) {
            return true;
        }
    }
    return false;
}

static const char *judge_period(const GrowThemeSpec *spec, Soft *soft) {
    size_t half = spec->bars / 2;
    if (!standard_form_phrase_roles_in_order(PHRASE_ROLE_ANTECEDENT, PHRASE_ROLE_CONSEQUENT)) {
        return "phrase roles out of order";
    }
    if (!standard_form_period_phrase_ideas_valid(IDEA_LABEL_BASIC_IDEA, IDEA_LABEL_CONTRASTING_IDEA)) {
        return "phrase ideas not a basic idea and a contrasting idea";
    }
    if (!standard_form_period_cadences_valid(cadence_of(spec->first_cadence), cadence_of(spec->cadence))) {
        return "period cadences not valid";
    }
    ask(soft, standard_form_antecedent_cadence_usual(cadence_of(spec->first_cadence)));
    ask(soft, standard_form_consequent_cadence_usual(cadence_of(spec->cadence)));
    ask(soft, standard_form_antecedent_length_usual(half));
    ask(soft, standard_form_consequent_length_usual(half, half));
    ask(soft, standard_form_consequent_contrast_usual(motives_differ(&spec->developed[GROW_PLACE_CONTRAST],
                                                                     &spec->developed[GROW_PLACE_CONSEQUENT_CONTRAST])));
    return NULL;
}

static const char *judge_hybrid(const GrowThemeSpec *spec, Soft *soft) {
    int beginning = grow_phrase_beginning(spec->pairing.beginning);
    int ending = grow_phrase_ending(spec->pairing.ending);
    if (!standard_form_pairing_is_hybrid(beginning, ending)) {
        return "pairing is not a hybrid";
    }
    const char *hard = spec->pairing.ending == PLAN_THEME_ENDING_CONTINUATION ? judge_continuation(spec) : NULL;
    if (hard) {
        return hard;
    }
    size_t half = spec->bars / 2;
    StandardFormCommonality commonality;
    if (!standard_form_pairing_commonality(beginning, ending, &commonality)) {
        return "pairing has no commonality";
    }
    /* Commonality counts for what it is worth: a very common pairing in full, a rare one for nothing. */
    for (int step = 0; step < STANDARD_FORM_COMMONALITY_COUNT - 1; step++) {
        ask(soft, step < STANDARD_FORM_COMMONALITY_COUNT - 1 - (int)commonality);
    }
    if (spec->pairing.beginning == PLAN_THEME_BEGINNING_PRESENTATION) {
        ask(soft, standard_form_presentation_length_usual(half));
        ask(soft, spec->has_developed[GROW_PLACE_REPETITION] && reads_as_repetition(spec, GROW_PLACE_REPETITION));
    }
    if (spec->pairing.ending == PLAN_THEME_ENDING_CONTINUATION) {
        ask(soft, standard_form_continuation_balanced(half, half));
    }
    return NULL;
}

/* The rules of the beginning and the ending a pairing is made of, then those of the form it makes. */
static const char *judge(const GrowThemeSpec *spec, Soft *soft) {
    memset(soft, 0, sizeof(*soft));
    PhraseLevelForm form;
    if (!standard_form_pairing_form(grow_phrase_beginning(spec->pairing.beginning),
                                    grow_phrase_ending(spec->pairing.ending), &form)) {
        return "pairing not known";
    }
    if (form == PHRASE_LEVEL_FORM_SENTENCE) {
        return judge_sentence(spec, soft);
    }
    if (!standard_form_basic_idea_length_valid(spec->bars / 4)) {
        return "basic idea length not valid";
    }
    /* A contrasting idea that keeps m0's contour would read as its repetition. */
    if (spec->pairing.beginning != PLAN_THEME_BEGINNING_PRESENTATION &&
        (!spec->has_developed[GROW_PLACE_CONTRAST] ||
         (!grow_idea_own(spec, GROW_PLACE_CONTRAST) && reads_as_repetition(spec, GROW_PLACE_CONTRAST)))) {
        return "contrasting idea reads as a repetition";
    }
    /* A theme with an antecedent ends in a cadence stronger than the antecedent's. */
    if (spec->pairing.beginning == PLAN_THEME_BEGINNING_ANTECEDENT &&
        harmony_cadence_strength(cadence_of(spec->first_cadence)) >= harmony_cadence_strength(cadence_of(spec->cadence))) {
        return "ending cadence not stronger than the antecedent's";
    }
    return form == PHRASE_LEVEL_FORM_PERIOD ? judge_period(spec, soft) : judge_hybrid(spec, soft);
}

bool grow_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                  const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, GROW_THEME_OPERATOR_NAME) != 0) {
        return harmony_phrase_checker(plan, target, operator_, candidate, context, out);
    }
    const GrowThemeSpec *spec = candidate->payload;
    Soft soft;
    if (spec && spec->magic == GROW_FREE_MAGIC) {
        memset(out, 0, sizeof(*out));
        out->score_denominator = 1;
        const char *hard = grow_free_judge(candidate->payload, &soft.held, &soft.total);
        if (hard) {
            out->hard_violation = true;
            snprintf(out->reason, sizeof(out->reason), "%s", hard);
            return true;
        }
        out->score_numerator = soft.held;
        out->score_denominator = soft.total ? soft.total : 1;
        snprintf(out->reason, sizeof(out->reason), "free phrase rules");
        return true;
    }
    if (!spec || spec->magic != GROW_PAYLOAD_MAGIC) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    const char *hard = judge(spec, &soft);
    if (hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", hard);
        return true;
    }
    out->score_numerator = soft.held;
    out->score_denominator = soft.total ? soft.total : 1;
    static const char *const RULES[] = {"sentence rules", "period rules", "hybrid rules"};
    int archetype = grow_pairing_archetype(&spec->pairing);
    snprintf(out->reason, sizeof(out->reason), "%s", RULES[archetype < 0 ? 0 : archetype]);
    return true;
}
