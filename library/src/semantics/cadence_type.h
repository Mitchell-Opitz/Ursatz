#ifndef MUSIC_SEMANTICS_CADENCE_TYPE_H
#define MUSIC_SEMANTICS_CADENCE_TYPE_H

#include <stdbool.h>

/*
 * CadenceType (Value): the kind of cadence that ends a phrase. A plain enum:
 * it names a kind and carries no EntityID, no chords, and no framework claim.
 * It is not the Cadence structure (structure/cadence.h), which identifies a
 * cadence in a piece.
 *
 *   PERFECT_AUTHENTIC    V to I, both in root position, with do in the soprano
 *                        over the tonic chord (PAC); conclusive
 *   IMPERFECT_AUTHENTIC  V to I, but either chord inverted or something other
 *                        than do in the soprano over the tonic (IAC);
 *                        conclusive, but less so
 *   HALF                 a phrase that ends on V, whatever came before (HC);
 *                        inconclusive
 *   SUBVERTED            a potential cadence that is avoided: what follows
 *                        repeats material from the middle of the phrase, so the
 *                        run-up to the cadence is tried again
 *   PLAGAL               IV to I (the "amen" cadence); conclusive, but weaker
 *                        than an authentic cadence
 *   DECEPTIVE            V to something other than I, most often vi; the
 *                        expected resolution is avoided
 *   AUTHENTIC_UNSPECIFIED  V to I, with PAC versus IAC not yet decided (that
 *                        needs the soprano line); a cadence type that is
 *                        authentic but unrefined
 *   UNKNOWN              no cadence type could be determined. A valid value
 *                        so callers need no separate "absent" flag; it is
 *                        never ranked by a strength ordering
 *
 * Which progressions and melodies make each type is a convention of a
 * Framework (frameworks/standard-harmony), not of this type.
 */

typedef enum {
    CADENCE_TYPE_PERFECT_AUTHENTIC = 0,
    CADENCE_TYPE_IMPERFECT_AUTHENTIC,
    CADENCE_TYPE_HALF,
    CADENCE_TYPE_SUBVERTED,
    CADENCE_TYPE_PLAGAL,
    CADENCE_TYPE_DECEPTIVE,
    CADENCE_TYPE_AUTHENTIC_UNSPECIFIED,
    CADENCE_TYPE_UNKNOWN,
    CADENCE_TYPE_COUNT
} CadenceType;

/* True for the eight types above; false for any other value. */
bool cadence_type_valid(CadenceType type);

/* "perfect-authentic" "imperfect-authentic" "half" "subverted" "plagal" "deceptive"
   "authentic-unspecified" "unknown"; NULL for an invalid type. */
const char *cadence_type_name(CadenceType type);

/* A human-readable label for display and debug output only: "authentic (PAC)",
   "authentic (IAC)", "half", "subverted", "plagal", "deceptive",
   "authentic (unspecified)", "unknown". NULL for an invalid type. Not a key to
   compare or parse; compare CadenceType values. */
const char *cadence_type_label(CadenceType type);

/* True for the authentic cadences (PAC and IAC), the ones that sound conclusive.
   False for the others and for an invalid type. */
bool cadence_type_is_authentic(CadenceType type);

#endif
