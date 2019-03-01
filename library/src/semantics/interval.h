#ifndef MUSIC_SEMANTICS_INTERVAL_H
#define MUSIC_SEMANTICS_INTERVAL_H

#include <stdbool.h>

#include "pitch/pitch_identity.h"

/*
 * Interval (Value): directed/undirected/chromatic distance between a
 * PitchIdentity pair (TDD §67).
 *
 * PitchClass and a tuning system (Phase 3, Deliverable D) do not exist
 * yet, so this type does not compute distance itself -- PitchIdentity
 * is an opaque token (pitch/pitch_identity.h) with no numeric
 * semantics to derive a distance from. Instead, each dimension is
 * supplied by the caller (whoever has the tuning-system knowledge to
 * measure it) as an independently-optional field. A dimension that was
 * not supplied stays unknown -- interval_has_* reports that, rather
 * than the value silently defaulting to zero.
 *
 * Dimensions represented (the subset of TDD §67 knowable without
 * PitchClass): directed (signed semitone displacement, sign carries
 * direction), undirected (unsigned semitone magnitude), and chromatic
 * distance (unsigned chromatic step count). Diatonic distance requires
 * scale-degree/PitchClass information and is not represented here.
 *
 * When both directed and undirected are supplied, they MUST be
 * consistent (undirected == |directed|) -- construction rejects an
 * inconsistent pair rather than silently preferring one.
 *
 * pitch_a/pitch_b are referenced, not owned -- interval_destroy never
 * destroys them.
 */

typedef struct Interval Interval;

typedef enum {
    INTERVAL_OK = 0,
    INTERVAL_ERR_NULL_PITCH,
    INTERVAL_ERR_INCONSISTENT_DIRECTED_UNDIRECTED,
    INTERVAL_ERR_ALLOC
} IntervalResult;

IntervalResult interval_create(const PitchIdentity *pitch_a, const PitchIdentity *pitch_b,
                                bool has_directed_semitones, long directed_semitones,
                                bool has_undirected_semitones, unsigned long undirected_semitones,
                                bool has_chromatic_distance, unsigned long chromatic_distance, Interval **out);
void interval_destroy(Interval *interval);

const PitchIdentity *interval_pitch_a(const Interval *interval);
const PitchIdentity *interval_pitch_b(const Interval *interval);

bool interval_has_directed_semitones(const Interval *interval);
long interval_directed_semitones(const Interval *interval);

bool interval_has_undirected_semitones(const Interval *interval);
unsigned long interval_undirected_semitones(const Interval *interval);

bool interval_has_chromatic_distance(const Interval *interval);
unsigned long interval_chromatic_distance(const Interval *interval);

#endif