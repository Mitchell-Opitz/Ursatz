#include <ursatz_composer/harmony_phrase.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <standard-harmony/analysis/analysis_cadence.h>
#include <standard-harmony/analysis/analysis_prolongation.h>
#include <standard-harmony/analysis/analysis_six_four.h>
#include <standard-harmony/harmony_cadence_rules.h>
#include <standard-harmony/harmony_deceptive.h>
#include <standard-harmony/harmony_phrase_rules.h>
#include <standard-harmony/harmony_plagal.h>
#include <standard-harmony/harmony_predominant_seventh.h>
#include <standard-harmony/harmony_strong_predominants.h>

#include <ursatz_composer/harmony_octave.h>
#include <ursatz_composer/harmony_schema.h>
#include <ursatz_composer/harmony_sequence.h>
#include <ursatz_composer/seam_hear.h>

#include "check/shaping.h"
#include "harmony/harmony_internal.h"
#include "util/util.h"

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
 * Whether a chord that is not a plain root-position one has a reason to be there: a 6/4 the
 * classifier names, the inside of a prolongation, a strong predominant (IV, ii6, ii), a
 * predominant seventh, or V7.
 */
static bool chord_is_explained(KeyStructureMode mode, const HarmonyChord *chord, bool classified_six_four,
                               bool inside_prolongation) {
    HarmonyStrongPredominant strong;
    HarmonyPredominantSeventhKind seventh;
    if (chord->inversion == 0 && !chord->seventh) {
        return true;
    }
    return classified_six_four || inside_prolongation ||
           harmony_strong_predominant_of(harmony_mode_of(mode), chord, &strong) ||
           harmony_predominant_seventh_of(chord, &seventh) ||
           (chord->seventh && chord->degree == 5 && chord->inversion == 0);
}

typedef struct {
    bool six_four;           /* classified as a 6/4 of some kind, cadential included */
    bool prolonging_six_four; /* a passing, neighbor or arpeggiating 6/4 */
    bool inside;             /* inside a prolongation */
} Reading;

/* How the analysis reads each segment: its 6/4 type and whether a prolongation explains it. */
static bool read_segments(const HarmonySegment *segments, size_t count, Reading *reading, bool *unexplained_six_four) {
    HarmonySixFourResult *six_fours = NULL;
    HarmonyProlongations prolongations = {NULL, 0};
    *unexplained_six_four = false;
    memset(reading, 0, count * sizeof(*reading));
    if (harmony_analysis_classify_six_fours(segments, count, &six_fours) != HARMONY_SIX_FOUR_OK) {
        return false;
    }
    if (harmony_analysis_find_prolongations(segments, count, &prolongations) != HARMONY_PROLONGATION_OK) {
        harmony_six_four_results_destroy(six_fours, count);
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (six_fours[i].is_six_four && !six_fours[i].classified) {
            *unexplained_six_four = true;
        }
        reading[i].six_four = six_fours[i].classified;
        reading[i].prolonging_six_four = six_fours[i].classified && six_fours[i].type != SIX_FOUR_TYPE_CADENTIAL;
    }
    for (size_t s = 0; s < prolongations.count; s++) {
        for (size_t k = 0; k < prolongations.spans[s].interior_count; k++) {
            reading[prolongations.spans[s].interior_segments[k]].inside = true;
        }
    }
    harmony_prolongations_destroy(&prolongations);
    harmony_six_four_results_destroy(six_fours, count);
    return true;
}

/*
 * Root progressions (ch. XII): of the root changes (consecutive chords of different roots), at least 2/3 are strong,
 * the root rising a fourth or a step, or falling a third or a step; the root falling a fourth (rising a fifth) and
 * rising a third are weak. Read in the degrees of the chords. Fewer than two changes hold (see harmony_internal.h).
 */
bool harmony_root_progression_holds(const HarmonyChord *chords, size_t count) {
    unsigned changes = 0;
    unsigned strong = 0;
    for (size_t i = 1; i < count; i++) {
        int step = ((chords[i].degree - chords[i - 1].degree) % 7 + 7) % 7;
        if (step == 0) {
            continue;
        }
        changes++;
        /* up a step, up a fourth, down a third (up a sixth), down a step */
        strong += step == 1 || step == 3 || step == 5 || step == 6;
    }
    return changes < 2 || strong * 3 >= changes * 2;
}

bool harmony_judge(const HeardPhrase *phrase, PhraseVerdict *verdict) {
    memset(verdict, 0, sizeof(*verdict));
    SegmentSet *set = harmony_segments_create(phrase);
    if (!set) {
        return false;
    }
    size_t count;
    const HarmonySegment *segments = harmony_segments_get(set, &count);
    Reading reading[HARMONY_MAX_CHORDS];
    bool unexplained_six_four;
    bool ok = read_segments(segments, count, reading, &unexplained_six_four);
    if (!ok) {
        harmony_segments_destroy(set);
        return false;
    }

    CadenceType cadence = phrase->requested;
    HarmonyChord model[HARMONY_MAX_CHORDS];
    size_t model_count = 0;
    /* A deceptive or plagal ending is not a cadential progression, so the phrase rules read the chords up
       to the dominant (deceptive) or to the authentic cadence's tonic (plagal). */
    size_t model_end = phrase->open_ended
                           ? count
                           : count - (cadence == CADENCE_TYPE_DECEPTIVE ? 1 : (cadence == CADENCE_TYPE_PLAGAL ? 2 : 0));

    const char *hard = NULL;
    if (unexplained_six_four && !phrase->open_ended) {
        hard = "6/4 chord without a type";
    }
    for (size_t i = 0; !hard && i < count; i++) {
        const HarmonySegment *segment = &segments[i];
        if (!segment->valid) {
            hard = "chord not readable";
        } else if (segment->applied) {
            /* An applied chord leads into its target, the next chord (or the cadential 6/4 that embellishes V). */
            const HarmonySegment *next = i + 1 < count ? &segments[i + 1] : NULL;
            bool resolves = next && next->valid && !next->applied &&
                            (next->chord.degree == segment->applied_chord.target ||
                             (segment->applied_chord.target == 5 && next->chord.cadential));
            if (!resolves) {
                hard = "applied chord does not resolve to its target";
            }
        } else if (!phrase->open_ended && !(phrase->exempt && phrase->exempt[i]) &&
                   !chord_is_explained(phrase->mode, &segment->chord, reading[i].six_four, reading[i].inside)) {
            hard = "chord with no reason to be there";
        }
    }
    for (size_t i = 0; !hard && i < model_end; i++) {
        if (!segments[i].applied && (phrase->open_ended || (!reading[i].inside && !reading[i].prolonging_six_four))) {
            model[model_count++] = segments[i].chord;
        }
    }

    unsigned soft_rules = phrase_soft_rules();
    unsigned soft_broken = 0;
    if (!hard) {
        HarmonyPhrase model_phrase = {harmony_mode_of(phrase->mode), model, model_count};
        unsigned phrase_hard = 0;
        unsigned phrase_soft = 0;
        bool checked = model_count > 0 && harmony_phrase_check(&model_phrase, &phrase_hard, &phrase_soft);
        if (phrase->no_opening_tonic) {
            phrase_hard &= ~(1u << HARMONY_PHRASE_TONIC_THEN_DOMINANT);
        }
        if (!checked || phrase_hard) {
            hard = "phrase model broken";
        }
        soft_broken += util_bits_set(phrase_soft);
    }

    /* The ending's own progression. */
    if (!hard && !phrase->open_ended && (count < 2 || segments[count - 2].applied || segments[count - 1].applied)) {
        hard = "no cadential progression";
    }
    if (!hard && !phrase->open_ended) {
        const HarmonyChord ending[2] = {segments[count - 2].chord, segments[count - 1].chord};
        if (cadence == CADENCE_TYPE_DECEPTIVE) {
            HarmonyDeceptiveKind kind;
            if (!harmony_deceptive_of(ending, &kind)) {
                hard = "not deceptive motion";
            }
        } else if (cadence == CADENCE_TYPE_PLAGAL) {
            if (!harmony_plagal_progression_is(ending, HARMONY_PLAGAL_AFTER_CADENCE)) {
                hard = "not plagal motion";
            }
        } else {
            HarmonyCadence rule = {ending[0], ending[1], 1, false};
            unsigned cadence_hard = 0;
            unsigned cadence_soft = 0;
            if (!harmony_cadence_check(&rule, &cadence_hard, &cadence_soft) || cadence_hard) {
                hard = "not a cadential progression";
            }
            soft_rules += cadence_soft_rules();
            soft_broken += util_bits_set(cadence_soft);
        }
    }

    /* The cadence the recognizer hears must be the one asked for, and it must end the phrase. */
    CadenceType heard = CADENCE_TYPE_UNKNOWN;
    if (!hard && !phrase->open_ended) {
        HarmonyCadences found = {NULL, 0};
        if (harmony_analysis_find_cadences(segments, count, NULL, &found) != HARMONY_CADENCE_ANALYSIS_OK) {
            ok = false;
        } else if (found.count == 0) {
            hard = "no cadence heard";
        } else {
            heard = found.items[found.count - 1].type;
            if (found.items[found.count - 1].arrival_last != count - 1 || heard != cadence) {
                hard = "other cadence heard";
            }
        }
        harmony_cadences_destroy(&found);
    }
    /* A prolongation hears no cadence anywhere in it, whatever the soprano. */
    if (!hard && phrase->no_cadence) {
        HarmonyCadences found = {NULL, 0};
        if (harmony_analysis_find_cadences(segments, count, NULL, &found) != HARMONY_CADENCE_ANALYSIS_OK) {
            ok = false;
        } else if (found.count > 0) {
            hard = "cadence heard in a prolongation";
            heard = found.items[0].type;
        }
        harmony_cadences_destroy(&found);
    }
    /* Read as the phrase rules read it: applied chords and the inside of a prolongation embellish and do not progress. */
    if (!hard && !phrase->plain_shaping) {
        soft_rules++;
        if (!harmony_root_progression_holds(model, model_count)) {
            soft_broken++;
            verdict->shaping_broken |= SHAPING_ROOT_PROGRESSION;
        }
    }
    harmony_segments_destroy(set);
    verdict->hard = hard;
    verdict->heard = heard;
    verdict->soft_total = soft_rules;
    verdict->soft_held = soft_rules > soft_broken ? soft_rules - soft_broken : 0;
    return ok;
}

static bool verdict_to_check(const HeardPhrase *phrase, FillCheck *out);

void harmony_seam_score(const PhraseProgression *progression, FillCheck *out) {
    if (!progression->request.at_seam || progression->request.after_count == 0 || progression->count == 0 ||
        out->hard_violation) {
        return;
    }
    const PhraseChord *last = &progression->chords[progression->count - 1];
    const PhraseChord *first = &progression->request.after[0];
    SeamNotes notes;
    SeamHeard heard;
    memset(&notes, 0, sizeof(notes));
    notes.key.mode = progression->request.mode;
    notes.has_chords = true;
    notes.before_function = last->function;
    notes.before_has_function = last->has_function;
    notes.after_function = first->function;
    notes.after_has_function = first->has_function;
    snprintf(notes.before_roman, sizeof(notes.before_roman), "%s", last->roman);
    snprintf(notes.after_roman, sizeof(notes.after_roman), "%s", first->roman);
    notes.bass_before = notes.bass_after = notes.melody_previous = notes.melody_before = notes.melody_after =
        notes.melody_next = -1;
    seam_hear_notes(&notes, &heard);
    out->score_numerator += heard.progression ? 1 : 0;
    out->score_denominator += 1;
}

/* The verdict on a phrase of this operator, then what the Given material around its target asks of the chords. */
static bool phrase_to_check(const HeardPhrase *phrase, const PhraseProgression *progression, FillCheck *out) {
    const char *given_hard = NULL;
    if (!verdict_to_check(phrase, out)) {
        return false;
    }
    if (out->hard_violation || !progression->request.around_given) {
        return true;
    }
    if (harmony_given_fits(&progression->request, progression->chords, progression->count, &given_hard) &&
        !harmony_given_join(&progression->request, progression->chords, progression->count, true, &given_hard)) {
        return false;
    }
    if (given_hard) {
        out->hard_violation = true;
        out->score_numerator = 0;
        snprintf(out->reason, sizeof(out->reason), "%s", given_hard);
    }
    harmony_seam_score(progression, out);
    return true;
}

static bool verdict_to_check(const HeardPhrase *phrase, FillCheck *out) {
    PhraseVerdict verdict;
    memset(out, 0, sizeof(*out));
    out->score_denominator = 1;
    if (!harmony_judge(phrase, &verdict)) {
        return false;
    }
    if (verdict.hard) {
        out->hard_violation = true;
        snprintf(out->reason, sizeof(out->reason), "%s", verdict.hard);
        return true;
    }
    out->score_numerator = verdict.soft_held;
    out->score_denominator = verdict.soft_total ? verdict.soft_total : 1;
    shaping_reason(out->reason, sizeof(out->reason), cadence_type_name(verdict.heard), verdict.shaping_broken);
    return true;
}

bool harmony_phrase_checker(const PiecePlan *plan, FillTarget target, const FillOperator *operator_,
                            const FillCandidate *candidate, void *context, FillCheck *out) {
    if (!plan || !operator_ || !candidate || !out) {
        return false;
    }
    if (strcmp(operator_->generator.name, HARMONY_PHRASE_OPERATOR_NAME) == 0) {
        const PhraseProgression *progression = candidate->payload;
        if (!progression || progression->count < 1 || progression->count > HARMONY_MAX_CHORDS) {
            return false;
        }
        PhraseChord sequence[HARMONY_JOIN_CHORDS + 1];
        PhraseMetre joined;
        bool short_phrase = progression->count == 1;
        if (short_phrase) {
            /* One chord stands alone only after Given chords: it is judged as the end of the chain they make (R-208). */
            if (!progression->request.around_given || progression->request.before_count == 0 ||
                !harmony_joined_chain(&progression->request, progression->chords, sequence, &joined)) {
                memset(out, 0, sizeof(*out));
                out->score_denominator = 1;
                out->hard_violation = true;
                snprintf(out->reason, sizeof(out->reason), "a phrase of one chord");
                return true;
            }
        }
        HeardPhrase phrase = {short_phrase ? sequence : progression->chords,
                              short_phrase ? progression->request.before_count + 1 : progression->count,
                              progression->request.mode, progression->request.cadence,
                              progression->request.cells_per_bar, progression->request.cell_length, false, NULL, false,
                              false, NULL, NULL, false, short_phrase ? &joined : &progression->request.metre};
        /* A phrase joined to Given chords begins in the middle of what they make: no opening tonic is asked. */
        phrase.no_opening_tonic = short_phrase || (progression->request.has_shape &&
                                  progression->request.shape.kind == PLAN_HARMONY_SHAPE_CADENTIAL);
        if (progression->request.pivot) {
            /* A phrase that opens a new key starts on a chord both keys have, which need not be a tonic. */
            phrase.no_opening_tonic = true;
            if (progression->chords[0].applied ||
                !harmony_pivot_chord_valid(progression->request.from_key, progression->request.key,
                                           &progression->chords[0].chord)) {
                memset(out, 0, sizeof(*out));
                out->score_denominator = 1;
                out->hard_violation = true;
                snprintf(out->reason, sizeof(out->reason), "first chord is not a pivot chord");
                return true;
            }
        }
        return phrase_to_check(&phrase, progression, out);
    }
    if (strcmp(operator_->generator.name, HARMONY_EMBELLISH_OPERATOR_NAME) == 0) {
        const EmbellishCandidate *edit = candidate->payload;
        if (!edit) {
            return false;
        }
        memset(out, 0, sizeof(*out));
        out->score_denominator = 1;
        if (edit->plain) {
            /* Leaving the progression as it is changes nothing, so there is nothing to reject. */
            snprintf(out->reason, sizeof(out->reason), "unchanged");
            return true;
        }
        if (edit->count < 2 || edit->count > HARMONY_MAX_CHORDS) {
            return false;
        }
        HeardPhrase phrase = {edit->chords, edit->count, edit->mode, edit->cadence, edit->cells_per_bar,
                              edit->cell_length, false, NULL, false, false, NULL, NULL, false, &edit->metre};
        return verdict_to_check(&phrase, out);
    }
    if (strcmp(operator_->generator.name, HARMONY_SCHEMA_OPERATOR_NAME) == 0) {
        return harmony_skeleton_check(candidate->payload, out);
    }
    if (strcmp(operator_->generator.name, HARMONY_SEQUENCE_OPERATOR_NAME) == 0) {
        return harmony_skeleton_check(candidate->payload, out);
    }
    if (strcmp(operator_->generator.name, HARMONY_BASS_OPERATOR_NAME) == 0) {
        return harmony_bass_check(candidate->payload, out);
    }
    if (strcmp(operator_->generator.name, HARMONY_OCTAVE_OPERATOR_NAME) == 0) {
        return harmony_octave_check(candidate->payload, out);
    }
    return fill_default_checker(plan, target, operator_, candidate, context, out);
}

bool harmony_check_lane_without_cadence(const PiecePlan *plan, PlanSpan span, KeyStructureMode mode,
                                         const char **violation) {
    PhraseMetre metre;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t count;
    PhraseVerdict verdict;
    if (!violation || !harmony_read_lane(plan, span, mode, chords, &count, &metre)) {
        return false;
    }
    HeardPhrase phrase = {chords, count, mode, CADENCE_TYPE_PERFECT_AUTHENTIC, metre.cells_per_bar, metre.cell_length, true, NULL, false, false, NULL, NULL, false, &metre};
    if (!harmony_judge(&phrase, &verdict)) {
        return false;
    }
    *violation = verdict.hard;
    return true;
}

/* Marks the chords of the lane over span that a schema made: they need no reason of their own to be there. */
static void mark_schema_chords(const PiecePlan *plan, PlanSpan span, size_t count, bool *exempt) {
    LaneSpanId ids[HARMONY_MAX_CHORDS];
    size_t found = plan_lane_find(plan, PLAN_LANE_HARMONY, 0, span, ids, HARMONY_MAX_CHORDS);
    for (size_t i = 0; i < count; i++) {
        exempt[i] = i < found && plan_lane_span_find_constraint(plan, ids[i], PLAN_CONSTRAINT_SCHEMA) != NULL;
    }
}

bool harmony_check_lane(const PiecePlan *plan, PlanSpan span, KeyStructureMode mode, CadenceType requested,
                        const char **violation) {
    PhraseMetre metre;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    bool exempt[HARMONY_MAX_CHORDS];
    size_t count;
    PhraseVerdict verdict;
    if (!violation || !harmony_read_lane(plan, span, mode, chords, &count, &metre)) {
        return false;
    }
    mark_schema_chords(plan, span, count, exempt);
    HeardPhrase phrase = {chords, count, mode, requested, metre.cells_per_bar, metre.cell_length, false, exempt, false, false, NULL, NULL, false, &metre};
    if (!harmony_judge(&phrase, &verdict)) {
        return false;
    }
    *violation = verdict.hard;
    return true;
}
