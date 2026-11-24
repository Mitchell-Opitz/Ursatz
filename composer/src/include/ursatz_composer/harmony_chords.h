#ifndef URSATZ_COMPOSER_HARMONY_CHORDS_H
#define URSATZ_COMPOSER_HARMONY_CHORDS_H

#include <stdbool.h>
#include <stddef.h>

#include <theory/knowledge/knowledge_snapshot.h>
#include <theory/harmony/harmony_chord.h>
#include <theory/knowledge/vocabulary_entry.h>
#include <theory/tonality/common_practice_materials.h>
#include <theory/tonality/modal_chords.h>
#include <theory/tonality/spelled_key.h>
#include <ursatz_composer/fill_operator.h>

/*
 * The chord context of a run (R-728): the knowledge snapshot and the common-practice materials resolved from it, so the chord types
 * the Composer plays are the snapshot's entries, not tables of its own. One is built per run (compose_run, api_progression) and
 * lent to every operator that reads a chord's tones; it must outlive the operators registered with it. A NULL pointer in place of
 * one means the fallback tables of the library (the call holds no snapshot).
 */
typedef struct {
    const KnowledgeSnapshot *knowledge;
    CommonPracticeMaterials materials;
} ComposerChords;

/* Resolves the materials from `knowledge`. False for a NULL argument or a snapshot without them, with `message` naming the id. */
bool composer_chords_init(ComposerChords *chords, const KnowledgeSnapshot *knowledge, char *message, size_t message_size);

/*
 * The cadence kind a modal material names (its `cadence` entry in the snapshot); NULL for a material with none, a NULL argument or an
 * id that is not a cadence-kind entry.
 */
const VocabularyCadence *composer_chords_cadence(const ComposerChords *chords, const PitchMaterial *material);

/*
 * The degree a common-practice plan degree stands for in a modal key (R-725, R-732): 5 (the dominant) is the cadence kind's last
 * approach degree, 4 (the predominant) the mode's primary degree that is neither the tonic nor an approach degree; every other
 * degree, and every degree of a major or minor key (`modal` NULL), a missing cadence kind or a NULL snapshot, stays as it is. Each
 * non-tonic primary chord of a church mode holds its characteristic step (Persichetti ch. 2), so a plan read this way sounds it.
 */
int composer_modal_plan_degree(const KnowledgeSnapshot *knowledge, const PitchMaterial *modal, int degree);

/* The material of a modal key (pitch_material_is_modal_key against the chords' materials), else NULL: a major or minor key, a NULL key. */
const PitchMaterial *composer_chords_modal(const ComposerChords *chords, const SpelledKey *key);

/*
 * The one Composer owner of a chord's pitch classes (R-725): the semitones of the root above the tonic and the chord's intervals above
 * its root. A major or minor key (`modal` NULL) reads the mode tables as before: harmony_diatonic_intervals, the leading-tone root on
 * the raised seventh, harmony_applied_scale_semitone for the rest. A modal key (`modal` its material) stacks the triad on the degree
 * from the material (pitch_material_triad_on), with no raised degree and no leading tone; a seventh chord or a leading-tone chord
 * is not written there. False for a chord the key does not hold. `intervals` is filled to `count` (3 or 4).
 */
bool composer_chord_intervals(const ComposerChords *chords, HarmonyMode mode, const PitchMaterial *modal, const HarmonyChord *chord,
                              int *root_semitones, int intervals[4], size_t *count);

/* The same for a key: *root_pc is the root's pitch class (0 to 11). */
bool composer_chord_tones(const ComposerChords *chords, const SpelledKey *key, const HarmonyChord *chord, int *root_pc, int intervals[4],
                          size_t *count);

/*
 * What a chord-reading operator keeps as its FillOperator context: its own params (NULL for the defaults, or a flag) and the
 * chords. composer_register_operator copies `operator_` into the registry with such a context, which the registry frees.
 */
typedef struct {
    const void *params;
    const ComposerChords *chords;
} ComposerOperatorContext;

FillResult composer_register_operator(FillOperatorRegistry *registry, FillOperator *operator_, const void *params,
                                      const ComposerChords *chords);

/* The params and the chords of an operator's context (NULL for a NULL context). */
const void *composer_context_params(const void *context);
const ComposerChords *composer_context_chords(const void *context);

#endif
