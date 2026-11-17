#include "theory/tonality/tonal_key.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "test_knowledge.h"

/*
 * The material-based tonal functions (tonal_key.h) against the mode-based ones: on the four tonal materials every degree, semitone,
 * tonic and key agrees; the signatures of the seven diatonic modes on every tonic; the imaginary keys beyond seven accidentals; and
 * the natural degrees that need no key.
 */

#define TONAL_COUNT 4

static const StandardTonalityMode MODES[TONAL_COUNT] = {STANDARD_TONALITY_MODE_MAJOR, STANDARD_TONALITY_MODE_NATURAL_MINOR,
                                                        STANDARD_TONALITY_MODE_HARMONIC_MINOR, STANDARD_TONALITY_MODE_MELODIC_MINOR};

static const PitchMaterial *tonal_material(int index) {
    const CommonPracticeMaterials *m = test_materials();
    const PitchMaterial *all[TONAL_COUNT] = {m->major, m->natural_minor, m->harmonic_minor, m->melodic_minor};
    return all[index];
}

static bool minor_of(int index) {
    return index != 0;
}

static void test_degrees_and_masks(void) {
    for (int index = 0; index < TONAL_COUNT; index++) {
        const PitchMaterial *material = tonal_material(index);
        for (int degree = -1; degree <= 9; degree++) {
            assert(standard_tonality_material_degree_semitone(material, degree) ==
                   standard_tonality_degree_semitone(MODES[index], degree));
        }
        for (int tonic = -13; tonic <= 25; tonic++) {
            int pitch_class = ((tonic % 12) + 12) % 12;
            assert(standard_tonality_material_pitch_classes(tonic, material) ==
                   standard_tonality_mode_pitch_classes(pitch_class, MODES[index]));
            assert(standard_tonality_material_pitch_classes_with_leading_tone(tonic, material) ==
                   standard_tonality_mode_pitch_classes_with_leading_tone(pitch_class, MODES[index]));
            assert(standard_tonality_material_pitch_classes(tonic, material) == pitch_material_mask(material, tonic));
        }
    }
    assert(standard_tonality_material_degree_semitone(NULL, 1) == -1);
    assert(standard_tonality_material_pitch_classes(0, NULL) == 0);
    assert(standard_tonality_material_pitch_classes_with_leading_tone(0, NULL) == 0);
}

static void test_semitone_readings(void) {
    for (int index = 0; index < TONAL_COUNT; index++) {
        const PitchMaterial *material = tonal_material(index);
        for (int semitone = -30; semitone <= 40; semitone++) {
            int mode_degree = 0;
            int mode_alteration = 0;
            int degree = 0;
            int alteration = 0;
            bool by_mode = standard_tonality_degree_at_or_below_semitone(semitone, MODES[index], &mode_degree, &mode_alteration);
            bool by_material = standard_tonality_material_degree_at_or_below_semitone(semitone, material, &degree, &alteration);
            assert(by_mode == by_material && degree == mode_degree && alteration == mode_alteration);

            by_mode = standard_tonality_degree_of_semitone(semitone, MODES[index], &mode_degree, &mode_alteration);
            by_material = standard_tonality_material_degree_of_semitone(semitone, material, &degree, &alteration);
            assert(by_mode == by_material && degree == mode_degree && alteration == mode_alteration);
        }
        int degree = 7;
        int alteration = 7;
        assert(!standard_tonality_material_degree_of_semitone(0, NULL, &degree, &alteration) && degree == 7 && alteration == 7);
        assert(!standard_tonality_material_degree_of_semitone(0, material, NULL, &alteration));
        assert(!standard_tonality_material_degree_at_or_below_semitone(0, material, &degree, NULL));
    }
}

/* The natural degrees read against the major scale with no key. */
static void test_natural_degrees(void) {
    const PitchMaterial *major = tonal_material(0);
    for (int degree = -1; degree <= 9; degree++) {
        assert(standard_tonality_natural_degree_semitone(degree) == standard_tonality_material_degree_semitone(major, degree));
    }
    for (int semitone = -30; semitone <= 40; semitone++) {
        int natural_degree = 0;
        int natural_alteration = 0;
        int degree = 0;
        int alteration = 0;
        assert(standard_tonality_natural_degree_of_semitone(semitone, &natural_degree, &natural_alteration));
        assert(standard_tonality_material_degree_of_semitone(semitone, major, &degree, &alteration));
        assert(natural_degree == degree && natural_alteration == alteration);
    }
    assert(!standard_tonality_natural_degree_of_semitone(0, NULL, NULL));
}

static void test_key_signatures_agree(void) {
    for (int index = 0; index < TONAL_COUNT; index++) {
        const PitchMaterial *material = tonal_material(index);
        for (int letter = PITCH_LETTER_A; letter <= PITCH_LETTER_G; letter++) {
            for (int accidental = -2; accidental <= 2; accidental++) {
                int by_mode = 99;
                int by_material = -99;
                assert(standard_tonality_key_fifths((PitchLetter)letter, accidental, minor_of(index), &by_mode));
                assert(standard_tonality_key_fifths_of((PitchLetter)letter, accidental, material, &by_material));
                assert(by_mode == by_material);

                KeySignature *mode_signature = NULL;
                KeySignature *material_signature = NULL;
                bool mode_ok = standard_tonality_key_signature_of((PitchLetter)letter, accidental, minor_of(index), &mode_signature);
                bool material_ok =
                    standard_tonality_key_signature_of_material((PitchLetter)letter, accidental, material, &material_signature);
                assert(mode_ok == material_ok);
                if (mode_ok) {
                    assert(key_signature_entry_count(mode_signature) == key_signature_entry_count(material_signature));
                    key_signature_destroy(mode_signature);
                    key_signature_destroy(material_signature);
                }
            }
        }
        for (int fifths = -12; fifths <= 12; fifths++) {
            PitchLetter mode_letter = PITCH_LETTER_A;
            PitchLetter material_letter = PITCH_LETTER_B;
            int mode_accidental = 0;
            int material_accidental = 9;
            assert(standard_tonality_key_of_fifths(fifths, minor_of(index), &mode_letter, &mode_accidental));
            assert(standard_tonality_key_of_fifths_material(fifths, material, &material_letter, &material_accidental));
            assert(mode_letter == material_letter && mode_accidental == material_accidental);
        }
        for (int pitch_class = 0; pitch_class < 12; pitch_class++) {
            PitchLetter mode_letter = PITCH_LETTER_A;
            PitchLetter material_letter = PITCH_LETTER_B;
            int mode_accidental = 0;
            int material_accidental = 9;
            assert(standard_tonality_key_tonic_of_pitch_class(pitch_class, minor_of(index), &mode_letter, &mode_accidental));
            assert(standard_tonality_key_tonic_of_pitch_class_material(pitch_class, material, &material_letter,
                                                                       &material_accidental));
            assert(mode_letter == material_letter && mode_accidental == material_accidental);
        }
    }
}

/* Harmonic and melodic minor use the natural minor's signature, as the modes do. */
static void test_minor_forms_share_a_signature(void) {
    for (int letter = PITCH_LETTER_A; letter <= PITCH_LETTER_G; letter++) {
        for (int accidental = -1; accidental <= 1; accidental++) {
            int natural = 0;
            int harmonic = 1;
            int melodic = 2;
            assert(standard_tonality_key_fifths_of((PitchLetter)letter, accidental, tonal_material(1), &natural));
            assert(standard_tonality_key_fifths_of((PitchLetter)letter, accidental, tonal_material(2), &harmonic));
            assert(standard_tonality_key_fifths_of((PitchLetter)letter, accidental, tonal_material(3), &melodic));
            assert(natural == harmonic && natural == melodic);
        }
    }
}

/* The seven diatonic modes on every tonic: the signature is the major key's of the tonic, moved by the mode's place on the line of
   fifths (lydian +1, ionian 0, mixolydian -1, dorian -2, aeolian -3, phrygian -4, locrian -5). */
static void test_seven_modes_on_every_tonic(void) {
    static const struct {
        const char *id;
        int offset;
    } DIATONIC[7] = {{"omt.material.lydian", 1},  {"omt.material.major", 0},    {"omt.material.mixolydian", -1}, {"omt.material.dorian", -2},
                     {"omt.material.natural-minor", -3}, {"omt.material.phrygian", -4}, {"omt.material.locrian", -5}};
    for (size_t mode = 0; mode < 7; mode++) {
        const PitchMaterial *material = knowledge_snapshot_material(test_knowledge(), DIATONIC[mode].id);
        assert(material != NULL && pitch_material_is_diatonic(material));
        for (int letter = PITCH_LETTER_A; letter <= PITCH_LETTER_G; letter++) {
            for (int accidental = -2; accidental <= 2; accidental++) {
                int major = 0;
                int fifths = 0;
                assert(standard_tonality_key_fifths((PitchLetter)letter, accidental, false, &major));
                assert(standard_tonality_key_fifths_of((PitchLetter)letter, accidental, material, &fifths));
                assert(fifths == major + DIATONIC[mode].offset);
            }
        }
    }
    int fifths = 99;
    const PitchMaterial *dorian = knowledge_snapshot_material(test_knowledge(), "omt.material.dorian");
    const PitchMaterial *phrygian = knowledge_snapshot_material(test_knowledge(), "omt.material.phrygian");
    const PitchMaterial *lydian = knowledge_snapshot_material(test_knowledge(), "omt.material.lydian");
    const PitchMaterial *mixolydian = knowledge_snapshot_material(test_knowledge(), "omt.material.mixolydian");
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_D, 0, dorian, &fifths) && fifths == 0);
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_E, 0, phrygian, &fifths) && fifths == 0);
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_F, 0, lydian, &fifths) && fifths == 0);
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_B, -1, mixolydian, &fifths) && fifths == -3);
}

/* Keys beyond seven accidentals have a count as before, and no KeySignature. */
static void test_imaginary_keys(void) {
    int fifths = 0;
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_G, 1, tonal_material(0), &fifths) && fifths == 8);
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_F, -1, tonal_material(0), &fifths) && fifths == -8);
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_A, 1, tonal_material(1), &fifths) && fifths == 7);
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_D, 1, tonal_material(1), &fifths) && fifths == 6);
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_G, 1, tonal_material(1), &fifths) && fifths == 5);
    assert(standard_tonality_key_fifths_of(PITCH_LETTER_D, 1, tonal_material(0), &fifths) && fifths == 9);
    KeySignature *signature = NULL;
    assert(!standard_tonality_key_signature_of_material(PITCH_LETTER_G, 1, tonal_material(0), &signature) && signature == NULL);
    assert(standard_tonality_key_signature_of_material(PITCH_LETTER_C, -1, tonal_material(0), &signature) && signature != NULL);
    key_signature_destroy(signature);
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 0;
    assert(standard_tonality_key_of_fifths_material(8, tonal_material(0), &letter, &accidental) && letter == PITCH_LETTER_G &&
           accidental == 1);
}

static void test_refusals(void) {
    int fifths = 5;
    assert(!standard_tonality_key_fifths_of(PITCH_LETTER_C, 0, NULL, &fifths) && fifths == 5);
    assert(!standard_tonality_key_fifths_of(PITCH_LETTER_C, 0, tonal_material(0), NULL));
    assert(!standard_tonality_key_fifths_of((PitchLetter)42, 0, tonal_material(0), &fifths) && fifths == 5);
    /* A material that is neither diatonic nor a variant of a diatonic one has no signature. */
    PitchMaterial pair;
    memset(&pair, 0, sizeof(pair));
    pair.step_count = 2;
    pair.steps[1] = 7;
    assert(!standard_tonality_key_fifths_of(PITCH_LETTER_C, 0, &pair, &fifths) && fifths == 5);
    PitchLetter letter = PITCH_LETTER_A;
    int accidental = 0;
    assert(!standard_tonality_key_of_fifths_material(0, &pair, &letter, &accidental));
    assert(!standard_tonality_key_tonic_of_pitch_class_material(0, &pair, &letter, &accidental));
    assert(!standard_tonality_key_tonic_of_pitch_class_material(12, tonal_material(0), &letter, &accidental));
    assert(!standard_tonality_key_of_fifths_material(0, NULL, &letter, &accidental));
}

int main(void) {
    test_degrees_and_masks();
    test_semitone_readings();
    test_natural_degrees();
    test_key_signatures_agree();
    test_minor_forms_share_a_signature();
    test_seven_modes_on_every_tonic();
    test_imaginary_keys();
    test_refusals();
    puts("tonal key materials: ok");
    return 0;
}
