#include <assert.h>
#include <stddef.h>

#include "doubling_reading.h"

static const int C_MAJOR[3] = {0, 4, 7};
static const int G7[4] = {0, 4, 7, 10};

/* C4 E4 G4 C5 written high to low: the root is doubled. */
static void test_root_doubled(void) {
    const long voices[4] = {72, 67, 64, 48};
    StandardVoicingDoublingReading reading;
    assert(standard_voicing_read_doubling(voices, 4, C_MAJOR, 3, 0, &reading));
    assert(reading.voices_sounding == 4);
    assert(reading.member_voices[0] == 2 && reading.member_voices[1] == 1 && reading.member_voices[2] == 1);
    assert(reading.non_chord_voices == 0);
    assert(reading.doubled_member == 0);
    assert(reading.gap_count == 3);
    assert(reading.gaps[0] == 5 && reading.gaps[1] == 3 && reading.gaps[2] == 16);
}

/* First inversion (E in the bass), the third and the root above: the third is doubled. */
static void test_first_inversion_third_doubled(void) {
    const long voices[4] = {76, 72, 67, 52};
    StandardVoicingDoublingReading reading;
    assert(standard_voicing_read_doubling(voices, 4, C_MAJOR, 3, 1, &reading));
    assert(reading.member_voices[0] == 1 && reading.member_voices[1] == 2 && reading.member_voices[2] == 1);
    assert(reading.doubled_member == 1);
}

static void test_no_doubling_in_a_seventh_chord(void) {
    const long voices[4] = {74, 71, 65, 55};
    StandardVoicingDoublingReading reading;
    assert(standard_voicing_read_doubling(voices, 4, G7, 4, 0, &reading));
    assert(reading.member_voices[0] == 1 && reading.member_voices[1] == 1 && reading.member_voices[2] == 1 &&
           reading.member_voices[3] == 1);
    assert(reading.doubled_member == -1);
}

static void test_non_chord_voice(void) {
    const long voices[3] = {74, 64, 48};
    StandardVoicingDoublingReading reading;
    assert(standard_voicing_read_doubling(voices, 3, C_MAJOR, 3, 0, &reading));
    assert(reading.non_chord_voices == 1);
    assert(reading.doubled_member == -1);
}

static void test_bass_is_the_lowest_pitch_not_the_last_voice(void) {
    const long voices[3] = {48, 64, 67};
    StandardVoicingDoublingReading reading;
    assert(standard_voicing_read_doubling(voices, 3, C_MAJOR, 3, 0, &reading));
    assert(reading.non_chord_voices == 0);
    assert(reading.gaps[0] == -16 && reading.gaps[1] == -3);
}

static void test_rejects_unreadable_input(void) {
    const long voices[4] = {72, 67, 64, 48};
    StandardVoicingDoublingReading reading;
    reading.voices_sounding = 99;
    assert(!standard_voicing_read_doubling(NULL, 4, C_MAJOR, 3, 0, &reading));
    assert(!standard_voicing_read_doubling(voices, 4, NULL, 3, 0, &reading));
    assert(!standard_voicing_read_doubling(voices, 4, C_MAJOR, 3, 0, NULL));
    assert(!standard_voicing_read_doubling(voices, 2, C_MAJOR, 3, 0, &reading));
    assert(!standard_voicing_read_doubling(voices, 7, C_MAJOR, 3, 0, &reading));
    assert(!standard_voicing_read_doubling(voices, 4, C_MAJOR, 0, 0, &reading));
    assert(!standard_voicing_read_doubling(voices, 4, C_MAJOR, 5, 0, &reading));
    assert(!standard_voicing_read_doubling(voices, 4, C_MAJOR, 3, 3, &reading));
    assert(!standard_voicing_read_doubling(voices, 4, C_MAJOR, 3, -1, &reading));
    assert(reading.voices_sounding == 99);
}

int main(void) {
    test_root_doubled();
    test_first_inversion_third_doubled();
    test_no_doubling_in_a_seventh_chord();
    test_non_chord_voice();
    test_bass_is_the_lowest_pitch_not_the_last_voice();
    test_rejects_unreadable_input();
    return 0;
}
