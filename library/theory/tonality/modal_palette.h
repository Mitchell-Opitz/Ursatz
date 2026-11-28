#ifndef URSATZ_THEORY_TONALITY_MODAL_PALETTE_H
#define URSATZ_THEORY_TONALITY_MODAL_PALETTE_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_material.h"
#include "theory/tonality/triad_rules.h"

/*
 * The palette of a center (R-765): the triads of one or two seven-step materials on the same center, which ones both hold (shared) and
 * which carry a pitch class the other material lacks (color). Nothing here knows the Composer's chord text; a palette chord is named by
 * its degree and alteration against the center's major scale ("bIII", "vio") and the Composer maps it onto text it reads.
 */

#define PALETTE_DEGREES 7
#define PALETTE_NUMERAL_MAX 8

typedef struct {
    int degree;                           /* 1 to 7 against the center's major scale (bIII is 3 with alteration -1) */
    int alteration;                       /* -1, 0 or +1 semitone on the root */
    StandardTonalityTriadQuality quality;
    int root_semitones;                   /* of the root above the center, 0 to 11 */
    char numeral[PALETTE_NUMERAL_MAX];    /* OMT notation: i, bIII, vio, IV, #ivo (case and o or + from the quality) */
    bool shared;                          /* the other material holds a triad of this root and quality */
    bool color;                           /* the triad holds a pitch class the other material lacks */
} PaletteChord;

/*
 * The triads on degrees 1 to 7 of each material (pitch_material_triad_on: a degree whose stack is no major, minor, diminished or
 * augmented triad is left out), in the material's degree order: out[0] for `first`, out[1] for `second`, counts[0] and counts[1] their
 * lengths. `second` may be NULL (counts[1] is 0, nothing is shared or color). False for a NULL `first` or `out` or `counts`, or a
 * material without seven steps; the outputs are then untouched.
 */
bool modal_palette(const PitchMaterial *first, const PitchMaterial *second, PaletteChord out[2][PALETTE_DEGREES], size_t counts[2]);

/* The degree (1 to 7) and alteration (-1, 0, +1) against the center's major scale of a root `semitones` above the center (0 to 11): b2, b3, #4, b6 and b7 for the five that are no step. */
void modal_palette_major_degree(int semitones, int *degree, int *alteration);

#endif
