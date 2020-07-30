#ifndef URSATZ_FRAMEWORKS_STANDARD_MARKINGS_ARTICULATION_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_MARKINGS_ARTICULATION_RULES_H

#include <stdbool.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Articulation rules for standard-markings-v1. Articulation is the
 * connection or separation between notes and the accent level at the
 * start of a note (its attack):
 *
 *   CONNECTION_CLASSES  legato (a curved slur) and tenuto (a short line)
 *                       are smooth and connected; staccato (a dot) is
 *                       separated, leaving space between notes
 *   ACCENT_FORCE        an accent (a sideways V) is extra stress; a
 *                       marcato (an upside-down V) is a more forceful
 *                       accent
 *   SFORZANDO_ACCENT    sfz, sf, and fz (sforzando, forzando, forzato)
 *                       are accents slightly more forceful, i.e. louder,
 *                       than a regular accent; they are written like
 *                       dynamics, directly above or below the note
 *
 * Kinds are opaque strings in events/articulation.h; this file interprets
 * them, case-insensitively. Only the force relations the theory actually
 * states are ordered: marcato and sforzando each outrank an accent, but
 * marcato versus sforzando is not stated, so it is not ordered here.
 * How an articulation is carried out on a given instrument is outside
 * this framework.
 */

typedef enum {
    STANDARD_MARKINGS_ARTICULATION_RULE_CONNECTION_CLASSES = 0,
    STANDARD_MARKINGS_ARTICULATION_RULE_ACCENT_FORCE,
    STANDARD_MARKINGS_ARTICULATION_RULE_SFORZANDO_ACCENT,
    STANDARD_MARKINGS_ARTICULATION_RULE_COUNT
} StandardMarkingsArticulationRule;

typedef enum {
    ARTICULATION_CLASS_UNKNOWN,
    ARTICULATION_CLASS_CONNECTED, /* legato, slur, tenuto */
    ARTICULATION_CLASS_SEPARATED, /* staccato */
    ARTICULATION_CLASS_ACCENTED   /* accent, marcato, sforzando */
} ArticulationClass;

const Rule *standard_markings_articulation_rule(StandardMarkingsArticulationRule which);
const Constraint *standard_markings_articulation_constraint(StandardMarkingsArticulationRule which);

/* CONNECTION_CLASSES / ACCENT_FORCE / SFORZANDO_ACCENT: the class of a kind.
   ARTICULATION_CLASS_UNKNOWN for anything unrecognized or NULL. */
ArticulationClass standard_markings_articulation_class(const char *kind);

/* ACCENT_FORCE / SFORZANDO_ACCENT: true only for the stated relations --
   marcato over accent, sforzando over accent. Everything else is false,
   including comparing a kind with itself. */
bool standard_markings_articulation_more_forceful(const char *stronger, const char *weaker);

/* SFORZANDO_ACCENT: sfz, sf, fz, sforzando, forzando, forzato. */
bool standard_markings_is_sforzando(const char *kind);

/* SFORZANDO_ACCENT: sforzando marks are placed like dynamics (directly
   above or below the note); the other articulations are symbols. */
bool standard_markings_articulation_is_dynamic_like(const char *kind);

#endif
