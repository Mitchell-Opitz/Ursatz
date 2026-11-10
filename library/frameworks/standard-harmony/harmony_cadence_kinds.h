#ifndef URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CADENCE_KINDS_H
#define URSATZ_FRAMEWORKS_STANDARD_HARMONY_HARMONY_CADENCE_KINDS_H

#include <stdbool.h>
#include <stddef.h>

#include "semantics/cadence_type.h"

/*
 * The cadence vocabulary of standard-harmony-v1 (R-415). CadenceType is the
 * frozen shared core; a kind is a name this framework gives a cadence, reduced
 * to a core type. Kinds the core has no value for (Caplin's evaded and
 * abandoned cadences, Hepokoski and Darcy's medial caesura) are entries here,
 * never new core values.
 *
 *   name          the key: [a-z-], one per kind; the core types' own entries are
 *                 named cadence_type_name(core)
 *   label         a name for people
 *   abbreviation  PAC, IAC, AC, PC, DC, HC, EV, AB, MC
 *   core          the CadenceType the kind reduces to; CADENCE_TYPE_UNKNOWN for
 *                 abandoned, which has no cadence
 *   strength      the order of cadence strength, -1 for a kind that is never
 *                 ranked (abandoned); harmony_cadence_strength reads it
 *   conclusive    it closes a phrase or a piece
 *
 * Evaded is Caplin's name for what the core calls subverted; medial caesura is
 * a half cadence that ends a transition.
 */

typedef struct {
    const char *name;
    const char *label;
    const char *abbreviation;
    CadenceType core;
    int strength;
    bool conclusive;
} HarmonyCadenceKind;

/* The number of kinds. */
size_t harmony_cadence_kind_count(void);

/* The kind at index in table order; NULL past the end. */
const HarmonyCadenceKind *harmony_cadence_kind_at(size_t index);

/* The kind with this name; NULL for an unknown name or NULL. */
const HarmonyCadenceKind *harmony_cadence_kind_find(const char *name);

/* The kind named cadence_type_name(core); NULL for CADENCE_TYPE_UNKNOWN and for
   an invalid value. */
const HarmonyCadenceKind *harmony_cadence_kind_of(CadenceType core);

#endif
