#include "theory/tonality/brightness.h"

#include <stdarg.h>
#include <stdio.h>

static void say(char *message, size_t size, const char *format, ...) {
    if (message == NULL || size == 0) {
        return;
    }
    va_list args;
    va_start(args, format);
    vsnprintf(message, size, format, args);
    va_end(args);
}

static const char *name_of(const PitchMaterial *material) {
    return material->name != NULL ? material->name : "material";
}

bool brightness_rank_of(const PitchMaterial *material, int *rank, char *message, size_t size) {
    if (material == NULL || rank == NULL) {
        say(message, size, "brightness: no material to rank");
        return false;
    }
    if (!material->has_brightness) {
        say(message, size, "%s has no brightness: only the diatonic modes rank", name_of(material));
        return false;
    }
    *rank = material->brightness_rank;
    return true;
}

const PitchMaterial *brightness_shift(const KnowledgeSnapshot *snapshot, const PitchMaterial *material, int steps, char *message,
                                      size_t size) {
    int rank = 0;
    if (snapshot == NULL) {
        say(message, size, "brightness: no knowledge snapshot");
        return NULL;
    }
    if (!brightness_rank_of(material, &rank, message, size)) {
        return NULL;
    }
    int wanted = rank + steps;
    const PitchMaterial *found = NULL;
    size_t count = 0;
    for (size_t i = 0; i < knowledge_snapshot_material_count(snapshot); i++) {
        const PitchMaterial *candidate = knowledge_snapshot_material_at(snapshot, i);
        if (candidate != NULL && candidate->has_brightness && candidate->brightness_rank == wanted) {
            found = candidate;
            count++;
        }
    }
    if (count == 0) {
        say(message, size, "%s: no diatonic mode has brightness rank %d (%d steps from rank %d)", name_of(material), wanted, steps,
            rank);
    } else if (count > 1) {
        say(message, size, "%s: more than one material has brightness rank %d", name_of(material), wanted);
    }
    return count == 1 ? found : NULL;
}
