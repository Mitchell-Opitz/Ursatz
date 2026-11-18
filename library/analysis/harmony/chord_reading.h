#ifndef URSATZ_ANALYSIS_HARMONY_CHORD_READING_H
#define URSATZ_ANALYSIS_HARMONY_CHORD_READING_H

#include <stdbool.h>

#include "model/pitch/chord_type.h"
#include "model/pitch/pitch_material.h"
#include "model/time/time_span.h"
#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The sonority is the base harmonic object and a chord is a reading of it (target architecture 4.4 and 5.2, R-592).
 *
 * SonorityPitches are the pitches sounding over a span: the 12-bit mask of their pitch classes (bit p is pitch class p) and the pitch
 * class of the lowest one, or -1 when it is not known. An aggregate that no chord reading explains is a valid sonority.
 *
 * A ChordReading is one reading of a sonority: a root, the chord type the snapshot gives for the pitch classes above that root, the
 * members of the type by role (a 12-bit pitch-class mask for each ChordMemberRole, indexed by the role), the bass, and what the reading
 * says about its place: `borrowed_from` is the material a chord is borrowed from (NULL for none), `role` is FUNCTIONAL for a chord that
 * carries a harmony and EMBELLISHING for one that decorates another (a cadential six-four), and an `expected` chord is the chord a
 * reading elides (has_expected false for none). A reading with no root (has_root false) has no type, no members and no bass role: it is
 * the aggregate left as it is.
 *
 * `type` is NULL when the snapshot has no chord type for the pitch classes above the root. The type is borrowed from the snapshot.
 */

typedef struct {
    TimeSpan span;
    unsigned pitch_classes;
    int bass_pitch_class;
} SonorityPitches;

typedef enum {
    CHORD_ROLE_FUNCTIONAL,
    CHORD_ROLE_EMBELLISHING
} ChordRole;

/* The number of chord member roles: the size of `members_by_role`. */
#define CHORD_READING_ROLE_COUNT 10

typedef struct {
    SonorityPitches sonority;
    bool has_root;
    int root_pitch_class;
    const ChordType *type;
    unsigned members_by_role[CHORD_READING_ROLE_COUNT];
    int bass_pitch_class;
    const PitchMaterial *borrowed_from;
    ChordRole role;
    bool has_expected;
    int expected_root_pitch_class;
    const ChordType *expected_type;
} ChordReading;

/*
 * Reads `sonority` as a chord on `root` (0 to 11), or as no chord when `root` is negative. The type is the snapshot's chord type whose
 * members above the root equal the sonority's pitch classes above it (the root itself is ignored: knowledge_snapshot_chord_type_of); the
 * members by role are the pitch classes of the type's members on the root, in the roles the type gives them. `*out` starts from its
 * defaults (no type, no members, NULL borrowed_from, FUNCTIONAL, no expected chord) and the bass is the sonority's. False, `*out`
 * untouched, for a NULL argument, a root above 11, pitch classes outside the 12 bits or a bass above 11 or below -1.
 */
bool chord_reading_of_sonority(const KnowledgeSnapshot *snapshot, SonorityPitches sonority, int root, ChordReading *out);

#endif
