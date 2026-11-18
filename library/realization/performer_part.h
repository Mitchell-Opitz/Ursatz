#ifndef URSATZ_REALIZATION_PERFORMER_PART_H
#define URSATZ_REALIZATION_PERFORMER_PART_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_spelling.h"
#include "model/time/duration.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"
#include "realization/derivation.h"
#include "realization/realization.h"

/*
 * A performer part (target architecture 4.11, R-534, R-541): the notes one performer, or one group of players of an instrument,
 * reads: an id, the instrument entry it is for (by id), a name, a number among the parts of that instrument, the player count, the
 * number of staves, the General MIDI program, notes with a staff and a voice each, sustain pedal markings and an optional local meter
 * map. The part takes plain values, so the caller copies them from the K12 entry and this header needs no theory include. It keeps
 * copies of the notes it is given and owns its local meter map.
 */

typedef struct {
    char id[REALIZATION_ID_MAX];
    MusicalTime onset;
    Duration duration;
    int midi;
    bool spelled; /* letter and accidental hold the written name; a spelled accidental is -2 to 2 */
    PitchLetter letter;
    int accidental;
    int staff; /* 1 to the part's staff count */
    int voice; /* from 1 */
    NoteDerivation derivation;
    bool edited;   /* a person changed the note: scoring keeps it */
    bool detached; /* a person cut the note from its line: scoring keeps it */
    bool review;   /* scoring found the note differs from what its source now gives */
} PerformerNote;

typedef enum {
    PEDAL_SUSTAIN
} PedalKind;

typedef struct {
    PedalKind kind;
    TimeSpan span;
} PedalMarking;

typedef struct PerformerPart PerformerPart;

/*
 * Makes an empty part. `id` and `instrument_id` are valid ids (BAD_ID), `name` a non-empty text of at most 63 characters, `number` and
 * `players` from 1, `staff_count` 1 to 4, `midi_program` 0 to 127 (BAD_VALUE). *out is untouched on failure.
 */
RealizationResult performer_part_create(const char *id, const char *instrument_id, const char *name, int number, int players,
                                        int staff_count, int midi_program, PerformerPart **out);
void performer_part_destroy(PerformerPart *part);

/* Copies a note in. BAD_ID, DUPLICATE_ID (the part holds the id), BAD_VALUE (staff outside 1 to the staff count, voice below 1, midi
   outside 0 to 127, onset below 0, duration not above 0, a spelled letter or accidental out of range, an invalid derivation). */
RealizationResult performer_part_add_note(PerformerPart *part, const PerformerNote *note);

/* Sets the edited and detached flags of the note with this id. UNKNOWN_REFERENCE when the part has no such note. */
RealizationResult performer_part_set_note_flags(PerformerPart *part, const char *note_id, bool edited, bool detached);

/* Adds a pedal marking, kept in start order. BAD_VALUE for a kind out of range, a span that does not hold, a start below 0 or an
   overlap with another marking of the part. */
RealizationResult performer_part_add_pedal(PerformerPart *part, const PedalMarking *pedal);

/* Takes ownership of `map` and frees the one held before; NULL clears. On ERR_NULL (no part) the caller keeps `map`. */
RealizationResult performer_part_set_local_meter_map(PerformerPart *part, MeterMap *map);

const char *performer_part_id(const PerformerPart *part);
const char *performer_part_instrument(const PerformerPart *part);
const char *performer_part_name(const PerformerPart *part);
int performer_part_number(const PerformerPart *part);
int performer_part_players(const PerformerPart *part);
int performer_part_staff_count(const PerformerPart *part);
int performer_part_midi_program(const PerformerPart *part);
size_t performer_part_note_count(const PerformerPart *part);
const PerformerNote *performer_part_note_at(const PerformerPart *part, size_t index); /* NULL out of range */
size_t performer_part_pedal_count(const PerformerPart *part);
const PedalMarking *performer_part_pedal_at(const PerformerPart *part, size_t index);
const MeterMap *performer_part_local_meter_map(const PerformerPart *part); /* NULL when none */

#endif
