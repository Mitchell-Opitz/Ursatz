#ifndef MUSIC_ANALYSIS_OUTER_VOICE_INTERNAL_H
#define MUSIC_ANALYSIS_OUTER_VOICE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/outer_voice.h"
#include "core/rational.h"
#include "time/time_rational.h"

/*
 * Internal to outer-voice extraction: the steps as separate functions so each has its own unit tests. Not
 * part of the public API (analysis/outer_voice.h is).
 */

/* One note, reduced to what the steps need. */
typedef struct {
    const char *id;       /* the note's EntityID value (borrowed) */
    Rational onset;
    Rational release;
    Rational pitch;  /* pitch height: the position value, under one tuning (never negative) */
} OuterNote;

/* One segment: its window and the notes sounding in it (indices into the OuterNote array). */
typedef struct {
    Rational start;
    Rational end;
    const size_t *members;
    size_t member_count;
} OuterSegment;

/* A line's entry for one segment while it is being built. */
typedef struct {
    bool present;
    bool bridged;
    bool guarded;
    size_t note; /* the OuterNote index when present */
} OuterNode;

/*
 * Extraction: for each segment, the highest (soprano) or lowest (bass) sounding note; an empty segment is
 * absent. Doublings pick the note attacked most recently, then the lowest id. nodes has segment_count
 * entries.
 */
void outer_voice_pick_line(const OuterNote *notes, const OuterSegment *segments, size_t segment_count,
                           bool soprano, OuterNode *nodes);

/*
 * The length of the beat at time `at`, from the beat grid (injected). False on failure. Smoothing measures
 * gaps in beats with it, so a gap is "short" by the beat of the meter it falls in (a dotted-quarter in 6/8).
 */
typedef bool (*OuterBeatLength)(void *context, Rational at, Rational *out_beat);

/*
 * Rules 2 and 3, the register guard. For a segment whose picked note falls more than `semitones` away from
 * the line's previous present note in the wrong direction (below it for the soprano, above it for the bass),
 * while that note ended less than `window_beats` beats before the segment, the segment is made absent and
 * marked guarded, rather than adopting an accompaniment note. The previous note is the last present entry
 * after earlier guarding, so a run of accompaniment-only segments stays absent until the window passes.
 * `semitones` is in pitch position units. nodes is edited in place.
 */
OuterVoiceResult outer_voice_guard_register(const OuterNote *notes, const OuterSegment *segments,
                                             size_t segment_count, bool soprano, int semitones,
                                             Rational window_beats, OuterBeatLength beat_length,
                                             void *beat_context, OuterNode *nodes);

/*
 * Rule 1, gap bridging. A run of absent entries between two present entries is a gap: from the end of the
 * earlier note (or the first absent segment's start, if later) to the start of the next present segment. If
 * the gap is under `window_beats` beats the run is marked bridged: a rest inside one line, never filled from
 * another voice. Runs at either end of the line are not bridged.
 */
OuterVoiceResult outer_voice_mark_bridged_gaps(const OuterNote *notes, const OuterSegment *segments,
                                                size_t segment_count, Rational window_beats,
                                                OuterBeatLength beat_length, void *beat_context,
                                                OuterNode *nodes);

#endif
