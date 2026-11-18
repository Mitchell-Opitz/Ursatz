#include "realization/derivation.h"

#include <string.h>

RealizationResult note_derivation_add_source(NoteDerivation *derivation, const char *note_id) {
    if (derivation == NULL || note_id == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(note_id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    if (derivation->source_count >= NOTE_DERIVATION_SOURCES_MAX) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    strcpy(derivation->sources[derivation->source_count++], note_id);
    return REALIZATION_OK;
}

static bool operation_name_valid(const char *name) {
    size_t length = strlen(name);
    if (length == 0 || length > sizeof(((NoteDerivation *)0)->operation) - 1 || name[0] == '-' || name[length - 1] == '-') {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        bool letter = name[i] >= 'a' && name[i] <= 'z';
        if (!letter && (name[i] != '-' || name[i + 1] == '-')) {
            return false;
        }
    }
    return true;
}

RealizationResult note_derivation_set_operation(NoteDerivation *derivation, const char *name) {
    if (derivation == NULL || name == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!operation_name_valid(name)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    strcpy(derivation->operation, name);
    return REALIZATION_OK;
}

RealizationResult note_derivation_set_run(NoteDerivation *derivation, const char *run) {
    if (derivation == NULL || run == NULL) {
        return REALIZATION_ERR_NULL;
    }
    size_t length = strlen(run);
    if (length > sizeof(derivation->run) - 1) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    for (size_t i = 0; i < length; i++) {
        if ((unsigned char)run[i] < 0x20 || (unsigned char)run[i] > 0x7E) {
            return REALIZATION_ERR_BAD_VALUE;
        }
    }
    strcpy(derivation->run, run);
    return REALIZATION_OK;
}

bool note_derivation_present(const NoteDerivation *derivation) {
    return derivation != NULL && (derivation->source_count > 0 || derivation->operation[0] != '\0' || derivation->run[0] != '\0');
}
