#include <ursatz_composer/harmony_phrase.h>

#include <stdio.h>
#include <string.h>

#include <harmony_cadence_rules.h>
#include <harmony_deceptive.h>
#include <harmony_phrase_rules.h>
#include <harmony_plagal.h>

#include "harmony/harmony_internal.h"

#define MAX_CHECKED_CHORDS 16

static void reject(FillCheck *out, const char *reason) {
    out->hard_violation = true;
    out->score_numerator = 0;
    out->score_denominator = 1;
    snprintf(out->reason, sizeof(out->reason), "%s", reason);
}

static unsigned bits_set(unsigned bits) {
    unsigned count = 0;
    for (; bits; bits >>= 1) {
        count += bits & 1u;
    }
    return count;
}

/* How many of the phrase rules are soft. */
static unsigned phrase_soft_rules(void) {
    unsigned count = 0;
    for (int rule = 0; rule < HARMONY_PHRASE_RULE_COUNT; rule++) {
        count += harmony_phrase_preference((HarmonyPhraseRule)rule) != NULL;
    }
    return count;
}

static unsigned cadence_soft_rules(void) {
    unsigned count = 0;
    for (int rule = 0; rule < HARMONY_CADENCE_RULE_COUNT; rule++) {
        count += harmony_cadence_preference((HarmonyCadenceRule)rule) != NULL;
    }
    return count;
}

/*
 * The chords the phrase model reads. A deceptive or plagal ending is not a
 * cadential progression, so the phrase rules read the chords up to the
 * dominant (deceptive) or to the authentic cadence's tonic (plagal), and the
 * ending is checked by its own rule.
 */
static size_t model_chord_count(PlanCadenceType cadence, size_t count) {
    if (cadence == PLAN_CADENCE_DC) {
        return count - 1;
    }
    return cadence == PLAN_CADENCE_PC ? count - 2 : count;
}

bool harmony_phrase_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                            const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, HARMONY_PHRASE_OPERATOR_NAME) != 0) {
        return fill_default_checker(plan, target, operator_, candidate, context, out);
    }
    const PhraseProgression *progression = candidate->payload;
    if (!progression || progression->count < 2 || progression->count > MAX_CHECKED_CHORDS) {
        return false;
    }
    PlanCadenceType cadence = progression->request.cadence;
    HarmonyMode mode = harmony_mode_of(progression->request.mode);
    size_t count = progression->count;
    HarmonyChord chords[MAX_CHECKED_CHORDS];
    for (size_t i = 0; i < count; i++) {
        chords[i] = progression->chords[i].chord;
    }
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;

    unsigned hard = 0;
    unsigned soft = 0;
    unsigned soft_rules = phrase_soft_rules();
    unsigned soft_broken = 0;
    HarmonyPhrase phrase = {mode, chords, model_chord_count(cadence, count)};
    if (!harmony_phrase_check(&phrase, &hard, &soft)) {
        reject(out, "not a phrase");
        return true;
    }
    if (hard) {
        reject(out, "phrase model broken");
        return true;
    }
    soft_broken += bits_set(soft);

    /* The ending's own progression. */
    const HarmonyChord *ending = &chords[count - 2];
    if (cadence == PLAN_CADENCE_DC) {
        HarmonyDeceptiveKind kind;
        if (!harmony_deceptive_of(ending, &kind)) {
            reject(out, "not deceptive motion");
            return true;
        }
    } else if (cadence == PLAN_CADENCE_PC) {
        if (!harmony_plagal_progression_is(ending, HARMONY_PLAGAL_AFTER_CADENCE)) {
            reject(out, "not plagal motion");
            return true;
        }
    } else {
        HarmonyCadence rule = {ending[0], ending[1], 1, false};
        unsigned cadence_hard = 0;
        unsigned cadence_soft = 0;
        if (!harmony_cadence_check(&rule, &cadence_hard, &cadence_soft) || cadence_hard) {
            reject(out, "not a cadential progression");
            return true;
        }
        soft_rules += cadence_soft_rules();
        soft_broken += bits_set(cadence_soft);
    }

    /* The cadence the recognizer hears must be the one asked for. */
    HeardPhrase heard_phrase = {progression->chords, count, progression->request.mode, cadence,
                                progression->request.cells_per_bar, progression->request.cell_length};
    CadenceType heard;
    bool at_end;
    if (!harmony_hear_phrase(&heard_phrase, &heard, &at_end)) {
        reject(out, "no cadence heard");
        return true;
    }
    if (!at_end || heard != harmony_cadence_type_of(cadence)) {
        reject(out, "other cadence heard");
        return true;
    }

    out->score_numerator = soft_rules - soft_broken;
    out->score_denominator = soft_rules ? soft_rules : 1;
    snprintf(out->reason, sizeof(out->reason), "%s", cadence_type_name(heard));
    return true;
}
