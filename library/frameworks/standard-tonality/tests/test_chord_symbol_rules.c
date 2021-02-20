#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "chord_symbol_rules.h"
#include "framework.h"
#include "identity/entity_id.h"
#include "interpretation/chord.h"
#include "spelling_test_util.h"

typedef StandardTonalityTriadQuality TriadQuality;

#define MAJOR STANDARD_TONALITY_TRIAD_MAJOR
#define MINOR STANDARD_TONALITY_TRIAD_MINOR
#define DIMINISHED STANDARD_TONALITY_TRIAD_DIMINISHED
#define AUGMENTED STANDARD_TONALITY_TRIAD_AUGMENTED

static void check_symbol(PitchLetter letter, int accidental, TriadQuality quality, bool has_bass, PitchLetter bass_letter,
                         int bass_accidental, const char *expected) {
    char buffer[32];
    assert(standard_tonality_chord_symbol(letter, accidental, quality, has_bass, bass_letter, bass_accidental, buffer,
                                          sizeof(buffer)));
    assert(strcmp(buffer, expected) == 0);
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_TONALITY_CHORD_SYMBOL_RULE_COUNT; i++) {
        const Rule *rule = standard_tonality_chord_symbol_rule((StandardTonalityChordSymbolRule)i);
        const Constraint *constraint = standard_tonality_chord_symbol_constraint((StandardTonalityChordSymbolRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_tonality_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_tonality_chord_symbol_rule(STANDARD_TONALITY_CHORD_SYMBOL_RULE_COUNT) == NULL);
}

/* QUALITY: major has no symbol, minor "mi", diminished a circle, augmented
   a plus sign. */
static void test_quality_symbols(void) {
    assert(strcmp(standard_tonality_triad_quality_symbol(MAJOR), "") == 0);
    assert(strcmp(standard_tonality_triad_quality_symbol(MINOR), "mi") == 0);
    assert(strcmp(standard_tonality_triad_quality_symbol(DIMINISHED), "\xC2\xB0") == 0);
    assert(strcmp(standard_tonality_triad_quality_symbol(AUGMENTED), "+") == 0);
    assert(standard_tonality_triad_quality_symbol((TriadQuality)99) == NULL);
}

/* C, Cmi, Co, C+. */
static void test_c_symbols(void) {
    check_symbol(PITCH_LETTER_C, 0, MAJOR, false, PITCH_LETTER_C, 0, "C");
    check_symbol(PITCH_LETTER_C, 0, MINOR, false, PITCH_LETTER_C, 0, "Cmi");
    check_symbol(PITCH_LETTER_C, 0, DIMINISHED, false, PITCH_LETTER_C, 0, "C\xC2\xB0");
    check_symbol(PITCH_LETTER_C, 0, AUGMENTED, false, PITCH_LETTER_C, 0, "C+");
}

/* The root's accidental is included: B-flat minor, F-sharp diminished. */
static void test_accidentals(void) {
    check_symbol(PITCH_LETTER_B, -1, MINOR, false, PITCH_LETTER_C, 0, "B\xE2\x99\xAD" "mi");
    check_symbol(PITCH_LETTER_F, 1, DIMINISHED, false, PITCH_LETTER_C, 0, "F\xE2\x99\xAF\xC2\xB0");
    check_symbol(PITCH_LETTER_E, -2, MAJOR, false, PITCH_LETTER_C, 0, "E\xF0\x9D\x84\xAB");
    check_symbol(PITCH_LETTER_G, 2, AUGMENTED, false, PITCH_LETTER_C, 0, "G\xF0\x9D\x84\xAA+");
}

/* SLASH_BASS, Example 5: Cmi/E-flat and Cmi/G. */
static void test_slash_bass(void) {
    check_symbol(PITCH_LETTER_C, 0, MINOR, true, PITCH_LETTER_E, -1, "Cmi/E\xE2\x99\xAD");
    check_symbol(PITCH_LETTER_C, 0, MINOR, true, PITCH_LETTER_G, 0, "Cmi/G");
    check_symbol(PITCH_LETTER_D, 0, MAJOR, true, PITCH_LETTER_F, 1, "D/F\xE2\x99\xAF");
    /* A bass note that is the root is the caller's choice to add or leave out. */
    check_symbol(PITCH_LETTER_C, 0, MAJOR, true, PITCH_LETTER_C, 0, "C/C");
}

static void test_symbol_rejects_invalid_arguments(void) {
    char buffer[32];
    assert(!standard_tonality_chord_symbol(PITCH_LETTER_C, 0, (TriadQuality)99, false, PITCH_LETTER_C, 0, buffer,
                                           sizeof(buffer)));
    assert(!standard_tonality_chord_symbol(PITCH_LETTER_C, 3, MAJOR, false, PITCH_LETTER_C, 0, buffer, sizeof(buffer)));
    assert(!standard_tonality_chord_symbol(PITCH_LETTER_C, 0, MAJOR, true, PITCH_LETTER_C, -3, buffer, sizeof(buffer)));
    assert(!standard_tonality_chord_symbol(PITCH_LETTER_C, 0, MAJOR, false, PITCH_LETTER_C, 0, NULL, 32));
    assert(!standard_tonality_chord_symbol(PITCH_LETTER_C, 0, MAJOR, false, PITCH_LETTER_C, 0, buffer, 0));
    /* Too small: "Cmi" needs four bytes with the terminator. */
    assert(!standard_tonality_chord_symbol(PITCH_LETTER_C, 0, MINOR, false, PITCH_LETTER_C, 0, buffer, 3));
    assert(standard_tonality_chord_symbol(PITCH_LETTER_C, 0, MINOR, false, PITCH_LETTER_C, 0, buffer, 4));
    assert(strcmp(buffer, "Cmi") == 0);
}

static void check_notes_symbol(const PitchSpelling *const *notes, size_t count, const char *expected) {
    char buffer[32];
    assert(standard_tonality_triad_chord_symbol(notes, count, buffer, sizeof(buffer)));
    assert(strcmp(buffer, expected) == 0);
}

/* Example 5: C minor with E-flat then G in the bass. */
static void test_symbol_from_notes(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *e_flat4 = spell(PITCH_LETTER_E, -1, 4);
    PitchSpelling *g4 = spell(PITCH_LETTER_G, 0, 4);
    PitchSpelling *e_flat3 = spell(PITCH_LETTER_E, -1, 3);
    PitchSpelling *g3 = spell(PITCH_LETTER_G, 0, 3);
    PitchSpelling *c5 = spell(PITCH_LETTER_C, 0, 5);

    const PitchSpelling *root_position[] = {c4, e_flat4, g4};
    const PitchSpelling *first_inversion[] = {e_flat3, g3, c4};
    const PitchSpelling *second_inversion[] = {g3, c4, e_flat4};
    const PitchSpelling *doubled_root[] = {c4, e_flat4, g4, c5};
    check_notes_symbol(root_position, 3, "Cmi");
    check_notes_symbol(first_inversion, 3, "Cmi/E\xE2\x99\xAD");
    check_notes_symbol(second_inversion, 3, "Cmi/G");
    check_notes_symbol(doubled_root, 4, "Cmi"); /* the bass is the root */

    char buffer[32];
    assert(!standard_tonality_triad_chord_symbol(root_position, 3, buffer, 3)); /* buffer too small */
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(e_flat4);
    pitch_spelling_destroy(g4);
    pitch_spelling_destroy(e_flat3);
    pitch_spelling_destroy(g3);
    pitch_spelling_destroy(c5);
}

static void test_symbol_from_non_triad(void) {
    PitchSpelling *c4 = spell(PITCH_LETTER_C, 0, 4);
    PitchSpelling *d4 = spell(PITCH_LETTER_D, 0, 4);
    PitchSpelling *e4 = spell(PITCH_LETTER_E, 0, 4);
    const PitchSpelling *not_thirds[] = {c4, d4, e4};
    char buffer[32] = "unchanged";
    assert(!standard_tonality_triad_chord_symbol(not_thirds, 3, buffer, sizeof(buffer)));
    assert(strcmp(buffer, "unchanged") == 0);
    pitch_spelling_destroy(c4);
    pitch_spelling_destroy(d4);
    pitch_spelling_destroy(e4);
}

static void test_parse_examples(void) {
    PitchLetter letter;
    int accidental;
    TriadQuality quality;
    bool has_bass;
    PitchLetter bass_letter;
    int bass_accidental;

    assert(standard_tonality_parse_chord_symbol("C", &letter, &accidental, &quality, &has_bass, &bass_letter,
                                                &bass_accidental));
    assert(letter == PITCH_LETTER_C && accidental == 0 && quality == MAJOR && !has_bass);

    assert(standard_tonality_parse_chord_symbol("B\xE2\x99\xAD" "mi", &letter, &accidental, &quality, &has_bass,
                                                &bass_letter, &bass_accidental));
    assert(letter == PITCH_LETTER_B && accidental == -1 && quality == MINOR && !has_bass);

    assert(standard_tonality_parse_chord_symbol("F\xE2\x99\xAF\xC2\xB0", &letter, &accidental, &quality, &has_bass,
                                                &bass_letter, &bass_accidental));
    assert(letter == PITCH_LETTER_F && accidental == 1 && quality == DIMINISHED);

    assert(standard_tonality_parse_chord_symbol("Cmi/E\xE2\x99\xAD", &letter, &accidental, &quality, &has_bass,
                                                &bass_letter, &bass_accidental));
    assert(letter == PITCH_LETTER_C && quality == MINOR && has_bass && bass_letter == PITCH_LETTER_E &&
           bass_accidental == -1);

    assert(standard_tonality_parse_chord_symbol("C+", &letter, &accidental, &quality, &has_bass, &bass_letter,
                                                &bass_accidental));
    assert(quality == AUGMENTED);
}

/* Plain-letter forms: b, #, and o. */
static void test_parse_plain_letters(void) {
    PitchLetter letter;
    int accidental;
    TriadQuality quality;
    bool has_bass;
    PitchLetter bass_letter;
    int bass_accidental;

    assert(standard_tonality_parse_chord_symbol("Bbmi", &letter, &accidental, &quality, &has_bass, &bass_letter,
                                                &bass_accidental));
    assert(letter == PITCH_LETTER_B && accidental == -1 && quality == MINOR);

    assert(standard_tonality_parse_chord_symbol("F#o", &letter, &accidental, &quality, &has_bass, &bass_letter,
                                                &bass_accidental));
    assert(letter == PITCH_LETTER_F && accidental == 1 && quality == DIMINISHED);

    assert(standard_tonality_parse_chord_symbol("Cmi/Eb", &letter, &accidental, &quality, &has_bass, &bass_letter,
                                                &bass_accidental));
    assert(has_bass && bass_letter == PITCH_LETTER_E && bass_accidental == -1);

    /* B alone is B major, not B-flat with nothing after it. */
    assert(standard_tonality_parse_chord_symbol("B", &letter, &accidental, &quality, &has_bass, &bass_letter,
                                                &bass_accidental));
    assert(letter == PITCH_LETTER_B && accidental == 0 && quality == MAJOR);
}

/* Property: parsing what was written gives back the root, quality, and
   bass, for every letter, accidental, and quality. */
static void test_round_trip(void) {
    PitchLetter letters[] = {PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_E, PITCH_LETTER_F,
                             PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_B};
    int round_trips = 0;
    for (size_t l = 0; l < 7; l++) {
        for (int accidental = -2; accidental <= 2; accidental++) {
            for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
                for (int with_bass = 0; with_bass < 2; with_bass++) {
                    PitchLetter bass_letter = letters[(l + 2) % 7];
                    int bass_accidental = (accidental + 1) % 3 - 1;
                    char buffer[32];
                    assert(standard_tonality_chord_symbol(letters[l], accidental, (TriadQuality)q, with_bass != 0,
                                                          bass_letter, bass_accidental, buffer, sizeof(buffer)));

                    PitchLetter parsed_letter;
                    int parsed_accidental;
                    TriadQuality parsed_quality;
                    bool parsed_bass;
                    PitchLetter parsed_bass_letter = PITCH_LETTER_A;
                    int parsed_bass_accidental = 0;
                    assert(standard_tonality_parse_chord_symbol(buffer, &parsed_letter, &parsed_accidental,
                                                                &parsed_quality, &parsed_bass, &parsed_bass_letter,
                                                                &parsed_bass_accidental));
                    assert(parsed_letter == letters[l] && parsed_accidental == accidental);
                    assert(parsed_quality == (TriadQuality)q);
                    assert(parsed_bass == (with_bass != 0));
                    if (with_bass) {
                        assert(parsed_bass_letter == bass_letter && parsed_bass_accidental == bass_accidental);
                    }
                    round_trips++;
                }
            }
        }
    }
    assert(round_trips == 7 * 5 * 4 * 2);
}

static void test_parse_rejects_malformed_symbols(void) {
    PitchLetter letter = PITCH_LETTER_D;
    int accidental = 42;
    TriadQuality quality = (TriadQuality)42;
    bool has_bass = true;
    PitchLetter bass_letter = PITCH_LETTER_D;
    int bass_accidental = 42;

    const char *bad[] = {"", "H", "c", "Cx", "Cm", "Cmi/", "C/", "Cmi/E\xE2\x99\xAD" "x", "Cmimi", "C+o", " C", "C "};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        assert(!standard_tonality_parse_chord_symbol(bad[i], &letter, &accidental, &quality, &has_bass, &bass_letter,
                                                     &bass_accidental));
    }
    assert(!standard_tonality_parse_chord_symbol(NULL, &letter, &accidental, &quality, &has_bass, &bass_letter,
                                                 &bass_accidental));
    assert(!standard_tonality_parse_chord_symbol("C", NULL, &accidental, &quality, &has_bass, &bass_letter,
                                                 &bass_accidental));
    assert(letter == PITCH_LETTER_D && accidental == 42 && quality == (TriadQuality)42 && has_bass &&
           bass_letter == PITCH_LETTER_D && bass_accidental == 42);
}

static void test_build_chord(void) {
    EntityID *target;
    assert(entity_id_create("sonority-test", &target) == ENTITY_ID_OK);

    Chord *chord = NULL;
    assert(standard_tonality_build_triad_chord(PITCH_LETTER_C, 0, MINOR, true, PITCH_LETTER_E, -1, target, &chord) ==
           CHORD_OK);
    assert(strcmp(chord_label(chord), "Cmi/E\xE2\x99\xAD") == 0);

    const Interpretation *interpretation = chord_interpretation(chord);
    assert(interpretation_framework_reference(interpretation) != NULL);
    assert(entity_id_equals(interpretation_target_id(interpretation), target));
    assert(strcmp(interpretation_author(interpretation), "standard-tonality-v1") == 0);
    chord_destroy(chord);
}

static void test_build_chord_labels_every_quality(void) {
    for (int q = 0; q < STANDARD_TONALITY_TRIAD_QUALITY_COUNT; q++) {
        EntityID *target;
        assert(entity_id_create("sonority-test-quality", &target) == ENTITY_ID_OK);
        Chord *chord = NULL;
        assert(standard_tonality_build_triad_chord(PITCH_LETTER_B, -1, (TriadQuality)q, false, PITCH_LETTER_C, 0,
                                                   target, &chord) == CHORD_OK);

        char expected[32];
        assert(standard_tonality_chord_symbol(PITCH_LETTER_B, -1, (TriadQuality)q, false, PITCH_LETTER_C, 0, expected,
                                              sizeof(expected)));
        assert(strcmp(chord_label(chord), expected) == 0);
        chord_destroy(chord);
    }
}

static void test_build_chord_rejects_invalid_symbols(void) {
    EntityID *target;
    assert(entity_id_create("sonority-test-reject", &target) == ENTITY_ID_OK);
    Chord *chord = NULL;
    assert(standard_tonality_build_triad_chord(PITCH_LETTER_C, 0, (TriadQuality)99, false, PITCH_LETTER_C, 0, target,
                                               &chord) == CHORD_ERR_NULL_LABEL);
    assert(standard_tonality_build_triad_chord(PITCH_LETTER_C, 5, MAJOR, false, PITCH_LETTER_C, 0, target, &chord) ==
           CHORD_ERR_NULL_LABEL);
    assert(chord == NULL);
    /* The caller still owns the target on failure. */
    entity_id_destroy(target);
}

int main(void) {
    test_rules_reference_framework();
    test_quality_symbols();
    test_c_symbols();
    test_accidentals();
    test_slash_bass();
    test_symbol_rejects_invalid_arguments();
    test_symbol_from_notes();
    test_symbol_from_non_triad();
    test_parse_examples();
    test_parse_plain_letters();
    test_round_trip();
    test_parse_rejects_malformed_symbols();
    test_build_chord();
    test_build_chord_labels_every_quality();
    test_build_chord_rejects_invalid_symbols();
    printf("test_chord_symbol_rules (standard-tonality): ok\n");
    return 0;
}
