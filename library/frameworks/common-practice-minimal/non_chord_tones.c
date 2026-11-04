#include "non_chord_tones.h"

#include <stdlib.h>
#include <string.h>
#include "pitch/midi_pitch.h"

static bool same_note(const CommonPracticeMember *a, const CommonPracticeMember *b) {
    if (a->note_id != NULL && b->note_id != NULL) {
        return strcmp(a->note_id, b->note_id) == 0;
    }
    return a == b;
}

static bool same_voice(const CommonPracticeMember *a, const CommonPracticeMember *b) {
    return a->voice_id != NULL && b->voice_id != NULL && strcmp(a->voice_id, b->voice_id) == 0;
}

/* The direction and size of a move from `from` to `to`. */
static int direction(int from, int to) {
    return to > from ? 1 : (to < from ? -1 : 0);
}

static bool is_step(int from, int to) {
    int distance = to > from ? to - from : from - to;
    return distance >= 1 && distance <= 2;
}

static bool is_leap(int from, int to) {
    int distance = to > from ? to - from : from - to;
    return distance > 2;
}

typedef struct {
    bool found;
    int pitch;
} Neighbor;

/* The nearest pitch to `pitch` among a segment's members that pass `keep`, excluding the tone's own note.
   Two nearest on opposite sides at one distance are ambiguous: not found. */
static Neighbor nearest_in(const CommonPracticeSegment *segment, const CommonPracticeMember *tone,
                           bool attacked_only) {
    Neighbor result = {false, 0};
    int best_distance = 0;
    bool ambiguous = false;
    for (size_t i = 0; i < segment->member_count; i++) {
        const CommonPracticeMember *candidate = &segment->members[i];
        if (same_note(candidate, tone) || (attacked_only && candidate->held)) {
            continue;
        }
        int distance = candidate->pitch > tone->pitch ? candidate->pitch - tone->pitch : tone->pitch - candidate->pitch;
        if (!result.found || distance < best_distance) {
            result.found = true;
            result.pitch = candidate->pitch;
            best_distance = distance;
            ambiguous = false;
        } else if (distance == best_distance && candidate->pitch != result.pitch) {
            ambiguous = true;
        }
    }
    if (ambiguous) {
        result.found = false;
    }
    return result;
}

/* The tone's approach and departure. */
static void find_neighbors(const CommonPracticeSegment *segments, size_t segment_count, size_t index,
                           const CommonPracticeMember *tone, Neighbor *approach, Neighbor *departure) {
    approach->found = false;
    departure->found = false;

    if (tone->voice_id != NULL) {
        double previous_onset = 0.0;
        double next_onset = 0.0;
        for (size_t s = 0; s < segment_count; s++) {
            for (size_t i = 0; i < segments[s].member_count; i++) {
                const CommonPracticeMember *other = &segments[s].members[i];
                if (!same_voice(other, tone) || same_note(other, tone)) {
                    continue;
                }
                if (other->onset < tone->onset && (!approach->found || other->onset > previous_onset)) {
                    approach->found = true;
                    approach->pitch = other->pitch;
                    previous_onset = other->onset;
                }
                if (other->onset > tone->onset && (!departure->found || other->onset < next_onset)) {
                    departure->found = true;
                    departure->pitch = other->pitch;
                    next_onset = other->onset;
                }
            }
        }
        return;
    }

    if (index > 0) {
        *approach = nearest_in(&segments[index - 1], tone, false);
    }
    if (index + 1 < segment_count) {
        *departure = nearest_in(&segments[index + 1], tone, true);
    }
}

static bool in_mask(unsigned mask, int pitch) {
    return (mask >> pitch_class_of_midi(pitch)) & 1u;
}

static bool is_bass_of(const CommonPracticeSegment *segment, const CommonPracticeMember *member) {
    for (size_t i = 0; i < segment->member_count; i++) {
        if (segment->members[i].pitch < member->pitch) {
            return false;
        }
    }
    return true;
}

static bool segment_holds_note(const CommonPracticeSegment *segment, const CommonPracticeMember *note) {
    for (size_t i = 0; i < segment->member_count; i++) {
        if (same_note(&segment->members[i], note)) {
            return true;
        }
    }
    return false;
}

/* The same note is the lowest pitch of three or more consecutive segments including `index`. */
static bool is_pedal(const CommonPracticeSegment *segments, size_t segment_count, size_t index,
                     const CommonPracticeMember *tone) {
    if (!is_bass_of(&segments[index], tone)) {
        return false;
    }
    size_t run = 1;
    for (size_t s = index; s > 0; s--) {
        const CommonPracticeSegment *before = &segments[s - 1];
        if (!segment_holds_note(before, tone) || !is_bass_of(before, tone)) {
            break;
        }
        run++;
    }
    for (size_t s = index + 1; s < segment_count; s++) {
        if (!segment_holds_note(&segments[s], tone) || !is_bass_of(&segments[s], tone)) {
            break;
        }
        run++;
    }
    return run >= 3;
}

/* Classifies one leftover tone. */
static CommonPracticeToneLabel classify(const CommonPracticeSegment *segments, const unsigned *masks,
                                        size_t segment_count, size_t index, size_t member_index) {
    const CommonPracticeMember *tone = &segments[index].members[member_index];
    CommonPracticeToneLabel label;
    label.member_index = member_index;
    label.classified = false;
    label.tone = NON_CHORD_TONE_PASSING;

    Neighbor approach;
    Neighbor departure;
    find_neighbors(segments, segment_count, index, tone, &approach, &departure);

    NonChordTone matches[8];
    size_t match_count = 0;

    if (tone->held) {
        /* Held over from earlier: a suspension if it then resolves down by step. */
        if (departure.found && is_step(tone->pitch, departure.pitch) && departure.pitch < tone->pitch) {
            matches[match_count++] = NON_CHORD_TONE_SUSPENSION;
        }
    } else if (approach.found && departure.found) {
        int in = direction(approach.pitch, tone->pitch);
        int out = direction(tone->pitch, departure.pitch);
        if (is_step(approach.pitch, tone->pitch) && is_step(tone->pitch, departure.pitch)) {
            matches[match_count++] = in == out ? NON_CHORD_TONE_PASSING : NON_CHORD_TONE_NEIGHBOR;
        }
        if (is_leap(approach.pitch, tone->pitch) && is_step(tone->pitch, departure.pitch) && out == -in) {
            matches[match_count++] = NON_CHORD_TONE_APPOGGIATURA;
        }
        if (is_step(approach.pitch, tone->pitch) && is_leap(tone->pitch, departure.pitch) && out == -in) {
            matches[match_count++] = NON_CHORD_TONE_ESCAPE_TONE;
        }
    }

    if (!tone->held && index + 1 < segment_count && masks[index + 1] != 0 && in_mask(masks[index + 1], tone->pitch)) {
        for (size_t i = 0; i < segments[index + 1].member_count; i++) {
            const CommonPracticeMember *later = &segments[index + 1].members[i];
            if (later->pitch == tone->pitch && (same_note(later, tone) || tone->voice_id == NULL || same_voice(later, tone))) {
                matches[match_count++] = NON_CHORD_TONE_ANTICIPATION;
                break;
            }
        }
    }

    if (is_pedal(segments, segment_count, index, tone)) {
        matches[match_count++] = NON_CHORD_TONE_PEDAL;
    }

    if (match_count == 1) {
        label.classified = true;
        label.tone = matches[0];
    }
    return label;
}

CommonPracticeToneStatus common_practice_label_leftover_tones(const CommonPracticeSegment *segments,
                                                               const unsigned *chord_pitch_class_masks,
                                                               size_t segment_count, size_t index,
                                                               CommonPracticeToneLabel **out_labels,
                                                               size_t *out_count) {
    if (segments == NULL || chord_pitch_class_masks == NULL || out_labels == NULL || out_count == NULL) {
        return COMMON_PRACTICE_TONES_ERR_NULL_ARGUMENT;
    }
    if (index >= segment_count) {
        return COMMON_PRACTICE_TONES_ERR_BAD_INDEX;
    }

    *out_labels = NULL;
    *out_count = 0;
    unsigned mask = chord_pitch_class_masks[index];
    const CommonPracticeSegment *segment = &segments[index];
    if (mask == 0 || segment->member_count == 0) {
        return COMMON_PRACTICE_TONES_OK;
    }

    CommonPracticeToneLabel *labels =
        (CommonPracticeToneLabel *)malloc(segment->member_count * sizeof(CommonPracticeToneLabel));
    if (labels == NULL) {
        return COMMON_PRACTICE_TONES_ERR_ALLOC;
    }
    size_t count = 0;
    for (size_t i = 0; i < segment->member_count; i++) {
        if (in_mask(mask, segment->members[i].pitch)) {
            continue;
        }
        labels[count++] = classify(segments, chord_pitch_class_masks, segment_count, index, i);
    }
    if (count == 0) {
        free(labels);
        return COMMON_PRACTICE_TONES_OK;
    }
    *out_labels = labels;
    *out_count = count;
    return COMMON_PRACTICE_TONES_OK;
}
