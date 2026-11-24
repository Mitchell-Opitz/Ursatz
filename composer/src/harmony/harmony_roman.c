#include "harmony/harmony_internal.h"
#include "util/util.h"

#include <string.h>

#include <theory/harmony/harmony_functions.h>
#include <theory/tonality/roman_numeral_grammar.h>
#include <theory/tonality/triad_in_key_rules.h>

HarmonyMode harmony_mode_of_material(const PitchMaterial *material) {
    return composer_material_is_minor(material) ? HARMONY_MODE_MINOR : HARMONY_MODE_MAJOR;
}

/*
 * A chord as the library's numeral value (R-232). Case comes from the quality: the chords of the mode's scale that have
 * a minor third are lower case, a diminished chord and the leading-tone chord of a minor key are diminished (a seventh on
 * the diminished chord of a major key or on the second degree of a minor key is half-diminished, on the raised seventh
 * fully diminished).
 */
static StandardTonalityNumeralText numeral_of(HarmonyMode mode, const HarmonyChord *chord) {
    StandardTonalityNumeralText numeral;
    memset(&numeral, 0, sizeof(numeral));
    numeral.degree = chord->degree;
    numeral.inversion = chord->inversion;
    bool minor_mode = mode == HARMONY_MODE_MINOR;
    bool diminished = minor_mode ? (chord->degree == 2 || chord->leading_tone) : chord->degree == 7;
    /* The quality of the triad on the degree: the natural scale's, with the major dominant of the harmonic minor, as
       harmony_applied_label picks the mode. */
    StandardTonalityTriadKey key = !minor_mode              ? STANDARD_TONALITY_TRIAD_KEY_MAJOR
                                   : chord->degree == 5     ? STANDARD_TONALITY_TRIAD_KEY_MINOR_HARMONIC
                                                            : STANDARD_TONALITY_TRIAD_KEY_MINOR_NATURAL;
    StandardTonalityTriadQuality triad = STANDARD_TONALITY_TRIAD_MAJOR;
    standard_tonality_triad_quality_in_key(key, chord->degree, &triad);
    bool minor_third = triad == STANDARD_TONALITY_TRIAD_MINOR;
    if (diminished) {
        numeral.quality = !chord->seventh ? STANDARD_TONALITY_NUMERAL_DIMINISHED
                          : chord->leading_tone ? STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH
                                                : STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH;
    } else if (chord->seventh) {
        numeral.quality = minor_third ? STANDARD_TONALITY_NUMERAL_MINOR_SEVENTH
                                      : STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH;
    } else {
        numeral.quality = minor_third ? STANDARD_TONALITY_NUMERAL_MINOR : STANDARD_TONALITY_NUMERAL_MAJOR;
    }
    return numeral;
}

HarmonyKind harmony_kind_of(const ComposerChords *chords, HarmonyMode mode, const PitchMaterial *modal) {
    HarmonyKind kind = {mode, modal, modal != NULL ? composer_chords_cadence(chords, modal) : NULL};
    return kind;
}

HarmonyKind harmony_kind(const ComposerChords *chords, const SpelledKey *key) {
    return harmony_kind_of(chords, harmony_key_mode(key), composer_chords_modal(chords, key));
}

/* The numeral of a chord in a modal key: the stacked triad's quality, or false for a chord the key does not hold. */
static bool numeral_of_modal(const PitchMaterial *modal, const HarmonyChord *chord, StandardTonalityNumeralText *out) {
    StandardTonalityTriadQuality triad;
    if (chord->seventh || chord->leading_tone || chord->half_diminished || !pitch_material_triad_on(modal, chord->degree, NULL, NULL, &triad)) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->degree = chord->degree;
    out->inversion = chord->inversion;
    switch (triad) {
    case STANDARD_TONALITY_TRIAD_MAJOR:
        out->quality = STANDARD_TONALITY_NUMERAL_MAJOR;
        return true;
    case STANDARD_TONALITY_TRIAD_MINOR:
        out->quality = STANDARD_TONALITY_NUMERAL_MINOR;
        return true;
    case STANDARD_TONALITY_TRIAD_DIMINISHED:
        out->quality = STANDARD_TONALITY_NUMERAL_DIMINISHED;
        return true;
    default:
        return false;
    }
}

static bool format_numeral(const StandardTonalityNumeralText *numeral, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    char text[PLAN_ROMAN_NUMERAL_MAX];
    if (!standard_tonality_numeral_format(numeral, text, sizeof(text))) {
        return false;
    }
    memset(out, 0, PLAN_ROMAN_NUMERAL_MAX);
    memcpy(out, text, strlen(text));
    return true;
}

bool harmony_roman_format_in(const HarmonyKind *kind, const HarmonyChord *chord, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    if (kind == NULL || kind->modal == NULL) {
        return harmony_roman_format(kind != NULL ? kind->mode : HARMONY_MODE_MAJOR, chord, out);
    }
    StandardTonalityNumeralText numeral;
    return chord && out && harmony_chord_valid(chord) && numeral_of_modal(kind->modal, chord, &numeral) && format_numeral(&numeral, out);
}

bool harmony_roman_parse_in(const HarmonyKind *kind, const char *roman, HarmonyChord *out) {
    if (kind == NULL || kind->modal == NULL) {
        return harmony_roman_parse(kind != NULL ? kind->mode : HARMONY_MODE_MAJOR, roman, out);
    }
    StandardTonalityNumeralText numeral;
    StandardTonalityNumeralText expected;
    if (!roman || !out || !standard_tonality_numeral_parse(roman, &numeral) || numeral.alteration != 0 || numeral.has_target ||
        standard_tonality_numeral_quality_is_seventh(numeral.quality)) {
        return false;
    }
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = numeral.degree;
    chord.inversion = numeral.inversion;
    /* Only the numerals of this mode: the same chord is written the same way. */
    if (!harmony_chord_valid(&chord) || !numeral_of_modal(kind->modal, &chord, &expected) || expected.quality != numeral.quality) {
        return false;
    }
    *out = chord;
    return true;
}

bool harmony_function_in(const HarmonyKind *kind, const HarmonyChord *chord, HarmonicFunction *out) {
    if (kind == NULL || kind->modal == NULL) {
        return harmony_function_of(kind != NULL ? kind->mode : HARMONY_MODE_MAJOR, chord, out);
    }
    ModalFunction function;
    if (chord == NULL || out == NULL || chord->seventh || kind->cadence == NULL || !modal_function_of(kind->modal, kind->cadence, chord->degree, &function)) {
        return false;
    }
    *out = function == MODAL_FUNCTION_TONIC ? HARMONIC_FUNCTION_TONIC
           : function == MODAL_FUNCTION_DOMINANT ? HARMONIC_FUNCTION_DOMINANT
                                                 : HARMONIC_FUNCTION_PREDOMINANT;
    return true;
}

int harmony_dominant_degree(const HarmonyKind *kind) {
    if (kind != NULL && kind->modal != NULL && kind->cadence != NULL && kind->cadence->approach_count > 0) {
        return kind->cadence->approach_degrees[kind->cadence->approach_count - 1];
    }
    return 5;
}

bool harmony_modal_tritone_approach(const HarmonyKind *kind) {
    if (kind == NULL || kind->modal == NULL || kind->cadence == NULL || kind->cadence->approach_count == 0) {
        return false;
    }
    int approach = pitch_material_step(kind->modal, kind->cadence->approach_degrees[kind->cadence->approach_count - 1]);
    int arrival = pitch_material_step(kind->modal, kind->cadence->arrival_degree);
    return approach >= 0 && arrival >= 0 && ((arrival - approach + 12) % 12) == 6;
}

bool harmony_roman_format(HarmonyMode mode, const HarmonyChord *chord, char out[PLAN_ROMAN_NUMERAL_MAX]) {
    if (!chord || !out || !harmony_chord_valid(chord) || chord->half_diminished) {
        return false;
    }
    StandardTonalityNumeralText numeral = numeral_of(mode, chord);
    char text[PLAN_ROMAN_NUMERAL_MAX];
    if (!standard_tonality_numeral_format(&numeral, text, sizeof(text))) {
        return false;
    }
    memset(out, 0, PLAN_ROMAN_NUMERAL_MAX);
    memcpy(out, text, strlen(text));
    return true;
}

static bool quality_is_diminished_seventh(StandardTonalityNumeralQuality quality) {
    return quality == STANDARD_TONALITY_NUMERAL_HALF_DIMINISHED_SEVENTH ||
           quality == STANDARD_TONALITY_NUMERAL_FULLY_DIMINISHED_SEVENTH;
}

bool harmony_roman_parse(HarmonyMode mode, const char *roman, HarmonyChord *out) {
    StandardTonalityNumeralText numeral;
    if (!roman || !out || !standard_tonality_numeral_parse(roman, &numeral) || numeral.alteration != 0 ||
        numeral.has_target) {
        return false;
    }
    HarmonyChord chord;
    memset(&chord, 0, sizeof(chord));
    chord.degree = numeral.degree;
    chord.seventh = standard_tonality_numeral_quality_is_seventh(numeral.quality);
    chord.inversion = numeral.inversion;
    /* The diminished chord on 7 of a minor key is the one on the raised seventh degree. */
    chord.leading_tone = mode == HARMONY_MODE_MINOR && numeral.degree == 7 &&
                         (numeral.quality == STANDARD_TONALITY_NUMERAL_DIMINISHED ||
                          quality_is_diminished_seventh(numeral.quality));
    if (!harmony_chord_valid(&chord)) {
        return false;
    }
    /* Only the numerals of this mode: the same chord must be written the same way (either diminished seventh reads). */
    StandardTonalityNumeralText expected = numeral_of(mode, &chord);
    if (expected.quality != numeral.quality &&
        !(quality_is_diminished_seventh(expected.quality) && quality_is_diminished_seventh(numeral.quality))) {
        return false;
    }
    *out = chord;
    return true;
}

bool harmony_roman_format_applied_inverted(HarmonyMode mode, const HarmonyAppliedChord *applied, int inversion,
                                           char out[PLAN_ROMAN_NUMERAL_MAX]) {
    char text[PLAN_ROMAN_NUMERAL_MAX];
    /* The figures go between the chord and its target: "V7/V" becomes "V6/5/V". */
    if (!applied || !out || !harmony_applied_label_inverted(mode, applied, inversion, text, sizeof(text))) {
        return false;
    }
    memset(out, 0, PLAN_ROMAN_NUMERAL_MAX);
    memcpy(out, text, strlen(text));
    return true;
}

bool harmony_roman_parse_applied_inverted(HarmonyMode mode, const char *roman, HarmonyAppliedChord *out,
                                          int *inversion) {
    if (!roman || !out || !inversion) {
        return false;
    }
    /* The labels are the framework's, so each valid one is generated and compared. */
    for (int quality = 0; quality < HARMONY_APPLIED_QUALITY_COUNT; quality++) {
        for (int target = 1; target <= 7; target++) {
            for (int position = 0; position <= 3; position++) {
                HarmonyAppliedChord applied;
                char label[PLAN_ROMAN_NUMERAL_MAX];
                memset(&applied, 0, sizeof(applied));
                applied.quality = (HarmonyAppliedQuality)quality;
                applied.target = target;
                if (!harmony_applied_quality_kind(applied.quality, &applied.kind) ||
                    !harmony_applied_valid(mode, &applied) ||
                    !harmony_roman_format_applied_inverted(mode, &applied, position, label)) {
                    continue;
                }
                if (strcmp(label, roman) == 0) {
                    *out = applied;
                    *inversion = position;
                    return true;
                }
            }
        }
    }
    return false;
}

bool harmony_roman_format_applied(HarmonyMode mode, const HarmonyAppliedChord *applied,
                                  char out[PLAN_ROMAN_NUMERAL_MAX]) {
    return harmony_roman_format_applied_inverted(mode, applied, 0, out);
}

