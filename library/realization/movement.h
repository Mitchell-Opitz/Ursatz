#ifndef URSATZ_REALIZATION_MOVEMENT_H
#define URSATZ_REALIZATION_MOVEMENT_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/region.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/tempo_map.h"
#include "model/time/time_span.h"
#include "realization/line.h"
#include "realization/performer_part.h"
#include "realization/realization.h"
#include "realization/scoring.h"

/*
 * A movement (target architecture 4.8, R-531, R-599, R-600): the unit that owns its meter map and its tempo map and holds lines,
 * scoring assignments and performer parts, so a movement of 3/4 and one of 4/4 sit in one work without sharing a map. It also holds
 * its key signatures and markers in time order, the span it plays (a repeated section is a played span: none means the written order
 * once), compound parts (a minuet and its trio), an optional home region and the id of the ensemble template it is scored for (none:
 * the work's). The movement owns the maps, the lines and the parts it is given and frees them with itself.
 */

typedef enum {
    MOVEMENT_KEY_MAJOR,
    MOVEMENT_KEY_MINOR,
    MOVEMENT_KEY_UNKNOWN
} MovementKeyMode;

typedef struct {
    MusicalTime position;
    int sharps_flats; /* -7 to 7 */
    MovementKeyMode mode;
} MovementKeySignature;

typedef struct {
    MusicalTime position;
    char text[64];
} MovementMarker;

/* A compound part of a movement: a minuet, a trio. */
typedef struct {
    char id[REALIZATION_ID_MAX];
    char name[64];
    TimeSpan span;
    bool has_home;
    Region home;
} MovementPart;

typedef struct Movement Movement;

/* `title` may be NULL. BAD_ID for an invalid id. *out is untouched on failure. */
RealizationResult movement_create(const char *id, const char *title, Movement **out);
void movement_destroy(Movement *movement);

/* Take ownership of the map and free the one held before; NULL clears. On a failure the caller keeps the map. */
RealizationResult movement_set_meter_map(Movement *movement, MeterMap *map);
RealizationResult movement_set_tempo_map(Movement *movement, TempoMap *map);

RealizationResult movement_set_home(Movement *movement, Region home);

/* The id of the ensemble template the movement is scored for; NULL inherits the work's. BAD_ID for an invalid id. */
RealizationResult movement_set_ensemble(Movement *movement, const char *ensemble_id);

/* Key signatures: sharps_flats -7 to 7, a mode in range, positions from 0 and not decreasing (BAD_VALUE). */
RealizationResult movement_add_key_signature(Movement *movement, const MovementKeySignature *key);

/* Markers: a position from 0, non-empty text of at most 63 characters (BAD_VALUE). */
RealizationResult movement_add_marker(Movement *movement, const MovementMarker *marker);

/* A span the movement plays, in order; a span that holds (BAD_VALUE). None means the written order once. */
RealizationResult movement_add_played_span(Movement *movement, TimeSpan span);

/* A compound part: a valid id (BAD_ID) unique in the movement (DUPLICATE_ID), a span that holds (BAD_VALUE). */
RealizationResult movement_add_compound_part(Movement *movement, const MovementPart *part);

/* Take ownership on success only (the caller keeps the line or part on a failure). BAD_ID cannot happen (the id was checked when it
   was made); DUPLICATE_ID when the movement holds that id. */
RealizationResult movement_add_line(Movement *movement, Line *line);
RealizationResult movement_add_part(Movement *movement, PerformerPart *part);

/* Copies an assignment in: scoring_assignment_check, and DUPLICATE_ID for an id the movement holds. The line and the part it names
   are checked when the movement is scored. */
RealizationResult movement_add_assignment(Movement *movement, const ScoringAssignment *assignment);

/* realization_score over the movement's own lines, assignments and parts. */
RealizationResult movement_score(Movement *movement, const char *run);

const char *movement_id(const Movement *movement);
const char *movement_title(const Movement *movement); /* NULL when none */
const MeterMap *movement_meter_map(const Movement *movement);
const TempoMap *movement_tempo_map(const Movement *movement);
bool movement_home(const Movement *movement, Region *out); /* false when none (out untouched) */
const char *movement_ensemble(const Movement *movement);   /* NULL when it inherits the work's */

size_t movement_key_signature_count(const Movement *movement);
const MovementKeySignature *movement_key_signature_at(const Movement *movement, size_t index);
size_t movement_marker_count(const Movement *movement);
const MovementMarker *movement_marker_at(const Movement *movement, size_t index);
size_t movement_played_span_count(const Movement *movement);
const TimeSpan *movement_played_span_at(const Movement *movement, size_t index);
size_t movement_compound_part_count(const Movement *movement);
const MovementPart *movement_compound_part_at(const Movement *movement, size_t index);
size_t movement_line_count(const Movement *movement);
Line *movement_line_at(const Movement *movement, size_t index);
Line *movement_line_named(const Movement *movement, const char *id);
size_t movement_assignment_count(const Movement *movement);
const ScoringAssignment *movement_assignment_at(const Movement *movement, size_t index);
size_t movement_part_count(const Movement *movement);
PerformerPart *movement_part_at(const Movement *movement, size_t index);
PerformerPart *movement_part_named(const Movement *movement, const char *id);

#endif
