#ifndef URSATZ_FRAMEWORKS_STANDARD_VOICING_DOUBLING_READING_H
#define URSATZ_FRAMEWORKS_STANDARD_VOICING_DOUBLING_READING_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The doubling of one chord: which member of the chord each sounding voice plays, which member is doubled, and the
 * distance between neighbouring voices. A reading, not a rule: it judges nothing (the rules are in doubling_rules.h
 * and spacing_rules.h).
 */

#define STANDARD_VOICING_READING_MAX_VOICES 6
#define STANDARD_VOICING_READING_MAX_MEMBERS 4

typedef struct {
    size_t voices_sounding;
    /* voices on each chord member, in the order of the intervals passed in (root, third, fifth, seventh) */
    size_t member_voices[STANDARD_VOICING_READING_MAX_MEMBERS];
    /* voices that play none of the chord members */
    size_t non_chord_voices;
    /* the first member with more than one voice (the first of the most), -1 when no member is doubled */
    int doubled_member;
    /* semitones from each voice to the next one down: semitones[i] - semitones[i + 1] */
    long gaps[STANDARD_VOICING_READING_MAX_VOICES - 1];
    size_t gap_count;
} StandardVoicingDoublingReading;

/*
 * semitones: the sounding pitch of each voice, highest voice first (MIDI numbers), 3 to 6 voices. intervals: the
 * semitones above the root of each chord member (1 to 4, any int is reduced to a pitch class); inversion: the index of
 * the member the lowest voice is expected to play. The root is that voice's pitch class minus the inverted member.
 * Returns false (*out untouched) for a NULL argument, fewer than 3 or more than 6 voices, no members or more than 4, or an
 * inversion that is not a member.
 */
bool standard_voicing_read_doubling(const long *semitones, size_t count, const int *intervals, size_t interval_count,
                                    int inversion, StandardVoicingDoublingReading *out);

#endif
