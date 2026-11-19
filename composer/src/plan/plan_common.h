#ifndef URSATZ_COMPOSER_PLAN_COMMON_H
#define URSATZ_COMPOSER_PLAN_COMMON_H

#include <stdbool.h>
#include <stddef.h>

#include <model/core/rational.h>
#include <model/time/time_signature.h>
#include <theory/vocabulary/cadence_type.h>

#include <ursatz_composer/lanes.h>
#include <ursatz_composer/piece_plan.h>

/* The helpers the plan's own files share: the plan's lanes, cadence names, meters and keys. Private to the plan and its readers. */

/* The time signature of a plan meter (beats_per_bar over beat_unit, with the signature's default beat groups), the one
   owner of the beat (time_signature_beat_length, time_signature_is_compound); false for a meter a signature cannot hold
   (a beat unit that is not 1, 2, 4, 8 or 16, or beats_per_bar outside 1 to 64). The caller destroys *out. */
bool plan_meter_signature(PlanMeter meter, TimeSignature **out);

/* The finest level a pickup may be written at: 1/PLAN_PICKUP_FINEST of a whole note. */
#define PLAN_PICKUP_FINEST 32

/* The beat of the meter's first measure and its division (half a beat in a simple meter, a third of one in a compound
   one: the local rhythmic grid, meter_map_grid_at); false for a meter a signature cannot hold. */
bool plan_meter_grid(PlanMeter meter, Rational *beat, Rational *division);

/* Whether a pickup of this length is on the meter's grid: zero (no pickup), or a whole number of the first measure's
   beat, division, the division halved down to 1/32, or the 3:2 tuplet of the beat or of the division (meter_grid_holds).
   Every length on 1/32 qualifies; a triplet of the beat or the division does too (1/6 and 1/12 in 4/4), 1/48 and 1/64 do
   not. The bounds a pickup must also keep (shorter than a bar, longer than nothing) belong to the caller. */
bool plan_meter_pickup_on_grid(PlanMeter meter, Rational pickup);

/* Whether two spans share any time (touching ends do not overlap); false when a span's end overflows. */
bool plan_spans_overlap(PlanSpan a, PlanSpan b);

/* Whether the lane holds notes: the bass, the melody or an inner voice. */
bool plan_is_note_lane(PlanLaneKind lane);

/* The lane names the plan file and the command lines use, in PlanLaneKind order. */
#define PLAN_LANE_NAME_COUNT 6
extern const char *const plan_lane_names[PLAN_LANE_NAME_COUNT];
/* The name of a lane; "" for a value that is not one. */
const char *plan_lane_name(PlanLaneKind lane);

/*
 * The cadence types a plan holds are the library's CadenceType: PAC, IAC, HC, DC and PC (perfect and imperfect authentic,
 * half, deceptive and plagal). The plan file and the command line name them with these tables, in this order.
 */
#define PLAN_CADENCE_COUNT 5
extern const CadenceType plan_cadence_types[PLAN_CADENCE_COUNT];
extern const char *const plan_cadence_names[PLAN_CADENCE_COUNT];      /* "PAC", as the command line writes it */
extern const char *const plan_cadence_file_names[PLAN_CADENCE_COUNT]; /* "pac", as the plan file writes it */
/* The place of a type in the tables; false for a type the plan does not hold (subverted, unspecified, unknown). */
bool plan_cadence_index(CadenceType type, size_t *index);
/* The command line name of a type the plan holds, else "". */
const char *plan_cadence_name(CadenceType type);

/* PLAN_OK for RATIONAL_OK, PLAN_ERR_NULL for a NULL result, else PLAN_ERR_BAD_TIME (a zero denominator or overflow). */
PlanResult plan_result_of_rational(RationalResult result);

/* True when the key's material is one the plan holds: none (the zero key reads as C major) or a major or a minor third (common_practice_key_mode_of). */
bool plan_key_valid(const SpelledKey *key);

/* The id of the key's material; the major's for a key with none, which reads as major. */
const char *plan_key_material_id(const SpelledKey *key);

/* The word after the tonic in the key's name ("minor" in "D minor"): the material's key word; "major" for a key with none. */
const char *plan_key_word(const SpelledKey *key);

#endif
