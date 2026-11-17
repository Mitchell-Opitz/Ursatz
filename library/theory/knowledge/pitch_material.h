#ifndef URSATZ_THEORY_KNOWLEDGE_PITCH_MATERIAL_H
#define URSATZ_THEORY_KNOWLEDGE_PITCH_MATERIAL_H

#include <stdbool.h>
#include <stddef.h>

/*
 * K1 pitch material (target architecture 5.2), unordered: a collection of pitch classes above a center, as the knowledge entry
 * gives it. `steps` are the semitones above the center, ascending from 0. A material with a different descending form also has
 * `descending_steps` (also ascending from 0); descending_step_count is 0 when the entry gives none. A snapshot owns it; the strings
 * live as long as the snapshot.
 */
typedef struct {
    const char *id;
    const char *name;
    size_t step_count;
    int steps[12];
    size_t descending_step_count;
    int descending_steps[12];
} PitchMaterial;

/* The semitones above the center of degree `degree` (1 to step_count), or -1 for another degree or a NULL material. */
int pitch_material_step(const PitchMaterial *material, int degree);

/* True when `semitones_above_center` (any integer, reduced to 0 to 11) is a step of the material. */
bool pitch_material_contains(const PitchMaterial *material, int semitones_above_center);

#endif
