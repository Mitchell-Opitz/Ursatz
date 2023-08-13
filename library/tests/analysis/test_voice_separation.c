#include "analysis/voice_separation.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation_internal.h"
#include "core/provenance_reference.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/temporal_anchor.h"

/*
 * Voice separation tests: two voices in contrary motion, a note that would cross a held note (crossing off
 * and on), a voice resting while the others continue, a chord with more notes than max_voices, a single
 * line, streams, and each internal step by itself. Notes are built with MIDI pitches under one tuning system.
 */

static EntityID *make_id(const char *value) {
    EntityID *id = NULL;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

static TuningSystem *make_tuning(const char *id) {
    TuningSystem *tuning = NULL;
    assert(tuning_system_create(make_id(id), "MIDI note number", &tuning) == TUNING_SYSTEM_OK);
    return tuning;
}

typedef struct {
    NoteEvent *notes[32];
    PitchNumericValue *values[32];
    size_t streams[32];
    size_t count;
} Notes;

static void add_note(Notes *set, const TuningSystem *tuning, const char *id, long on_n, unsigned long on_d, long dur_n,
                     unsigned long dur_d, int midi, size_t stream) {
    static const PitchLetter LETTER[12] = {PITCH_LETTER_C, PITCH_LETTER_C, PITCH_LETTER_D, PITCH_LETTER_D,
                                           PITCH_LETTER_E, PITCH_LETTER_F, PITCH_LETTER_F, PITCH_LETTER_G,
                                           PITCH_LETTER_G, PITCH_LETTER_A, PITCH_LETTER_A, PITCH_LETTER_B};
    static const int ACCIDENTAL[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};

    MusicalTime *position = NULL;
    assert(musical_time_create(on_n, on_d, &position) == MUSICAL_TIME_OK);
    TemporalAnchor *anchor = NULL;
    assert(temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK);
    Duration *duration = NULL;
    assert(duration_create(dur_n, dur_d, &duration) == DURATION_OK);
    PitchIdentity *pitch = NULL;
    assert(pitch_identity_create("pitch", 6, &pitch) == PITCH_IDENTITY_OK);
    PitchSpelling *spelling = NULL;
    assert(pitch_spelling_create(LETTER[midi % 12], ACCIDENTAL[midi % 12], midi / 12 - 1, &spelling) ==
           PITCH_SPELLING_OK);
    ProvenanceReference *provenance = NULL;
    assert(provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK);
    assert(note_event_create(make_id(id), anchor, duration, pitch, spelling, NULL, provenance,
                             &set->notes[set->count]) == NOTE_EVENT_OK);
    assert(pitch_numeric_value_create(tuning, midi, 1, &set->values[set->count]) == PITCH_NUMERIC_VALUE_OK);
    set->streams[set->count] = stream;
    set->count++;
}

static void destroy_notes(Notes *set) {
    for (size_t i = 0; i < set->count; i++) {
        note_event_destroy(set->notes[i]);
        pitch_numeric_value_destroy(set->values[i]);
    }
}

static VoiceSeparationResult separate(Notes *set, bool with_streams, const VoiceSeparationConfig *config,
                                      VoiceAssignment *out) {
    return voice_separate(set->notes, set->values, set->count, with_streams ? set->streams : NULL, config, out);
}

static void assert_voice(const VoiceAssignment *a, size_t note, size_t voice) {
    assert(a->entries[note].voice == voice);
}

static void test_two_voices_in_contrary_motion(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, {0}, 0};
    int upper[4] = {72, 74, 76, 77};
    int lower[4] = {60, 58, 57, 55};
    for (int k = 0; k < 4; k++) {
        char name[8];
        snprintf(name, sizeof(name), "u%d", k);
        add_note(&set, tuning, name, k, 4, 1, 4, upper[k], 0);
        snprintf(name, sizeof(name), "l%d", k);
        add_note(&set, tuning, name, k, 4, 1, 4, lower[k], 0);
    }
    VoiceAssignment a = {0};
    assert(separate(&set, false, NULL, &a) == VOICE_SEPARATION_OK);
    assert(a.count == 8 && a.stream_count == 1 && a.streams[0].voice_count == 2);
    for (size_t k = 0; k < 8; k += 2) {
        assert_voice(&a, k, 0);
        assert_voice(&a, k + 1, 1);
        assert(!a.entries[k].chord_member && !a.entries[k + 1].chord_member);
        assert(a.entries[k].confidence >= 0.0 && a.entries[k].confidence <= 1.0);
        assert(strcmp(entity_id_value(a.entries[k].note_id), k == 0 ? "u0" : entity_id_value(a.entries[k].note_id)) == 0);
    }
    voice_assignment_destroy(&a);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

/* A held note in the middle voice, then a note that sits just below it. */
static void build_held_middle(Notes *set, const TuningSystem *tuning) {
    add_note(set, tuning, "top", 0, 1, 1, 4, 70, 0);
    add_note(set, tuning, "mid", 0, 1, 1, 1, 64, 0);
    add_note(set, tuning, "low", 0, 1, 1, 4, 50, 0);
    add_note(set, tuning, "new", 1, 4, 1, 4, 63, 0);
}

static void test_a_note_that_would_cross_a_held_note(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, {0}, 0};
    build_held_middle(&set, tuning);

    VoiceAssignment a = {0};
    assert(separate(&set, false, NULL, &a) == VOICE_SEPARATION_OK);
    assert(a.streams[0].voice_count == 3);
    assert_voice(&a, 0, 0);
    assert_voice(&a, 1, 1);
    assert_voice(&a, 2, 2);
    assert_voice(&a, 3, 2); /* below the held 64: the lower free voice, though farther from its last pitch */
    voice_assignment_destroy(&a);

    VoiceSeparationConfig crossing = voice_separation_config_default();
    crossing.allow_crossing = true;
    assert(separate(&set, false, &crossing, &a) == VOICE_SEPARATION_OK);
    assert_voice(&a, 3, 0); /* nearest its last pitch (70), below the held 64's voice: a crossing */
    voice_assignment_destroy(&a);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_one_voice_rests_while_the_others_continue(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, {0}, 0};
    add_note(&set, tuning, "u0", 0, 1, 1, 4, 72, 0);
    add_note(&set, tuning, "l0", 0, 1, 1, 4, 48, 0);
    add_note(&set, tuning, "u1", 1, 4, 1, 4, 74, 0);
    add_note(&set, tuning, "u2", 1, 2, 1, 4, 72, 0);
    add_note(&set, tuning, "u3", 3, 4, 1, 4, 71, 0);
    add_note(&set, tuning, "l1", 3, 4, 1, 4, 50, 0);
    VoiceAssignment a = {0};
    assert(separate(&set, false, NULL, &a) == VOICE_SEPARATION_OK);
    assert(a.streams[0].voice_count == 2);
    assert_voice(&a, 0, 0);
    assert_voice(&a, 1, 1);
    assert_voice(&a, 2, 0); /* alone, the melody stays in the top voice */
    assert_voice(&a, 3, 0);
    assert_voice(&a, 4, 0);
    assert_voice(&a, 5, 1); /* the bass comes back to the lower voice */
    voice_assignment_destroy(&a);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_a_chord_with_more_notes_than_max_voices(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, {0}, 0};
    add_note(&set, tuning, "c0", 0, 1, 1, 4, 72, 0);
    add_note(&set, tuning, "c1", 0, 1, 1, 4, 67, 0);
    add_note(&set, tuning, "c2", 0, 1, 1, 4, 60, 0);
    VoiceSeparationConfig two = voice_separation_config_default();
    two.max_voices = 2;
    VoiceAssignment a = {0};
    assert(separate(&set, false, &two, &a) == VOICE_SEPARATION_OK);
    assert(a.streams[0].voice_count == 2);
    assert_voice(&a, 0, 0);
    assert(!a.entries[0].chord_member);
    assert_voice(&a, 1, 0); /* the lower of the closest pair (72, 67) joins the nearest voice, the top one */
    assert(a.entries[1].chord_member);
    assert_voice(&a, 2, 1);
    assert(!a.entries[2].chord_member);
    voice_assignment_destroy(&a);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_a_single_line(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, {0}, 0};
    int line[4] = {60, 62, 64, 62};
    for (int k = 0; k < 4; k++) {
        char name[8];
        snprintf(name, sizeof(name), "n%d", k);
        add_note(&set, tuning, name, k, 4, 1, 4, line[k], 0);
    }
    VoiceAssignment a = {0};
    assert(separate(&set, false, NULL, &a) == VOICE_SEPARATION_OK);
    assert(a.streams[0].voice_count == 1);
    for (size_t k = 0; k < 4; k++) {
        assert_voice(&a, k, 0);
        assert(fabs(a.entries[k].confidence - 1.0) < 1e-12);
    }
    voice_assignment_destroy(&a);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_streams_are_separated_independently(void) {
    TuningSystem *tuning = make_tuning("midi");
    Notes set = {{0}, {0}, {0}, 0};
    add_note(&set, tuning, "a0", 0, 1, 1, 4, 72, 5);
    add_note(&set, tuning, "a1", 0, 1, 1, 4, 67, 5);
    add_note(&set, tuning, "b0", 0, 1, 1, 4, 48, 2);
    VoiceAssignment a = {0};
    assert(separate(&set, true, NULL, &a) == VOICE_SEPARATION_OK);
    assert(a.stream_count == 2);
    assert(a.streams[0].stream == 2 && a.streams[0].voice_count == 1);
    assert(a.streams[1].stream == 5 && a.streams[1].voice_count == 2);
    assert_voice(&a, 0, 0);
    assert_voice(&a, 1, 1);
    assert_voice(&a, 2, 0); /* alone in its stream, though it is the lowest note */
    assert(a.entries[2].stream == 2);
    voice_assignment_destroy(&a);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
}

static void test_bad_input_and_no_notes(void) {
    TuningSystem *tuning = make_tuning("midi");
    TuningSystem *other = make_tuning("other");
    Notes set = {{0}, {0}, {0}, 0};
    add_note(&set, tuning, "n0", 0, 1, 1, 4, 60, 0);
    VoiceAssignment a = {0};
    assert(voice_separate(NULL, NULL, 1, NULL, NULL, &a) == VOICE_SEPARATION_ERR_NULL_ARGUMENT);
    assert(separate(&set, false, NULL, NULL) == VOICE_SEPARATION_ERR_NULL_ARGUMENT);
    VoiceSeparationConfig bad = voice_separation_config_default();
    bad.max_voices = 0;
    assert(separate(&set, false, &bad, &a) == VOICE_SEPARATION_ERR_BAD_CONFIG);
    bad.max_voices = 9;
    assert(separate(&set, false, &bad, &a) == VOICE_SEPARATION_ERR_BAD_CONFIG);
    add_note(&set, other, "n1", 1, 4, 1, 4, 60, 0);
    assert(separate(&set, false, NULL, &a) == VOICE_SEPARATION_ERR_MIXED_TUNING);
    Notes none = {{0}, {0}, {0}, 0};
    assert(separate(&none, false, NULL, &a) == VOICE_SEPARATION_OK);
    assert(a.count == 0 && a.stream_count == 0);
    voice_assignment_destroy(&a);
    voice_assignment_destroy(NULL);
    destroy_notes(&set);
    tuning_system_destroy(tuning);
    tuning_system_destroy(other);
}

/* --- the internal steps --- */

static VsVoice free_voice(unsigned long pitch, unsigned long recency) {
    VsVoice v = {false, er_make(pitch, 1), recency};
    return v;
}

static VsAttack attack(const char *id, unsigned long pitch) {
    VsAttack a = {id, er_make(pitch, 1)};
    return a;
}

static void test_assign_onset_minimizes_distance(void) {
    VsVoice voices[2] = {free_voice(72, 1), free_voice(60, 1)};
    VsAttack attacks[2] = {attack("a", 59), attack("b", 73)};
    size_t out[2];
    double margin = -1.0;
    assert(vs_assign_onset(voices, 2, attacks, 2, false, out, &margin) == 0);
    assert(out[0] == 1 && out[1] == 0);
    assert(margin > 0.0 && margin <= 1.0);
}

static void test_assign_onset_ties_go_to_recency_then_id(void) {
    /* One note exactly between two voices: equal distance, the more recently sounding voice wins. */
    VsVoice voices[2] = {free_voice(64, 1), free_voice(60, 5)};
    VsAttack attacks[1] = {attack("a", 62)};
    size_t out[1];
    double margin = -1.0;
    assert(vs_assign_onset(voices, 2, attacks, 1, false, out, &margin) == 0);
    assert(out[0] == 1);
    assert(margin == 0.0); /* a dead tie in cost: no confidence */

    /* A unison with equal recency: the lower note id takes the lower-index voice. */
    VsVoice same[2] = {free_voice(60, 1), free_voice(60, 1)};
    VsAttack pair[2] = {attack("z", 60), attack("b", 60)};
    size_t two[2];
    assert(vs_assign_onset(same, 2, pair, 2, false, two, &margin) == 0);
    assert(two[1] == 0 && two[0] == 1); /* "b" is pair[1] and takes voice 0 */
}

static void test_assign_onset_counts_crossings_against_held_notes(void) {
    VsVoice voices[3] = {free_voice(70, 1), {true, er_make(64, 1), 1}, free_voice(50, 1)};
    VsAttack attacks[1] = {attack("a", 63)};
    size_t out[1];
    double margin = 0.0;
    assert(vs_assign_onset(voices, 3, attacks, 1, false, out, &margin) == 0);
    assert(out[0] == 2);
    assert(vs_assign_onset(voices, 3, attacks, 1, true, out, &margin) == 0);
    assert(out[0] == 0);
    VsAttack many[3] = {attack("a", 63), attack("b", 62), attack("c", 61)};
    size_t big[3];
    assert(vs_assign_onset(voices, 3, many, 3, false, big, &margin) == -2); /* only two free voices */
}

static void test_split_overflow_drops_the_lower_of_the_closest_pair(void) {
    VsAttack attacks[4] = {attack("a", 72), attack("b", 67), attack("c", 66), attack("d", 48)};
    bool member[4];
    assert(vs_split_overflow(attacks, 4, 3, member) == 0);
    assert(!member[0] && !member[1] && member[2] == true && !member[3]); /* 66 is the lower of (66, 67) */
    assert(vs_split_overflow(attacks, 4, 2, member) == 0);
    assert(member[2] && member[1] && !member[0] && !member[3]); /* then (67, 72) is closest */
    assert(vs_split_overflow(attacks, 4, 4, member) == 0);
    assert(!member[0] && !member[1] && !member[2] && !member[3]);
    assert(vs_split_overflow(attacks, 4, 0, member) == 0);
    assert(member[0] && member[1] && member[2] && member[3]);
}

static void test_nearest_voice(void) {
    ExactRational current[3] = {er_make(72, 1), er_make(65, 1), er_make(48, 1)};
    bool has[3] = {true, true, true};
    assert(vs_nearest_voice(current, has, 3, er_make(70, 1)) == 0);
    assert(vs_nearest_voice(current, has, 3, er_make(50, 1)) == 2);
    assert(vs_nearest_voice(current, has, 3, er_make(68, 1)) == 1); /* 3 from 65, 4 from 72 */
    assert(vs_nearest_voice(current, has, 3, er_make(69, 2)) == 2); /* 34.5: nearer 48 */
    has[1] = false;
    assert(vs_nearest_voice(current, has, 3, er_make(68, 1)) == 0);
    has[0] = has[2] = false;
    assert(vs_nearest_voice(current, has, 3, er_make(68, 1)) == 3);
    ExactRational even[2] = {er_make(60, 1), er_make(64, 1)};
    bool both[2] = {true, true};
    assert(vs_nearest_voice(even, both, 2, er_make(62, 1)) == 0); /* a tie: the lower index */
}

int main(void) {
    test_two_voices_in_contrary_motion();
    test_a_note_that_would_cross_a_held_note();
    test_one_voice_rests_while_the_others_continue();
    test_a_chord_with_more_notes_than_max_voices();
    test_a_single_line();
    test_streams_are_separated_independently();
    test_bad_input_and_no_notes();
    test_assign_onset_minimizes_distance();
    test_assign_onset_ties_go_to_recency_then_id();
    test_assign_onset_counts_crossings_against_held_notes();
    test_split_overflow_drops_the_lower_of_the_closest_pair();
    test_nearest_voice();
    printf("test_voice_separation: ok\n");
    return 0;
}
