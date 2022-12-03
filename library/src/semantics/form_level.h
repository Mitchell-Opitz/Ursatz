#ifndef MUSIC_SEMANTICS_FORM_LEVEL_H
#define MUSIC_SEMANTICS_FORM_LEVEL_H

#include <stdbool.h>

/*
 * FormLevel (Value): a level in the hierarchy of musical form, from the whole piece down to
 * the smallest recurring unit. A plain enum: it names a level and carries no EntityID, no
 * container, and no framework claim.
 *
 *   PIECE, MOVEMENT, SECTION, THEME, PHRASE, SUBPHRASE, IDEA, MOTIVE
 *
 * The values run from largest to smallest. The enum only names the levels. How the levels
 * nest, and that two adjacent levels may collapse into one (a single phrase can be a whole
 * section), are conventions of a Framework (frameworks/standard-form), not of this type, so
 * a framework with a different hierarchy can reuse the same names.
 */

typedef enum {
    FORM_LEVEL_PIECE = 0,
    FORM_LEVEL_MOVEMENT,
    FORM_LEVEL_SECTION,
    FORM_LEVEL_THEME,
    FORM_LEVEL_PHRASE,
    FORM_LEVEL_SUBPHRASE,
    FORM_LEVEL_IDEA,
    FORM_LEVEL_MOTIVE,
    FORM_LEVEL_COUNT
} FormLevel;

/* True for the eight levels above; false for any other value. */
bool form_level_valid(FormLevel level);

/* "piece" "movement" "section" "theme" "phrase" "subphrase" "idea" "motive"; NULL for an
   invalid level. */
const char *form_level_name(FormLevel level);

#endif
