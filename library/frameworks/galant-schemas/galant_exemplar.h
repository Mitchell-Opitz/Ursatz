#ifndef URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_EXEMPLAR_H
#define URSATZ_FRAMEWORKS_GALANT_SCHEMAS_GALANT_EXEMPLAR_H

#include <stdbool.h>
#include <stddef.h>

#include "pitch/pitch_spelling.h"
#include "semantics/beat_strength.h"

/*
 * An exemplar for galant-schemas-v1: a single pattern that may
 * resemble a prototype. The caller says which notes are its stages; finding
 * the stages in a passage is not done here. Each stage is the pitch of the top
 * (melodic) voice, the pitch of the bottom (bass) voice, and the strength of the
 * beat the stage falls on (BeatStrength: strong "S" or weak "W"). A stage whose
 * melody has two notes (the Monte's do-te and re-do) also has a second melody
 * pitch, `melody_next`, and a stage with one has NULL there. The tonic is
 * the key the degrees are counted from. Pitches are borrowed, not owned, and
 * read as sounding pitches.
 */

#define GALANT_MAX_STAGES 8

typedef struct {
    const PitchSpelling *melody;
    const PitchSpelling *bass;
    BeatStrength strength;
    const PitchSpelling *melody_next;
} GalantExemplarStage;

typedef struct {
    const GalantExemplarStage *stages;
    size_t stage_count;
    const PitchSpelling *tonic;
} GalantExemplar;

/* True when the exemplar has one to GALANT_MAX_STAGES stages, every melody and
   bass pitch and the tonic non-NULL, and every strength a valid BeatStrength. */
bool galant_exemplar_valid(const GalantExemplar *exemplar);

#endif
