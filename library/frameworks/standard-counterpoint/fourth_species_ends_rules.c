#include "fourth_species_ends_rules.h"

#include <stddef.h>

#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"
#include "fourth_species_suspension.h"
#include "rule_table.h"

static const char *const NAMES[STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_COUNT] = {
    "begin", "cantus-ends-re-do", "penultimate-suspension", "final-bar"};

static StandardCounterpointRulePair g_pairs[STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_COUNT];
static bool g_built[STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_COUNT];
static const StandardCounterpointRuleTable TABLE = {
    "fourth-species-ends", NAMES, STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_COUNT, NULL, g_pairs, g_built};

static bool valid_rule(StandardCounterpointFourthSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_valid(&TABLE, (int)which);
}

const Rule *standard_counterpoint_fourth_species_ends_rule(StandardCounterpointFourthSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_rule(&TABLE, (int)which);
}

const Constraint *standard_counterpoint_fourth_species_ends_constraint(StandardCounterpointFourthSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_constraint(&TABLE, (int)which);
}

const char *standard_counterpoint_fourth_species_ends_rule_name(StandardCounterpointFourthSpeciesEndsRule which) {
    return standard_counterpoint_rule_table_name(&TABLE, (int)which);
}

typedef StandardCounterpointFourthSpeciesExercise Exercise;

static bool is_tonic(const PitchSpelling *note, const PitchSpelling *tonic) {
    return pitch_spelling_letter(note) == pitch_spelling_letter(tonic) &&
           pitch_spelling_accidental(note) == pitch_spelling_accidental(tonic);
}

/* A perfect fifth above the tonic, at any octave. */
static bool is_sol(const PitchSpelling *note, const PitchSpelling *tonic) {
    int letters = (((standard_counterpoint_step_number(note) - standard_counterpoint_step_number(tonic)) % 7) + 7) % 7;
    int semitones =
        (((standard_counterpoint_semitone_number(note) - standard_counterpoint_semitone_number(tonic)) % 12) + 12) % 12;
    return letters == 4 && semitones == 7;
}

/* The leading tone: the letter below the tonic's, a half step below it. */
static bool is_leading_tone(const PitchSpelling *note, const PitchSpelling *tonic) {
    int letters = (((standard_counterpoint_step_number(note) - standard_counterpoint_step_number(tonic)) % 7) + 7) % 7;
    int semitones =
        (((standard_counterpoint_semitone_number(tonic) - standard_counterpoint_semitone_number(note)) % 12) + 12) % 12;
    return letters == 6 && semitones == 1;
}

/* The supertonic: the letter above the tonic's, a whole or half step above it. */
static bool is_supertonic(const PitchSpelling *note, const PitchSpelling *tonic) {
    int letters = (((standard_counterpoint_step_number(note) - standard_counterpoint_step_number(tonic)) % 7) + 7) % 7;
    int semitones =
        (((standard_counterpoint_semitone_number(note) - standard_counterpoint_semitone_number(tonic)) % 12) + 12) % 12;
    return letters == 1 && (semitones == 1 || semitones == 2);
}

static bool check_begin(const Exercise *exercise, const PitchSpelling *tonic) {
    const PitchSpelling *first = standard_counterpoint_fourth_species_weak(exercise, 0);
    if (exercise->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER) {
        return is_tonic(first, tonic) || is_sol(first, tonic);
    }
    return is_tonic(first, tonic);
}

static bool check_cantus_ends_re_do(const Exercise *exercise, const PitchSpelling *tonic) {
    size_t bars = exercise->cantus->count;
    return is_supertonic(exercise->cantus->notes[bars - 2], tonic) && is_tonic(exercise->cantus->notes[bars - 1], tonic);
}

static bool check_penultimate_suspension(const Exercise *exercise, const PitchSpelling *tonic) {
    size_t bar = exercise->cantus->count - 2;
    StandardCounterpointSuspensionType type;
    if (!standard_counterpoint_fourth_species_suspension(exercise, bar, &type)) {
        return false;
    }
    bool above = exercise->cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER;
    StandardCounterpointSuspensionType expected =
        above ? STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX : STANDARD_COUNTERPOINT_SUSPENSION_TWO_THREE;
    return type == expected && is_tonic(standard_counterpoint_fourth_species_downbeat(exercise, bar), tonic) &&
           is_leading_tone(standard_counterpoint_fourth_species_weak(exercise, bar), tonic);
}

static bool check_final_bar(const Exercise *exercise, const PitchSpelling *tonic) {
    size_t bar = exercise->cantus->count - 1;
    return is_tonic(standard_counterpoint_fourth_species_downbeat(exercise, bar), tonic);
}

bool standard_counterpoint_fourth_species_ends_rule_holds(StandardCounterpointFourthSpeciesEndsRule which,
                                                          const StandardCounterpointFourthSpeciesExercise *exercise,
                                                          const PitchSpelling *tonic) {
    if (!valid_rule(which) || tonic == NULL || !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    switch (which) {
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_BEGIN:
            return check_begin(exercise, tonic);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_CANTUS_ENDS_RE_DO:
            return check_cantus_ends_re_do(exercise, tonic);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_PENULTIMATE_SUSPENSION:
            return check_penultimate_suspension(exercise, tonic);
        case STANDARD_COUNTERPOINT_FOURTH_SPECIES_ENDS_FINAL_BAR:
            return check_final_bar(exercise, tonic);
        default:
            return false;
    }
}

typedef struct {
    const StandardCounterpointFourthSpeciesExercise *exercise;
    const PitchSpelling *tonic;
} Subject;

static bool holds_adapter(int which, const void *subject) {
    const Subject *s = subject;
    return standard_counterpoint_fourth_species_ends_rule_holds((StandardCounterpointFourthSpeciesEndsRule)which,
                                                                s->exercise, s->tonic);
}

bool standard_counterpoint_fourth_species_ends_check(const StandardCounterpointFourthSpeciesExercise *exercise,
                                                     const PitchSpelling *tonic, unsigned *violations) {
    if (violations == NULL || tonic == NULL || !standard_counterpoint_fourth_species_exercise_valid(exercise)) {
        return false;
    }
    Subject subject = {exercise, tonic};
    unsigned unused_soft;
    standard_counterpoint_rule_table_check(&TABLE, holds_adapter, &subject, violations, &unused_soft);
    return true;
}
