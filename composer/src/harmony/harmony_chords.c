#include <ursatz_composer/harmony_chords.h>

#include <stdlib.h>

#include <theory/harmony/harmony_applied.h>

#include "harmony/harmony_internal.h"

bool composer_chords_init(ComposerChords *chords, const KnowledgeSnapshot *knowledge, char *message, size_t message_size) {
    if (chords == NULL || knowledge == NULL) {
        if (message != NULL && message_size > 0) {
            message[0] = '\0';
        }
        return false;
    }
    ComposerChords built;
    built.knowledge = knowledge;
    if (!common_practice_materials_resolve(knowledge, &built.materials, message, message_size)) {
        return false;
    }
    *chords = built;
    return true;
}

FillResult composer_register_operator(FillOperatorRegistry *registry, FillOperator *operator_, const void *params,
                                      const ComposerChords *chords) {
    if (registry == NULL || operator_ == NULL) {
        return FILL_ERR_NULL;
    }
    ComposerOperatorContext *context = malloc(sizeof(*context));
    if (context == NULL) {
        return FILL_ERR_ALLOC;
    }
    context->params = params;
    context->chords = chords;
    operator_->context = context;
    operator_->destroy_context = free;
    FillResult result = fill_registry_register(registry, operator_);
    if (result != FILL_OK) {
        free(context);
    }
    return result;
}

const void *composer_context_params(const void *context) {
    return context != NULL ? ((const ComposerOperatorContext *)context)->params : NULL;
}

const ComposerChords *composer_context_chords(const void *context) {
    return context != NULL ? ((const ComposerOperatorContext *)context)->chords : NULL;
}

const VocabularyCadence *composer_chords_cadence(const ComposerChords *chords, const PitchMaterial *material) {
    if (chords == NULL || chords->knowledge == NULL || material == NULL || material->cadence == NULL) {
        return NULL;
    }
    const VocabularyEntry *entry = knowledge_snapshot_vocabulary(chords->knowledge, material->cadence);
    return entry != NULL && entry->subtype == VOCABULARY_CADENCE_KIND ? &entry->cadence : NULL;
}

const PitchMaterial *composer_chords_modal(const ComposerChords *chords, const SpelledKey *key) {
    if (chords == NULL || key == NULL) {
        return NULL;
    }
    return pitch_material_is_modal_key(&chords->materials, key->material) ? key->material : NULL;
}

/* The leading-tone chord of a minor key stands on the raised seventh degree. */
#define LEADING_TONE_SEMITONES 11

bool composer_chord_intervals(const ComposerChords *chords, HarmonyMode mode, const PitchMaterial *modal, const HarmonyChord *chord,
                              int *root_semitones, int intervals[4], size_t *count) {
    if (chord == NULL || root_semitones == NULL || intervals == NULL || count == NULL) {
        return false;
    }
    if (modal != NULL) {
        int triad[3];
        int root;
        if (chord->seventh || chord->leading_tone || !pitch_material_triad_on(modal, chord->degree, &root, triad, NULL)) {
            return false;
        }
        intervals[0] = triad[0];
        intervals[1] = triad[1];
        intervals[2] = triad[2];
        intervals[3] = 0;
        *root_semitones = root;
        *count = 3;
        return true;
    }
    int found[4];
    size_t found_count;
    if (!harmony_diatonic_intervals(chords, mode, chord, found, &found_count)) {
        return false;
    }
    int root = mode == HARMONY_MODE_MINOR && chord->leading_tone ? LEADING_TONE_SEMITONES
                                                                 : harmony_applied_scale_semitone(mode, chord->degree);
    if (root < 0) {
        return false;
    }
    for (size_t i = 0; i < 4; i++) {
        intervals[i] = found[i];
    }
    *root_semitones = root;
    *count = found_count;
    return true;
}

bool composer_chord_tones(const ComposerChords *chords, const SpelledKey *key, const HarmonyChord *chord, int *root_pc, int intervals[4],
                          size_t *count) {
    int root;
    if (key == NULL || root_pc == NULL ||
        !composer_chord_intervals(chords, harmony_key_mode(key), composer_chords_modal(chords, key), chord, &root, intervals, count)) {
        return false;
    }
    *root_pc = (spelled_key_tonic_pitch_class(key) + root) % 12;
    return true;
}
