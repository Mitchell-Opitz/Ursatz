#include "theory/tonality/modal_palette.h"

#include <string.h>

#include "theory/tonality/modal_chords.h"
#include "theory/tonality/roman_numeral_grammar.h"

#define STEPS 7

/* The degree and alteration against the major scale of the center of a root `semitones` above it: b2, b3, #4, b6 and b7 for the five that are not steps. */
static void major_degree_of(int semitones, int *degree, int *alteration) {
    static const int DEGREE[12] = {1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7};
    static const int ALTERATION[12] = {0, -1, 0, -1, 0, 0, 1, 0, -1, 0, -1, 0};
    *degree = DEGREE[semitones];
    *alteration = ALTERATION[semitones];
}

static bool has_triad(const PitchMaterial *material, int root, StandardTonalityTriadQuality quality) {
    for (int degree = 1; degree <= STEPS; degree++) {
        int found_root;
        StandardTonalityTriadQuality found_quality;
        if (pitch_material_triad_on(material, degree, &found_root, NULL, &found_quality) && found_root == root && found_quality == quality) {
            return true;
        }
    }
    return false;
}

static size_t fill(const PitchMaterial *material, const PitchMaterial *other, PaletteChord out[PALETTE_DEGREES]) {
    size_t count = 0;
    for (int degree = 1; degree <= STEPS; degree++) {
        int root;
        int intervals[3];
        StandardTonalityTriadQuality quality;
        if (!pitch_material_triad_on(material, degree, &root, intervals, &quality)) {
            continue;
        }
        PaletteChord *chord = &out[count];
        memset(chord, 0, sizeof(*chord));
        chord->quality = quality;
        chord->root_semitones = root;
        major_degree_of(root, &chord->degree, &chord->alteration);
        StandardTonalityNumeralText text = {chord->alteration, chord->degree, (StandardTonalityNumeralQuality)quality, 0, false, 0, 0, false, NULL};
        if (!standard_tonality_numeral_format(&text, chord->numeral, sizeof(chord->numeral))) {
            chord->numeral[0] = '\0';
        }
        if (other != NULL) {
            chord->shared = has_triad(other, root, quality);
            for (int k = 0; k < 3; k++) {
                chord->color = chord->color || !pitch_material_contains(other, root + intervals[k]);
            }
        }
        count++;
    }
    return count;
}

bool modal_palette(const PitchMaterial *first, const PitchMaterial *second, PaletteChord out[2][PALETTE_DEGREES], size_t counts[2]) {
    if (first == NULL || out == NULL || counts == NULL || first->step_count != STEPS || (second != NULL && second->step_count != STEPS)) {
        return false;
    }
    counts[0] = fill(first, second, out[0]);
    counts[1] = second != NULL ? fill(second, first, out[1]) : 0;
    return true;
}
