#include "io/musicxml_importer.h"
#include <common/files.h>
#include <common/hash.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezxml.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/core/provenance_reference.h"
#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "io/midi_key_signature.h"
#include "model/pitch/pitch_identity.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/pitch_spelling.h"
#include "model/pitch/tuning_system.h"
#include "model/time/duration.h"
#include "model/core/rational.h"
#include "model/time/meter_map.h"
#include "model/time/note_value.h"
#include "model/time/tempo_map.h"
#include "model/time/time_span.h"
#include "model/time/tuplet.h"
#include "model/events/dynamics.h"
#include "model/events/tempo_marking.h"
#include "model/time/metronome_marking.h"
#include "model/time/time_signature.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"
#include "io/musicxml_import_internal.h"

static bool add_open_tie(Ctx *ctx, size_t note, Rational end) {
    if (ctx->tie_count == ctx->tie_capacity) {
        size_t capacity = ctx->tie_capacity == 0 ? 16 : ctx->tie_capacity * 2;
        OpenTie *grown = (OpenTie *)realloc(ctx->ties, capacity * sizeof(OpenTie));
        if (grown == NULL) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
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
    if (!musicxml_import_read_text(ezxml_child(pitch, "step"), text, sizeof(text)) || text[1] != '\0') {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<pitch> without a <step> of A to G");
        return false;
    }
    if (!pitch_letter_from_char(text[0], &note->letter)) {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<step> is \"%s\", not A to G", text);
        return false;
    }
    int semitone = pitch_letter_semitone(note->letter);
    long octave = 0;
    if (!musicxml_import_read_long(ezxml_child(pitch, "octave"), &octave) || octave < -1 || octave > 20) {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<pitch> without a usable <octave>");
        return false;
    }
    long accidental = 0;
    if (ezxml_child(pitch, "alter") != NULL) {
        char alter_text[32];
        char *end = NULL;
        if (!musicxml_import_read_text(ezxml_child(pitch, "alter"), alter_text, sizeof(alter_text))) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<alter> is empty");
            return false;
        }
        double alter = strtod(alter_text, &end);
        if (end == alter_text || *end != '\0' || !isfinite(alter) || fabs(alter) > 4.0) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<alter> is \"%s\"", alter_text);
            return false;
        }
        accidental = lround(alter);
        if (alter != (double)accidental) {
            musicxml_import_warn(ctx, "microtonal <alter> %s rounded to %ld", alter_text, accidental);
        }
    }
    note->accidental = (int)accidental;
    note->octave = (int)octave;
    note->midi = 12 * (octave + 1) + semitone + accidental;
    return true;
}

static bool to_whole_notes(Ctx *ctx, long divisions, long duration, Rational *out) {
    if (divisions <= 0) {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_MISSING_DIVISIONS, "a duration appears before any <divisions>");
        return false;
    }
    if (duration < 0) {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "a negative <duration>");
        return false;
    }
    if (divisions > LONG_MAX / 4 || rational_make(duration, 4LL * divisions, out) != RATIONAL_OK) {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "<divisions> %ld is too large", divisions);
        return false;
    }
    return true;
}

typedef struct {
    bool has_signature;      /* a <time> has been read in this part */
    Rational measure_length; /* of that signature, in whole notes */
    long divisions;          /* 0 until the first <divisions> */
    Rational cursor;    /* where the next note starts */
    Rational reached;   /* the furthest the measure has gone: where the next measure starts */
    Rational measure_start;
    Rational last_onset;/* the onset of the last note that was not a chord note */
    long transpose;          /* semitones from written to sounding pitch */
    int staves;              /* <staves> */
} PartState;

static bool advance(Ctx *ctx, PartState *state, Rational by) {
    if (rational_add(state->cursor, by, &state->cursor) != RATIONAL_OK) {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "the position does not fit");
        return false;
    }
    if (rational_cmp(state->cursor, state->reached) > 0) {
        state->reached = state->cursor;
    }
    return true;
}

/* <time-modification> is the notated tuplet: <actual-notes> in the time of <normal-notes>. One that is not a Tuplet
   (counts outside 2 to 64 and 1 to 64, or equal) is warned about and left off; the duration stays exact. */
static void read_time_modification(Ctx *ctx, ezxml_t note, XNote *parsed) {
    ezxml_t modification = ezxml_child(note, "time-modification");
    if (modification == NULL) {
        return;
    }
    long actual = 0;
    long normal = 0;
    if (!musicxml_import_read_long(ezxml_child(modification, "actual-notes"), &actual) ||
        !musicxml_import_read_long(ezxml_child(modification, "normal-notes"), &normal) || actual < TUPLET_MIN_ACTUAL ||
        actual > TUPLET_MAX_COUNT || normal < 1 || normal > TUPLET_MAX_COUNT || actual == normal) {
        musicxml_import_warn(ctx, "<time-modification> is not a tuplet ratio; left off");
        return;
    }
    parsed->tuplet_actual = (unsigned)actual;
    parsed->tuplet_normal = (unsigned)normal;
}

static bool handle_note(Ctx *ctx, PartState *state, size_t part, size_t *order, ezxml_t note) {
    bool is_chord = ezxml_child(note, "chord") != NULL;
    bool is_grace = ezxml_child(note, "grace") != NULL;
    bool is_cue = ezxml_child(note, "cue") != NULL;
    bool is_rest = ezxml_child(note, "rest") != NULL;
    ezxml_t pitch = ezxml_child(note, "pitch");

    if (is_grace) {
        musicxml_import_warn(ctx, "grace note skipped");
        return true; /* a grace note has no duration and takes no time */
    }

    long duration = 0;
    if (!musicxml_import_read_long(ezxml_child(note, "duration"), &duration)) {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "a note without a usable <duration>");
        return false;
    }
    Rational length = {0, 1};
    if (!to_whole_notes(ctx, state->divisions, duration, &length)) {
        return false;
    }

    Rational onset = is_chord ? state->last_onset : state->cursor;
    if (!is_chord) {
        state->last_onset = state->cursor;
    }

    if (is_rest || is_cue || pitch == NULL) {
        if (!is_rest && !is_cue && pitch == NULL) {
            musicxml_import_warn(ctx, "unpitched note skipped");
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
    if (musicxml_import_read_text(ezxml_child(note, "voice"), voice, sizeof(voice))) {
        snprintf(parsed.voice, sizeof(parsed.voice), "%s", voice);
    }
    long staff = 1;
    if (ezxml_child(note, "staff") != NULL && (!musicxml_import_read_long(ezxml_child(note, "staff"), &staff) || staff < 1 || staff > 64)) {
        musicxml_import_warn(ctx, "<staff> is not a number from 1 to 64; staff 1 used");
        staff = 1;
    }
    parsed.staff = (int)staff;
    parsed.midi += state->transpose;
    read_time_modification(ctx, note, &parsed);

    if (length.num == 0) {
        musicxml_import_warn(ctx, "note with a zero duration skipped");
        return is_chord ? true : advance(ctx, state, length);
    }
    if (parsed.midi < 0) {
        musicxml_import_warn(ctx, "note below MIDI 0 skipped");
        return is_chord ? true : advance(ctx, state, length);
    }

    bool tie_stop = has_tie(note, "stop");
    bool tie_start = has_tie(note, "start");
    bool merged = false;
    if (tie_stop) {
        for (size_t t = 0; t < ctx->tie_count; t++) {
            XNote *open = &ctx->notes[ctx->ties[t].note];
            if (open->part == part && open->midi == parsed.midi && strcmp(open->voice, parsed.voice) == 0 &&
                rational_cmp(ctx->ties[t].end, onset) == 0) {
                Rational extended = {0, 1};
                Rational end = {0, 1};
                if (rational_add(open->duration, length, &extended) != RATIONAL_OK || rational_add(onset, length, &end) != RATIONAL_OK) {
                    musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "a tied note does not fit");
                    return false;
                }
                open->duration = extended;
                if (open->tuplet_actual != parsed.tuplet_actual || open->tuplet_normal != parsed.tuplet_normal) {
                    open->tuplet_actual = 0; /* tied to a note written another way: no one ratio describes it */
                    open->tuplet_normal = 0;
                }
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
            musicxml_import_warn(ctx, "tie stop with no matching tie start");
        }
    }
    if (!merged) {
        /* the note's own xml:id; a tied note keeps its first segment's (a merged continuation adds none), without the "_a" the
           export puts on a first segment */
        const char *xml_id = ezxml_attr(note, "xml:id");
        if (xml_id != NULL && xml_id[0] != '\0' && strlen(xml_id) < sizeof(parsed.xml_id)) {
            snprintf(parsed.xml_id, sizeof(parsed.xml_id), "%s", xml_id);
            size_t id_length = strlen(parsed.xml_id);
            if (tie_start && id_length > 2 && strcmp(parsed.xml_id + id_length - 2, "_a") == 0) {
                parsed.xml_id[id_length - 2] = '\0';
            }
        }
        size_t index = 0;
        Rational end = {0, 1};
        if (!musicxml_import_add_note(ctx, &parsed, &index) || rational_add(onset, length, &end) != RATIONAL_OK) {
            if (ctx->status == MUSICXML_IMPORT_OK) {
                musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "a note does not fit");
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
    if (!musicxml_import_read_long(ezxml_child(element, "duration"), &duration)) {
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<%s> without a usable <duration>", backward ? "backup" : "forward");
        return false;
    }
    Rational length = {0, 1};
    if (!to_whole_notes(ctx, state->divisions, duration, &length)) {
        return false;
    }
    if (!backward) {
        return advance(ctx, state, length);
    }
    if (rational_cmp(length, state->cursor) > 0 || rational_cmp(state->cursor, state->measure_start) < 0 ||
        rational_sub(state->cursor, length, &state->cursor) != RATIONAL_OK || rational_cmp(state->cursor, state->measure_start) < 0) {
        musicxml_import_warn(ctx, "<backup> goes before the start of the measure; stopped at the start");
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
        musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_ALLOC, "out of memory");
        return false;
    }
    *array = grown;
    *capacity = grown_capacity;
    return true;
}

/* <time>: <beats> (a whole number, or "3+2", the beat groups) and <beat-type> (a power of two). */
static bool handle_time(Ctx *ctx, PartState *state, ezxml_t time) {
    if (ezxml_child(time, "senza-misura") != NULL) {
        return true;
    }
    char beats_text[32];
    long beat_type = 0;
    if (!musicxml_import_read_text(ezxml_child(time, "beats"), beats_text, sizeof(beats_text)) ||
        !musicxml_import_read_long(ezxml_child(time, "beat-type"), &beat_type)) {
        musicxml_import_warn(ctx, "<time> without usable <beats> and <beat-type> skipped");
        return true;
    }
    unsigned long beats = 0;
    unsigned groups[TIME_SIGNATURE_MAX_GROUPS];
    size_t group_count = 0;
    bool groupable = true; /* every part fits a beat group and there are few enough of them */
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
        if (group_count < TIME_SIGNATURE_MAX_GROUPS && part <= TIME_SIGNATURE_MAX_GROUP_UNITS) {
            groups[group_count++] = (unsigned)part;
        } else {
            groupable = false;
        }
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
        musicxml_import_warn(ctx, "time signature %s/%ld skipped (not a whole number of beats over a power of two)", beats_text, beat_type);
        return true;
    }
    if (additive && !groupable) {
        musicxml_import_warn(ctx, "additive meter %s/%ld summed to %lu/%ld (beat groups are 1 to %d of up to %d units)", beats_text,
             beat_type, beats, beat_type, TIME_SIGNATURE_MAX_GROUPS, TIME_SIGNATURE_MAX_GROUP_UNITS);
    }
    if (!grow(ctx, (void **)&ctx->times, &ctx->time_capacity, ctx->time_count, sizeof(TimeEvent))) {
        return false;
    }
    TimeEvent *event = &ctx->times[ctx->time_count++];
    memset(event, 0, sizeof(*event));
    event->position = state->cursor;
    event->beats = (unsigned)beats;
    event->exponent = exponent;
    if (additive && groupable) {
        event->group_count = group_count;
        memcpy(event->groups, groups, group_count * sizeof(unsigned));
    }
    event->part = ctx->current_part;
    state->has_signature = true;
    state->measure_length = rational_ratio(beats, 1ull << exponent);
    return true;
}

/* <key>: <fifths> and an optional <mode>. */
static bool handle_key(Ctx *ctx, PartState *state, ezxml_t key) {
    long fifths = 0;
    if (!musicxml_import_read_long(ezxml_child(key, "fifths"), &fifths) || fifths < -7 || fifths > 7) {
        musicxml_import_warn(ctx, "<key> without <fifths> from -7 to 7 skipped");
        return true;
    }
    MusicXmlModeHint hint = MUSICXML_MODE_HINT_UNKNOWN;
    char mode[32];
    if (musicxml_import_read_text(ezxml_child(key, "mode"), mode, sizeof(mode))) {
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

/* --- tempo and dynamics --- */

/* A decimal in an element's text ("120", "92.5", "c. 120", "120-132" reads 120) as a rational over 1000. */
static bool read_decimal(const char *text, unsigned long *numerator, unsigned long *denominator) {
    while (*text != '\0' && !isdigit((unsigned char)*text)) {
        text++;
    }
    if (*text == '\0') {
        return false;
    }
    unsigned long whole = 0;
    while (isdigit((unsigned char)*text)) {
        whole = whole * 10 + (unsigned long)(*text++ - '0');
        if (whole > 100000) {
            return false;
        }
    }
    unsigned long thousandths = whole * 1000;
    if (*text == '.') {
        text++;
        unsigned long scale = 100;
        while (isdigit((unsigned char)*text) && scale > 0) {
            thousandths += (unsigned long)(*text++ - '0') * scale;
            scale /= 10;
        }
    }
    if (thousandths == 0) {
        return false;
    }
    Rational reduced = rational_ratio(thousandths, 1000);
    *numerator = (unsigned long)reduced.num;
    *denominator = (unsigned long)reduced.den;
    return true;
}

/* The duration of a <beat-unit> name with its dots, in whole notes. */
static bool beat_unit_duration(const char *name, int dots, Rational *out) {
    static const struct {
        const char *name;
        unsigned long long numerator;
        unsigned long long denominator;
    } UNITS[] = {{"long", 4, 1}, {"breve", 2, 1}, {"whole", 1, 1}, {"half", 1, 2}, {"quarter", 1, 4},
                 {"eighth", 1, 8}, {"16th", 1, 16}, {"32nd", 1, 32}, {"64th", 1, 64}, {"128th", 1, 128}};
    for (size_t i = 0; i < sizeof(UNITS) / sizeof(UNITS[0]); i++) {
        if (strcmp(name, UNITS[i].name) == 0 && dots >= 0 && dots <= 4) {
            /* base * (2^(dots+1) - 1) / 2^dots */
            *out = rational_ratio(UNITS[i].numerator * ((1ull << (dots + 1)) - 1), UNITS[i].denominator * (1ull << dots));
            return true;
        }
    }
    return false;
}

static void record_tempo(Ctx *ctx, Rational position, const char *text) {
    char *end = NULL;
    double bpm = strtod(text, &end);
    if (end == text || *end != '\0' || !isfinite(bpm) || bpm <= 0.0) {
        musicxml_import_warn(ctx, "<sound tempo=\"%s\"> is not a positive number of quarter notes per minute; skipped", text);
        return;
    }
    double microseconds = 60000000.0 / bpm;
    long rounded = lround(microseconds);
    if (rounded < 1 || rounded > 2000000000L) {
        musicxml_import_warn(ctx, "<sound tempo=\"%s\"> is out of range; skipped", text);
        return;
    }
    if (fabs(microseconds - (double)rounded) > 1e-6) {
        musicxml_import_warn(ctx, "tempo %s is %.3f microseconds per quarter note, rounded to %ld", text, microseconds, rounded);
    }
    if (!grow(ctx, (void **)&ctx->tempos, &ctx->tempo_capacity, ctx->tempo_count, sizeof(TempoEvent))) {
        return;
    }
    ctx->tempos[ctx->tempo_count].position = position;
    ctx->tempos[ctx->tempo_count].microseconds_per_quarter = (unsigned long)rounded;
    ctx->tempos[ctx->tempo_count].part = ctx->current_part;
    ctx->tempo_count++;
}

static bool handle_direction(Ctx *ctx, PartState *state, ezxml_t direction) {
    Rational position = state->cursor;
    long offset = 0;
    if (ezxml_child(direction, "offset") != NULL) {
        if (!musicxml_import_read_long(ezxml_child(direction, "offset"), &offset)) {
            musicxml_import_warn(ctx, "<offset> is not a whole number; ignored");
            offset = 0;
        }
    }
    bool before_zero = false;
    if (offset != 0 && state->divisions > 0) {
        Rational shift = {0, 1};
        if (to_whole_notes(ctx, state->divisions, offset < 0 ? -offset : offset, &shift)) {
            if (offset > 0) {
                if (rational_add(position, shift, &position) != RATIONAL_OK) {
                    return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "a direction's position does not fit") ==
                           MUSICXML_IMPORT_OK;
                }
            } else if (rational_cmp(shift, position) > 0 || rational_sub(position, shift, &position) != RATIONAL_OK) {
                before_zero = true;
            }
        } else {
            return false;
        }
    }
    long staff = 1;
    if (ezxml_child(direction, "staff") != NULL && (!musicxml_import_read_long(ezxml_child(direction, "staff"), &staff) || staff < 1 || staff > 64)) {
        staff = 1;
    }

    bool has_sound_tempo = false;
    ezxml_t sound = ezxml_child(direction, "sound");
    if (sound != NULL && ezxml_attr(sound, "tempo") != NULL) {
        has_sound_tempo = true;
        record_tempo(ctx, state->cursor, ezxml_attr(sound, "tempo"));
        if (ctx->status != MUSICXML_IMPORT_OK) {
            return false;
        }
    }

    MarkingEvent marking;
    memset(&marking, 0, sizeof(marking));
    bool has_metronome = false;
    bool has_words = false;
    for (ezxml_t type = ezxml_child(direction, "direction-type"); type != NULL; type = type->next) {
        for (ezxml_t item = type->child; item != NULL; item = item->ordered) {
            const char *name = ezxml_name(item);
            if (strcmp(name, "words") == 0) {
                char words[160];
                if (musicxml_import_read_text(item, words, sizeof(words))) {
                    size_t used = strlen(marking.text);
                    snprintf(marking.text + used, sizeof(marking.text) - used, "%s%s", used > 0 ? " " : "", words);
                    has_words = true;
                }
            } else if (strcmp(name, "metronome") == 0 && !has_metronome) {
                char unit[24];
                char per_minute[48];
                int dots = 0;
                for (ezxml_t dot = ezxml_child(item, "beat-unit-dot"); dot != NULL; dot = dot->next) {
                    dots++;
                }
                if (musicxml_import_read_text(ezxml_child(item, "beat-unit"), unit, sizeof(unit)) &&
                    musicxml_import_read_text(ezxml_child(item, "per-minute"), per_minute, sizeof(per_minute)) &&
                    beat_unit_duration(unit, dots, &marking.beat_unit) &&
                    read_decimal(per_minute, &marking.bpm_numerator, &marking.bpm_denominator)) {
                    has_metronome = true;
                } else {
                    musicxml_import_warn(ctx, "<metronome> is not a beat unit and a number per minute; skipped");
                }
            } else if (strcmp(name, "dynamics") == 0) {
                for (ezxml_t mark = item->child; mark != NULL; mark = mark->ordered) {
                    char dynamic[40];
                    const char *mark_name = ezxml_name(mark);
                    if (strcmp(mark_name, "other-dynamics") == 0) {
                        if (!musicxml_import_read_text(mark, dynamic, sizeof(dynamic))) {
                            continue;
                        }
                    } else {
                        snprintf(dynamic, sizeof(dynamic), "%s", mark_name);
                    }
                    if (before_zero) {
                        musicxml_import_warn(ctx, "dynamic marking %s is placed before the start of the piece; skipped", dynamic);
                        continue;
                    }
                    if (!grow(ctx, (void **)&ctx->dynamics, &ctx->dynamic_capacity, ctx->dynamic_count,
                              sizeof(DynamicEvent))) {
                        return false;
                    }
                    DynamicEvent *event = &ctx->dynamics[ctx->dynamic_count++];
                    event->position = position;
                    event->part = ctx->current_part;
                    event->order = ctx->direction_order++;
                    event->staff = (int)staff;
                    snprintf(event->name, sizeof(event->name), "%s", dynamic);
                }
            }
        }
    }

    if (has_metronome || (has_sound_tempo && has_words)) {
        if (before_zero) {
            musicxml_import_warn(ctx, "tempo marking is placed before the start of the piece; skipped");
        } else if (grow(ctx, (void **)&ctx->markings, &ctx->marking_capacity, ctx->marking_count, sizeof(MarkingEvent))) {
            marking.position = position;
            marking.part = ctx->current_part;
            marking.order = ctx->direction_order++;
            marking.has_text = has_words;
            marking.has_metronome = has_metronome;
            ctx->markings[ctx->marking_count++] = marking;
        } else {
            return false;
        }
    } else if (has_words) {
        ctx->unclassified_words++;
    }
    return true;
}

static bool handle_attributes(Ctx *ctx, PartState *state, ezxml_t attributes) {
    if (ezxml_child(attributes, "divisions") != NULL) {
        long divisions = 0;
        if (!musicxml_import_read_long(ezxml_child(attributes, "divisions"), &divisions) || divisions <= 0) {
            musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_BAD_VALUE, "<divisions> is not a positive whole number");
            return false;
        }
        state->divisions = divisions;
    }
    if (ezxml_child(attributes, "staves") != NULL) {
        long staves = 0;
        if (!musicxml_import_read_long(ezxml_child(attributes, "staves"), &staves) || staves < 1 || staves > 64) {
            musicxml_import_warn(ctx, "<staves> is not a number from 1 to 64; ignored");
        } else {
            state->staves = (int)staves;
        }
    }
    ezxml_t transpose = ezxml_child(attributes, "transpose");
    if (transpose != NULL) {
        long chromatic = 0;
        long octaves = 0;
        bool has_chromatic = ezxml_child(transpose, "chromatic") != NULL;
        bool has_octaves = ezxml_child(transpose, "octave-change") != NULL;
        if ((has_chromatic && !musicxml_import_read_long(ezxml_child(transpose, "chromatic"), &chromatic)) ||
            (has_octaves && !musicxml_import_read_long(ezxml_child(transpose, "octave-change"), &octaves)) || chromatic < -48 ||
            chromatic > 48 || octaves < -4 || octaves > 4) {
            musicxml_import_warn(ctx, "<transpose> is not read (unusable <chromatic> or <octave-change>)");
        } else {
            long semitones = chromatic + 12 * octaves;
            if (semitones != state->transpose) {
                state->transpose = semitones;
                if (semitones != 0) {
                    musicxml_import_warn(ctx, "transposing part: sounding pitch is written pitch %+ld semitones; written spelling kept",
                         semitones);
                }
            }
        }
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

bool musicxml_import_process_part(Ctx *ctx, ezxml_t part, size_t part_index) {
    PartState state;
    memset(&state, 0, sizeof(state));
    state.cursor = state.reached = state.measure_start = state.last_onset = rational_ratio(0, 1);
    state.staves = 1;
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
            } else if (strcmp(name, "direction") == 0) {
                ok = handle_direction(ctx, &state, element);
            } else if (strcmp(name, "sound") == 0 && ezxml_attr(element, "tempo") != NULL) {
                record_tempo(ctx, state.cursor, ezxml_attr(element, "tempo"));
                ok = ctx->status == MUSICXML_IMPORT_OK;
            } else if (strcmp(name, "backup") == 0) {
                ok = handle_move(ctx, &state, element, true);
            } else if (strcmp(name, "forward") == 0) {
                ok = handle_move(ctx, &state, element, false);
            }
            if (!ok) {
                return false;
            }
        }
        Rational length = {0, 1};
        if (rational_sub(state.reached, state.measure_start, &length) != RATIONAL_OK) {
            return musicxml_import_fail(ctx, MUSICXML_IMPORT_ERR_OVERFLOW, "the measure length does not fit") == MUSICXML_IMPORT_OK;
        }
        const char *implicit = ezxml_attr(measure, "implicit");
        bool is_implicit = implicit != NULL && strcmp(implicit, "yes") == 0;
        if (first_measure) {
            first_measure = false;
            if (part_index == 0 && is_implicit && length.num > 0 &&
                (!state.has_signature || rational_cmp(length, state.measure_length) < 0)) {
                ctx->has_pickup = true;
                ctx->pickup_length = length;
            }
        }
        if (state.has_signature && !is_implicit && rational_cmp(length, state.measure_length) != 0) {
            musicxml_import_warn(ctx, "measure is %llu/%llu of a whole note, its time signature's is %llu/%llu",
                 length.num, length.den, state.measure_length.num, state.measure_length.den);
        }
        state.measure_start = state.reached;
        if (state.staves > ctx->part_staves[part_index]) {
            ctx->part_staves[part_index] = state.staves;
        }
    }
    if (ctx->tie_count > 0) {
        musicxml_import_warn(ctx, "%zu tie start(s) never stopped", ctx->tie_count);
    }
    ctx->part_end[part_index] = state.measure_start;
    return true;
}

/* Time order, then part, then document order: stable, and the same for the same input. */
int musicxml_import_compare_items(const void *a, const void *b) {
    const SortItem *x = (const SortItem *)a;
    const SortItem *y = (const SortItem *)b;
    int by_onset = rational_cmp(x->onset, y->onset);
    if (by_onset != 0) {
        return by_onset;
    }
    if (x->part != y->part) {
        return x->part < y->part ? -1 : 1;
    }
    return x->order < y->order ? -1 : (x->order > y->order ? 1 : 0);
}
