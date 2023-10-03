#include "io/musicxml_importer.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ezxml.h"

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "io/midi_key_signature.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/pitch_spelling.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/exact_rational.h"
#include "time/meter_map.h"
#include "time/note_value.h"
#include "time/time_signature.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * MusicXMLImporter: see musicxml_importer.h and docs/modules/io.md. This is the only file that includes the
 * vendored XML parser (third_party/ezxml), and it does not let one of its types out through the header.
 *
 * One pass per part, in document order. Time is an ExactRational count of whole notes (a duration of d
 * divisions at D divisions per quarter note is d / (4 * D)), so no step rounds. A note is collected first and its
 * NoteEvent is built last, because a tie may still lengthen it.
 */

#define MUSICXML_IMPORTER_PROVENANCE_ID "musicxml-importer-v1"

typedef struct {
    ExactRational onset;
    ExactRational duration;
    PitchLetter letter;
    int accidental;
    int octave;
    long midi;       /* the sounding pitch: 12 * (octave + 1) + the step's semitone + alter */
    size_t part;
    size_t order;    /* document order within the part */
    char voice[32];  /* the notated <voice>, or "" */
} XNote;

typedef struct {
    size_t note;     /* index into notes */
    ExactRational end;
} OpenTie;

typedef struct {
    ExactRational position;
    unsigned beats;
    int exponent;       /* the beat unit is 2^-exponent of a whole note: 0 whole, 2 quarter, 3 eighth */
    size_t part;
} TimeEvent;

typedef struct {
    ExactRational position;
    int fifths;
    MusicXmlModeHint hint;
    size_t part;
} KeyEvent;

typedef struct {
    MusicXmlImport *out;
    TimeEvent *times;
    size_t time_count;
    size_t time_capacity;
    KeyEvent *keys;
    size_t key_count;
    size_t key_capacity;
    bool has_pickup;               /* the first part's first measure is implicit and shorter than a measure */
    ExactRational pickup_length;
    XNote *notes;
    size_t note_count;
    size_t note_capacity;
    OpenTie *ties;
    size_t tie_count;
    size_t tie_capacity;
    MusicXmlImportStatus status;
    /* where the reader is, for messages */
    const char *part_id;
    const char *measure_number;
    size_t current_part;
} Ctx;

/* --- warnings and errors --- */

static void warn(Ctx *ctx, const char *format, ...) {
    MusicXmlImportWarnings *warnings = &ctx->out->warnings;
    warnings->count++;
    if (warnings->message_count >= MUSICXML_IMPORT_MAX_WARNING_MESSAGES) {
        return;
    }
    char text[MUSICXML_IMPORT_WARNING_MESSAGE_SIZE];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    snprintf(warnings->messages[warnings->message_count++], MUSICXML_IMPORT_WARNING_MESSAGE_SIZE, "part %s, measure %s: %s",
             ctx->part_id != NULL ? ctx->part_id : "?", ctx->measure_number != NULL ? ctx->measure_number : "?", text);
}

static MusicXmlImportStatus fail(Ctx *ctx, MusicXmlImportStatus status, const char *format, ...) {
    if (ctx->status != MUSICXML_IMPORT_OK) {
        return ctx->status; /* the first error wins */
    }
    ctx->status = status;
    char text[MUSICXML_IMPORT_ERROR_MESSAGE_SIZE - 64];
    va_list args;
    va_start(args, format);
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    snprintf(ctx->out->error_message, MUSICXML_IMPORT_ERROR_MESSAGE_SIZE, "part %s, measure %s: %s",
             ctx->part_id != NULL ? ctx->part_id : "?", ctx->measure_number != NULL ? ctx->measure_number : "?", text);
    return status;
}

/* --- reading text --- */

/* The trimmed text of an element into buffer; false when it has none. */
static bool read_text(ezxml_t element, char *buffer, size_t size) {
    if (element == NULL) {
        return false;
    }
    const char *text = ezxml_txt(element);
    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        length--;
    }
    if (length == 0 || length >= size) {
        return false;
    }
    memcpy(buffer, text, length);
    buffer[length] = '\0';
    return true;
}

/* A whole number in an element's text; false when absent or not a number. */
static bool read_long(ezxml_t element, long *out) {
    char text[32];
    if (!read_text(element, text, sizeof(text))) {
        return false;
    }
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    *out = value;
    return true;
}

/* --- notes --- */

static bool add_note(Ctx *ctx, const XNote *note, size_t *out_index) {
    if (ctx->note_count == ctx->note_capacity) {
        size_t capacity = ctx->note_capacity == 0 ? 256 : ctx->note_capacity * 2;
        XNote *grown = (XNote *)realloc(ctx->notes, capacity * sizeof(XNote));
        if (grown == NULL) {
            fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
            return false;
        }
        ctx->notes = grown;
        ctx->note_capacity = capacity;
    }
    ctx->notes[ctx->note_count] = *note;
    *out_index = ctx->note_count++;
    return true;
}

static bool add_open_tie(Ctx *ctx, size_t note, ExactRational end) {
    if (ctx->tie_count == ctx->tie_capacity) {
        size_t capacity = ctx->tie_capacity == 0 ? 16 : ctx->tie_capacity * 2;
        OpenTie *grown = (OpenTie *)realloc(ctx->ties, capacity * sizeof(OpenTie));
        if (grown == NULL) {
            fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
            return false;
        }
        ctx->ties = grown;
        ctx->tie_capacity = capacity;
    }
    ctx->ties[ctx->tie_count].note = note;
    ctx->ties[ctx->tie_count].end = end;
    ctx->tie_count++;
    return true;
}

/* Whether the element has a <tie type="..."> (or, with none, a <notations><tied type="...">) of this type. */
static bool has_tie(ezxml_t note, const char *type) {
    bool any_tie = false;
    for (ezxml_t tie = ezxml_child(note, "tie"); tie != NULL; tie = tie->next) {
        any_tie = true;
        const char *value = ezxml_attr(tie, "type");
        if (value != NULL && strcmp(value, type) == 0) {
            return true;
        }
    }
    if (any_tie) {
        return false;
    }
    ezxml_t notations = ezxml_child(note, "notations");
    for (; notations != NULL; notations = notations->next) {
        for (ezxml_t tied = ezxml_child(notations, "tied"); tied != NULL; tied = tied->next) {
            const char *value = ezxml_attr(tied, "type");
            if (value != NULL && strcmp(value, type) == 0) {
                return true;
            }
        }
    }
    return false;
}

static bool read_pitch(Ctx *ctx, ezxml_t pitch, XNote *note) {
    char text[32];
    if (!read_text(ezxml_child(pitch, "step"), text, sizeof(text)) || text[1] != '\0') {
        fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<pitch> without a <step> of A to G");
        return false;
    }
    int semitone = 0;
    switch (text[0]) {
        case 'C': note->letter = PITCH_LETTER_C; semitone = 0; break;
        case 'D': note->letter = PITCH_LETTER_D; semitone = 2; break;
        case 'E': note->letter = PITCH_LETTER_E; semitone = 4; break;
        case 'F': note->letter = PITCH_LETTER_F; semitone = 5; break;
        case 'G': note->letter = PITCH_LETTER_G; semitone = 7; break;
        case 'A': note->letter = PITCH_LETTER_A; semitone = 9; break;
        case 'B': note->letter = PITCH_LETTER_B; semitone = 11; break;
        default:
            fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<step> is \"%s\", not A to G", text);
            return false;
    }
    long octave = 0;
    if (!read_long(ezxml_child(pitch, "octave"), &octave) || octave < -1 || octave > 20) {
        fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<pitch> without a usable <octave>");
        return false;
    }
    long accidental = 0;
    if (ezxml_child(pitch, "alter") != NULL) {
        char alter_text[32];
        char *end = NULL;
        if (!read_text(ezxml_child(pitch, "alter"), alter_text, sizeof(alter_text))) {
            fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<alter> is empty");
            return false;
        }
        double alter = strtod(alter_text, &end);
        if (end == alter_text || *end != '\0' || !isfinite(alter) || fabs(alter) > 4.0) {
            fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<alter> is \"%s\"", alter_text);
            return false;
        }
        accidental = lround(alter);
        if (alter != (double)accidental) {
            warn(ctx, "microtonal <alter> %s rounded to %ld", alter_text, accidental);
        }
    }
    note->accidental = (int)accidental;
    note->octave = (int)octave;
    note->midi = 12 * (octave + 1) + semitone + accidental;
    return true;
}

static bool to_whole_notes(Ctx *ctx, long divisions, long duration, ExactRational *out) {
    if (divisions <= 0) {
        fail(ctx, MUSICXML_IMPORT_ERR_MISSING_DIVISIONS, "a duration appears before any <divisions>");
        return false;
    }
    if (duration < 0) {
        fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "a negative <duration>");
        return false;
    }
    unsigned long long unit = 0;
    if (er_mul_u64(4ull, (unsigned long long)divisions, &unit) != 0) {
        fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "<divisions> %ld is too large", divisions);
        return false;
    }
    *out = er_make((unsigned long long)duration, unit);
    return true;
}

typedef struct {
    bool has_signature;      /* a <time> has been read in this part */
    ExactRational measure_length; /* of that signature, in whole notes */
    long divisions;          /* 0 until the first <divisions> */
    ExactRational cursor;    /* where the next note starts */
    ExactRational reached;   /* the furthest the measure has gone: where the next measure starts */
    ExactRational measure_start;
    ExactRational last_onset;/* the onset of the last note that was not a chord note */
} PartState;

static bool advance(Ctx *ctx, PartState *state, ExactRational by) {
    if (er_add(state->cursor, by, &state->cursor) != 0) {
        fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "the position does not fit");
        return false;
    }
    if (er_cmp(state->cursor, state->reached) > 0) {
        state->reached = state->cursor;
    }
    return true;
}

static bool handle_note(Ctx *ctx, PartState *state, size_t part, size_t *order, ezxml_t note) {
    bool is_chord = ezxml_child(note, "chord") != NULL;
    bool is_grace = ezxml_child(note, "grace") != NULL;
    bool is_cue = ezxml_child(note, "cue") != NULL;
    bool is_rest = ezxml_child(note, "rest") != NULL;
    ezxml_t pitch = ezxml_child(note, "pitch");

    if (is_grace) {
        warn(ctx, "grace note skipped");
        return true; /* a grace note has no duration and takes no time */
    }

    long duration = 0;
    if (!read_long(ezxml_child(note, "duration"), &duration)) {
        fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "a note without a usable <duration>");
        return false;
    }
    ExactRational length = {0, 1};
    if (!to_whole_notes(ctx, state->divisions, duration, &length)) {
        return false;
    }

    ExactRational onset = is_chord ? state->last_onset : state->cursor;
    if (!is_chord) {
        state->last_onset = state->cursor;
    }

    if (is_rest || is_cue || pitch == NULL) {
        if (!is_rest && !is_cue && pitch == NULL) {
            warn(ctx, "unpitched note skipped");
        }
        return is_chord ? true : advance(ctx, state, length);
    }

    XNote parsed;
    memset(&parsed, 0, sizeof(parsed));
    parsed.onset = onset;
    parsed.duration = length;
    parsed.part = part;
    parsed.order = (*order)++;
    if (!read_pitch(ctx, pitch, &parsed)) {
        return false;
    }
    char voice[32];
    if (read_text(ezxml_child(note, "voice"), voice, sizeof(voice))) {
        snprintf(parsed.voice, sizeof(parsed.voice), "%s", voice);
    }

    if (length.n == 0) {
        warn(ctx, "note with a zero duration skipped");
        return is_chord ? true : advance(ctx, state, length);
    }
    if (parsed.midi < 0) {
        warn(ctx, "note below MIDI 0 skipped");
        return is_chord ? true : advance(ctx, state, length);
    }

    bool tie_stop = has_tie(note, "stop");
    bool tie_start = has_tie(note, "start");
    bool merged = false;
    if (tie_stop) {
        for (size_t t = 0; t < ctx->tie_count; t++) {
            XNote *open = &ctx->notes[ctx->ties[t].note];
            if (open->part == part && open->midi == parsed.midi && strcmp(open->voice, parsed.voice) == 0 &&
                er_cmp(ctx->ties[t].end, onset) == 0) {
                ExactRational extended = {0, 1};
                ExactRational end = {0, 1};
                if (er_add(open->duration, length, &extended) != 0 || er_add(onset, length, &end) != 0) {
                    fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "a tied note does not fit");
                    return false;
                }
                open->duration = extended;
                if (tie_start) {
                    ctx->ties[t].end = end;
                } else {
                    ctx->ties[t] = ctx->ties[--ctx->tie_count];
                }
                merged = true;
                break;
            }
        }
        if (!merged) {
            warn(ctx, "tie stop with no matching tie start");
        }
    }
    if (!merged) {
        size_t index = 0;
        ExactRational end = {0, 1};
        if (!add_note(ctx, &parsed, &index) || er_add(onset, length, &end) != 0) {
            if (ctx->status == MUSICXML_IMPORT_OK) {
                fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "a note does not fit");
            }
            return false;
        }
        if (tie_start && !add_open_tie(ctx, index, end)) {
            return false;
        }
    }
    return is_chord ? true : advance(ctx, state, length);
}

static bool handle_move(Ctx *ctx, PartState *state, ezxml_t element, bool backward) {
    long duration = 0;
    if (!read_long(ezxml_child(element, "duration"), &duration)) {
        fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<%s> without a usable <duration>", backward ? "backup" : "forward");
        return false;
    }
    ExactRational length = {0, 1};
    if (!to_whole_notes(ctx, state->divisions, duration, &length)) {
        return false;
    }
    if (!backward) {
        return advance(ctx, state, length);
    }
    if (er_cmp(length, state->cursor) > 0 || er_cmp(state->cursor, state->measure_start) < 0 ||
        er_sub(state->cursor, length, &state->cursor) != 0 || er_cmp(state->cursor, state->measure_start) < 0) {
        warn(ctx, "<backup> goes before the start of the measure; stopped at the start");
        state->cursor = state->measure_start;
    }
    return true;
}

static bool grow(Ctx *ctx, void **array, size_t *capacity, size_t count, size_t element_size) {
    if (count < *capacity) {
        return true;
    }
    size_t grown_capacity = *capacity == 0 ? 8 : *capacity * 2;
    void *grown = realloc(*array, grown_capacity * element_size);
    if (grown == NULL) {
        fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        return false;
    }
    *array = grown;
    *capacity = grown_capacity;
    return true;
}

/* <time>: <beats> (a whole number, or "3+2") and <beat-type> (a power of two). */
static bool handle_time(Ctx *ctx, PartState *state, ezxml_t time) {
    if (ezxml_child(time, "senza-misura") != NULL) {
        return true;
    }
    char beats_text[32];
    long beat_type = 0;
    if (!read_text(ezxml_child(time, "beats"), beats_text, sizeof(beats_text)) ||
        !read_long(ezxml_child(time, "beat-type"), &beat_type)) {
        warn(ctx, "<time> without usable <beats> and <beat-type> skipped");
        return true;
    }
    unsigned long beats = 0;
    bool additive = false;
    bool valid = true;
    for (const char *cursor = beats_text; *cursor != '\0' && valid;) {
        char *end = NULL;
        long part = strtol(cursor, &end, 10);
        if (end == cursor || part <= 0 || part > 1000) {
            valid = false;
            break;
        }
        beats += (unsigned long)part;
        cursor = end;
        if (*cursor == '+') {
            additive = true;
            cursor++;
            if (*cursor == '\0') {
                valid = false;
            }
        } else if (*cursor != '\0') {
            valid = false;
        }
    }
    int exponent = -1;
    for (int e = 0; e <= 7; e++) {
        if (beat_type == (1L << e)) {
            exponent = e;
        }
    }
    if (!valid || beats == 0 || exponent < 0) {
        warn(ctx, "time signature %s/%ld skipped (not a whole number of beats over a power of two)", beats_text, beat_type);
        return true;
    }
    if (additive) {
        warn(ctx, "additive meter %s/%ld summed to %lu/%ld", beats_text, beat_type, beats, beat_type);
    }
    if (!grow(ctx, (void **)&ctx->times, &ctx->time_capacity, ctx->time_count, sizeof(TimeEvent))) {
        return false;
    }
    TimeEvent *event = &ctx->times[ctx->time_count++];
    event->position = state->cursor;
    event->beats = (unsigned)beats;
    event->exponent = exponent;
    event->part = ctx->current_part;
    state->has_signature = true;
    state->measure_length = er_make(beats, 1ull << exponent);
    return true;
}

/* <key>: <fifths> and an optional <mode>. */
static bool handle_key(Ctx *ctx, PartState *state, ezxml_t key) {
    long fifths = 0;
    if (!read_long(ezxml_child(key, "fifths"), &fifths) || fifths < -7 || fifths > 7) {
        warn(ctx, "<key> without <fifths> from -7 to 7 skipped");
        return true;
    }
    MusicXmlModeHint hint = MUSICXML_MODE_HINT_UNKNOWN;
    char mode[32];
    if (read_text(ezxml_child(key, "mode"), mode, sizeof(mode))) {
        if (strcmp(mode, "major") == 0) {
            hint = MUSICXML_MODE_HINT_MAJOR;
        } else if (strcmp(mode, "minor") == 0) {
            hint = MUSICXML_MODE_HINT_MINOR;
        }
    }
    if (!grow(ctx, (void **)&ctx->keys, &ctx->key_capacity, ctx->key_count, sizeof(KeyEvent))) {
        return false;
    }
    KeyEvent *event = &ctx->keys[ctx->key_count++];
    event->position = state->cursor;
    event->fifths = (int)fifths;
    event->hint = hint;
    event->part = ctx->current_part;
    return true;
}

static bool handle_attributes(Ctx *ctx, PartState *state, ezxml_t attributes) {
    if (ezxml_child(attributes, "divisions") != NULL) {
        long divisions = 0;
        if (!read_long(ezxml_child(attributes, "divisions"), &divisions) || divisions <= 0) {
            fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<divisions> is not a positive whole number");
            return false;
        }
        state->divisions = divisions;
    }
    ezxml_t time = ezxml_child(attributes, "time");
    if (time != NULL && !handle_time(ctx, state, time)) {
        return false;
    }
    ezxml_t key = ezxml_child(attributes, "key");
    if (key != NULL && !handle_key(ctx, state, key)) {
        return false;
    }
    return true;
}

static bool process_part(Ctx *ctx, ezxml_t part, size_t part_index) {
    PartState state;
    memset(&state, 0, sizeof(state));
    state.cursor = state.reached = state.measure_start = state.last_onset = er_make(0, 1);
    size_t order = 0;
    bool first_measure = true;
    ctx->tie_count = 0;
    ctx->current_part = part_index;

    for (ezxml_t measure = ezxml_child(part, "measure"); measure != NULL; measure = measure->next) {
        const char *number = ezxml_attr(measure, "number");
        ctx->measure_number = number != NULL ? number : "?";
        state.cursor = state.reached = state.last_onset = state.measure_start;

        for (ezxml_t element = measure->child; element != NULL; element = element->ordered) {
            const char *name = ezxml_name(element);
            bool ok = true;
            if (strcmp(name, "attributes") == 0) {
                ok = handle_attributes(ctx, &state, element);
            } else if (strcmp(name, "note") == 0) {
                ok = handle_note(ctx, &state, part_index, &order, element);
            } else if (strcmp(name, "backup") == 0) {
                ok = handle_move(ctx, &state, element, true);
            } else if (strcmp(name, "forward") == 0) {
                ok = handle_move(ctx, &state, element, false);
            }
            if (!ok) {
                return false;
            }
        }
        ExactRational length = {0, 1};
        if (er_sub(state.reached, state.measure_start, &length) != 0) {
            return fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "the measure length does not fit") == MUSICXML_IMPORT_OK;
        }
        const char *implicit = ezxml_attr(measure, "implicit");
        bool is_implicit = implicit != NULL && strcmp(implicit, "yes") == 0;
        if (first_measure) {
            first_measure = false;
            if (part_index == 0 && is_implicit && length.n > 0 &&
                (!state.has_signature || er_cmp(length, state.measure_length) < 0)) {
                ctx->has_pickup = true;
                ctx->pickup_length = length;
            }
        }
        if (state.has_signature && !is_implicit && er_cmp(length, state.measure_length) != 0) {
            warn(ctx, "measure is %llu/%llu of a whole note, its time signature's is %llu/%llu",
                 length.n, length.d, state.measure_length.n, state.measure_length.d);
        }
        state.measure_start = state.reached;
    }
    if (ctx->tie_count > 0) {
        warn(ctx, "%zu tie start(s) never stopped", ctx->tie_count);
    }
    return true;
}

/* --- building the NoteEvents --- */

typedef struct {
    ExactRational onset;
    size_t part;
    size_t order;
    size_t index; /* into the collected notes */
} SortItem;

/* Time order, then part, then document order: stable, and the same for the same input. */
static int compare_items(const void *a, const void *b) {
    const SortItem *x = (const SortItem *)a;
    const SortItem *y = (const SortItem *)b;
    int by_onset = er_cmp(x->onset, y->onset);
    if (by_onset != 0) {
        return by_onset;
    }
    if (x->part != y->part) {
        return x->part < y->part ? -1 : 1;
    }
    return x->order < y->order ? -1 : (x->order > y->order ? 1 : 0);
}

static bool build_event(Ctx *ctx, const XNote *note, const char *part_id, const TuningSystem *tuning, NoteEvent **out_event,
                        PitchNumericValue **out_value) {
    EntityID *id = NULL;
    MusicalTime *position = NULL;
    TemporalAnchor *anchor = NULL;
    Duration *duration = NULL;
    PitchNumericValue *numeric = NULL;
    PitchIdentity *pitch = NULL;
    PitchSpelling *spelling = NULL;
    EntityID *provenance_id = NULL;
    ProvenanceReference *provenance = NULL;
    NoteEvent *event = NULL;

    char id_text[128];
    snprintf(id_text, sizeof(id_text), "musicxml-%s-%zu", part_id, note->order);
    position = er_to_time(note->onset);
    bool fits = note->duration.n <= (unsigned long long)LONG_MAX && note->duration.d <= (unsigned long long)ULONG_MAX;
    bool ok = position != NULL && fits && entity_id_create(id_text, &id) == ENTITY_ID_OK &&
              temporal_anchor_create(position, &anchor) == TEMPORAL_ANCHOR_OK &&
              duration_create((long)note->duration.n, (unsigned long)note->duration.d, &duration) == DURATION_OK &&
              pitch_numeric_value_create(tuning, note->midi, 1, &numeric) == PITCH_NUMERIC_VALUE_OK &&
              pitch_numeric_value_to_identity(numeric, &pitch) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK &&
              pitch_spelling_create(note->letter, note->accidental, note->octave, &spelling) == PITCH_SPELLING_OK &&
              entity_id_create(MUSICXML_IMPORTER_PROVENANCE_ID, &provenance_id) == ENTITY_ID_OK &&
              provenance_reference_create_present(provenance_id, &provenance) == PROVENANCE_REFERENCE_OK;
    if (ok) {
        provenance_id = NULL; /* owned by the provenance reference */
        ok = note_event_create(id, anchor, duration, pitch, spelling, NULL, provenance, &event) == NOTE_EVENT_OK;
    }
    if (!ok) {
        if (!fits || position == NULL) {
            fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "a note's position or duration does not fit");
        } else {
            fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        }
        entity_id_destroy(id);
        temporal_anchor_destroy(anchor);
        if (anchor == NULL) {
            musical_time_destroy(position);
        }
        duration_destroy(duration);
        pitch_identity_destroy(pitch);
        pitch_spelling_destroy(spelling);
        pitch_numeric_value_destroy(numeric);
        entity_id_destroy(provenance_id);
        provenance_reference_destroy(provenance);
        return false;
    }
    *out_event = event;
    *out_value = numeric;
    return true;
}

static void clear_import(MusicXmlImport *import) {
    memset(import, 0, sizeof(*import));
}

void musicxml_import_destroy(MusicXmlImport *import) {
    if (import == NULL) {
        return;
    }
    for (size_t i = 0; i < import->count; i++) {
        note_event_destroy(import->events != NULL ? import->events[i] : NULL);
        pitch_numeric_value_destroy(import->pitch_values != NULL ? import->pitch_values[i] : NULL);
    }
    free(import->events);
    free(import->pitch_values);
    free(import->part_index);
    meter_map_destroy(import->meter_map);
    for (size_t i = 0; i < import->key_signature_count; i++) {
        key_signature_destroy(import->key_signatures[i].key_signature);
        musical_time_destroy(import->key_signatures[i].position);
    }
    free(import->key_signatures);
    clear_import(import);
}

/* --- the meter map and the key signature events --- */

static int compare_time_events(const void *a, const void *b) {
    const TimeEvent *x = (const TimeEvent *)a;
    const TimeEvent *y = (const TimeEvent *)b;
    int by_position = er_cmp(x->position, y->position);
    if (by_position != 0) {
        return by_position;
    }
    return x->part < y->part ? -1 : (x->part > y->part ? 1 : 0);
}

static int compare_key_events(const void *a, const void *b) {
    const KeyEvent *x = (const KeyEvent *)a;
    const KeyEvent *y = (const KeyEvent *)b;
    int by_position = er_cmp(x->position, y->position);
    if (by_position != 0) {
        return by_position;
    }
    return x->part < y->part ? -1 : (x->part > y->part ? 1 : 0);
}

static bool same_signature(const TimeEvent *a, const TimeEvent *b) {
    return a->beats == b->beats && a->exponent == b->exponent;
}

static bool build_meter_map(Ctx *ctx) {
    MusicXmlImport *out = ctx->out;
    EntityID *map_id = NULL;
    if (entity_id_create("musicxml-meter-map", &map_id) != ENTITY_ID_OK) {
        return fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
    }
    if (ctx->time_count == 0) {
        if (meter_map_create_defaulted(map_id, &out->meter_map) != METER_MAP_OK) {
            entity_id_destroy(map_id);
            return fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
        }
        return true;
    }

    qsort(ctx->times, ctx->time_count, sizeof(TimeEvent), compare_time_events);
    /* the signatures kept: one per position (the lowest part's wins a conflict), plus the pickup's end */
    TimeEvent *kept = (TimeEvent *)calloc(ctx->time_count + 1, sizeof(TimeEvent));
    if (kept == NULL) {
        entity_id_destroy(map_id);
        return fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
    }
    size_t kept_count = 0;
    for (size_t i = 0; i < ctx->time_count; i++) {
        if (kept_count > 0 && er_cmp(kept[kept_count - 1].position, ctx->times[i].position) == 0) {
            if (!same_signature(&kept[kept_count - 1], &ctx->times[i])) {
                warn(ctx, "parts disagree about the time signature at one position; part %zu's is used",
                     kept[kept_count - 1].part);
            }
            continue;
        }
        kept[kept_count++] = ctx->times[i];
    }
    if (ctx->has_pickup) {
        if (kept[0].position.n != 0) {
            warn(ctx, "the implicit first measure is not read as a pickup: no time signature starts the piece");
        } else if (kept_count == 1 || er_cmp(kept[1].position, ctx->pickup_length) > 0) {
            /* nothing ends the pickup's region where the pickup ends: end it there, with the same signature, which
               is what the MeterMap reads as a short first region (a pickup) */
            for (size_t m = kept_count; m > 1; m--) {
                kept[m] = kept[m - 1];
            }
            kept[1] = kept[0];
            kept[1].position = ctx->pickup_length;
            kept_count++;
        }
    }

    MeterMapChangePoint *points = (MeterMapChangePoint *)calloc(kept_count, sizeof(MeterMapChangePoint));
    MusicalTime **starts = (MusicalTime **)calloc(kept_count, sizeof(MusicalTime *));
    TimeSignature **signatures = (TimeSignature **)calloc(kept_count, sizeof(TimeSignature *));
    bool ok = points != NULL && starts != NULL && signatures != NULL;
    for (size_t i = 0; i < kept_count && ok; i++) {
        NoteValue *unit = NULL;
        starts[i] = er_to_time(kept[i].position);
        ok = starts[i] != NULL && note_value_create(kept[i].exponent, &unit) == NOTE_VALUE_OK &&
             time_signature_create(kept[i].beats, unit, &signatures[i]) == TIME_SIGNATURE_OK;
        note_value_destroy(unit);
        points[i].time_signature = signatures[i];
        points[i].start = starts[i];
    }
    ok = ok && meter_map_create(map_id, points, kept_count, &out->meter_map) == METER_MAP_OK;
    for (size_t i = 0; starts != NULL && signatures != NULL && i < kept_count; i++) {
        musical_time_destroy(starts[i]);
        time_signature_destroy(signatures[i]);
    }
    free(starts);
    free(signatures);
    free(points);
    free(kept);
    if (!ok) {
        entity_id_destroy(map_id);
        return fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "the time signatures could not form a meter map") == MUSICXML_IMPORT_OK;
    }
    return true;
}

static bool build_key_signatures(Ctx *ctx) {
    MusicXmlImport *out = ctx->out;
    if (ctx->key_count == 0) {
        return true;
    }
    qsort(ctx->keys, ctx->key_count, sizeof(KeyEvent), compare_key_events);
    out->key_signatures = (MusicXmlKeySignatureEvent *)calloc(ctx->key_count, sizeof(MusicXmlKeySignatureEvent));
    if (out->key_signatures == NULL) {
        return fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
    }
    for (size_t i = 0; i < ctx->key_count; i++) {
        const KeyEvent *event = &ctx->keys[i];
        bool duplicate = false;
        for (size_t k = 0; k < out->key_signature_count && !duplicate; k++) {
            const MusicXmlKeySignatureEvent *seen = &out->key_signatures[k];
            MusicalTime *position = er_to_time(event->position);
            duplicate = position != NULL && musical_time_equals(seen->position, position) &&
                        seen->sharps_flats == event->fifths && seen->mode_hint == event->hint;
            musical_time_destroy(position);
        }
        if (duplicate) {
            continue;
        }
        MusicXmlKeySignatureEvent *slot = &out->key_signatures[out->key_signature_count];
        slot->position = er_to_time(event->position);
        slot->sharps_flats = (int8_t)event->fifths;
        slot->mode_hint = event->hint;
        slot->part_index = event->part;
        if (slot->position == NULL || midi_key_signature_build(event->fifths, &slot->key_signature) != MIDI_KEY_SIGNATURE_BUILD_OK) {
            musical_time_destroy(slot->position);
            slot->position = NULL;
            return fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory") == MUSICXML_IMPORT_OK;
        }
        out->key_signature_count++;
    }
    return true;
}

/* --- the document --- */

static MusicXmlImportStatus import_document(Ctx *ctx, ezxml_t root) {
    const char *root_name = ezxml_name(root);
    if (root_name == NULL || strcmp(root_name, "score-partwise") != 0) {
        if (root_name != NULL && strcmp(root_name, "score-timewise") == 0) {
            return fail(ctx, MUSICXML_IMPORT_ERR_WRONG_ROOT, "score-timewise is not supported, only score-partwise");
        }
        return fail(ctx, MUSICXML_IMPORT_ERR_WRONG_ROOT, "the root element is <%s>, not <score-partwise>",
                    root_name != NULL ? root_name : "");
    }

    size_t part_count = 0;
    char **part_ids = NULL;
    for (ezxml_t part = ezxml_child(root, "part"); part != NULL; part = part->next) {
        part_count++;
    }
    part_ids = (char **)calloc(part_count > 0 ? part_count : 1, sizeof(char *));
    if (part_ids == NULL) {
        return fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
    }
    size_t index = 0;
    for (ezxml_t part = ezxml_child(root, "part"); part != NULL && ctx->status == MUSICXML_IMPORT_OK;
         part = part->next, index++) {
        const char *id = ezxml_attr(part, "id");
        char fallback[32];
        snprintf(fallback, sizeof(fallback), "part-%zu", index);
        part_ids[index] = strdup(id != NULL && id[0] != '\0' ? id : fallback);
        if (part_ids[index] == NULL) {
            fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
            break;
        }
        ctx->part_id = part_ids[index];
        ctx->measure_number = NULL;
        process_part(ctx, part, index);
    }
    ctx->part_id = NULL;

    MusicXmlImport *out = ctx->out;
    if (ctx->status == MUSICXML_IMPORT_OK) {
        build_meter_map(ctx);
    }
    if (ctx->status == MUSICXML_IMPORT_OK) {
        build_key_signatures(ctx);
    }
    if (ctx->status == MUSICXML_IMPORT_OK) {
        size_t count = ctx->note_count;
        SortItem *order = (SortItem *)calloc(count > 0 ? count : 1, sizeof(SortItem));
        out->events = (NoteEvent **)calloc(count > 0 ? count : 1, sizeof(NoteEvent *));
        out->pitch_values = (PitchNumericValue **)calloc(count > 0 ? count : 1, sizeof(PitchNumericValue *));
        out->part_index = (size_t *)calloc(count > 0 ? count : 1, sizeof(size_t));
        EntityID *tuning_id = NULL;
        TuningSystem *tuning = NULL;
        if (order == NULL || out->events == NULL || out->pitch_values == NULL || out->part_index == NULL ||
            entity_id_create("midi-note-number", &tuning_id) != ENTITY_ID_OK) {
            fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        } else if (tuning_system_create(tuning_id, "MIDI note number", &tuning) != TUNING_SYSTEM_OK) {
            entity_id_destroy(tuning_id);
            fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        }
        if (ctx->status == MUSICXML_IMPORT_OK) {
            for (size_t i = 0; i < count; i++) {
                order[i].onset = ctx->notes[i].onset;
                order[i].part = ctx->notes[i].part;
                order[i].order = ctx->notes[i].order;
                order[i].index = i;
            }
            qsort(order, count, sizeof(SortItem), compare_items);
            for (size_t i = 0; i < count && ctx->status == MUSICXML_IMPORT_OK; i++) {
                const XNote *note = &ctx->notes[order[i].index];
                if (build_event(ctx, note, part_ids[note->part], tuning, &out->events[i], &out->pitch_values[i])) {
                    out->part_index[i] = note->part;
                    out->count = i + 1;
                }
            }
        }
        tuning_system_destroy(tuning);
        free(order);
    }
    for (size_t i = 0; i < part_count; i++) {
        free(part_ids[i]);
    }
    free(part_ids);
    return ctx->status;
}

MusicXmlImportStatus musicxml_import_buffer(const char *data, size_t size, MusicXmlImport *out) {
    if (out == NULL) {
        return MUSICXML_IMPORT_ERR_NULL_ARGUMENT;
    }
    clear_import(out);
    if (data == NULL) {
        snprintf(out->error_message, sizeof(out->error_message), "no document");
        return MUSICXML_IMPORT_ERR_NULL_ARGUMENT;
    }
    if (size > MUSICXML_IMPORT_MAX_BYTES) {
        snprintf(out->error_message, sizeof(out->error_message), "the document is %zu bytes, over the %u byte limit", size,
                 (unsigned)MUSICXML_IMPORT_MAX_BYTES);
        return MUSICXML_IMPORT_ERR_TOO_LARGE;
    }

    /* the parser edits its buffer in place: give it a copy, without a UTF-8 byte order mark or trailing space */
    const char *start = data;
    size_t length = size;
    if (length >= 3 && (unsigned char)start[0] == 0xEF && (unsigned char)start[1] == 0xBB && (unsigned char)start[2] == 0xBF) {
        start += 3;
        length -= 3;
    }
    while (length > 0 && isspace((unsigned char)start[length - 1])) {
        length--;
    }
    bool utf16 = length >= 2 && (((unsigned char)start[0] == 0xFF && (unsigned char)start[1] == 0xFE) ||
                                 ((unsigned char)start[0] == 0xFE && (unsigned char)start[1] == 0xFF));
    if (length == 0 || (!utf16 && memchr(start, '\0', length) != NULL)) {
        snprintf(out->error_message, sizeof(out->error_message), "malformed XML: %s",
                 length == 0 ? "the document is empty" : "the document contains a NUL byte");
        return MUSICXML_IMPORT_ERR_MALFORMED_XML;
    }
    char *copy = (char *)malloc(length + 2);
    if (copy == NULL) {
        snprintf(out->error_message, sizeof(out->error_message), "out of memory");
        return MUSICXML_IMPORT_ERR_ALLOC;
    }
    memcpy(copy, start, length);
    copy[length] = '\0';
    copy[length + 1] = '\0';

    ezxml_t root = ezxml_parse_str(copy, length);
    Ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.out = out;
    ctx.status = MUSICXML_IMPORT_OK;
    MusicXmlImportStatus status = MUSICXML_IMPORT_OK;
    if (root == NULL) {
        status = fail(&ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
    } else if (ezxml_error(root)[0] != '\0') {
        snprintf(out->error_message, sizeof(out->error_message), "malformed XML: %s", ezxml_error(root));
        ctx.status = status = MUSICXML_IMPORT_ERR_MALFORMED_XML;
    } else {
        status = import_document(&ctx, root);
    }
    ezxml_free(root);
    free(copy);
    free(ctx.notes);
    free(ctx.ties);
    free(ctx.times);
    free(ctx.keys);
    if (status != MUSICXML_IMPORT_OK) {
        char message[MUSICXML_IMPORT_ERROR_MESSAGE_SIZE];
        snprintf(message, sizeof(message), "%s", out->error_message);
        musicxml_import_destroy(out);
        snprintf(out->error_message, sizeof(out->error_message), "%s", message);
    }
    return status;
}

MusicXmlImportStatus musicxml_import_file(const char *path, MusicXmlImport *out) {
    if (out == NULL) {
        return MUSICXML_IMPORT_ERR_NULL_ARGUMENT;
    }
    clear_import(out);
    FILE *file = path != NULL ? fopen(path, "rb") : NULL;
    if (file == NULL) {
        snprintf(out->error_message, sizeof(out->error_message), "cannot open \"%s\"", path != NULL ? path : "(null)");
        return MUSICXML_IMPORT_ERR_IO;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        snprintf(out->error_message, sizeof(out->error_message), "cannot read \"%s\"", path);
        return MUSICXML_IMPORT_ERR_IO;
    }
    long size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        snprintf(out->error_message, sizeof(out->error_message), "cannot read \"%s\"", path);
        return MUSICXML_IMPORT_ERR_IO;
    }
    if ((unsigned long)size > MUSICXML_IMPORT_MAX_BYTES) {
        fclose(file);
        snprintf(out->error_message, sizeof(out->error_message), "\"%s\" is %ld bytes, over the %u byte limit", path, size,
                 (unsigned)MUSICXML_IMPORT_MAX_BYTES);
        return MUSICXML_IMPORT_ERR_TOO_LARGE;
    }
    char *buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        snprintf(out->error_message, sizeof(out->error_message), "out of memory");
        return MUSICXML_IMPORT_ERR_ALLOC;
    }
    size_t read_count = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(buffer);
        snprintf(out->error_message, sizeof(out->error_message), "cannot read \"%s\"", path);
        return MUSICXML_IMPORT_ERR_IO;
    }
    MusicXmlImportStatus status = musicxml_import_buffer(buffer, (size_t)size, out);
    free(buffer);
    return status;
}
