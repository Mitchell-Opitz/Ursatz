#include "analysis/harmony/roman_numeral_adapter.h"

#include "analysis/harmony/applied_chords.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/tonality/roman_numeral_analysis_rules.h"
#include "theory/tonality/roman_numeral_grammar.h"
#include "theory/tonality/seventh_rules.h"
#include "theory/tonality/tonal_key.h"
#include "theory/tonality/common_practice_materials.h"
#include "theory/framework/framework.h"
#include "theory/tonality/triad_rules.h"
#include "model/pitch/midi_pitch.h"

#define NO_RESOLUTION ((size_t)-1)


typedef struct {
    int intervals[3];
    StandardTonalityTriadQuality quality;
} TriadShape;

typedef struct {
    int intervals[4];
    StandardTonalitySeventhQuality quality;
} SeventhShape;

static const TriadShape TRIADS[] = {{{0, 4, 7}, STANDARD_TONALITY_TRIAD_MAJOR},
                                    {{0, 3, 7}, STANDARD_TONALITY_TRIAD_MINOR},
                                    {{0, 3, 6}, STANDARD_TONALITY_TRIAD_DIMINISHED},
                                    {{0, 4, 8}, STANDARD_TONALITY_TRIAD_AUGMENTED}};
static const SeventhShape SEVENTHS[] = {{{0, 4, 7, 11}, STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR},
                                        {{0, 4, 7, 10}, STANDARD_TONALITY_SEVENTH_MAJOR_MINOR},
                                        {{0, 3, 7, 10}, STANDARD_TONALITY_SEVENTH_MINOR_MINOR},
                                        {{0, 3, 6, 10}, STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED},
                                        {{0, 3, 6, 9}, STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED}};

/* The degree 1..7 of the root `offset` semitones above the tonic, or 0; *raised set for minor's raised 7th,
   *raised_sixth for its raised 6th (melodic minor). */
static int degree_of_offset(const StandardTonalityChordInput *input, int offset, bool *raised, bool *raised_sixth) {
    StandardTonalityRomanKeyMode mode = input->mode;
    *raised = false;
    *raised_sixth = false;
    int degree = 0;
    int alteration = 0;
    const CommonPracticeMaterials *materials = input->materials;
    if (materials == NULL || !standard_tonality_material_degree_of_semitone(
            offset, mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR ? materials->major : materials->natural_minor, &degree,
            &alteration)) {
        return 0;
    }
    if (alteration == 0) {
        return degree;
    }
    /* In minor a seventh or a sixth a semitone above its natural place is the raised leading tone or sixth. */
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MINOR && alteration == 1) {
        if (degree == 7) {
            *raised = true;
            return 7;
        }
        if (degree == 6) {
            *raised_sixth = true;
            return 6;
        }
    }
    return 0;
}

/*
 * The chords only melodic minor gives, which the natural and harmonic tables (standard_tonality_roman_numeral_fits_key)
 * do not hold: on the 2nd degree ii and ii7 (minor, minor-minor), on the 4th IV and IV7 (major, major-minor), on the
 * raised 6th vi-diminished and the half-diminished vi-7, and on the raised 7th the half-diminished vii-7. The raised
 * 6th is only ever read here: its natural-form tables are the lowered 6th's.
 */
static bool melodic_minor_fits(int degree, bool raised, bool raised_sixth, const StandardTonalityRomanNumeral *numeral) {
    if (raised_sixth) {
        return degree == 6 && (numeral->seventh ? numeral->seventh_quality == STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
                                                : numeral->triad == STANDARD_TONALITY_TRIAD_DIMINISHED);
    }
    if (raised) {
        return degree == 7 && numeral->seventh && numeral->seventh_quality == STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED;
    }
    if (degree == 2) {
        return numeral->seventh ? numeral->seventh_quality == STANDARD_TONALITY_SEVENTH_MINOR_MINOR
                                : numeral->triad == STANDARD_TONALITY_TRIAD_MINOR;
    }
    if (degree == 4) {
        return numeral->seventh ? numeral->seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
                                : numeral->triad == STANDARD_TONALITY_TRIAD_MAJOR;
    }
    return false;
}

static bool numeral_from_intervals(const int *intervals, size_t count, int degree, StandardTonalityRomanNumeral *out) {
    if (count == 3) {
        for (size_t i = 0; i < sizeof(TRIADS) / sizeof(TRIADS[0]); i++) {
            if (memcmp(intervals, TRIADS[i].intervals, sizeof(TRIADS[i].intervals)) == 0) {
                return standard_tonality_roman_numeral_triad(degree, TRIADS[i].quality, out);
            }
        }
    } else if (count == 4) {
        for (size_t i = 0; i < sizeof(SEVENTHS) / sizeof(SEVENTHS[0]); i++) {
            if (memcmp(intervals, SEVENTHS[i].intervals, sizeof(SEVENTHS[i].intervals)) == 0) {
                return standard_tonality_roman_numeral_seventh_chord(degree, SEVENTHS[i].quality, out);
            }
        }
    }
    return false;
}

/* The chord member in the bass: 0 root, 1 third, 2 fifth, 3 seventh. Root position for an unknown bass or a
   bass that is not a chord member. */
static int inversion_of(const StandardTonalityChordInput *input) {
    if (input->bass_pitch_class < 0) {
        return 0;
    }
    int above_root = pitch_class_of_midi(input->bass_pitch_class - input->root_pitch_class);
    for (size_t member = 0; member < input->interval_count && member < 4; member++) {
        if (input->intervals[member] == above_root) {
            return (int)member;
        }
    }
    return 0;
}

/* The diatonic reading: the degree and quality the key gives. */
static bool read_diatonic(const StandardTonalityChordInput *input, StandardTonalityRomanReading *out) {
    int offset = pitch_class_of_midi(input->root_pitch_class - input->tonic_pitch_class);
    bool raised = false;
    bool raised_sixth = false;
    int degree = degree_of_offset(input, offset, &raised, &raised_sixth);
    if (degree == 0) {
        return false;
    }
    StandardTonalityRomanNumeral numeral;
    if (!numeral_from_intervals(input->intervals, input->interval_count, degree, &numeral)) {
        return false;
    }
    bool is_minor = input->mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
    /* "IV7" is a major seventh in a major key; in minor no natural or harmonic IV is major, so IV7 is melodic
       minor's dominant seventh on the 4th, the one uppercase seventh off the dominant that is not ambiguous. */
    bool melodic_dominant_seventh = is_minor && degree == 4 && numeral.seventh &&
                                    numeral.seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MINOR;
    if (!standard_tonality_roman_numeral_is_unambiguous(&numeral) && !melodic_dominant_seventh) {
        return false;
    }
    bool fits = !raised_sixth && standard_tonality_roman_numeral_fits_key(input->mode, degree, raised, &numeral);
    if (!fits && !(is_minor && melodic_minor_fits(degree, raised, raised_sixth, &numeral))) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->numeral = numeral;
    out->raised_leading_tone = raised;
    out->raised_sixth = raised_sixth;
    out->romantext_spelling = is_minor;
    out->inversion = inversion_of(input);
    return true;
}

bool standard_tonality_read_roman_numeral(const StandardTonalityChordInput *input,
                                          StandardTonalityRomanReading *out) {
    if (input == NULL || out == NULL || !input->has_numeral) {
        return false;
    }
    /* A chord the key does not give may be an applied chord that identification already read (R-380). */
    return read_diatonic(input, out) || standard_tonality_read_applied_chord(input, out);
}

bool standard_tonality_format_roman_reading(const StandardTonalityRomanReading *reading, char *buffer, size_t size) {
    if (reading == NULL || buffer == NULL || size == 0) {
        return false;
    }
    StandardTonalityNumeralText text;
    if (!standard_tonality_numeral_from_base(&reading->numeral, &text)) {
        return false;
    }
    text.inversion = reading->inversion;
    if (reading->applied) {
        StandardTonalityNumeralText target;
        if (!standard_tonality_numeral_from_base(&reading->applied_target, &target)) {
            return false;
        }
        text.has_target = true;
        text.target_degree = target.degree;
        text.target_minor = target.quality != STANDARD_TONALITY_NUMERAL_MAJOR &&
                            target.quality != STANDARD_TONALITY_NUMERAL_AUGMENTED &&
                            target.quality != STANDARD_TONALITY_NUMERAL_DOMINANT_SEVENTH;
    }
    /* RomanText spelling (R-031): "viio7", "ii/o7" rather than the degree sign and the empty set sign. */
    return standard_tonality_numeral_format_styled(&text,
                                                   reading->romantext_spelling
                                                       ? STANDARD_TONALITY_NUMERAL_STYLE_ROMANTEXT
                                                       : STANDARD_TONALITY_NUMERAL_STYLE_DISPLAY,
                                                   buffer, size);
}

static double clamp01(double value) {
    return value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
}

static bool fill_claim(const StandardTonalityRomanReading *reading, StandardTonalityRomanClaim *claim) {
    memset(claim, 0, sizeof(*claim));
    if (!standard_tonality_format_roman_reading(reading, claim->label, sizeof(claim->label))) {
        return false;
    }
    claim->degree = reading->numeral.degree;
    claim->seventh = reading->numeral.seventh;
    claim->quality = reading->numeral.seventh ? (int)reading->numeral.seventh_quality : (int)reading->numeral.triad;
    claim->inversion = reading->inversion;
    if (reading->applied) {
        claim->applied = true;
        claim->applied_kind = (int)reading->applied_kind;
        claim->applied_target_degree = reading->applied_target.degree;
    }
    return true;
}

StandardTonalityRomanStatus standard_tonality_build_roman_numerals_with_config(
    const StandardTonalityChordInput *inputs, size_t count, const StandardTonalityAppliedConfig *config,
    StandardTonalityRomanResult **out) {
    if (out == NULL || (inputs == NULL && count > 0) ||
        (config != NULL && !standard_tonality_applied_config_valid(config))) {
        return STANDARD_TONALITY_ROMAN_ERR_NULL_ARGUMENT;
    }
    StandardTonalityRomanResult *results =
        (StandardTonalityRomanResult *)calloc(count > 0 ? count : 1, sizeof(*results));
    if (results == NULL) {
        return STANDARD_TONALITY_ROMAN_ERR_ALLOC;
    }

    /* Pass 1: the diatonic reading of every segment, then the applied re-check of each (it looks at the
       following segments' chords, which are the diatonic ones). */
    for (size_t i = 0; i < count; i++) {
        results[i].segment_id = inputs[i].segment_id;
        results[i].has_numeral = standard_tonality_read_roman_numeral(&inputs[i], &results[i].reading);
    }
    StandardTonalityAppliedResult applied[1];
    for (size_t i = 0; i < count; i++) {
        if (!results[i].has_numeral) {
            continue;
        }
        if (results[i].reading.applied) {
            /* Identification read it as applied: it is linked only when it resolves. */
            size_t resolution = 0;
            bool deceptive = false;
            if (standard_tonality_applied_resolution(inputs, count, i, &results[i].reading, &resolution, &deceptive)) {
                results[i].resolution_index = resolution;
                results[i].deceptive = deceptive;
            } else {
                results[i].resolution_index = NO_RESOLUTION;
            }
        } else if (standard_tonality_recheck_applied_chord(inputs, count, i, config, &applied[0])) {
            results[i].reading = applied[0].reading;
            results[i].resolution_index = applied[0].resolution_index;
            results[i].deceptive = applied[0].deceptive;
        }
    }

    /* Pass 2: the numerals and the resolutions. */
    for (size_t i = 0; i < count; i++) {
        StandardTonalityRomanResult *result = &results[i];
        if (!result->has_numeral) {
            continue;
        }
        if (!fill_claim(&result->reading, &result->claim)) {
            result->has_numeral = false;
            continue;
        }
        result->uncertainty =
            clamp01(1.0 - (1.0 - clamp01(inputs[i].chord_uncertainty)) * (1.0 - clamp01(inputs[i].key_uncertainty)));
        result->resolved = result->reading.applied && result->resolution_index != NO_RESOLUTION;
    }
    *out = results;
    return STANDARD_TONALITY_ROMAN_OK;
}

void standard_tonality_roman_results_destroy(StandardTonalityRomanResult *results, size_t count) {
    (void)count;
    free(results);
}

