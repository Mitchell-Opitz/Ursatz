#ifndef URSATZ_REALIZATION_WORK_H
#define URSATZ_REALIZATION_WORK_H

#include <stddef.h>

#include "realization/movement.h"
#include "realization/realization.h"

/*
 * A work (target architecture 4.8, R-531): movements in order, with the defaults they inherit: a title, a composer, the ensemble
 * template id the movements are scored for unless they name another, and the material the work draws on (references written as spec
 * 4.13 writes them, `<kind>:<name>`). The work owns its movements and frees them with itself. A project (a work in the store) stays
 * the store's, with no library type (R-600).
 */

typedef struct Work Work;

/* `title` and `composer` may be NULL. BAD_ID for an invalid id. *out is untouched on failure. */
RealizationResult work_create(const char *id, const char *title, const char *composer, Work **out);
void work_destroy(Work *work);

/* The default ensemble template id of the work's movements; NULL clears. BAD_ID for an invalid id. */
RealizationResult work_set_ensemble(Work *work, const char *ensemble_id);

/* A material reference: ^[a-z]+(-[a-z]+)*:[A-Za-z0-9_.-]+$ (BAD_VALUE otherwise). Kept in order. */
RealizationResult work_add_material(Work *work, const char *reference);

/* Takes ownership on success only. DUPLICATE_ID when the work holds a movement with that id. Kept in order. */
RealizationResult work_add_movement(Work *work, Movement *movement);

const char *work_id(const Work *work);
const char *work_title(const Work *work);    /* NULL when none */
const char *work_composer(const Work *work); /* NULL when none */
const char *work_ensemble(const Work *work); /* NULL when none */
size_t work_material_count(const Work *work);
const char *work_material_at(const Work *work, size_t index);
size_t work_movement_count(const Work *work);
Movement *work_movement_at(const Work *work, size_t index);

/* The ensemble template id a movement is scored for: the movement's own, else the work's, else NULL. */
const char *work_movement_ensemble(const Work *work, const Movement *movement);

#endif
