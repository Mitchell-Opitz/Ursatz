#include "fugue_answer_rules.h"

#include <stdlib.h>

#include "fugue_rule_table.h"
#include "standard-counterpoint/counterpoint_interval.h"
#include "standard-counterpoint/counterpoint_pitch.h"

static const char *const NAMES[FUGUE_ANSWER_RULE_COUNT] = {"answer-at-fifth", "tonic-dominant-start",
                                                           "answer-not-free", "rhythm-preserved",
                                                           "minimal-adjustment"};

static bool table_is_hard(int which) {
    return which <= FUGUE_ANSWER_RHYTHM_PRESERVED;
}

static FugueRulePair g_pairs[FUGUE_ANSWER_RULE_COUNT];
static bool g_built[FUGUE_ANSWER_RULE_COUNT];
static const FugueRuleTable TABLE = {"answer", NAMES, FUGUE_ANSWER_RULE_COUNT, table_is_hard, g_pairs, g_built};

const Rule *fugue_answer_rule(FugueAnswerRule which) {
    return fugue_rule_table_rule(&TABLE, (int)which);
}

const Constraint *fugue_answer_constraint(FugueAnswerRule which) {
    return fugue_rule_table_constraint(&TABLE, (int)which);
}

const char *fugue_answer_rule_name(FugueAnswerRule which) {
    return fugue_rule_table_name(&TABLE, (int)which);
}

/* The first subject statement; false when there is none. */
static bool first_subject(const FugueExposition *exposition, size_t *index) {
    for (size_t i = 0; i < exposition->statement_count; i++) {
        if (exposition->statements[i].role == FUGUE_ROLE_SUBJECT) {
            *index = i;
            return true;
        }
    }
    return false;
}

typedef struct {
    FugueNote subject[FUGUE_MAX_NOTES];
    size_t subject_count;
    FugueNote answer[FUGUE_MAX_NOTES];
    size_t answer_count;
} Pair;

static bool load(const FugueExposition *exposition, size_t answer_statement, Pair *pair) {
    size_t subject;
    return first_subject(exposition, &subject) &&
           fugue_statement_notes(exposition, subject, pair->subject, FUGUE_MAX_NOTES, &pair->subject_count) &&
           fugue_statement_notes(exposition, answer_statement, pair->answer, FUGUE_MAX_NOTES,
                                 &pair->answer_count);
}

/* The number of intervals (from i to i + 1) and the generic size of one. */
static bool generic_size(const FugueNote *notes, size_t i, int *size) {
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(notes[i].pitch, notes[i + 1].pitch, &interval)) {
        return false;
    }
    *size = interval.size;
    return true;
}

static int signed_semitones(const FugueNote *notes, size_t i) {
    return standard_counterpoint_semitone_number(notes[i + 1].pitch) -
           standard_counterpoint_semitone_number(notes[i].pitch);
}

static int direction(const FugueNote *notes, size_t i) {
    return standard_counterpoint_direction(notes[i].pitch, notes[i + 1].pitch);
}

bool fugue_answer_kind(const FugueExposition *exposition, size_t statement, ImitationAnswer *out) {
    if (out == NULL || !fugue_exposition_valid(exposition) || statement >= exposition->statement_count ||
        exposition->statements[statement].role != FUGUE_ROLE_ANSWER) {
        return false;
    }
    Pair pair;
    if (!load(exposition, statement, &pair)) {
        return false;
    }
    if (pair.subject_count != pair.answer_count) {
        *out = IMITATION_ANSWER_FREE;
        return true;
    }
    bool real = true;
    bool tonal = true;
    for (size_t i = 0; i + 1 < pair.subject_count; i++) {
        int sub_size, ans_size;
        if (!generic_size(pair.subject, i, &sub_size) || !generic_size(pair.answer, i, &ans_size)) {
            return false;
        }
        real = real && signed_semitones(pair.subject, i) == signed_semitones(pair.answer, i);
        tonal = tonal && direction(pair.subject, i) == direction(pair.answer, i) && abs(sub_size - ans_size) <= 1;
    }
    *out = real ? IMITATION_ANSWER_REAL : tonal ? IMITATION_ANSWER_TONAL : IMITATION_ANSWER_FREE;
    return true;
}

/* The scale degree of a pitch above the tonic, 0 for the tonic to 6, counted
   in letter names. */
static int degree_of(const PitchSpelling *pitch, const PitchSpelling *tonic) {
    int d = (standard_counterpoint_step_number(pitch) - standard_counterpoint_step_number(tonic)) % 7;
    return d < 0 ? d + 7 : d;
}

static bool check_one(FugueAnswerRule which, const FugueExposition *exposition, size_t statement) {
    Pair pair;
    if (!load(exposition, statement, &pair)) {
        return false;
    }
    switch (which) {
        case FUGUE_ANSWER_AT_FIFTH: {
            StandardCounterpointInterval interval;
            return standard_counterpoint_interval_between(pair.subject[0].pitch, pair.answer[0].pitch, &interval) &&
                   interval.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT &&
                   (interval.simple_size == 4 || interval.simple_size == 5);
        }
        case FUGUE_ANSWER_TONIC_DOMINANT_START: {
            if (exposition->tonic == NULL) {
                return true;
            }
            int subject = degree_of(pair.subject[0].pitch, exposition->tonic);
            int answer = degree_of(pair.answer[0].pitch, exposition->tonic);
            return subject == 0 ? answer == 4 : subject == 4 ? answer == 0 : true;
        }
        case FUGUE_ANSWER_NOT_FREE: {
            ImitationAnswer kind;
            return fugue_answer_kind(exposition, statement, &kind) && kind != IMITATION_ANSWER_FREE;
        }
        case FUGUE_ANSWER_RHYTHM_PRESERVED:
            if (pair.subject_count != pair.answer_count) {
                return false;
            }
            for (size_t i = 0; i < pair.subject_count; i++) {
                if (pair.subject[i].length != pair.answer[i].length) {
                    return false;
                }
            }
            return true;
        case FUGUE_ANSWER_MINIMAL_ADJUSTMENT: {
            if (pair.subject_count != pair.answer_count) {
                return false;
            }
            size_t altered = 0;
            for (size_t i = 0; i + 1 < pair.subject_count; i++) {
                int sub_size, ans_size;
                if (!generic_size(pair.subject, i, &sub_size) || !generic_size(pair.answer, i, &ans_size)) {
                    return false;
                }
                altered += sub_size != ans_size ? 1 : 0;
            }
            return altered <= fugue_exposition_limits(exposition).max_altered_intervals;
        }
        default:
            return false;
    }
}

bool fugue_answer_rule_holds(FugueAnswerRule which, const FugueExposition *exposition) {
    if (!fugue_rule_table_valid(&TABLE, (int)which) || !fugue_exposition_valid(exposition)) {
        return false;
    }
    for (size_t i = 0; i < exposition->statement_count; i++) {
        if (exposition->statements[i].role == FUGUE_ROLE_ANSWER && !check_one(which, exposition, i)) {
            return false;
        }
    }
    return true;
}

static bool holds_adapter(int which, const void *subject) {
    return fugue_answer_rule_holds((FugueAnswerRule)which, subject);
}

bool fugue_answer_check(const FugueExposition *exposition, unsigned *hard_violations, unsigned *soft_violations) {
    if (hard_violations == NULL || soft_violations == NULL || !fugue_exposition_valid(exposition)) {
        return false;
    }
    fugue_rule_table_check(&TABLE, holds_adapter, exposition, hard_violations, soft_violations);
    return true;
}
