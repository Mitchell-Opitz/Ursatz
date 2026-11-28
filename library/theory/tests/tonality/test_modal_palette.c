#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/tonality/modal_palette.h"
#include "ursatz_test_knowledge.h"

/*
 * The palette (R-765): C natural minor with C dorian share degrees 1, 3, 5 and 7 (Cm, Eb, Gm, Bb: i, bIII, v, bVII); dorian's color carriers are
 * ii, IV and vio, the aeolian ones iio, iv and bVI; one material alone has nothing shared and no color; the numerals are the OMT spelling against the
 * center's major scale; a material of another size is refused.
 */

static const PitchMaterial *material(const char *word) {
    const PitchMaterial *found = knowledge_snapshot_material_named(test_knowledge(), word);
    assert(found != NULL);
    return found;
}

static const PaletteChord *chord_named(const PaletteChord *list, size_t count, const char *numeral) {
    for (size_t i = 0; i < count; i++) {
        if (strcmp(list[i].numeral, numeral) == 0) {
            return &list[i];
        }
    }
    return NULL;
}

static void test_minor_and_dorian(void) {
    PaletteChord palette[2][PALETTE_DEGREES];
    size_t counts[2];
    assert(modal_palette(material("minor"), material("dorian"), palette, counts));
    assert(counts[0] == 7 && counts[1] == 7);
    static const char *const SHARED[] = {"i", "bIII", "v", "bVII"};
    for (size_t i = 0; i < 2; i++) {
        size_t shared = 0;
        for (size_t c = 0; c < counts[i]; c++) {
            shared += palette[i][c].shared;
        }
        assert(shared == 4);
        for (size_t s = 0; s < 4; s++) {
            const PaletteChord *chord = chord_named(palette[i], counts[i], SHARED[s]);
            assert(chord != NULL && chord->shared && !chord->color);
        }
    }
    /* the same degrees of the minor (1, 3, 5, 7) are the shared ones */
    assert(palette[0][0].shared && palette[0][2].shared && palette[0][4].shared && palette[0][6].shared);
    assert(!palette[0][1].shared && !palette[0][3].shared && !palette[0][5].shared);
    static const char *const DORIAN_COLOR[] = {"ii", "IV", "vio"};
    static const char *const MINOR_COLOR[] = {"iio", "iv", "bVI"};
    size_t dorian_color = 0;
    size_t minor_color = 0;
    for (size_t c = 0; c < 7; c++) {
        dorian_color += palette[1][c].color;
        minor_color += palette[0][c].color;
    }
    assert(dorian_color == 3 && minor_color == 3);
    for (size_t s = 0; s < 3; s++) {
        const PaletteChord *d = chord_named(palette[1], counts[1], DORIAN_COLOR[s]);
        const PaletteChord *m = chord_named(palette[0], counts[0], MINOR_COLOR[s]);
        assert(d != NULL && d->color && !d->shared && m != NULL && m->color && !m->shared);
    }
    /* the roots, qualities and degrees */
    const PaletteChord *four = chord_named(palette[1], counts[1], "IV");
    assert(four->root_semitones == 5 && four->quality == STANDARD_TONALITY_TRIAD_MAJOR && four->degree == 4 && four->alteration == 0);
    const PaletteChord *flat_three = chord_named(palette[0], counts[0], "bIII");
    assert(flat_three->root_semitones == 3 && flat_three->degree == 3 && flat_three->alteration == -1);
    const PaletteChord *six = chord_named(palette[1], counts[1], "vio");
    assert(six->root_semitones == 9 && six->quality == STANDARD_TONALITY_TRIAD_DIMINISHED && six->degree == 6);
}

static void test_one_material(void) {
    PaletteChord palette[2][PALETTE_DEGREES];
    size_t counts[2] = {9, 9};
    assert(modal_palette(material("major"), NULL, palette, counts));
    assert(counts[0] == 7 && counts[1] == 0);
    for (size_t c = 0; c < counts[0]; c++) {
        assert(!palette[0][c].shared && !palette[0][c].color);
    }
    assert(strcmp(palette[0][0].numeral, "I") == 0 && strcmp(palette[0][1].numeral, "ii") == 0 && strcmp(palette[0][6].numeral, "viio") == 0);
    /* lydian on the same center: the raised fourth is the sharp fourth's diminished chord */
    assert(modal_palette(material("major"), material("lydian"), palette, counts));
    const PaletteChord *two = chord_named(palette[1], counts[1], "II");
    assert(two != NULL && two->color && !two->shared && two->root_semitones == 2);
    assert(chord_named(palette[1], counts[1], "#ivo") != NULL);
}

static void test_refusals(void) {
    PaletteChord palette[2][PALETTE_DEGREES];
    size_t counts[2] = {1, 1};
    assert(!modal_palette(NULL, NULL, palette, counts) && !modal_palette(material("major"), NULL, NULL, counts) &&
           !modal_palette(material("major"), NULL, palette, NULL));
    PitchMaterial five = *material("major"); /* a material of five steps (a copy with fewer steps) */
    five.step_count = 5;
    assert(!modal_palette(&five, NULL, palette, counts) && !modal_palette(material("major"), &five, palette, counts));
    assert(counts[0] == 1 && counts[1] == 1);
}

int main(void) {
    test_minor_and_dorian();
    test_one_material();
    test_refusals();
    return 0;
}
