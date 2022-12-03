#ifndef MUSIC_SEMANTICS_MOTIVE_KIND_H
#define MUSIC_SEMANTICS_MOTIVE_KIND_H

#include <stdbool.h>

/*
 * MotiveKind (Value): the musical dimension a motive is recognized by. A plain enum: it names
 * a dimension and carries no EntityID, no Motif, and no framework claim.
 *
 *   PITCH      recognized by its pitches; what "motive" means by itself
 *   RHYTHMIC   recognized mainly by its rhythmic design
 *   CONTOUR    recognized mainly by its shape of rises and falls
 *   TIMBRE     recognized mainly by its color
 *
 * Which kind a bare "motive" means is a convention of a Framework (frameworks/standard-form),
 * not of this type.
 */

typedef enum {
    MOTIVE_KIND_PITCH = 0,
    MOTIVE_KIND_RHYTHMIC,
    MOTIVE_KIND_CONTOUR,
    MOTIVE_KIND_TIMBRE,
    MOTIVE_KIND_COUNT
} MotiveKind;

/* True for the four kinds above; false for any other value. */
bool motive_kind_valid(MotiveKind kind);

/* "pitch" "rhythmic" "contour" "timbre"; NULL for an invalid kind. */
const char *motive_kind_name(MotiveKind kind);

#endif
