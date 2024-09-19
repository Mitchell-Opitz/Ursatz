#ifndef MUSIC_SEMANTICS_IDEA_LABEL_H
#define MUSIC_SEMANTICS_IDEA_LABEL_H

#include <stdbool.h>

/*
 * IdeaLabel (Value): the label an idea carries in a phrase-level analysis. A plain enum: it names a value and carries no EntityID, no
 * container, and no framework claim.
 *
 *   BASIC_IDEA            the opening idea of a presentation or of a period phrase (b.i.)
 *   CONTRASTING_IDEA      the idea that follows a basic idea in a period phrase (c.i.)
 *   FRAGMENT              a unit shorter than the units before it (f.)
 *   UNIT                  a grouping that only expresses the traits of the grouping above it (u.)
 *
 * Numbering (b.i.1, b.i.2) is left to the caller. Where each label goes is a convention of a
 * Framework (frameworks/standard-form), not of this type.
 */

typedef enum {
    IDEA_LABEL_BASIC_IDEA = 0,
    IDEA_LABEL_CONTRASTING_IDEA,
    IDEA_LABEL_FRAGMENT,
    IDEA_LABEL_UNIT,
    IDEA_LABEL_COUNT
} IdeaLabel;

/* True for the 4 values above; false for any other value. */
bool idea_label_valid(IdeaLabel value);

/* "basic-idea" "contrasting-idea" "fragment" "unit"; NULL for an invalid
   value. */
const char *idea_label_name(IdeaLabel value);

/* "b.i." "c.i." "f." "u."; NULL for an invalid value. */
const char *idea_label_abbreviation(IdeaLabel value);

#endif
