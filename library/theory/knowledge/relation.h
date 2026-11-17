#ifndef URSATZ_THEORY_KNOWLEDGE_RELATION_H
#define URSATZ_THEORY_KNOWLEDGE_RELATION_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_material.h"

/*
 * K6 relation (target architecture 5.2): a way to go from one center to another, as the knowledge entry gives it. A tonal relation
 * gives the target center by semitones above the anchor's (`root_semitones`) or by a scale degree of the anchor's material (`degree`,
 * `by_degree`), and the material it takes there: the anchor's own (SAME), the parallel one (PARALLEL: major and the minor forms
 * swap), the major or natural minor the triad on that degree makes (DIATONIC, with a degree) or an entry (ENTRY, `material_entry`,
 * resolved when the snapshot is built). A modal relation keeps the center (`same_center`) or gives another (`center`) and moves
 * `brightness_steps` along the brightness ranks. A neo-Riemannian relation is a chain of moves P, L and R, in order. A set relation
 * transposes by `n` or, with `inversion`, inverts at `n`. A serial relation is kept by its common fields only. A snapshot owns it;
 * the strings and the material live as long as the snapshot.
 */
typedef enum {
    RELATION_FAMILY_TONAL,
    RELATION_FAMILY_MODAL,
    RELATION_FAMILY_NEO_RIEMANNIAN,
    RELATION_FAMILY_SET,
    RELATION_FAMILY_SERIAL
} RelationFamily;

typedef enum {
    RELATION_MATERIAL_SAME,
    RELATION_MATERIAL_PARALLEL,
    RELATION_MATERIAL_DIATONIC,
    RELATION_MATERIAL_ENTRY
} RelationMaterial;

typedef enum {
    RELATION_ANCHOR_HOME,
    RELATION_ANCHOR_PREVIOUS
} RelationAnchor;

#define RELATION_MOVE_MAX 8

typedef struct {
    const char *id;
    const char *name;
    RelationFamily family;
    union {
        struct {
            bool by_degree;
            int root_semitones; /* set when by_degree is false */
            int degree;         /* 1 to 7, set when by_degree is true */
            RelationMaterial material;
            const PitchMaterial *material_entry; /* set when material is ENTRY */
        } tonal;
        struct {
            bool same_center;
            int center; /* 0 to 11, set when same_center is false */
            int brightness_steps;
        } modal;
        struct {
            size_t move_count;
            char moves[RELATION_MOVE_MAX]; /* 'P', 'L' or 'R' */
        } neo_riemannian;
        struct {
            bool inversion;
            int n;
        } set;
    };
    RelationAnchor anchor_default; /* HOME when the entry gives none */
} Relation;

#endif
