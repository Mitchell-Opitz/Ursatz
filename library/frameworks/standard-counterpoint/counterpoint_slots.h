#ifndef URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_SLOTS_H
#define URSATZ_FRAMEWORKS_STANDARD_COUNTERPOINT_COUNTERPOINT_SLOTS_H

#include <stddef.h>

#include "pitch/pitch_spelling.h"

/*
 * Slots for the species with more than one note against the cantus firmus
 * note. There is no rhythm model, so a counterpoint is a list of slots, a
 * fixed number to a bar, one bar for each note of the cantus firmus. Which
 * slots are strong, medium, or weak, and which shapes are valid, depend on
 * the species (second_species_exercise.h, third_species_exercise.h).
 *
 * A slot is one of:
 *
 *   ATTACK   a new note, with its pitch
 *   TIE      a continuation of the previous slot's note, with that same
 *            pitch: a whole note is an attack followed by ties
 *   REST     silence, with no pitch
 *
 * Pitches are borrowed, not owned.
 */

typedef enum {
    STANDARD_COUNTERPOINT_SLOT_ATTACK = 0,
    STANDARD_COUNTERPOINT_SLOT_TIE,
    STANDARD_COUNTERPOINT_SLOT_REST
} StandardCounterpointSlotKind;

typedef struct {
    StandardCounterpointSlotKind kind;
    const PitchSpelling *pitch;
} StandardCounterpointSlot;

typedef struct {
    const StandardCounterpointSlot *slots;
    size_t count;
} StandardCounterpointSlotLine;

#endif
