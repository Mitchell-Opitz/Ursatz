#include <ursatz_composer/harmony_chords.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/harmony/harmony_applied.h>
#include <theory/tonality/brightness.h>
#include <theory/tonality/region_relation.h>

#include "harmony/harmony_internal.h"

void composer_mixture_default(ComposerMixture *out) {
    static const char *const WORDS[] = {"parallel", "one step darker", "one step brighter"};
    memset(out, 0, sizeof(*out));
    out->share.num = 0;
    out->share.den = 1;
    out->from_count = sizeof(WORDS) / sizeof(WORDS[0]);
    for (size_t i = 0; i < out->from_count; i++) {
        snprintf(out->from[i], COMPOSER_MIXTURE_WORD_MAX, "%s", WORDS[i]);
    }
}

bool composer_chords_init(ComposerChords *chords, const KnowledgeSnapshot *knowledge, char *message, size_t message_size) {
    if (chords == NULL || knowledge == NULL) {
        if (message != NULL && message_size > 0) {
            message[0] = '\0';
        }
        return false;
    }
    ComposerChords built;
    memset(&built, 0, sizeof(built));
    built.knowledge = knowledge;
    composer_mixture_default(&built.mixture);
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

int composer_modal_plan_degree(const KnowledgeSnapshot *knowledge, const PitchMaterial *modal, int degree) {
    if (knowledge == NULL || modal == NULL || modal->cadence == NULL || (degree != 4 && degree != 5)) {
        return degree;
    }
    const VocabularyEntry *entry = knowledge_snapshot_vocabulary(knowledge, modal->cadence);
    if (entry == NULL || entry->subtype != VOCABULARY_CADENCE_KIND || entry->cadence.approach_count == 0) {
        return degree;
    }
    const VocabularyCadence *cadence = &entry->cadence;
    if (degree == 5) {
        return cadence->approach_degrees[cadence->approach_count - 1];
    }
    for (size_t i = 0; i < modal->primary_count; i++) {
        int primary = modal->primary_degrees[i];
        bool approach = false;
        for (size_t a = 0; a < cadence->approach_count; a++) {
            approach = approach || cadence->approach_degrees[a] == primary;
        }
        if (primary != 1 && !approach) {
            return primary;
        }
    }
    return degree;
}

const PitchMaterial *composer_chords_modal(const ComposerChords *chords, const SpelledKey *key) {
    if (key == NULL) {
        return NULL;
    }
    return pitch_material_is_modal_key(chords != NULL ? &chords->materials : NULL, key->material) ? key->material : NULL;
}

const PitchMaterial *composer_chords_key_material(const ComposerChords *chords, HarmonyMode mode, const PitchMaterial *modal) {
    if (modal != NULL) {
        return modal;
    }
    return chords != NULL ? (mode == HARMONY_MODE_MINOR ? chords->materials.natural_minor : chords->materials.major) : NULL;
}

static size_t mixture_add(const PitchMaterial *key_material, const PitchMaterial *material, const PitchMaterial **out, size_t count) {
    if (material == NULL || material == key_material || !pitch_material_is_diatonic(material)) {
        return count;
    }
    for (size_t i = 0; i < count; i++) {
        if (out[i] == material) {
            return count;
        }
    }
    if (count < COMPOSER_MIXTURE_MAX) {
        out[count++] = material;
    }
    return count;
}

size_t composer_chords_mixture_materials(const ComposerChords *chords, const PitchMaterial *key_material, const char *const *from,
                                         size_t from_count, bool only_from, const PitchMaterial **out) {
    if (chords == NULL || chords->knowledge == NULL || key_material == NULL || out == NULL || !key_material->has_brightness) {
        return 0;
    }
    size_t count = 0;
    Region anchor = region_tonal(0, key_material);
    for (size_t i = 0; from != NULL && i < from_count; i++) {
        Region reached;
        char why[160];
        if (region_resolve_named(chords->knowledge, from[i], &anchor, &reached, why, sizeof(why)) == REGION_RELATION_OK) {
            count = mixture_add(key_material, reached.material, out, count);
        }
    }
    for (int distance = 1; !only_from && distance <= 6; distance++) {
        for (int sign = -1; sign <= 1; sign += 2) {
            count = mixture_add(key_material, brightness_shift(chords->knowledge, key_material, sign * distance, NULL, 0), out, count);
        }
    }
    return count;
}

const PitchMaterial *composer_chords_borrow_source(const ComposerChords *chords, const PitchMaterial *key_material, int degree,
                                                   int root_semitones, StandardTonalityTriadQuality quality) {
    const PitchMaterial *candidates[COMPOSER_MIXTURE_MAX];
    size_t count = composer_chords_mixture_materials(chords, key_material, NULL, 0, false, candidates);
    for (size_t i = 0; i < count; i++) {
        int root;
        StandardTonalityTriadQuality found;
        if (pitch_material_triad_on(candidates[i], degree, &root, NULL, &found) && root == root_semitones && found == quality) {
            return candidates[i];
        }
    }
    return NULL;
}

/* The leading-tone chord of a minor key stands on the raised seventh degree. */
#define LEADING_TONE_SEMITONES 11

bool composer_chord_intervals(const ComposerChords *chords, HarmonyMode mode, const PitchMaterial *modal, const HarmonyChord *chord,
                              int *root_semitones, int intervals[4], size_t *count) {
    if (chord == NULL || root_semitones == NULL || intervals == NULL || count == NULL) {
        return false;
    }
    if (chord->borrowed_from != NULL) {
        int triad[3];
        StandardTonalityTriadQuality quality;
        if (chord->seventh || chord->leading_tone || !pitch_material_triad_on(chord->borrowed_from, chord->degree, root_semitones, triad, &quality) ||
            quality == STANDARD_TONALITY_TRIAD_AUGMENTED) {
            return false;
        }
        intervals[0] = triad[0];
        intervals[1] = triad[1];
        intervals[2] = triad[2];
        intervals[3] = 0;
        *count = 3;
        return true;
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

bool composer_chord_tones(const ComposerChords *chords, const SpelledKey *key, const HarmonyChord *chord, int *root_pc,
                          int intervals[COMPOSER_CHORD_TONES_MAX], size_t *count) {
    int root;
    int basic[4];
    size_t basic_count;
    if (key == NULL || root_pc == NULL || intervals == NULL || count == NULL ||
        !composer_chord_intervals(chords, harmony_key_mode(key), composer_chords_modal(chords, key), chord, &root, basic, &basic_count)) {
        return false;
    }
    *root_pc = (spelled_key_tonic_pitch_class(key) + root) % 12;
    if (chord->color != NULL && chord->color->member_count <= COMPOSER_CHORD_TONES_MAX) {
        for (size_t i = 0; i < chord->color->member_count; i++) {
            intervals[i] = chord->color->members[i].semitones;
        }
        *count = chord->color->member_count;
        return true;
    }
    for (size_t i = 0; i < basic_count; i++) {
        intervals[i] = basic[i];
    }
    *count = basic_count;
    return true;
}
