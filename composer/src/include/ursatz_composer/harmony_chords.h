#ifndef URSATZ_COMPOSER_HARMONY_CHORDS_H
#define URSATZ_COMPOSER_HARMONY_CHORDS_H

#include <stdbool.h>
#include <stddef.h>

#include <theory/knowledge/knowledge_snapshot.h>
#include <theory/tonality/common_practice_materials.h>
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
