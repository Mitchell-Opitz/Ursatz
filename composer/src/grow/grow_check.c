#include <ursatz_composer/grow.h>

#include <stdio.h>
#include <string.h>

#include <hybrid_rules.h>
#include <phrase_beginning.h>
#include <phrase_ending.h>
#include <phrase_level_form.h>
#include <semantics/subphrase_role.h>
#include <sentence_rules.h>

#include <ursatz_composer/harmony_phrase.h>

#include "grow/grow_internal.h"

#define TICKS_PER_WHOLE 128L
/* The soft rules grow_checker scores. */
#define SOFT_RULES 4

/* A length as a whole number of ticks, the unit the rules below are asked in; 0 when it is not a whole number. */
static size_t ticks_of(PlanTime time) {
    long scaled = time.num * TICKS_PER_WHOLE;
    return time.den != 0 && scaled > 0 && scaled % (long)time.den == 0 ? (size_t)(scaled / (long)time.den) : 0;
}

static PhraseBeginning beginning_of(PlanThemeBeginning beginning) {
    switch (beginning) {
    case PLAN_THEME_BEGINNING_ANTECEDENT:
        return PHRASE_BEGINNING_ANTECEDENT;
    case PLAN_THEME_BEGINNING_COMPOUND_BASIC_IDEA:
        return PHRASE_BEGINNING_COMPOUND_BASIC_IDEA;
    case PLAN_THEME_BEGINNING_PRESENTATION:
        return PHRASE_BEGINNING_PRESENTATION;
    }
    return PHRASE_BEGINNING_COUNT;
}

static PhraseEnding ending_of(PlanThemeEnding ending) {
    switch (ending) {
    case PLAN_THEME_ENDING_CONTINUATION:
        return PHRASE_ENDING_CONTINUATION;
    case PLAN_THEME_ENDING_CONSEQUENT:
        return PHRASE_ENDING_CONSEQUENT;
    case PLAN_THEME_ENDING_CADENTIAL:
        return PHRASE_ENDING_CADENTIAL;
    }
    return PHRASE_ENDING_COUNT;
}

static int direction(int semitones) {
    return semitones > 0 ? 1 : (semitones < 0 ? -1 : 0);
}

/* Whether two motives go up and down in the same places. */
static bool same_contour(const Motive *a, const Motive *b) {
    if (a->note_count != b->note_count) {
        return false;
    }
    for (size_t i = 1; i < a->note_count; i++) {
        if (direction(a->notes[i].midi_pitch - a->notes[i - 1].midi_pitch) !=
            direction(b->notes[i].midi_pitch - b->notes[i - 1].midi_pitch)) {
            return false;
        }
    }
    return true;
}

/* The verdict on a planned sentence: standard-form's rule functions, called on the lengths the candidate plans. */
static const char *judge(const GrowThemeSpec *spec, unsigned *soft_held) {
    PlanTime bar = grow_bar_length(&spec->meter);
    PlanTime unit = {0, 1};
    *soft_held = 0;
    if (plan_time_make(bar.num * (long)spec->bars, bar.den * 8, &unit) != PLAN_OK) {
        return "theme not measurable";
    }
    PlanTime basic = {unit.num * 2, unit.den};
    size_t basic_ticks = ticks_of(basic);
    size_t fragment_ticks = ticks_of(unit);
    size_t measures = spec->bars / 4;
    PhraseLevelForm form;

    if (!standard_form_pairing_form(beginning_of(spec->pairing.beginning), ending_of(spec->pairing.ending), &form) ||
        form != PHRASE_LEVEL_FORM_SENTENCE) {
        return "pairing is not a sentence";
    }
    if (!standard_form_subphrase_roles_in_order(SUBPHRASE_ROLE_PRESENTATION, SUBPHRASE_ROLE_CONTINUATION)) {
        return "subphrase roles out of order";
    }
    if (!standard_form_phrase_is_sentential(true, true)) {
        return "phrase not sentential";
    }
    if (!standard_form_basic_idea_length_valid(measures)) {
        return "basic idea length not valid";
    }
    /* Each fragment is shorter than the basic idea, in its span and in its notes. */
    if (basic_ticks == 0 || fragment_ticks == 0 || !standard_form_is_fragmentation(basic_ticks, fragment_ticks)) {
        return "continuation does not fragment";
    }
    for (GrowIdea idea = GROW_IDEA_FRAGMENT_1; idea <= GROW_IDEA_FRAGMENT_2; idea++) {
        if (!spec->has_developed[idea] ||
            !standard_form_is_fragmentation(ticks_of(spec->source.length), ticks_of(spec->developed[idea].length))) {
            return "fragment not shorter than the basic idea";
        }
    }

    size_t half = spec->bars / 2;
    *soft_held += standard_form_sentence_is_archetypal(spec->bars, half, half, true);
    *soft_held += standard_form_presentation_length_usual(half);
    *soft_held += standard_form_continuation_balanced(half, half);
    *soft_held += spec->has_developed[GROW_IDEA_REPETITION] &&
                  standard_form_idea_reads_as_repetition(same_contour(&spec->source, &spec->developed[GROW_IDEA_REPETITION]));
    return NULL;
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
    unsigned held;
    if (!spec || spec->magic != GROW_PAYLOAD_MAGIC) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    const char *hard = judge(spec, &held);
    if (hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", hard);
        return true;
    }
    out->score_numerator = held;
    out->score_denominator = SOFT_RULES;
    snprintf(out->reason, sizeof(out->reason), "sentence rules");
    return true;
}
