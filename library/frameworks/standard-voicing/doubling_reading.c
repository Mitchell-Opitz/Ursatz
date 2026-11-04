#include "doubling_reading.h"

#include <string.h>

#include "pitch/midi_pitch.h"

static int pitch_class_of(long semitone) {
    return pitch_class_of_midi((int)(semitone % 12));
}

bool standard_voicing_read_doubling(const long *semitones, size_t count, const int *intervals, size_t interval_count,
                                    int inversion, StandardVoicingDoublingReading *out) {
    if (semitones == NULL || intervals == NULL || out == NULL || count < 3 ||
        count > STANDARD_VOICING_READING_MAX_VOICES || interval_count == 0 ||
        interval_count > STANDARD_VOICING_READING_MAX_MEMBERS || inversion < 0 ||
        (size_t)inversion >= interval_count) {
        return false;
    }
    size_t bass = 0;
    for (size_t i = 1; i < count; i++) {
        if (semitones[i] < semitones[bass]) {
            bass = i;
        }
    }
    int root = pitch_class_of_midi(pitch_class_of(semitones[bass]) - intervals[inversion]);

    StandardVoicingDoublingReading reading;
    memset(&reading, 0, sizeof(reading));
    reading.voices_sounding = count;
    reading.doubled_member = -1;
    for (size_t i = 0; i < count; i++) {
        int member = -1;
        for (size_t m = 0; m < interval_count; m++) {
            if (pitch_class_of_midi(intervals[m]) == pitch_class_of_midi(pitch_class_of(semitones[i]) - root)) {
                member = (int)m;
                break;
            }
        }
        if (member < 0) {
            reading.non_chord_voices++;
        } else {
            reading.member_voices[member]++;
        }
        if (i + 1 < count) {
            reading.gaps[reading.gap_count++] = semitones[i] - semitones[i + 1];
        }
    }
    size_t most = 1;
    for (size_t m = 0; m < STANDARD_VOICING_READING_MAX_MEMBERS; m++) {
        if (reading.member_voices[m] > most) {
            most = reading.member_voices[m];
            reading.doubled_member = (int)m;
        }
    }
    *out = reading;
    return true;
}
