#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "analysis/harmony/chord_reading.h"
#include "ursatz_test_knowledge.h"

/*
 * Chord readings (chord_reading.h): a sonority read on a root gives the snapshot's chord type for the nine embedded types, the members by
 * role, the bass and the field defaults; an aggregate with no root or no type is a valid reading; invalid arguments leave the output as it was.
 */

typedef struct {
    const char *id;
    int count;
    int above[4];  /* semitones above the root of each member after the root */
    int roles[4];  /* ChordMemberRole of each */
} TypeCase;

static const TypeCase TYPES[9] = {
    {"omt.chord-type.major-triad", 2, {4, 7}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH}},
    {"omt.chord-type.minor-triad", 2, {3, 7}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH}},
    {"omt.chord-type.diminished-triad", 2, {3, 6}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH}},
    {"omt.chord-type.augmented-triad", 2, {4, 8}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH}},
    {"omt.chord-type.dominant-seventh", 3, {4, 7, 10}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH, CHORD_MEMBER_SEVENTH}},
    {"omt.chord-type.major-seventh", 3, {4, 7, 11}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH, CHORD_MEMBER_SEVENTH}},
    {"omt.chord-type.minor-seventh", 3, {3, 7, 10}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH, CHORD_MEMBER_SEVENTH}},
    {"omt.chord-type.half-diminished-seventh", 3, {3, 6, 10}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH, CHORD_MEMBER_SEVENTH}},
    {"omt.chord-type.diminished-seventh", 3, {3, 6, 9}, {CHORD_MEMBER_THIRD, CHORD_MEMBER_FIFTH, CHORD_MEMBER_SEVENTH}},
};

static unsigned bit(int root, int semitones) {
    return 1u << ((root + semitones) % 12);
}

static SonorityPitches sonority_of(const TypeCase *c, int root, bool with_root, int bass) {
    SonorityPitches s;
    memset(&s, 0, sizeof(s));
    s.span.start.value.num = 1;
    s.span.start.value.den = 2;
    s.span.end.value.num = 3;
    s.span.end.value.den = 4;
    if (with_root) {
        s.pitch_classes |= bit(root, 0);
    }
    for (int i = 0; i < c->count; i++) {
        s.pitch_classes |= bit(root, c->above[i]);
    }
    s.bass_pitch_class = bass;
    return s;
}

static void assert_defaults(const ChordReading *r) {
    assert(r->borrowed_from == NULL);
    assert(r->role == CHORD_ROLE_FUNCTIONAL);
    assert(!r->has_expected);
    assert(r->expected_root_pitch_class == 0);
    assert(r->expected_type == NULL);
}

static void test_nine_types_on_every_root(void) {
    const KnowledgeSnapshot *snapshot = test_knowledge();
    for (size_t t = 0; t < 9; t++) {
        const ChordType *expected = knowledge_snapshot_chord_type(snapshot, TYPES[t].id);
        assert(expected != NULL);
        for (int root = 0; root < 12; root++) {
            for (int rooted = 0; rooted < 2; rooted++) {
                SonorityPitches s = sonority_of(&TYPES[t], root, rooted == 1, (root + TYPES[t].above[0]) % 12);
                ChordReading r;
                memset(&r, 0x5a, sizeof(r));
                assert(chord_reading_of_sonority(snapshot, s, root, &r));
                assert(r.has_root && r.root_pitch_class == root);
                assert(r.type == expected); /* the root's own bit does not matter */
                assert(r.bass_pitch_class == s.bass_pitch_class);
                assert(r.sonority.pitch_classes == s.pitch_classes && r.sonority.bass_pitch_class == s.bass_pitch_class);
                assert(r.sonority.span.start.value.num == 1 && r.sonority.span.start.value.den == 2);
                assert(r.sonority.span.end.value.num == 3 && r.sonority.span.end.value.den == 4);
                assert_defaults(&r);
            }
        }
    }
}

static void test_members_by_role(void) {
    const KnowledgeSnapshot *snapshot = test_knowledge();
    /* G7 on G: G, B, D, F. */
    SonorityPitches s;
    memset(&s, 0, sizeof(s));
    s.pitch_classes = (1u << 7) | (1u << 11) | (1u << 2) | (1u << 5);
    s.bass_pitch_class = 11;
    ChordReading r;
    assert(chord_reading_of_sonority(snapshot, s, 7, &r));
    assert(r.type != NULL && strcmp(r.type->id, "omt.chord-type.dominant-seventh") == 0);
    assert(r.members_by_role[CHORD_MEMBER_ROOT] == (1u << 7));
    assert(r.members_by_role[CHORD_MEMBER_THIRD] == (1u << 11));
    assert(r.members_by_role[CHORD_MEMBER_FIFTH] == (1u << 2));
    assert(r.members_by_role[CHORD_MEMBER_SEVENTH] == (1u << 5));
    assert(r.members_by_role[CHORD_MEMBER_NINTH] == 0 && r.members_by_role[CHORD_MEMBER_ELEVENTH] == 0);
    assert(r.members_by_role[CHORD_MEMBER_THIRTEENTH] == 0 && r.members_by_role[CHORD_MEMBER_ADDED_SECOND] == 0);
    assert(r.members_by_role[CHORD_MEMBER_ADDED_SIXTH] == 0 && r.members_by_role[CHORD_MEMBER_SUSPENDED_FOURTH] == 0);
    assert(r.bass_pitch_class == 11);
    /* The members of every type, on every root, are the type's pitch classes in its roles. */
    for (size_t t = 0; t < 9; t++) {
        for (int root = 0; root < 12; root++) {
            SonorityPitches c = sonority_of(&TYPES[t], root, true, -1);
            assert(chord_reading_of_sonority(snapshot, c, root, &r));
            unsigned all = 0;
            for (int i = 0; i < CHORD_READING_ROLE_COUNT; i++) {
                all |= r.members_by_role[i];
            }
            assert(all == c.pitch_classes);
            assert(r.members_by_role[CHORD_MEMBER_ROOT] == bit(root, 0));
            for (int i = 0; i < TYPES[t].count; i++) {
                assert(r.members_by_role[TYPES[t].roles[i]] == bit(root, TYPES[t].above[i]));
            }
            assert(r.bass_pitch_class == -1);
        }
    }
}

static void test_aggregates_are_valid(void) {
    const KnowledgeSnapshot *snapshot = test_knowledge();
    SonorityPitches s;
    memset(&s, 0, sizeof(s));
    s.pitch_classes = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 6); /* a cluster no type has */
    s.bass_pitch_class = 6;
    ChordReading r;
    memset(&r, 0x5a, sizeof(r));
    /* no root: an aggregate */
    assert(chord_reading_of_sonority(snapshot, s, -1, &r));
    assert(!r.has_root && r.root_pitch_class == 0 && r.type == NULL);
    for (int i = 0; i < CHORD_READING_ROLE_COUNT; i++) {
        assert(r.members_by_role[i] == 0);
    }
    assert(r.sonority.pitch_classes == s.pitch_classes && r.bass_pitch_class == 6);
    assert_defaults(&r);
    /* a root, but no type for the pitch classes above it */
    memset(&r, 0x5a, sizeof(r));
    assert(chord_reading_of_sonority(snapshot, s, 0, &r));
    assert(r.has_root && r.root_pitch_class == 0 && r.type == NULL);
    for (int i = 0; i < CHORD_READING_ROLE_COUNT; i++) {
        assert(r.members_by_role[i] == 0);
    }
    assert_defaults(&r);
    /* a root with nothing else sounding: no type (the empty set is no chord) */
    memset(&s, 0, sizeof(s));
    s.bass_pitch_class = -1;
    assert(chord_reading_of_sonority(snapshot, s, 3, &r));
    assert(r.has_root && r.type == NULL && r.bass_pitch_class == -1);
    /* a root with no bass known */
    s.pitch_classes = (1u << 0) | (1u << 4) | (1u << 7);
    assert(chord_reading_of_sonority(snapshot, s, 0, &r));
    assert(r.type != NULL && strcmp(r.type->id, "omt.chord-type.major-triad") == 0 && r.bass_pitch_class == -1);
}

static void test_the_fields_default(void) {
    const KnowledgeSnapshot *snapshot = test_knowledge();
    SonorityPitches s = sonority_of(&TYPES[0], 0, true, 0);
    ChordReading r;
    memset(&r, 0xff, sizeof(r));
    assert(chord_reading_of_sonority(snapshot, s, 0, &r));
    assert_defaults(&r);
    assert(CHORD_ROLE_FUNCTIONAL != CHORD_ROLE_EMBELLISHING);
    assert(CHORD_READING_ROLE_COUNT == 10 && CHORD_MEMBER_SUSPENDED_FOURTH + 1 == CHORD_READING_ROLE_COUNT);
}

static void test_invalid_arguments(void) {
    const KnowledgeSnapshot *snapshot = test_knowledge();
    SonorityPitches s = sonority_of(&TYPES[0], 0, true, 0);
    ChordReading r;
    memset(&r, 0x5a, sizeof(r));
    ChordReading before = r;
    assert(!chord_reading_of_sonority(NULL, s, 0, &r));
    assert(!chord_reading_of_sonority(snapshot, s, 0, NULL));
    assert(!chord_reading_of_sonority(snapshot, s, 12, &r));
    SonorityPitches wide = s;
    wide.pitch_classes |= 1u << 12;
    assert(!chord_reading_of_sonority(snapshot, wide, 0, &r));
    SonorityPitches high_bass = s;
    high_bass.bass_pitch_class = 12;
    assert(!chord_reading_of_sonority(snapshot, high_bass, 0, &r));
    SonorityPitches low_bass = s;
    low_bass.bass_pitch_class = -2;
    assert(!chord_reading_of_sonority(snapshot, low_bass, 0, &r));
    assert(memcmp(&r, &before, sizeof(r)) == 0);
}

int main(void) {
    test_nine_types_on_every_root();
    test_members_by_role();
    test_aggregates_are_valid();
    test_the_fields_default();
    test_invalid_arguments();
    printf("All chord reading tests passed.\n");
    return 0;
}
