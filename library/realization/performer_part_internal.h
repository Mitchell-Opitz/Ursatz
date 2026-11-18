#ifndef URSATZ_REALIZATION_PERFORMER_PART_INTERNAL_H
#define URSATZ_REALIZATION_PERFORMER_PART_INTERNAL_H

#include "realization/performer_part.h"

/* The part's fields, for the files of the realization group that write its notes in one step (scoring.c). Never installed. */
struct PerformerPart {
    char id[REALIZATION_ID_MAX];
    char instrument[REALIZATION_ID_MAX];
    char name[64];
    int number;
    int players;
    int staff_count;
    int midi_program;
    PerformerNote *notes;
    size_t note_count;
    size_t note_capacity;
    PedalMarking *pedals;
    size_t pedal_count;
    size_t pedal_capacity;
    MeterMap *local_meter_map;
};

#endif
