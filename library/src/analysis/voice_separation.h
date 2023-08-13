#ifndef MUSIC_ANALYSIS_VOICE_SEPARATION_H
#define MUSIC_ANALYSIS_VOICE_SEPARATION_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_numeric_value.h"

/*
 * VoiceSeparation (analysis, theory-neutral): assigns each note of a stream to a voice by pitch HEIGHT
 * (PitchNumericValue) and time alone. It makes no theory claim and knows no key, chord or framework. A voice
 * here is an index (0 = top), not a Voice (L4) object: a later caller builds those from the assignment.
 *
 * Streams. The caller says which stream each note belongs to (stream_of_note, parallel to the notes; NULL puts
 * every note in stream 0). Streams are separated independently. The recommended setup is one stream per
 * importer track, since a track is usually a hand; one stream for the whole file is the "single stream" mode.
 * The kernel does not read the importer's per-track Voice (which stays as it is).
 *
 * Algorithm, per stream (each parameter in VoiceSeparationConfig):
 *   1. The voice count is the most notes sounding at once, capped at max_voices.
 *   2. Voices are ordered top to bottom and, unless allow_crossing, may not cross.
 *   3. At each onset, the attacked notes are assigned to the free voices (a voice holding a sounding note is
 *      not free: a held note keeps its voice) so that the total pitch distance from each voice's last pitch is
 *      least. Without allow_crossing the attacked notes keep their pitch order, and a note that would sit on
 *      the wrong side of a held note is counted as a violation, which ranks before distance (so a crossing is
 *      chosen only when no assignment avoids one).
 *   4. A voice with no note to take rests. A voice that has never sounded is measured from its nominal pitch:
 *      the stream's pitch range is split into equal bands, one per voice, top to bottom.
 *   5. More attacked notes than free voices: the extra notes join the nearest voice as chord members and are
 *      flagged. The extras are chosen by repeatedly dropping the lower note of the closest pair.
 *   6. Ties go to the lower total distance, then to the more recently sounding voices (keep the previous
 *      voice), then to the lower note id.
 *
 * confidence (in [0, 1]) comes from the cost margin of the assignment of one onset: (second best - best) /
 * second best, 1 when it was the only assignment, 0 when two assignments tie. Every note attacked at one onset
 * shares it.
 *
 * Pitch heights are compared as position values, so every note must share one tuning system.
 */

typedef struct {
    size_t max_voices;     /* 1..8, default 4 */
    bool allow_crossing;   /* default false */
} VoiceSeparationConfig;

/* max_voices 4, no crossing. */
VoiceSeparationConfig voice_separation_config_default(void);

typedef struct {
    EntityID *note_id;     /* owned: a copy of the note's id */
    size_t stream;
    size_t voice;          /* 0 = top */
    double confidence;     /* in [0, 1] */
    bool chord_member;     /* an extra note joined this voice (more notes than free voices) */
} VoiceAssignmentEntry;

typedef struct {
    size_t stream;
    size_t voice_count;
} VoiceStreamCount;

typedef struct {
    VoiceAssignmentEntry *entries; /* owned; one per input note, in input order */
    size_t count;
    VoiceStreamCount *streams;     /* owned; one per stream, by ascending stream */
    size_t stream_count;
} VoiceAssignment;

typedef enum {
    VOICE_SEPARATION_OK = 0,
    VOICE_SEPARATION_ERR_NULL_ARGUMENT,
    VOICE_SEPARATION_ERR_NULL_NOTE,
    VOICE_SEPARATION_ERR_BAD_CONFIG,
    VOICE_SEPARATION_ERR_MIXED_TUNING,
    VOICE_SEPARATION_ERR_UNSUPPORTED_PITCH,
    VOICE_SEPARATION_ERR_OVERFLOW,
    VOICE_SEPARATION_ERR_ALLOC
} VoiceSeparationResult;

/*
 * Separates the notes. notes and pitch_values are parallel (note_count entries); stream_of_note is NULL or
 * parallel too; config may be NULL for the defaults. BAD_CONFIG for max_voices outside 1..8,
 * UNSUPPORTED_PITCH for a negative pitch or time. On OK, *out owns its entries (voice_assignment_destroy);
 * otherwise *out is untouched.
 */
VoiceSeparationResult voice_separate(NoteEvent *const *notes, PitchNumericValue *const *pitch_values,
                                      size_t note_count, const size_t *stream_of_note,
                                      const VoiceSeparationConfig *config, VoiceAssignment *out);

/* Destroys every entry's note id and the arrays. NULL is a no-op. */
void voice_assignment_destroy(VoiceAssignment *assignment);

#endif
