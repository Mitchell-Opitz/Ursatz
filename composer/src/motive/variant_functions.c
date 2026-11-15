#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/variant_registry.h>

#include "motive/variant_internal.h"

/* The scale degree of a Roman numeral ("V7", "bVI", "ii6"), 0 when it has none. */
static int roman_degree(const char *roman) {
    while (*roman == 'b' || *roman == '#') {
        roman++;
    }
    int value = 0;
    int count = 0;
    while (roman[count] == 'i' || roman[count] == 'I' || roman[count] == 'v' || roman[count] == 'V') {
        count++;
    }
    char letters[8] = "";
    if (count == 0 || count >= (int)sizeof(letters)) {
        return 0;
    }
    for (int i = 0; i < count; i++) {
        letters[i] = (char)(roman[i] | 0x20);
    }
    static const char *const NAMES[] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    for (int degree = 0; degree < 7; degree++) {
        if (strcmp(letters, NAMES[degree]) == 0) {
            value = degree + 1;
        }
    }
    return value;
}

/*
 * The degree of the motive's first or last chord: its own harmony, else the best implied chord. 0 when it has no chord
 * (no key). *altered is set for a chord that is not diatonic.
 */
static int chord_degree(const Motive *motive, const MotiveAnalysis *analysis, bool last, bool *altered) {
    *altered = false;
    if (motive->chord_count > 0) {
        const MotiveSketchChord *chord = &motive->chords[last ? motive->chord_count - 1 : 0];
        *altered = chord->alteration != 0;
        return chord->degree;
    }
    if (analysis->harmony_known && analysis->candidate_count > 0) {
        const char *roman = analysis->candidates[0].roman;
        *altered = roman[0] == 'b' || roman[0] == '#';
        return roman_degree(roman);
    }
    return 0;
}

static bool chain_holds(const VariantRecord *record, const char *op) {
    for (size_t s = 0; s < record->step_count; s++) {
        if (strcmp(record->steps[s].op, op) == 0) {
            return true;
        }
    }
    return false;
}

static bool chain_thins(const VariantRecord *record) {
    static const char *const OPS[] = {"fragment", "liquidate", "liquidate-features", "condense", "repeat-feature"};
    for (size_t i = 0; i < sizeof(OPS) / sizeof(OPS[0]); i++) {
        if (chain_holds(record, OPS[i])) {
            return true;
        }
    }
    return false;
}

/* The last note is the longest of the motive's notes. */
static bool last_is_longest(const Motive *form) {
    for (size_t i = 0; i + 1 < form->note_count; i++) {
        if (rational_cmp(form->notes[i].duration, form->notes[form->note_count - 1].duration) > 0) {
            return false;
        }
    }
    return form->note_count > 0;
}

bool variant_functions(const Motive *basic, const Motive *form, const VariantRecord *record, const char *out[4],
                       size_t *count) {
    if (!basic || !form || !record || !out || !count) {
        return false;
    }
    *count = 0;
    MotiveAnalysis analysis;
    if (!motive_analyze(form, &analysis)) {
        return false;
    }

    /* Remoteness from the record, else searched here. */
    unsigned long long remote_num = record->remoteness_num;
    unsigned long long remote_den = record->remoteness_den;
    if (remote_den == 0) {
        VariantRecord searched;
        memset(&searched, 0, sizeof(searched));
        if (variant_set_remoteness(basic, form, VARIANT_CHAIN_MAX, &searched) != VARIANT_OK) {
            return false;
        }
        remote_num = searched.remoteness_num;
        remote_den = searched.remoteness_den;
    }
    bool remote_half_or_less = remote_num * 2 <= remote_den;

    bool shorter = rational_cmp(form->length, basic->length) < 0;
    bool continuation = shorter || chain_thins(record);
    bool contrast = !remote_half_or_less;
    bool beginning = false;
    bool cadential = false;
    if (form->form == MOTIVE_SKETCH_FORM_FULL) {
        VariantRecord difference;
        memset(&difference, 0, sizeof(difference));
        variant_record_difference(basic, form, &difference);
        bool rhythm_kept = false;
        for (size_t i = 0; i < difference.kept_count; i++) {
            rhythm_kept = rhythm_kept || strcmp(difference.kept[i], "rhythm") == 0;
        }
        bool altered;
        int first = chord_degree(form, &analysis, false, &altered);
        bool opens_on_tonic = first == 1 && !altered;
        beginning = opens_on_tonic && rhythm_kept && !difference.rhythm_scaled && remote_half_or_less;
        contrast = contrast || (first != 0 && !opens_on_tonic);

        int last = chord_degree(form, &analysis, true, &altered);
        int note = analysis.degrees_known && form->note_count > 0 ? analysis.degrees[form->note_count - 1].degree : 0;
        cadential = (last == 1 || last == 5) && !altered && (note == 1 || note == 3 || note == 5) &&
                    last_is_longest(form);
    }

    if (beginning) {
        out[(*count)++] = "beginning";
    }
    if (continuation) {
        out[(*count)++] = "continuation";
    }
    if (contrast) {
        out[(*count)++] = "contrast";
    }
    if (cadential) {
        out[(*count)++] = "cadential";
    }
    return true;
}
