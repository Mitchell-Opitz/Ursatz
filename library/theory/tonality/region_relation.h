#ifndef URSATZ_THEORY_TONALITY_REGION_RELATION_H
#define URSATZ_THEORY_TONALITY_REGION_RELATION_H

#include <stddef.h>

#include "model/pitch/region.h"
#include "theory/knowledge/knowledge_snapshot.h"
#include "theory/knowledge/relation.h"

/*
 * Regions named by a relation to an anchor (target architecture 4.5, 5.2): a plan says "the dominant", "two steps brighter" or "the
 * relative minor", and the relation entry (K6) resolves it from the anchor region to a concrete one. The resolved region is
 * a tonal or modal one with an ESTABLISHED strength. Only a tonal or modal anchor with a material and a center resolves; a pitch
 * field, a serial or an empty region is refused (set relations on pitch fields come with P80). Which anchor a plan means, the home
 * region or the previous one, is the caller's choice (`Relation.anchor_default` says the usual one).
 *
 * The rules, which tools/ci/check-knowledge.py checks the entries' examples against:
 *   tonal by root     the center is the anchor's plus root_semitones.
 *   tonal by degree   the center is the anchor's plus the semitones of that degree of the anchor's material.
 *   material          SAME keeps the anchor's; PARALLEL swaps major with the minor forms (major gives natural minor, natural, harmonic
 *                     and melodic minor give major); DIATONIC is major or natural minor by the quality of the triad on that degree
 *                     of the anchor's material, refused when it is diminished or augmented (there are no such keys: the closely
 *                     related keys of harmony_related_key_of); ENTRY is the entry's. Major and minor are the materials the
 *                     snapshot finds for those words, and none when the word is ambiguous.
 *   modal             the anchor's center (or that plus `center`), the diatonic material `brightness_steps` brighter
 *                     (brightness_shift); refused past locrian or lydian and for a material with no brightness.
 *   neo-Riemannian    the moves P, L and R in order on a major or natural minor triad region (P: same center, the other mode; L: a
 *                     major third up from a major center and down from a minor one, the other mode; R: a minor third down from a
 *                     major center and up from a minor one, the other mode); refused on any other material.
 *   set               Tn: the center plus n, the same material. In: the inverted collection (every pitch class p becomes n - p, the
 *                     center too) matched by mask to a material entry on the inverted center, the first in id order; refused when
 *                     no material has it.
 */
typedef enum {
    REGION_RELATION_OK,
    REGION_RELATION_ERR_ARGUMENT,         /* a NULL argument, or an anchor that is not a tonal or modal region with a material */
    REGION_RELATION_ERR_UNKNOWN_RELATION, /* region_resolve_named: no relation has the word */
    REGION_RELATION_REFUSED               /* the relation does not apply to this anchor */
} RegionRelationStatus;

/*
 * Resolves `relation` from `anchor` into *out. On anything but OK *out is untouched and `message` (when not NULL, with size above 0)
 * says why; a refusal names the relation and the anchor ("relation 'supertonic' from A natural minor: the triad on degree 2 is
 * not major or minor").
 */
RegionRelationStatus region_resolve(const KnowledgeSnapshot *snapshot, const Relation *relation, const Region *anchor, Region *out,
                                    char *message, size_t size);

/* As region_resolve, for the relation the snapshot finds by id, name or alias (case-insensitive). */
RegionRelationStatus region_resolve_named(const KnowledgeSnapshot *snapshot, const char *word, const Region *anchor, Region *out,
                                          char *message, size_t size);

#endif
