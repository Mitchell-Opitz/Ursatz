#include "roman_numeral_adapter.h"

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
#include "seventh_rules.h"
#include "theory/framework.h"
#include "triad_rules.h"

#define CONFIDENCE_DENOMINATOR 1000000UL

static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
static const int NATURAL_MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 10};
static const int RAISED_LEADING_TONE_OFFSET = 11;

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

/* The degree 1..7 of the root `offset` semitones above the tonic, or 0; *raised set for minor's raised 7th. */
static int degree_of_offset(StandardTonalityRomanKeyMode mode, int offset, bool *raised) {
    const int *scale = mode == STANDARD_TONALITY_ROMAN_KEY_MAJOR ? MAJOR_SCALE : NATURAL_MINOR_SCALE;
    *raised = false;
    for (int i = 0; i < 7; i++) {
        if (scale[i] == offset) {
            return i + 1;
        }
    }
    if (mode == STANDARD_TONALITY_ROMAN_KEY_MINOR && offset == RAISED_LEADING_TONE_OFFSET) {
        *raised = true;
        return 7;
    }
    return 0;
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
    int degree = degree_of_offset(input->mode, offset, &raised);
    if (degree == 0) {
        return false;
    }
    StandardTonalityRomanNumeral numeral;
    if (!numeral_from_intervals(input->intervals, input->interval_count, degree, &numeral) ||
        !standard_tonality_roman_numeral_is_unambiguous(&numeral) ||
        !standard_tonality_roman_numeral_fits_key(input->mode, degree, raised, &numeral)) {
        return false;
    }
    out->numeral = numeral;
    out->raised_leading_tone = raised;
    out->inversion = inversion_of(input);
    return true;
}

static const char *figure_of(bool seventh, int inversion) {
    static const char *const TRIAD_FIGURES[3] = {"", "6", "6/4"};
    static const char *const SEVENTH_FIGURES[4] = {"", "6/5", "4/3", "4/2"};
    if (seventh) {
        return inversion >= 0 && inversion < 4 ? SEVENTH_FIGURES[inversion] : NULL;
    }
    return inversion >= 0 && inversion < 3 ? TRIAD_FIGURES[inversion] : NULL;
}

bool standard_tonality_format_roman_reading(const StandardTonalityRomanReading *reading, char *buffer, size_t size) {
    if (reading == NULL || buffer == NULL || size == 0) {
        return false;
    }
    const char *figure = figure_of(reading->numeral.seventh, reading->inversion);
    if (figure == NULL || !standard_tonality_roman_numeral_format(&reading->numeral, buffer, size)) {
        return false;
    }
    size_t length = strlen(buffer);
    if (reading->numeral.seventh && reading->inversion > 0 && length > 0) {
        buffer[--length] = '\0'; /* every seventh numeral ends in the 7, which the figure replaces */
    }
    if (length + strlen(figure) + 1 > size) {
        return false;
    }
    strcpy(buffer + length, figure);
    return true;
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

StandardTonalityRomanStatus standard_tonality_build_roman_numerals(const StandardTonalityChordInput *inputs,
                                                                   size_t count, StandardTonalityRomanResult **out) {
    if (out == NULL || (inputs == NULL && count > 0)) {
        return STANDARD_TONALITY_ROMAN_ERR_NULL_ARGUMENT;
    }
    StandardTonalityRomanResult *results =
        (StandardTonalityRomanResult *)calloc(count > 0 ? count : 1, sizeof(*results));
    if (results == NULL) {
        return STANDARD_TONALITY_ROMAN_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        StandardTonalityRomanResult *result = &results[i];
        result->segment_id = inputs[i].segment_id;
        if (!standard_tonality_read_roman_numeral(&inputs[i], &result->reading) ||
            !fill_claim(&result->reading, &result->claim)) {
            continue;
        }
        result->has_numeral = true;
        result->uncertainty =
            clamp01(1.0 - (1.0 - clamp01(inputs[i].chord_uncertainty)) * (1.0 - clamp01(inputs[i].key_uncertainty)));
        result->interpretation = build_interpretation(inputs[i].segment_id, &result->claim, result->uncertainty);
        if (result->interpretation == NULL) {
            standard_tonality_roman_results_destroy(results, count);
            return STANDARD_TONALITY_ROMAN_ERR_ALLOC;
        }
    }
    *out = results;
    return STANDARD_TONALITY_ROMAN_OK;
}

void standard_tonality_roman_results_destroy(StandardTonalityRomanResult *results, size_t count) {
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        interpretation_destroy(results[i].interpretation);
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
