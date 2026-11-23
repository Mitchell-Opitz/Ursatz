#include <ursatz_composer/harmony_chords.h>

#include <stdlib.h>

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
