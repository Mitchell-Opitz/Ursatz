#ifndef URSATZ_THEORY_KNOWLEDGE_VOICING_ENTRY_H
#define URSATZ_THEORY_KNOWLEDGE_VOICING_ENTRY_H

#include <stddef.h>

#include "model/pitch/chord_type.h"

/*
 * K3 voicing (target architecture 4.4 and 5.2): how the members of a chord are spread over the registers, as the knowledge entry
 * gives it. The members are roles and semitones above the root, ascending from the root at 0, up to 35 (a shell holds the third, the
 * seventh and sometimes the ninth, and leaves the fifth out). `fits` are the chord types the voicing can sound. A snapshot owns it;
 * the strings and the pointed-to chord types live as long as the snapshot.
 */
typedef struct {
    const char *id;
    const char *name;
    size_t member_count;
    struct {
        ChordMemberRole role;
        int semitones;
    } members[CHORD_TYPE_MEMBER_MAX];
    size_t fit_count;
    const ChordType *fits[CHORD_TYPE_MEMBER_MAX];
} VoicingEntry;

#endif
