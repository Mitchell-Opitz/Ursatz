#include "pipeline/roman_numeral_pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "standard-tonality/analysis/applied_chords.h"
#include "standard-tonality/analysis/chord_input.h"
#include "standard-tonality/analysis/roman_numeral_adapter.h"

#define REASON_NO_REGION "no key region"
#define REASON_NO_CHORD "the library identified no chord"
#define REASON_NO_NUMERAL "the library gives no Roman numeral for this chord in its key"

/* The numeral without its inversion figure, formatted by the library from the same reading. */
static char *base_numeral_of(const StandardTonalityRomanResult *result) {
    StandardTonalityRomanReading root_position = result->reading;
    root_position.inversion = 0;
    char text[STANDARD_TONALITY_ROMAN_LABEL_MAX];
    if (!standard_tonality_format_roman_reading(&root_position, text, sizeof(text))) {
        return NULL;
    }
    return strdup(text);
}

/* The applied chord's parts, from the library's reading: the chord as it reads in the tonicized key and the
   chord tonicized, each formatted by the library. */
static bool fill_applied(const StandardTonalityRomanResult *result, AnalysisRomanNumeral *roman) {
    StandardTonalityRomanReading chord_alone = result->reading;
    chord_alone.applied = false;
    char applied_text[STANDARD_TONALITY_ROMAN_LABEL_MAX];
    char target_text[STANDARD_TONALITY_ROMAN_LABEL_MAX];
    if (!standard_tonality_format_roman_reading(&chord_alone, applied_text, sizeof(applied_text)) ||
        !standard_tonality_roman_numeral_format(&result->reading.applied_target, target_text, sizeof(target_text))) {
        return false;
    }
    roman->applied_numeral = strdup(applied_text);
    roman->target_numeral = strdup(target_text);
    roman->applied = true;
    roman->target_degree = result->claim.applied_target_degree;
    roman->applied_kind = applied_chord_kind_name((AppliedChordKind)result->claim.applied_kind);
    roman->resolution_entry = result->resolution_index;
    roman->deceptive = result->deceptive;
    return roman->applied_numeral != NULL && roman->target_numeral != NULL && roman->applied_kind != NULL;
}

static const char *unlabelled_reason(const ChordEstimate *chord) {
    if (!chord->in_region) {
        return REASON_NO_REGION;
    }
    if (!chord->found) {
        return REASON_NO_CHORD;
    }
    return REASON_NO_NUMERAL;
}

void roman_numerals_destroy(RomanNumerals *romans) {
    *romans = (RomanNumerals){0};
}

bool read_roman_numerals(const AnalysisChain *library, RomanNumerals *out) {
    *out = (RomanNumerals){0};
    out->count = analysis_chain_segment_count(library);
    out->inputs = analysis_chain_chord_inputs(library);
    out->results = analysis_chain_numerals(library);
    if (!out->inputs || !out->results) {
        out->count = 0;
    }
    return true;
}

bool assign_roman_numerals(const RomanNumerals *romans, const ChordEstimate *chords, AnalysisReportEntry *entries) {
    for (size_t i = 0; i < romans->count; i++) {
        const StandardTonalityRomanResult *result = &romans->results[i];
        AnalysisRomanNumeral *roman = &entries[i].roman;
        if (result->has_numeral) {
            roman->numeral = strdup(result->claim.label);
            roman->base_numeral = base_numeral_of(result);
            roman->labelled = roman->numeral != NULL && roman->base_numeral != NULL;
            roman->degree = result->claim.degree;
            roman->uncertainty = result->uncertainty;
            if (!roman->labelled || (result->reading.applied && !fill_applied(result, roman))) {
                return false;
            }
        } else {
            roman->reason = strdup(unlabelled_reason(&chords[i]));
            if (!roman->reason) {
                return false;
            }
        }
    }
    return true;
}
