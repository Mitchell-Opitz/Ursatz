#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_SUBJECT_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_SUBJECT_H

#include "counterpoint_limits.h"
#include "counterpoint_line.h"
#include "counterpoint_rules.h"
#include "fifth_species_exercise.h"
#include "fourth_species_exercise.h"
#include "pitch/pitch_spelling.h"
#include "second_species_exercise.h"
#include "third_species_exercise.h"

/*
 * The subject of a rule that needs more than one input. A rule table
 * evaluates a candidate (constraint_evaluate) whose Value holds the bytes of
 * its subject: a Line, a Pair or an Exercise for a rule that needs nothing
 * else, and one of the structs below for a rule that also needs the tonic, the
 * cantus part or the limits. The Value's type tag is
 * "standard-counterpoint/<table prefix>" (see
 * standard_counterpoint_rule_table_candidate_type). The struct is copied into
 * the Value, but the pointers in it stay borrowed: whatever they point at must
 * outlive the constraint_evaluate call.
 */

/* A single line and its tonic (cantus firmus rules). */
typedef struct {
    const StandardCounterpointLine *line;
    const PitchSpelling *tonic;
} StandardCounterpointLineTonic;

/* A single line and the limits it is written within (melodic rules). */
typedef struct {
    const StandardCounterpointLine *line;
    const StandardCounterpointLimits *limits;
} StandardCounterpointLineLimits;

/* A two-part pair and which part is the cantus firmus. */
typedef struct {
    const StandardCounterpointPair *pair;
    StandardCounterpointCantusPart cantus_part;
} StandardCounterpointPairCantus;

/* A two-part pair, which part is the cantus firmus, and the tonic. */
typedef struct {
    const StandardCounterpointPair *pair;
    StandardCounterpointCantusPart cantus_part;
    const PitchSpelling *tonic;
} StandardCounterpointPairCantusTonic;

/* A species exercise and its tonic. */
typedef struct {
    const StandardCounterpointSecondSpeciesExercise *exercise;
    const PitchSpelling *tonic;
} StandardCounterpointSecondSpeciesTonic;

typedef struct {
    const StandardCounterpointThirdSpeciesExercise *exercise;
    const PitchSpelling *tonic;
} StandardCounterpointThirdSpeciesTonic;

typedef struct {
    const StandardCounterpointFourthSpeciesExercise *exercise;
    const PitchSpelling *tonic;
} StandardCounterpointFourthSpeciesTonic;

typedef struct {
    const StandardCounterpointFifthSpeciesExercise *exercise;
    const PitchSpelling *tonic;
} StandardCounterpointFifthSpeciesTonic;

#endif
