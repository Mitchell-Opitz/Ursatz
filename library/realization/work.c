#include "realization/work.h"

#include <stdlib.h>
#include <string.h>

struct Work {
    char id[REALIZATION_ID_MAX];
    char *title;
    char *composer;
    char ensemble[REALIZATION_ID_MAX];
    bool has_ensemble;
    char **materials;
    size_t material_count, material_capacity;
    Movement **movements;
    size_t movement_count, movement_capacity;
};

static char *copy_text(const char *text) {
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }
    return copy;
}

RealizationResult work_create(const char *id, const char *title, const char *composer, Work **out) {
    if (id == NULL || out == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!realization_id_valid(id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    Work *work = calloc(1, sizeof(*work));
    if (work == NULL) {
        return REALIZATION_ERR_ALLOC;
    }
    strcpy(work->id, id);
    if ((title != NULL && (work->title = copy_text(title)) == NULL) ||
        (composer != NULL && (work->composer = copy_text(composer)) == NULL)) {
        work_destroy(work);
        return REALIZATION_ERR_ALLOC;
    }
    *out = work;
    return REALIZATION_OK;
}

void work_destroy(Work *work) {
    if (work == NULL) {
        return;
    }
    for (size_t i = 0; i < work->material_count; i++) {
        free(work->materials[i]);
    }
    for (size_t i = 0; i < work->movement_count; i++) {
        movement_destroy(work->movements[i]);
    }
    free(work->title);
    free(work->composer);
    free(work->materials);
    free(work->movements);
    free(work);
}

RealizationResult work_set_ensemble(Work *work, const char *ensemble_id) {
    if (work == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (ensemble_id == NULL) {
        work->has_ensemble = false;
        work->ensemble[0] = '\0';
        return REALIZATION_OK;
    }
    if (!realization_id_valid(ensemble_id)) {
        return REALIZATION_ERR_BAD_ID;
    }
    strcpy(work->ensemble, ensemble_id);
    work->has_ensemble = true;
    return REALIZATION_OK;
}

/* ^[a-z]+(-[a-z]+)*:[A-Za-z0-9_.-]+$ */
static bool material_reference_valid(const char *text) {
    const char *colon = strchr(text, ':');
    if (colon == NULL || colon == text || colon[1] == '\0') {
        return false;
    }
    for (const char *p = text; p < colon; p++) {
        bool letter = *p >= 'a' && *p <= 'z';
        if (!letter && (*p != '-' || p == text || p + 1 == colon || p[1] == '-')) {
            return false;
        }
    }
    for (const char *p = colon + 1; *p != '\0'; p++) {
        bool ok = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_' || *p == '.' ||
                  *p == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

RealizationResult work_add_material(Work *work, const char *reference) {
    if (work == NULL || reference == NULL) {
        return REALIZATION_ERR_NULL;
    }
    if (!material_reference_valid(reference)) {
        return REALIZATION_ERR_BAD_VALUE;
    }
    if (work->material_count == work->material_capacity) {
        size_t capacity = work->material_capacity == 0 ? 4 : work->material_capacity * 2;
        char **grown = realloc(work->materials, capacity * sizeof(char *));
        if (grown == NULL) {
            return REALIZATION_ERR_ALLOC;
        }
        work->materials = grown;
        work->material_capacity = capacity;
    }
    char *copy = copy_text(reference);
    if (copy == NULL) {
        return REALIZATION_ERR_ALLOC;
    }
    work->materials[work->material_count++] = copy;
    return REALIZATION_OK;
}

RealizationResult work_add_movement(Work *work, Movement *movement) {
    if (work == NULL || movement == NULL) {
        return REALIZATION_ERR_NULL;
    }
    for (size_t i = 0; i < work->movement_count; i++) {
        if (strcmp(movement_id(work->movements[i]), movement_id(movement)) == 0) {
            return REALIZATION_ERR_DUPLICATE_ID;
        }
    }
    if (work->movement_count == work->movement_capacity) {
        size_t capacity = work->movement_capacity == 0 ? 4 : work->movement_capacity * 2;
        Movement **grown = realloc(work->movements, capacity * sizeof(Movement *));
        if (grown == NULL) {
            return REALIZATION_ERR_ALLOC;
        }
        work->movements = grown;
        work->movement_capacity = capacity;
    }
    work->movements[work->movement_count++] = movement;
    return REALIZATION_OK;
}

const char *work_id(const Work *work) {
    return work != NULL ? work->id : NULL;
}

const char *work_title(const Work *work) {
    return work != NULL ? work->title : NULL;
}

const char *work_composer(const Work *work) {
    return work != NULL ? work->composer : NULL;
}

const char *work_ensemble(const Work *work) {
    return work != NULL && work->has_ensemble ? work->ensemble : NULL;
}

size_t work_material_count(const Work *work) {
    return work != NULL ? work->material_count : 0;
}

const char *work_material_at(const Work *work, size_t index) {
    return work != NULL && index < work->material_count ? work->materials[index] : NULL;
}

size_t work_movement_count(const Work *work) {
    return work != NULL ? work->movement_count : 0;
}

Movement *work_movement_at(const Work *work, size_t index) {
    return work != NULL && index < work->movement_count ? work->movements[index] : NULL;
}

const char *work_movement_ensemble(const Work *work, const Movement *movement) {
    const char *own = movement_ensemble(movement);
    return own != NULL ? own : work_ensemble(work);
}
