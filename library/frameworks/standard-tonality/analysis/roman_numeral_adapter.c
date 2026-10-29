#include "roman_numeral_adapter.h"

#include "applied_chords.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/framework_reference.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "framework.h"
#include "identity/entity_id.h"
#include "interpretation/uncertainty.h"
#include "roman_numeral_analysis_rules.h"
#include "roman_numeral_grammar.h"
#include "seventh_rules.h"
#include "theory/framework.h"
#include "triad_rules.h"

#define CONFIDENCE_DENOMINATOR 1000000UL

static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
static const int NATURAL_MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 10};
static const int RAISED_LEADING_TONE_OFFSET = 11;
static const int RAISED_SIXTH_OFFSET = 9;

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
static int degree_of_offset(StandardTonalityRomanKeyMode mode, int offset, bool *raised, bool *raised_sixth) {
    const int *scale = mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR ? MAJOR_SCALE : NATURAL_MINOR_SCALE;
    *raised = false;
    *raised_sixth = false;
    for (int i = 0; i < 7; i++) {
        if (scale[i] == offset) {
            return i + 1;
        }
    }
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MINOR && offset == RAISED_LEADING_TONE_OFFSET) {
        *raised = true;
        return 7;
    }
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MINOR && offset == RAISED_SIXTH_OFFSET) {
        *raised_sixth = true;
        return 6;
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
    int above_root = ((input->bass_pitch_class - input->root_pitch_class) % 12 + 12) % 12;
    for (size_t member = 0; member < input->interval_count && member < 4; member++) {
        if (input->intervals[member] == above_root) {
            return (int)member;
        }
    }
    return 0;
}

bool standard_tonality_read_roman_numeral(const StandardTonalityChordInput *input,
                                          StandardTonalityRomanReading *out) {
    if (input == NULL || out == NULL || !input->has_numeral) {
        return false;
    }
    int offset = ((input->root_pitch_class - input->tonic_pitch_class) % 12 + 12) % 12;
    bool raised = false;
    bool raised_sixth = false;
    int degree = degree_of_offset(input->mode, offset, &raised, &raised_sixth);
    if (degree == 0) {
        return false;
    }
    StandardTonalityRomanNumeral numeral;
    if (!numeral_from_intervals(input->intervals, input->interval_count, degree, &numeral)) {
        return false;
    }
    bool minor = input->mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
    /* "IV7" is a major seventh in a major key; in minor no natural or harmonic IV is major, so IV7 is melodic
       minor's dominant seventh on the 4th, the one uppercase seventh off the dominant that is not ambiguous. */
    bool melodic_dominant_seventh = minor && degree == 4 && numeral.seventh &&
                                    numeral.seventh_quality == STANDARD_TONALITY_SEVENTH_MAJOR_MINOR;
    if (!standard_tonality_roman_numeral_is_unambiguous(&numeral) && !melodic_dominant_seventh) {
        return false;
    }
    bool fits = !raised_sixth && standard_tonality_roman_numeral_fits_key(input->mode, degree, raised, &numeral);
    if (!fits && !(minor && melodic_minor_fits(degree, raised, raised_sixth, &numeral))) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->numeral = numeral;
    out->raised_leading_tone = raised;
    out->raised_sixth = raised_sixth;
    out->romantext_spelling = minor;
    out->inversion = inversion_of(input);
    return true;
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

static unsigned long millionths(double confidence) {
    confidence = clamp01(confidence);
    return (unsigned long)(confidence * (double)CONFIDENCE_DENOMINATOR + 0.5);
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

/* The Interpretation over the segment's Sonority. Owns nothing the caller passed. */
static Interpretation *build_interpretation(const char *segment_id, const StandardTonalityRomanClaim *claim_fields,
                                            double uncertainty_value) {
    const Framework *framework = standard_tonality_framework();
    if (framework == NULL) {
        return NULL;
    }
    char interpretation_id_value[256];
    snprintf(interpretation_id_value, sizeof(interpretation_id_value), "standard-tonality-roman-numeral-%s", segment_id);

    EntityID *interpretation_id = NULL;
    EntityID *target_id = NULL;
    EntityID *framework_target_id = NULL;
    FrameworkReference *framework_reference = NULL;
    Value *claim = NULL;
    Uncertainty *uncertainty = NULL;
    ProvenanceReference *provenance = NULL;
    Interpretation *interpretation = NULL;

    if (entity_id_create(interpretation_id_value, &interpretation_id) != ENTITY_ID_OK ||
        entity_id_create(segment_id, &target_id) != ENTITY_ID_OK ||
        entity_id_create(entity_id_value(framework_id(framework)), &framework_target_id) != ENTITY_ID_OK) {
        goto fail;
    }
    if (framework_reference_create(framework_target_id, &framework_reference) != FRAMEWORK_REFERENCE_OK) {
        goto fail;
    }
    framework_target_id = NULL; /* owned by the reference now */
    if (value_create(STANDARD_TONALITY_ROMAN_CLAIM_TYPE, claim_fields, sizeof(*claim_fields), &claim) != VALUE_OK ||
        uncertainty_create_scalar_confidence(millionths(1.0 - uncertainty_value), CONFIDENCE_DENOMINATOR,
                                             &uncertainty) != UNCERTAINTY_OK ||
        provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        goto fail;
    }
    if (interpretation_create(interpretation_id, target_id, claim, NULL, NULL, framework_reference, uncertainty,
                              "standard-tonality-v1", "roman-numeral-adapter",
                              INTERPRETATION_STATUS_MACHINE_GENERATED, provenance, &interpretation) !=
        INTERPRETATION_OK) {
        goto fail;
    }
    return interpretation;

fail:
    entity_id_destroy(interpretation_id);
    entity_id_destroy(target_id);
    entity_id_destroy(framework_target_id);
    framework_reference_destroy(framework_reference);
    value_destroy(claim);
    uncertainty_destroy(uncertainty);
    provenance_reference_destroy(provenance);
    return NULL;
}

/* The "resolves-to" Relationship from one segment's Sonority to another's. */
static Relationship *build_resolves_to(const char *source_segment, const char *target_segment) {
    char relationship_id_value[512];
    snprintf(relationship_id_value, sizeof(relationship_id_value), "standard-tonality-resolves-to-%s-%s",
             source_segment, target_segment);
    EntityID *relationship_id = NULL;
    EntityID *source = NULL;
    EntityID *target = NULL;
    ProvenanceReference *provenance = NULL;
    Relationship *relationship = NULL;
    if (entity_id_create(relationship_id_value, &relationship_id) != ENTITY_ID_OK ||
        entity_id_create(source_segment, &source) != ENTITY_ID_OK ||
        entity_id_create(target_segment, &target) != ENTITY_ID_OK ||
        provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK ||
        relationship_create(relationship_id, source, STANDARD_TONALITY_RESOLVES_TO, target, NULL, NULL, NULL, NULL,
                            NULL, provenance, &relationship) != RELATIONSHIP_OK) {
        entity_id_destroy(relationship_id);
        entity_id_destroy(source);
        entity_id_destroy(target);
        provenance_reference_destroy(provenance);
        return NULL;
    }
    return relationship;
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
        if (results[i].has_numeral &&
            standard_tonality_recheck_applied_chord(inputs, count, i, config, &applied[0])) {
            results[i].reading = applied[0].reading;
            results[i].resolution_index = applied[0].resolution_index;
            results[i].deceptive = applied[0].deceptive;
        }
    }

    /* Pass 2: the claims and the links. */
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
        result->interpretation = build_interpretation(inputs[i].segment_id, &result->claim, result->uncertainty);
        if (result->interpretation == NULL) {
            standard_tonality_roman_results_destroy(results, count);
            return STANDARD_TONALITY_ROMAN_ERR_ALLOC;
        }
        if (result->reading.applied) {
            result->resolves_to = build_resolves_to(inputs[i].segment_id, inputs[result->resolution_index].segment_id);
            if (result->resolves_to == NULL) {
                standard_tonality_roman_results_destroy(results, count);
                return STANDARD_TONALITY_ROMAN_ERR_ALLOC;
            }
        }
    }
    *out = results;
    return STANDARD_TONALITY_ROMAN_OK;
}

StandardTonalityRomanStatus standard_tonality_build_roman_numerals(const StandardTonalityChordInput *inputs,
                                                                   size_t count, StandardTonalityRomanResult **out) {
    return standard_tonality_build_roman_numerals_with_config(inputs, count, NULL, out);
}

void standard_tonality_roman_results_destroy(StandardTonalityRomanResult *results, size_t count) {
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        interpretation_destroy(results[i].interpretation);
        relationship_destroy(results[i].resolves_to);
    }
    free(results);
}

bool standard_tonality_roman_claim_read(const Interpretation *interpretation, StandardTonalityRomanClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), STANDARD_TONALITY_ROMAN_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
