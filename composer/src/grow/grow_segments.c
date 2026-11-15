#include <string.h>

#include "grow/grow_internal.h"

/* The end of a note. */
static bool note_end(const MotiveSketchNote *note, Rational *out) {
    return rational_add(note->onset, note->duration, out) == RATIONAL_OK;
}

/* The base harmony of the segment: the chords that sound under it, the first moved to the segment's start. */
static bool shift_chords(const Motive *source, Rational start, Rational end, Motive *out) {
    out->chord_count = 0;
    for (size_t c = 0; c < source->chord_count; c++) {
        Rational next_onset = c + 1 < source->chord_count ? source->chords[c + 1].onset : source->length;
        /* A chord sounds from its onset to the next chord's; it is under the segment when that overlaps [start, end). */
        if (rational_cmp(next_onset, start) <= 0 || rational_cmp(source->chords[c].onset, end) >= 0) {
            continue;
        }
        MotiveSketchChord chord = source->chords[c];
        if (rational_cmp(chord.onset, start) <= 0) {
            chord.onset = (Rational){0, 1};
        } else if (rational_sub(chord.onset, start, &chord.onset) != RATIONAL_OK) {
            return false;
        }
        out->chords[out->chord_count++] = chord;
    }
    return true;
}

bool grow_segment_motive(const Motive *source, const MotiveSketchFeature *segment, Motive *out) {
    if (!source || !segment || !out || segment->first >= segment->last || segment->last >= source->note_count) {
        return false;
    }
    Motive motive = *source;
    Rational start = source->notes[segment->first].onset;
    Rational end;
    if (!note_end(&source->notes[segment->last], &end)) {
        return false;
    }
    /* The notes, from time 0, with the roles the caller gave them (a role's note index moves with the note). */
    motive.note_count = segment->last - segment->first + 1;
    for (size_t i = 0; i < motive.note_count; i++) {
        motive.notes[i] = source->notes[segment->first + i];
        if (rational_sub(motive.notes[i].onset, start, &motive.notes[i].onset) != RATIONAL_OK) {
            return false;
        }
    }
    if (rational_sub(end, start, &motive.length) != RATIONAL_OK || !shift_chords(source, start, end, &motive)) {
        return false;
    }
    motive.pickup = (Rational){0, 1};
    motive.feature_count = 0;
    motive.tone_override_count = 0;
    for (size_t t = 0; t < source->tone_override_count; t++) {
        const MotiveToneOverride *override = &source->tone_overrides[t];
        if (override->note >= segment->first && override->note <= segment->last) {
            motive.tone_overrides[motive.tone_override_count] = *override;
            motive.tone_overrides[motive.tone_override_count++].note = override->note - segment->first;
        }
    }
    *out = motive;
    return true;
}
