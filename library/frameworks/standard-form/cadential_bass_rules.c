#include "cadential_bass_rules.h"

#include "rule_pair.h"

static const char *const NAMES[STANDARD_FORM_CADENTIAL_BASS_RULE_COUNT] = {
    "cadential-bass-core-line",
    "cadential-bass-harmonization",
    "cadential-bass-embellishment",
    "cadential-bass-clearest",
    "cadential-bass-variants"};

static RulePairTable g_table = RULE_PAIR_TABLE_INIT(standard_form_framework, "standard-form",
    NAMES, STANDARD_FORM_CADENTIAL_BASS_RULE_COUNT);

URSATZ_RULE_ACCESSORS(standard_form_cadential_bass, StandardFormCadentialBassRule, STANDARD_FORM_CADENTIAL_BASS_RULE_COUNT, g_table)

static const int CORE_DEGREES[4] = {3, 4, 5, 1};

size_t standard_form_cadential_core_length(void) {
    return 4;
}

int standard_form_cadential_core_degree(size_t index) {
    return index < 4 ? CORE_DEGREES[index] : 0;
}

static bool note_is(const StandardFormBassNote *note, int degree, int alteration) {
    return note->degree == degree && note->alteration == alteration;
}

bool standard_form_bass_has_fi_embellishment(const StandardFormBassNote *notes, size_t count) {
    return notes != NULL && count == 5 && note_is(&notes[0], 3, 0) && note_is(&notes[1], 4, 0) &&
           note_is(&notes[2], 4, 1) && note_is(&notes[3], 5, 0) && note_is(&notes[4], 1, 0);
}

bool standard_form_bass_is_cadential(const StandardFormBassNote *notes, size_t count) {
    if (notes == NULL) {
        return false;
    }
    if (count == 5) {
        return standard_form_bass_has_fi_embellishment(notes, count);
    }
    if (count != 4) {
        return false;
    }
    for (size_t i = 0; i < 4; i++) {
        if (!note_is(&notes[i], CORE_DEGREES[i], 0)) {
            return false;
        }
    }
    return true;
}

bool standard_form_cadential_chord_allowed(size_t core_index, StandardFormCadentialChord chord) {
    switch (core_index) {
        case 0:
            return chord == STANDARD_FORM_CADENTIAL_CHORD_I6;
        case 1:
            return chord == STANDARD_FORM_CADENTIAL_CHORD_II6 || chord == STANDARD_FORM_CADENTIAL_CHORD_IV;
        case 2:
            return chord == STANDARD_FORM_CADENTIAL_CHORD_V || chord == STANDARD_FORM_CADENTIAL_CHORD_V7;
        case 3:
            return chord == STANDARD_FORM_CADENTIAL_CHORD_I;
        default:
            return false;
    }
}

bool standard_form_cadential_six_four_elaborates(size_t core_index) {
    return core_index == 2;
}

bool standard_form_cadential_is_clearest(size_t measures, size_t bass_note_count) {
    return measures == 4 && bass_note_count == 4;
}

bool standard_form_bass_is_cadential_variant(const StandardFormBassNote *notes, size_t count,
                                             StandardFormCadentialVariant variant) {
    if (notes == NULL) {
        return false;
    }
    switch (variant) {
        case STANDARD_FORM_CADENTIAL_VARIANT_DI_RE_SOL_DO:
            return count == 4 && note_is(&notes[0], 1, 1) && note_is(&notes[1], 2, 0) && note_is(&notes[2], 5, 0) &&
                   note_is(&notes[3], 1, 0);
        case STANDARD_FORM_CADENTIAL_VARIANT_FI_SOL_DO:
            return count == 3 && note_is(&notes[0], 4, 1) && note_is(&notes[1], 5, 0) && note_is(&notes[2], 1, 0);
        default:
            return false;
    }
}

bool standard_form_bass_is_any_cadential_variant(const StandardFormBassNote *notes, size_t count) {
    return standard_form_bass_is_cadential_variant(notes, count, STANDARD_FORM_CADENTIAL_VARIANT_DI_RE_SOL_DO) ||
           standard_form_bass_is_cadential_variant(notes, count, STANDARD_FORM_CADENTIAL_VARIANT_FI_SOL_DO);
}
