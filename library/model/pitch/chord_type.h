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

#define CHORD_TYPE_MEMBER_MAX 8

typedef struct {
    const char *id;
    const char *name;
    size_t member_count;
    struct {
        ChordMemberRole role;
        int semitones;
    } members[CHORD_TYPE_MEMBER_MAX];
} ChordType;

#endif
