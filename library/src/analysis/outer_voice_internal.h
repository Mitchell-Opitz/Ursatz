#ifndef MUSIC_ANALYSIS_OUTER_VOICE_INTERNAL_H
#define MUSIC_ANALYSIS_OUTER_VOICE_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/outer_voice.h"
#include "time/exact_rational.h"

/*
 * Internal to outer-voice extraction: the steps as separate functions so each has its own unit tests. Not
 * part of the public API (analysis/outer_voice.h is).
 */

/* One note, reduced to what the steps need. */
typedef struct {
    const char *id;       /* the note's EntityID value (borrowed) */
    ExactRational onset;
    ExactRational release;
    ExactRational pitch;  /* pitch height: the position value, under one tuning (never negative) */
} OuterNote;

/* One segment: its window and the notes sounding in it (indices into the OuterNote array). */
typedef struct {
    ExactRational start;
    ExactRational end;
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

#endif
