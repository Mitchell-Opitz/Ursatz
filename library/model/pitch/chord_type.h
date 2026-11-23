#ifndef URSATZ_MODEL_PITCH_CHORD_TYPE_H
#define URSATZ_MODEL_PITCH_CHORD_TYPE_H

#include <stddef.h>

/*
 * K2 chord type (target architecture 4.4 and 5.2): the members of a chord above its root, as the knowledge entry gives them. The
 * first member is the root at 0 semitones. A member beyond the octave (a ninth is 14) keeps its semitones as the entry gives them.
 * A snapshot owns it; the strings live as long as the snapshot.
 */
typedef enum {
    CHORD_MEMBER_ROOT,
    CHORD_MEMBER_THIRD,
    CHORD_MEMBER_FIFTH,
    CHORD_MEMBER_SEVENTH,
    CHORD_MEMBER_NINTH,
    CHORD_MEMBER_ELEVENTH,
    CHORD_MEMBER_THIRTEENTH,
    CHORD_MEMBER_ADDED_SECOND,
    CHORD_MEMBER_ADDED_SIXTH,
    CHORD_MEMBER_SUSPENDED_FOURTH
} ChordMemberRole;

/* How the members are built up: stacked thirds, fourths or seconds, a triad with added notes, two chords, a mix, a cluster, a power chord. */
typedef enum {
    CHORD_CONSTRUCTION_TERTIAN = 0,
    CHORD_CONSTRUCTION_QUARTAL,
    CHORD_CONSTRUCTION_SECUNDAL,
    CHORD_CONSTRUCTION_ADDED_NOTE,
    CHORD_CONSTRUCTION_POLYCHORD,
    CHORD_CONSTRUCTION_COMPOUND,
    CHORD_CONSTRUCTION_CLUSTER,
    CHORD_CONSTRUCTION_POWER
} ChordConstruction;

#define CHORD_TYPE_MEMBER_MAX 8

typedef struct {
    const char *id;
    const char *name;
    size_t member_count;
    struct {
        ChordMemberRole role;
        int semitones;
    } members[CHORD_TYPE_MEMBER_MAX];
    ChordConstruction construction; /* the entry's content.construction (tertian when it gives none) */
} ChordType;

#endif
