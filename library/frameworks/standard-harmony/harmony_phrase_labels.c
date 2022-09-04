#include "harmony_phrase_labels.h"

#include "harmony_functions.h"

static bool is_tonic(HarmonyMode mode, const HarmonyChord *chord) {
    HarmonicFunction function;
    return harmony_function_of(mode, chord, &function) && function == HARMONIC_FUNCTION_TONIC;
}

/* V or V7 (degree 5), not the cadential 6/4. */
static bool is_cadential_dominant(const HarmonyChord *chord) {
    return chord->degree == 5 && !chord->cadential;
}

static bool is_predominant(HarmonyMode mode, const HarmonyChord *chord) {
    HarmonicFunction function;
    return harmony_function_of(mode, chord, &function) && function == HARMONIC_FUNCTION_PREDOMINANT;
}

static bool is_strong(HarmonyMode mode, const HarmonyChord *chord) {
    HarmonyPredominantStrength strength;
    return harmony_predominant_strength(mode, chord, &strength) && strength == HARMONY_PREDOMINANT_STRONG;
}

static void put(HarmonyPhraseLabels *labels, size_t index, PhraseModelLabel label) {
    labels->has_label[index] = true;
    labels->label[index] = label;
}

bool harmony_phrase_model_label(const HarmonyPhrase *phrase, CadenceType cadence, HarmonyPhraseLabels *out) {
    if (out == NULL || !harmony_phrase_valid(phrase) || phrase->chord_count > HARMONY_PHRASE_LABELS_MAX) {
        return false;
    }
    bool authentic = cadence_type_is_authentic(cadence);
    if (!authentic && cadence != CADENCE_TYPE_HALF) {
        return false;
    }

    const HarmonyChord *chords = phrase->chords;
    size_t n = phrase->chord_count;
    size_t dominant;
    if (authentic) {
        if (n < 2 || !is_tonic(phrase->mode, &chords[n - 1]) || !is_cadential_dominant(&chords[n - 2])) {
            return false;
        }
        dominant = n - 2;
    } else {
        if (!is_cadential_dominant(&chords[n - 1])) {
            return false;
        }
        dominant = n - 1;
    }

    HarmonyPhraseLabels labels;
    labels.chord_count = n;
    for (size_t i = 0; i < HARMONY_PHRASE_LABELS_MAX; i++) {
        labels.has_label[i] = false;
        labels.label[i] = PHRASE_MODEL_LABEL_DOMINANT;
    }

    put(&labels, dominant, PHRASE_MODEL_LABEL_DOMINANT);
    if (authentic) {
        put(&labels, n - 1, PHRASE_MODEL_LABEL_TONIC_ENDING);
    }

    /* The run of predominants just before the dominant, past a cadential 6/4. */
    size_t end = dominant;
    if (end > 0 && chords[end - 1].cadential) {
        end--;
    }
    size_t start = end;
    while (start > 0 && is_predominant(phrase->mode, &chords[start - 1])) {
        start--;
    }
    for (size_t i = start; i < end; i++) {
        if (is_strong(phrase->mode, &chords[i])) {
            put(&labels, i, PHRASE_MODEL_LABEL_PREDOMINANT);
            break;
        }
    }

    if (dominant > 0 && is_tonic(phrase->mode, &chords[0])) {
        put(&labels, 0, PHRASE_MODEL_LABEL_TONIC_BEGINNING);
    }

    *out = labels;
    return true;
}

int harmony_phrase_labels_index_of(const HarmonyPhraseLabels *labels, PhraseModelLabel label) {
    if (labels == NULL || !phrase_model_label_valid(label)) {
        return -1;
    }
    for (size_t i = 0; i < labels->chord_count; i++) {
        if (labels->has_label[i] && labels->label[i] == label) {
            return (int)i;
        }
    }
    return -1;
}
