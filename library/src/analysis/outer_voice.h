#ifndef MUSIC_ANALYSIS_OUTER_VOICE_H
#define MUSIC_ANALYSIS_OUTER_VOICE_H

/*
 * Owner of outer-line extraction (outer_voice_extract) (docs/ARCHITECTURE.md, Analytic job owners).
 */

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "identity/entity_id.h"
#include "pitch/pitch_numeric_value.h"
#include "semantics/analysis_note.h"
#include "semantics/sonority_segmentation.h"
#include "time/meter_map.h"
#include "time/time_span.h"

/*
 * OuterVoice (analysis, theory-neutral): the soprano (highest) and bass (lowest) sounding lines of a
 * sequence of segments. It uses pitch HEIGHT only, by PitchNumericValue, and makes no theory claim: it knows
 * no key, chord or scale degree (common-practice-minimal reads the lines as scale degrees).
 *
 * Input is the adaptive sonority segments (semantics/sonority_segmentation.h: every note sounding in a
 * segment, attacked or held), the notes they name, and each note's PitchNumericValue as the importer returns
 * them (a NoteEvent's own pitch is opaque, so the values arrive as a parallel array). Heights are compared as
 * position values, so every note must share one tuning system; under the importer's MIDI tuning a unit of
 * position is a semitone, and register_guard_semitones is in those units. The beat grid is INJECTED as a
 * BeatGridProvider (as sonority segmentation takes it), with the MeterMap it reads: this file includes no
 * framework.
 *
 * A line has one entry per segment: {segment, the note, its pitch, the segment's TimeSpan}. An entry is
 * either PRESENT (a note) or ABSENT (none: the line is not sounding there). ABSENT is not the same as taking
 * whatever note happens to be highest: when the melody rests and only the accompaniment sounds, the
 * accompaniment's top note is not the soprano. Time with no sounding note at all has no segment and so no
 * entry.
 *
 * Extraction picks, for each segment, the highest sounding note for the soprano and the lowest for the bass.
 * Doublings (the same pitch) pick the note attacked most recently, then the lowest note id. Smoothing
 * (outer_voice.c, configurable below) then turns some highest or lowest notes into ABSENT entries:
 *   1. Gap bridging: an absent entry in a short gap (under gap_bridge_max beats, between two present notes of
 *      its line) is marked bridged: a rest inside one line, never filled from another voice.
 *   2. Register guard (soprano): if the highest note of a segment falls more than register_guard_semitones
 *      BELOW the previous soprano note, and that note ended less than gap_bridge_max beats before the
 *      segment, the soprano is absent in it rather than adopting an accompaniment note.
 *   3. Register guard (bass): the mirror, for a lowest note more than that far ABOVE the previous bass.
 * Beat length is the length of the beat at the gap's start, from the beat grid.
 */

typedef struct {
    unsigned long gap_bridge_max_numerator;   /* the short-gap limit, in beats (default 1/1) */
    unsigned long gap_bridge_max_denominator;
    int register_guard_semitones;             /* default 12 */
    bool bridge_gaps;                         /* rule 1: mark bridged rests (default true) */
    bool guard_soprano_register;              /* rule 2 (default true) */
    bool guard_bass_register;                 /* rule 3 (default true) */
} OuterVoiceConfig;

/* gap_bridge_max 1 beat, register_guard_semitones 12, every rule on. */
OuterVoiceConfig outer_voice_config_default(void);

typedef struct {
    size_t segment_index;
    bool present;
    bool bridged;               /* an absent entry inside a short gap (rule 1) */
    bool guarded;               /* absent because a register guard dropped it (rules 2 and 3) */
    EntityID *note_id;          /* owned; NULL when absent */
    PitchNumericValue *pitch;   /* owned; NULL when absent */
    TimeSpan *span;             /* owned: the segment's window */
} OuterVoiceEntry;

typedef struct {
    OuterVoiceEntry *soprano; /* owned; count entries */
    OuterVoiceEntry *bass;    /* owned; count entries */
    size_t count;             /* the number of segments */
} OuterVoiceLines;

typedef enum {
    OUTER_VOICE_OK = 0,
    OUTER_VOICE_ERR_NULL_ARGUMENT,
    OUTER_VOICE_ERR_NULL_NOTE,
    OUTER_VOICE_ERR_BAD_CONFIG,
    OUTER_VOICE_ERR_MIXED_TUNING,
    OUTER_VOICE_ERR_UNSUPPORTED_PITCH,
    OUTER_VOICE_ERR_UNKNOWN_NOTE,
    OUTER_VOICE_ERR_BEAT_GRID,
    OUTER_VOICE_ERR_METER_MAP,
    OUTER_VOICE_ERR_OVERFLOW,
    OUTER_VOICE_ERR_ALLOC
} OuterVoiceResult;

/*
 * Extracts both lines. notes and pitch_values are parallel (note_count entries); config may be NULL for the
 * defaults. UNSUPPORTED_PITCH for a negative pitch position, MIXED_TUNING for notes in different tuning
 * systems, UNKNOWN_NOTE for a segment member that is not among the notes. On OK, *out owns its entries
 * (outer_voice_lines_destroy); otherwise *out is untouched.
 */
OuterVoiceResult outer_voice_extract(const AdaptiveSonorities *segments, NoteEvent *const *notes,
                                      PitchNumericValue *const *pitch_values, size_t note_count, const MeterMap *map,
                                      BeatGridProvider provider, void *provider_context,
                                      const OuterVoiceConfig *config, OuterVoiceLines *out);

/* Destroys every entry's note id, pitch and span, and the arrays. NULL is a no-op. */
void outer_voice_lines_destroy(OuterVoiceLines *lines);

/*
 * The soprano at every attack: indices into notes->notes of the line that, at each distinct onset, is the
 * highest note sounding then (the later attack among equal pitches), kept when it is attacked at that onset.
 * Notes of unknown pitch (ANALYSIS_PITCH_UNKNOWN) are skipped. notes must be a valid, sorted AnalysisNoteSet
 * (analysis_note_set_valid), else OUTER_VOICE_ERR_NULL_NOTE. On OK, *out is owned by the caller (free) and
 * NULL with *count 0 when there is no soprano; otherwise *out and *count are untouched. Overflow of the
 * exact times is OUTER_VOICE_ERR_OVERFLOW.
 */
OuterVoiceResult outer_voice_attack_soprano(const AnalysisNoteSet *notes, size_t **out, size_t *count);

#endif
