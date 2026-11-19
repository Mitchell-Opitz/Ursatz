#ifndef URSATZ_READING_READING_PLAN_H
#define URSATZ_READING_READING_PLAN_H

#include "plan/plan.h"
#include "reading/reading.h"
#include "realization/movement.h"
#include "theory/knowledge/knowledge_snapshot.h"

/* Why a reading was not made into a plan: a sentence naming what the reading or the movement lacks. */
typedef struct {
    char message[160];
} ReadingPlanError;

/*
 * The minimal model composition of a reading (R-635): the plan a reader's claims over `movement` give, with no notes.
 *
 *   form      `form` over [0, end), end being the last performer note's end of the movement rounded up to the next barline of its meter
 *             map;
 *   meters    Given, one for each region of the movement's meter map (cut to end), owned by the form: beats_per_bar the top number,
 *             beat_unit the bottom, bars_per_hyperbar 0;
 *   sections  one for each top-level group claim of kind section, in order and cut to end (one section `A` over [0, end) when there is
 *             none): id and letter the claim's label when it is one letter A to Z, else A, B, ... by position (an id another section
 *             holds is made unique with a number); cadence the type of the last cadence claim whose arrival is in the section's last bar;
 *   keys      Given, from the region claims: the tonic spelled by plan_key_of_pitch_class in the claim's material, a claim cut at the
 *             section boundaries and where an earlier claim already covers, each part owned by the section holding it (the form for
 *             time no section holds);
 *   harmony   Given, from the chord claims with a numeral, cut the same way, owned by the section holding the chord's start;
 *   source    plan_derived_from is {the reading's source name, the reading's target digest}.
 *
 * REALIZATION_ERR_BAD_VALUE, with a message in *error (may be NULL), for a reading that names no region ("the reading names no region":
 * a plan needs a key), no realization version (an empty target digest) or a movement with no notes; REALIZATION_ERR_ALLOC when out of
 * memory. `knowledge` is the snapshot the reading's materials are borrowed from. *out is untouched on failure.
 */
RealizationResult reading_to_plan(const Reading *reading, const Movement *movement, const KnowledgeSnapshot *knowledge, Plan **out,
                                  ReadingPlanError *error);

#endif
