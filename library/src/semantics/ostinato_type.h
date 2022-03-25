#ifndef MUSIC_SEMANTICS_OSTINATO_TYPE_H
#define MUSIC_SEMANTICS_OSTINATO_TYPE_H

#include <stdbool.h>

/*
 * OstinatoType (Value): the kind of repeating pattern a work or section is
 * built on, from broad to specific. A plain enum: it names a kind and
 * carries no EntityID, no pitches, and no framework claim.
 *
 *   OSTINATO     any pattern that repeats throughout a long section or whole
 *                work in one voice, like the side drum rhythm of Ravel's
 *                Boléro
 *   GROUND_BASS  a specific type of ostinato in which a bass line repeats
 *                throughout a work or section; the work itself can also be
 *                called a ground bass
 *   CHACONNE     a sub-genre of the Baroque ground bass, in triple meter
 *   PASSACAGLIA  a sub-genre of the Baroque ground bass, in triple meter
 *
 * Every chaconne or passacaglia is also a ground bass, and every ground
 * bass is also an ostinato, but not the reverse. What a framework asks of
 * each kind (frameworks/baroque-ground-bass) is its own convention.
 */

typedef enum {
    OSTINATO_TYPE_OSTINATO = 0,
    OSTINATO_TYPE_GROUND_BASS,
    OSTINATO_TYPE_CHACONNE,
    OSTINATO_TYPE_PASSACAGLIA,
    OSTINATO_TYPE_COUNT
} OstinatoType;

/* True for the four kinds above; false for any other value. */
bool ostinato_type_valid(OstinatoType type);

/* "ostinato" "ground-bass" "chaconne" "passacaglia"; NULL for an invalid
   type. */
const char *ostinato_type_name(OstinatoType type);

/* True for the kinds that are a ground bass: the ground bass itself, the
   chaconne, and the passacaglia. False for the plain ostinato and for an
   invalid type. */
bool ostinato_type_is_ground_bass(OstinatoType type);

/* True for the kinds that are in triple meter: the chaconne and the
   passacaglia. False for the others and for an invalid type. */
bool ostinato_type_is_triple_meter_form(OstinatoType type);

#endif
