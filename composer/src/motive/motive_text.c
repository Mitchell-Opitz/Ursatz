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

/* ---------------------------------------------------------------- harmony */

static const char *const NUMERALS[] = {"VII", "VI", "V", "IV", "III", "II", "I"};
static const int NUMERAL_DEGREES[] = {7, 6, 5, 4, 3, 2, 1};

/* The figure of a chord: "" and "6" and "64" for a triad, "7", "65", "43" and "42" for a seventh; NULL when none. */
static const char *chord_figure(int inversion, bool seventh) {
    static const char *const TRIAD[] = {"", "6", "64"};
    static const char *const SEVENTH[] = {"7", "65", "43", "42"};
    if (inversion < 0 || inversion > 3 || (!seventh && inversion > 2)) {
        return NULL;
    }
    return seventh ? SEVENTH[inversion] : TRIAD[inversion];
}

/* Reads "6", "64", "7", "65", "43", "42" or nothing. False when the figure is something else. */
static bool read_figure(const char *figure, int *inversion, bool *seventh) {
    static const char *const FIGURES[] = {"", "6", "64", "7", "65", "43", "42"};
    static const int INVERSIONS[] = {0, 1, 2, 0, 1, 2, 3};
    for (size_t i = 0; i < sizeof(FIGURES) / sizeof(FIGURES[0]); i++) {
        if (strcmp(figure, FIGURES[i]) == 0) {
            *inversion = INVERSIONS[i];
            *seventh = i >= 3;
            return true;
        }
    }
    return false;
}

/* "2", "1/2": beats of the beat unit, as whole notes. False for anything else. */
static bool read_beats(const char *text, int beat_unit, PlanTime *out) {
    char *end = NULL;
    if (*text < '0' || *text > '9') {
        return false;
    }
    long numerator = strtol(text, &end, 10);
    long denominator = 1;
    if (*end == '/') {
        const char *rest = end + 1;
        if (*rest < '0' || *rest > '9') {
            return false;
        }
        denominator = strtol(rest, &end, 10);
        if (denominator < 1) {
            return false;
        }
    }
    return *end == '\0' && numerator <= 1000000L && denominator <= 1000000L &&
           plan_time_make(numerator, (unsigned long)(denominator * beat_unit), out) == PLAN_OK;
}

MotiveResult motive_harmony_parse(const char *text, const PlanMeter *meter, MotiveChord *out, size_t *count,
                                  MotiveError *error) {
    if (!text || !meter || !out || !count) {
        motive_set_error(error, "", "text, meter or output is NULL");
        return MOTIVE_ERR_NULL;
    }
    size_t found = 0;
    size_t number = 0;
    const char *cursor = text;
    while (*cursor) {
        if (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r') {
            cursor++;
            continue;
        }
        const char *start = cursor;
        while (*cursor && *cursor != ' ' && *cursor != '\t' && *cursor != '\n' && *cursor != '\r') {
            cursor++;
        }
        number++;
        char location[MOTIVE_ERROR_LOCATION_MAX];
        snprintf(location, sizeof(location), "token %zu (column %zu)", number, (size_t)(start - text) + 1);
        char token[32];
        size_t length = (size_t)(cursor - start);
        if (length >= sizeof(token)) {
            motive_set_error(error, location, "that is not a chord");
            return MOTIVE_ERR_SYNTAX;
        }
        memcpy(token, start, length);
        token[length] = '\0';
        if (found == MOTIVE_MAX_CHORDS) {
            motive_set_error(error, location, "a motive has at most %d chords", MOTIVE_MAX_CHORDS);
            return MOTIVE_ERR_VALUE;
        }

        MotiveChord chord;
        memset(&chord, 0, sizeof(chord));
        chord.onset = motive_time(0, 1);
        char *at = strchr(token, '@');
        if (at) {
            *at = '\0';
            if (!read_beats(at + 1, meter->beat_unit, &chord.onset)) {
                motive_set_error(error, location, "'%s' is not an onset; write the beats after @, as @2 or @1/2",
                                 at + 1);
                return MOTIVE_ERR_SYNTAX;
            }
        } else if (found > 0) {
            motive_set_error(error, location, "'%s' needs an onset in beats after @, as %s@2", token, token);
            return MOTIVE_ERR_SYNTAX;
        }
        const char *symbol = token;
        if (*symbol == 'b' || *symbol == '#') {
            chord.alteration = *symbol == 'b' ? -1 : 1;
            symbol++;
        }
        bool numeral = false;
        for (size_t k = 0; k < sizeof(NUMERALS) / sizeof(NUMERALS[0]) && !numeral; k++) {
            size_t size = strlen(NUMERALS[k]);
            if (strncmp(symbol, NUMERALS[k], size) == 0) {
                chord.degree = NUMERAL_DEGREES[k];
                symbol += size;
                numeral = true;
            }
        }
        if (!numeral) {
            motive_set_error(error, location,
                             "'%s' is not a chord; start with an upper case numeral I to VII (b or # before it for "
                             "an altered one)",
                             token);
            return MOTIVE_ERR_SYNTAX;
        }
        if (!read_figure(symbol, &chord.inversion, &chord.seventh)) {
            motive_set_error(error, location, "'%s' is not a figure; use 6, 64, 7, 65, 43 or 42 after the numeral",
                             symbol);
            return MOTIVE_ERR_SYNTAX;
        }
        if (found > 0 && plan_time_compare(chord.onset, out[found - 1].onset) <= 0) {
            motive_set_error(error, location, "the chord must come after the one before it");
            return MOTIVE_ERR_VALUE;
        }
        out[found++] = chord;
    }
    *count = found;
    return MOTIVE_OK;
}

static unsigned long gcd(unsigned long a, unsigned long b) {
    while (b != 0) {
        unsigned long rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

bool motive_chord_numeral(const MotiveChord *chord, char *out, size_t size) {
    const char *figure = chord ? chord_figure(chord->inversion, chord->seventh) : NULL;
    size_t numeral = chord && chord->degree >= 1 && chord->degree <= 7 ? (size_t)(7 - chord->degree) : 7;
    if (!figure || numeral == 7 || chord->alteration < -1 || chord->alteration > 1 || !out) {
        return false;
    }
    int written = snprintf(out, size, "%s%s%s", chord->alteration < 0 ? "b" : (chord->alteration > 0 ? "#" : ""),
                           NUMERALS[numeral], figure);
    return written > 0 && (size_t)written < size;
}

char *motive_harmony_to_text(const Motive *motive) {
    if (!motive) {
        return NULL;
    }
    Text text = {NULL, 0, 0, false};
    for (size_t i = 0; i < motive->harmony_count; i++) {
        const MotiveChord *chord = &motive->harmony[i];
        char numeral[MOTIVE_CHORD_NUMERAL_MAX];
        if (!motive_chord_numeral(chord, numeral, sizeof(numeral))) {
            free(text.data);
            return NULL;
        }
        text_add(&text, "%s", numeral);
        /* The beats are the onset over the beat length, in lowest terms. */
        long beats = chord->onset.num * (long)motive->meter.beat_unit;
        unsigned long den = chord->onset.den;
        unsigned long divisor = gcd((unsigned long)(beats < 0 ? -beats : beats), den);
        if (divisor > 1) {
            beats /= (long)divisor;
            den /= divisor;
        }
        if (beats != 0 && den == 1) {
            text_add(&text, "@%ld", beats);
        } else if (beats != 0) {
            text_add(&text, "@%ld/%lu", beats, den);
        }
        text_add(&text, " ");
    }
    trim_end(&text);
    if (!text.failed && !text.data) {
        text.data = calloc(1, 1);
        text.failed = text.data == NULL;
    }
    return text_finish(&text);
}

/* --------------------------------------------------------------- features */

MotiveResult motive_feature_parse(const char *text, MotiveFeature *out, MotiveError *error) {
    if (!text || !out) {
        motive_set_error(error, "", "text or output is NULL");
        return MOTIVE_ERR_NULL;
    }
    const char *colon = strchr(text, ':');
    size_t label_size = colon ? (size_t)(colon - text) : 0;
    if (!colon || label_size < 1 || label_size > MOTIVE_FEATURE_LABEL_MAX) {
        motive_set_error(error, text, "write a feature as label:first-last, with a label of 1 to %d characters, as a:0-2",
                         MOTIVE_FEATURE_LABEL_MAX);
        return MOTIVE_ERR_SYNTAX;
    }
    for (size_t i = 0; i < label_size; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c <= ' ' || c == '"' || c == '\\' || c == 0x7f) {
            motive_set_error(error, text, "a feature label has no spaces, quotes or control characters");
            return MOTIVE_ERR_SYNTAX;
        }
    }
    const char *range = colon + 1;
    char *end = NULL;
    if (*range < '0' || *range > '9') {
        motive_set_error(error, text, "'%s' is not a note range; write 0-2 or 3", range);
        return MOTIVE_ERR_SYNTAX;
    }
    long first = strtol(range, &end, 10);
    long last = first;
    if (*end == '-') {
        const char *rest = end + 1;
        if (*rest < '0' || *rest > '9') {
            motive_set_error(error, text, "'%s' is not a note range; write 0-2 or 3", range);
            return MOTIVE_ERR_SYNTAX;
        }
        last = strtol(rest, &end, 10);
    }
    if (*end != '\0' || first > 100000L || last > 100000L) {
        motive_set_error(error, text, "'%s' is not a note range; write 0-2 or 3", range);
        return MOTIVE_ERR_SYNTAX;
    }
    if (last < first) {
        motive_set_error(error, text, "the range runs backwards: %ld is before %ld", last, first);
        return MOTIVE_ERR_VALUE;
    }
    memset(out, 0, sizeof(*out));
    memcpy(out->label, text, label_size);
    out->first = (size_t)first;
    out->last = (size_t)last;
    return MOTIVE_OK;
}

/* ------------------------------------------------------------------- role */

bool motive_role_parse(const char *name, MotiveRole *out) {
    if (!name || !out) {
        return false;
    }
    if (strcmp(name, "melodic") == 0) {
        *out = MOTIVE_ROLE_MELODIC;
        return true;
    }
    if (strcmp(name, "accompaniment") == 0) {
        *out = MOTIVE_ROLE_ACCOMPANIMENT;
        return true;
    }
    return false;
}

const char *motive_role_name(MotiveRole role) {
    return role == MOTIVE_ROLE_ACCOMPANIMENT ? "accompaniment" : "melodic";
}
