#ifndef URSATZ_REALIZATION_LINE_H
#define URSATZ_REALIZATION_LINE_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_spelling.h"
#include "model/time/duration.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"
#include "realization/derivation.h"
#include "realization/realization.h"

/*
 * A line (target architecture 4.8, R-600): one voice of the music as composed, a sequence of notes that belongs to no instrument.
 * Scoring assignments (realization/scoring.h) place lines into performer parts. A line has an id, a role, an optional name, its
 * notes in insertion order (each with an id unique in the line, an onset, a duration, a MIDI number and optionally a spelling) and
 * relations to other lines by id. The line keeps copies of what it is given.
 */

typedef enum {
    LINE_ROLE_MELODY,
    LINE_ROLE_COUNTER_MELODY,
    LINE_ROLE_HARMONY_LINE,
    LINE_ROLE_BASS,
    LINE_ROLE_CHORDS,
    LINE_ROLE_PAD,
    LINE_ROLE_ACCOMPANIMENT,
    LINE_ROLE_DRUMS,
    LINE_ROLE_PEDAL,
    LINE_ROLE_SUBJECT,
    LINE_ROLE_ANSWER,
    LINE_ROLE_COUNTERSUBJECT
} LineRole;

/* "melody", "counter-melody", "harmony-line", "bass", "chords", "pad", "accompaniment", "drums", "pedal", "subject", "answer",
   "countersubject"; NULL for a value out of range. */
const char *line_role_name(LineRole role);

/* The role of a name; false (and *out untouched) for an unknown name. */
bool line_role_from_name(const char *name, LineRole *out);

typedef struct {
    char id[REALIZATION_ID_MAX];
    MusicalTime onset;
    Duration duration;
    int midi;
    bool spelled; /* letter and accidental hold the written name; a spelled accidental is -2 to 2 */
    PitchLetter letter;
    int accidental;
    NoteDerivation derivation;
} LineNote;

typedef enum {
    LINE_RELATION_PARALLEL,
    LINE_RELATION_IMITATION,
    LINE_RELATION_ANSWERS,
    LINE_RELATION_CALL_RESPONSE
} LineRelationKind;

typedef struct {
    LineRelationKind kind;
    char target[REALIZATION_ID_MAX]; /* the id of another line */
    int interval;                    /* in semitones, as the relation uses it */
    Duration delay;
    bool has_span;
    TimeSpan span;
} LineRelation;

typedef struct Line Line;

/* Makes an empty line. `name` may be NULL (no name). BAD_ID for an invalid id, BAD_VALUE for a role out of range or a name of more
   than 63 characters. *out is untouched on failure. */
RealizationResult line_create(const char *id, LineRole role, const char *name, Line **out);
void line_destroy(Line *line);

/* Copies a note in. BAD_ID (invalid id), DUPLICATE_ID (the line holds it), BAD_VALUE (onset below 0, duration not above 0, midi
   outside 0 to 127, a spelled letter or accidental out of range, an invalid derivation). */
RealizationResult line_add_note(Line *line, const LineNote *note);

/* Copies a relation in. BAD_ID (invalid target), BAD_VALUE (the line's own id as target, a kind out of range, a span that does not
   hold). */
RealizationResult line_add_relation(Line *line, const LineRelation *relation);

const char *line_id(const Line *line);
LineRole line_role(const Line *line);
const char *line_name(const Line *line); /* "" when the line has none */
size_t line_note_count(const Line *line);
const LineNote *line_note_at(const Line *line, size_t index); /* insertion order; NULL out of range */
const LineNote *line_note_named(const Line *line, const char *note_id); /* NULL when absent */
size_t line_relation_count(const Line *line);
const LineRelation *line_relation_at(const Line *line, size_t index);

#endif
