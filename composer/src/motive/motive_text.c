#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_internal.h"

/*
 * The motive written back as text: a note list, a degree list, a rhythm or a contour (see motive.h). Each is read
 * back by its reader into the same notes.
 */

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
    bool failed;
} Text;

static void text_add(Text *text, const char *format, ...) {
    if (text->failed) {
        return;
    }
    char piece[64];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(piece, sizeof(piece), format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= sizeof(piece)) {
        text->failed = true;
        return;
    }
    if (text->size + (size_t)length + 1 > text->capacity) {
        size_t capacity = text->capacity ? text->capacity * 2 : 256;
        while (capacity < text->size + (size_t)length + 1) {
            capacity *= 2;
        }
        char *grown = realloc(text->data, capacity);
        if (!grown) {
            text->failed = true;
            return;
        }
        text->data = grown;
        text->capacity = capacity;
    }
    memcpy(text->data + text->size, piece, (size_t)length + 1);
    text->size += (size_t)length;
}

static char *text_finish(Text *text) {
    if (text->failed) {
        free(text->data);
        return NULL;
    }
    return text->data;
}

/* The longest plain or dotted value (w to t, up to two dots) that is not longer than time; false when none is. */
static bool longest_value(PlanTime time, char *letter, int *dots, PlanTime *value) {
    static const char LETTERS[] = {'w', 'h', 'q', 'e', 's', 't'};
    bool found = false;
    for (unsigned k = 0; k < sizeof(LETTERS); k++) {
        for (unsigned d = 0; d < 3; d++) {
            long numerator = d == 0 ? 1 : (d == 1 ? 3 : 7);
            unsigned long denominator = (1UL << k) * (d == 0 ? 1UL : (d == 1 ? 2UL : 4UL));
            PlanTime candidate = motive_time(numerator, denominator);
            if (plan_time_compare(candidate, time) <= 0 &&
                (!found || plan_time_compare(candidate, *value) > 0)) {
                *letter = LETTERS[k];
                *dots = (int)d;
                *value = candidate;
                found = true;
            }
        }
    }
    return found;
}

/*
 * Writes time as one or more tokens "<head>:<duration>", all but the last tied when tie is set, each followed by a
 * space. False when time is not a sum of values from w to t.
 */
static bool add_pieces(Text *text, const char *head, PlanTime time, bool tie) {
    while (time.num > 0) {
        char letter;
        int dots;
        PlanTime value;
        if (!longest_value(time, &letter, &dots, &value)) {
            return false;
        }
        time = motive_time_minus(time, value);
        text_add(text, "%s:%c%.*s%s ", head, letter, dots, "..", tie && time.num > 0 ? "~" : "");
    }
    return true;
}

static void trim_end(Text *text) {
    if (text->size > 0 && text->data[text->size - 1] == ' ') {
        text->data[--text->size] = '\0';
    }
}

typedef enum { WRITE_NOTES, WRITE_DEGREES, WRITE_RHYTHM } ListKind;

static bool add_head(const Motive *motive, ListKind kind, int pitch, char *head, size_t size) {
    switch (kind) {
    case WRITE_NOTES:
        motive_pitch_name(pitch, head, size);
        return true;
    case WRITE_RHYTHM:
        snprintf(head, size, "x");
        return true;
    case WRITE_DEGREES: {
        /* The reader puts octave 0 at MIDI 60 to 71 for the tonic's pitch class, so shift 60 down first. */
        long position;
        int alteration;
        motive_scale_position(&motive->key, pitch - 60, &position, &alteration);
        long octave = position / 7;
        long degree = position % 7;
        if (degree < 0) {
            degree += 7;
            octave--;
        }
        size_t at = 0;
        if (alteration > 0) {
            head[at++] = '#';
        }
        head[at++] = (char)('1' + degree);
        for (long i = 0; i < (octave < 0 ? -octave : octave) && at + 1 < size; i++) {
            head[at++] = octave < 0 ? ',' : '\'';
        }
        head[at] = '\0';
        return true;
    }
    }
    return false;
}

static char *write_list(const Motive *motive, ListKind kind) {
    if (!motive || motive->note_count == 0) {
        return NULL;
    }
    if (kind == WRITE_DEGREES && motive->key_source == MOTIVE_KEY_UNSPECIFIED) {
        return NULL;
    }
    Text text = {NULL, 0, 0, false};
    PlanTime cursor = motive_time(0, 1);
    for (size_t i = 0; i < motive->note_count; i++) {
        const PlanNote *note = &motive->notes[i];
        if (plan_time_compare(note->offset, cursor) > 0 &&
            !add_pieces(&text, "r", motive_time_minus(note->offset, cursor), false)) {
            free(text.data);
            return NULL;
        }
        char head[24];
        if (!add_head(motive, kind, note->midi_pitch, head, sizeof(head)) ||
            !add_pieces(&text, head, note->duration, true)) {
            free(text.data);
            return NULL;
        }
        cursor = motive_note_end(note);
    }
    trim_end(&text);
    return text_finish(&text);
}

char *motive_to_notes_text(const Motive *motive) {
    return write_list(motive, WRITE_NOTES);
}

char *motive_to_degrees_text(const Motive *motive) {
    return write_list(motive, WRITE_DEGREES);
}

char *motive_to_rhythm_text(const Motive *motive) {
    return write_list(motive, WRITE_RHYTHM);
}

char *motive_to_contour_text(const Motive *motive) {
    if (!motive || motive->note_count == 0) {
        return NULL;
    }
    Text text = {NULL, 0, 0, false};
    for (size_t i = 0; i < motive->note_count; i++) {
        /* The rank is how many distinct lower pitches the motive has. */
        size_t rank = 0;
        for (size_t j = 0; j < motive->note_count; j++) {
            if (motive->notes[j].midi_pitch >= motive->notes[i].midi_pitch) {
                continue;
            }
            bool seen = false;
            for (size_t k = 0; k < j && !seen; k++) {
                seen = motive->notes[k].midi_pitch == motive->notes[j].midi_pitch;
            }
            rank += seen ? 0 : 1;
        }
        text_add(&text, "%zu ", rank);
    }
    trim_end(&text);
    return text_finish(&text);
}
