#include <ursatz_composer/motive.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <key_identification.h>

#include <ursatz_composer/plan_state.h>

#include "motive/motive_internal.h"
#include "plan/plan_json_internal.h"

#define FILE_SIZE_LIMIT (64L * 1024L * 1024L)

/* ------------------------------------------------------------------ names */

static const char *const RESULT_NAMES[] = {"ok",    "null",       "alloc",     "io",        "format",     "syntax",
                                           "value", "empty",      "too many notes", "polyphonic", "meter", "import"};

const char *motive_result_name(MotiveResult result) {
    return (size_t)result < sizeof(RESULT_NAMES) / sizeof(RESULT_NAMES[0]) ? RESULT_NAMES[result] : "unknown";
}

static const char *const FORMAT_NAMES[] = {"auto", "midi", "musicxml", "notes", "json", "degrees", "rhythm", "contour"};

bool motive_format_parse(const char *name, MotiveFormat *out) {
    if (!name || !out) {
        return false;
    }
    for (size_t i = 0; i < sizeof(FORMAT_NAMES) / sizeof(FORMAT_NAMES[0]); i++) {
        if (strcmp(name, FORMAT_NAMES[i]) == 0) {
            *out = (MotiveFormat)i;
            return true;
        }
    }
    return false;
}

const char *motive_format_name(MotiveFormat format) {
    return (size_t)format < sizeof(FORMAT_NAMES) / sizeof(FORMAT_NAMES[0]) ? FORMAT_NAMES[format] : "unknown";
}

static const char *const FORM_NAMES[] = {"full", "rhythm", "contour"};

bool motive_form_parse(const char *name, MotiveForm *out) {
    if (!name || !out) {
        return false;
    }
    for (size_t i = 0; i < sizeof(FORM_NAMES) / sizeof(FORM_NAMES[0]); i++) {
        if (strcmp(name, FORM_NAMES[i]) == 0) {
            *out = (MotiveForm)i;
            return true;
        }
    }
    return false;
}

const char *motive_form_name(MotiveForm form) {
    return (size_t)form < sizeof(FORM_NAMES) / sizeof(FORM_NAMES[0]) ? FORM_NAMES[form] : "unknown";
}

MotiveFormat motive_detect_format(const void *data, size_t size) {
    const unsigned char *bytes = data;
    if (!bytes) {
        return MOTIVE_FORMAT_NOTES;
    }
    if (size >= 4 && memcmp(bytes, "MThd", 4) == 0) {
        return MOTIVE_FORMAT_MIDI;
    }
    if (size >= 2 && ((bytes[0] == 0xFF && bytes[1] == 0xFE) || (bytes[0] == 0xFE && bytes[1] == 0xFF))) {
        return MOTIVE_FORMAT_MUSICXML;
    }
    size_t at = 0;
    if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        at = 3;
    }
    while (at < size && isspace(bytes[at])) {
        at++;
    }
    if (at < size && bytes[at] == '<') {
        return MOTIVE_FORMAT_MUSICXML;
    }
    if (at < size && bytes[at] == '{') {
        return MOTIVE_FORMAT_JSON;
    }
    return MOTIVE_FORMAT_NOTES;
}

/* ---------------------------------------------------------- key and meter */

static bool equals_ignore_case(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

bool motive_parse_key(const char *text, PlanKey *out) {
    static const int NATURAL[7] = {9, 11, 0, 2, 4, 5, 7}; /* A B C D E F G */
    if (!text || !out) {
        return false;
    }
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char letter = (char)toupper((unsigned char)*text);
    if (letter < 'A' || letter > 'G') {
        return false;
    }
    int pc = NATURAL[letter - 'A'];
    text++;
    if (*text == '#') {
        pc++;
        text++;
    } else if (*text == 'b') {
        pc--;
        text++;
    }
    if (!isspace((unsigned char)*text)) {
        return false;
    }
    while (isspace((unsigned char)*text)) {
        text++;
    }
    char mode[8];
    size_t length = 0;
    while (text[length] && !isspace((unsigned char)text[length]) && length + 1 < sizeof(mode)) {
        mode[length] = text[length];
        length++;
    }
    mode[length] = '\0';
    const char *rest = text + length;
    while (isspace((unsigned char)*rest)) {
        rest++;
    }
    if (*rest != '\0') {
        return false;
    }
    if (equals_ignore_case(mode, "major")) {
        out->mode = PLAN_MODE_MAJOR;
    } else if (equals_ignore_case(mode, "minor")) {
        out->mode = PLAN_MODE_MINOR;
    } else {
        return false;
    }
    out->tonic_pitch_class = (pc % 12 + 12) % 12;
    return true;
}

static bool meter_valid(const PlanMeter *meter) {
    int unit = meter->beat_unit;
    return meter->beats_per_bar >= 1 && meter->beats_per_bar <= 64 &&
           (unit == 1 || unit == 2 || unit == 4 || unit == 8 || unit == 16);
}

bool motive_parse_meter(const char *text, PlanMeter *out) {
    if (!text || !out) {
        return false;
    }
    char *end = NULL;
    long beats = strtol(text, &end, 10);
    if (end == text || *end != '/') {
        return false;
    }
    const char *unit_text = end + 1;
    long unit = strtol(unit_text, &end, 10);
    if (end == unit_text || *end != '\0' || beats < 1 || beats > 64 || unit < 1 || unit > 16) {
        return false;
    }
    PlanMeter meter = {(int)beats, (int)unit, 0};
    if (!meter_valid(&meter)) {
        return false;
    }
    *out = meter;
    return true;
}

PlanKey motive_key_from_signature(int sharps_flats, PlanKeyMode mode) {
    int major_tonic = ((sharps_flats * 7) % 12 + 12) % 12;
    PlanKey key;
    key.tonic_pitch_class = mode == PLAN_MODE_MINOR ? (major_tonic + 9) % 12 : major_tonic;
    key.mode = mode;
    return key;
}

/* ------------------------------------------------------------------- time */

PlanTime motive_time(long num, unsigned long den) {
    PlanTime time = {0, 1};
    plan_time_make(num, den, &time);
    return time;
}

PlanTime motive_time_plus(PlanTime a, PlanTime b) {
    PlanTime sum = {0, 1};
    plan_time_add(a, b, &sum);
    return sum;
}

PlanTime motive_time_minus(PlanTime a, PlanTime b) {
    return motive_time(a.num * (long)b.den - b.num * (long)a.den, a.den * b.den);
}

PlanTime motive_pickup(const Motive *motive) {
    return motive->pickup.den == 0 ? motive_time(0, 1) : motive->pickup;
}

PlanTime motive_note_end(const PlanNote *note) {
    return motive_time_plus(note->offset, note->duration);
}

void motive_set_error(MotiveError *error, const char *location, const char *format, ...) {
    if (!error) {
        return;
    }
    snprintf(error->location, sizeof(error->location), "%s", location ? location : "");
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof(error->message), format, args);
    va_end(args);
}

/* ------------------------------------------------------------ validation */

MotiveResult motive_check_notes(const Motive *motive, PlanTime *length, MotiveError *error) {
    if (motive->note_count == 0) {
        motive_set_error(error, "", "the motive has no notes");
        return MOTIVE_ERR_EMPTY;
    }
    if (motive->note_count > MOTIVE_MAX_NOTES) {
        motive_set_error(error, "", "the motive has %zu notes; the most is %d", motive->note_count, MOTIVE_MAX_NOTES);
        return MOTIVE_ERR_TOO_MANY_NOTES;
    }
    PlanTime end = motive_time(0, 1);
    for (size_t i = 0; i < motive->note_count; i++) {
        const PlanNote *note = &motive->notes[i];
        char location[MOTIVE_ERROR_LOCATION_MAX];
        snprintf(location, sizeof(location), "notes[%zu]", i);
        if (note->midi_pitch < 0 || note->midi_pitch > 127) {
            motive_set_error(error, location, "pitch %d is outside the MIDI range 0 to 127", note->midi_pitch);
            return MOTIVE_ERR_VALUE;
        }
        if (note->duration.num <= 0) {
            motive_set_error(error, location, "the duration must be longer than zero");
            return MOTIVE_ERR_VALUE;
        }
        if (note->offset.num < 0) {
            motive_set_error(error, location, "the offset must not be before the start of the motive");
            return MOTIVE_ERR_VALUE;
        }
        if (plan_time_compare(note->offset, end) < 0) {
            motive_set_error(error, location, "the note starts at %ld/%lu, before the note before it ends at %ld/%lu",
                             note->offset.num, note->offset.den, end.num, end.den);
            return MOTIVE_ERR_VALUE;
        }
        end = motive_note_end(note);
    }
    *length = end;
    return MOTIVE_OK;
}

/* ------------------------------------------------------------------- key */

/*
 * Ursatz's common-practice key identification takes the pitch classes above a candidate tonic and answers only
 * when all seven degrees of a major or natural minor scale are there. A motive seldom has seven, so most are left
 * unspecified; no key is guessed from fewer pitches. A scale and its relative share their seven pitch classes, so
 * two answers are settled by the last note and, failing that, left open.
 */
static bool infer_key(const Motive *motive, PlanKey *out) {
    bool present[12] = {false};
    for (size_t i = 0; i < motive->note_count; i++) {
        present[motive->notes[i].midi_pitch % 12] = true;
    }
    PlanKey found[24];
    size_t found_count = 0;
    for (int tonic = 0; tonic < 12; tonic++) {
        int relative[12];
        size_t count = 0;
        for (int pc = 0; pc < 12; pc++) {
            if (present[pc]) {
                relative[count++] = (pc - tonic + 12) % 12;
            }
        }
        CommonPracticeKeyMode mode;
        if (common_practice_identify_key_mode(relative, count, &mode)) {
            found[found_count].tonic_pitch_class = tonic;
            found[found_count].mode = mode == COMMON_PRACTICE_KEY_MINOR ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR;
            found_count++;
        }
    }
    if (found_count == 1) {
        *out = found[0];
        return true;
    }
    int last = motive->notes[motive->note_count - 1].midi_pitch % 12;
    size_t match = found_count;
    for (size_t i = 0; i < found_count; i++) {
        if (found[i].tonic_pitch_class == last) {
            if (match != found_count) {
                return false;
            }
            match = i;
        }
    }
    if (match == found_count) {
        return false;
    }
    *out = found[match];
    return true;
}

/* -------------------------------------------------------------- complete */

MotiveResult motive_complete(Motive *motive, const MotiveDocument *document, const MotiveOptions *options,
                             MotiveError *error) {
    PlanTime length;
    MotiveResult checked = motive_check_notes(motive, &length, error);
    if (checked != MOTIVE_OK) {
        return checked;
    }
    motive->length = length;

    bool asked_meter = options && options->has_meter;
    if (asked_meter && !meter_valid(&options->meter)) {
        motive_set_error(error, "meter", "%d/%d is not a meter; use 1 to 64 beats over a beat unit of 1, 2, 4, 8 or 16",
                         options->meter.beats_per_bar, options->meter.beat_unit);
        return MOTIVE_ERR_VALUE;
    }
    if (document->has_meter && document->meter_declared) {
        if (asked_meter && (options->meter.beats_per_bar != document->meter.beats_per_bar ||
                            options->meter.beat_unit != document->meter.beat_unit)) {
            motive_set_error(error, "meter", "the meter %d/%d conflicts with the %d/%d the input declares",
                             options->meter.beats_per_bar, options->meter.beat_unit, document->meter.beats_per_bar,
                             document->meter.beat_unit);
            return MOTIVE_ERR_METER;
        }
        motive->meter = document->meter;
    } else if (asked_meter) {
        motive->meter = options->meter;
        motive->meter.bars_per_hyperbar = 0;
    } else if (document->has_meter) {
        motive->meter = document->meter;
    } else {
        motive_set_error(error, "meter", "the input has no meter; give one");
        return MOTIVE_ERR_METER;
    }
    if (!meter_valid(&motive->meter)) {
        motive_set_error(error, "meter", "%d/%d is not a meter; use 1 to 64 beats over a beat unit of 1, 2, 4, 8 or 16",
                         motive->meter.beats_per_bar, motive->meter.beat_unit);
        return MOTIVE_ERR_VALUE;
    }

    if (options && options->max_bars > 0) {
        /* The notes end at motive->length; the limit is that many whole bars of the meter. */
        PlanTime bar;
        PlanTime limit;
        if (plan_time_make((long)motive->meter.beats_per_bar, (unsigned long)motive->meter.beat_unit, &bar) != PLAN_OK ||
            plan_time_make((long)options->max_bars * bar.num, bar.den, &limit) != PLAN_OK ||
            plan_time_compare(motive->length, limit) > 0) {
            motive_set_error(error, "", "the notes fill more than %zu bars of %d/%d", options->max_bars,
                             motive->meter.beats_per_bar, motive->meter.beat_unit);
            return MOTIVE_ERR_VALUE;
        }
    }

    PlanTime pickup = options && options->has_pickup ? options->pickup
                      : document->has_pickup           ? document->pickup
                                                       : motive_time(0, 1);
    PlanTime bar_length;
    if (pickup.den == 0 || pickup.num < 0 ||
        plan_time_make((long)motive->meter.beats_per_bar, (unsigned long)motive->meter.beat_unit, &bar_length) !=
            PLAN_OK ||
        plan_time_compare(pickup, bar_length) >= 0) {
        motive_set_error(error, "pickup", "the pickup must be at least 0 and shorter than one bar of %d/%d",
                         motive->meter.beats_per_bar, motive->meter.beat_unit);
        return MOTIVE_ERR_VALUE;
    }
    motive->pickup = pickup;
    motive->form = document->form;

    PlanKey key;
    if (options && options->has_key) {
        key = options->key;
        motive->key_source = MOTIVE_KEY_GIVEN;
    } else if (document->has_key) {
        key = document->key;
        motive->key_source = MOTIVE_KEY_GIVEN;
    } else if (infer_key(motive, &key)) {
        motive->key_source = MOTIVE_KEY_INFERRED;
    } else {
        memset(&key, 0, sizeof(key));
        motive->key_source = MOTIVE_KEY_UNSPECIFIED;
    }
    if (key.tonic_pitch_class < 0 || key.tonic_pitch_class > 11) {
        motive_set_error(error, "key", "the tonic pitch class %d is not from 0 to 11", key.tonic_pitch_class);
        return MOTIVE_ERR_VALUE;
    }
    motive->key = key;
    return MOTIVE_OK;
}

/* --------------------------------------------------------------- loading */

static void begin(Motive *motive, MotiveDocument *document, MotiveError *error) {
    memset(motive, 0, sizeof(*motive));
    memset(document, 0, sizeof(*document));
    motive->length = motive_time(0, 1);
    motive->pickup = motive_time(0, 1);
    if (error) {
        memset(error, 0, sizeof(*error));
    }
}

typedef MotiveResult (*TextParser)(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error);

static MotiveResult load_text(const char *text, TextParser parse, const MotiveOptions *options, Motive *out,
                              MotiveError *error) {
    if (!text || !out) {
        motive_set_error(error, "", "input or output is NULL");
        return MOTIVE_ERR_NULL;
    }
    Motive motive;
    MotiveDocument document;
    begin(&motive, &document, error);
    if (options && options->has_key) {
        /* A key the caller gives is the key a degree list is read in; for the other inputs it changes nothing. */
        document.has_key = true;
        document.key = options->key;
    }
    MotiveResult result = parse(text, &motive, &document, error);
    if (result == MOTIVE_OK) {
        result = motive_complete(&motive, &document, options, error);
    }
    if (result == MOTIVE_OK) {
        *out = motive;
    }
    return result;
}

MotiveResult motive_from_notes_text(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error) {
    return load_text(text, motive_parse_notes_text, options, out, error);
}

MotiveResult motive_from_degrees_text(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error) {
    return load_text(text, motive_parse_degrees_text, options, out, error);
}

MotiveResult motive_from_rhythm_text(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error) {
    return load_text(text, motive_parse_rhythm_text, options, out, error);
}

MotiveResult motive_from_contour_text(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error) {
    return load_text(text, motive_parse_contour_text, options, out, error);
}

MotiveResult motive_from_json(const char *text, const MotiveOptions *options, Motive *out, MotiveError *error) {
    return load_text(text, motive_parse_json_text, options, out, error);
}

MotiveResult motive_from_midi(const unsigned char *data, size_t size, const MotiveOptions *options, Motive *out,
                              MotiveError *error) {
    if (!data || !out) {
        motive_set_error(error, "", "input or output is NULL");
        return MOTIVE_ERR_NULL;
    }
    Motive motive;
    MotiveDocument document;
    begin(&motive, &document, error);
    MotiveResult result = motive_read_midi(data, size, &motive, &document, error);
    if (result == MOTIVE_OK) {
        result = motive_complete(&motive, &document, options, error);
    }
    if (result == MOTIVE_OK) {
        *out = motive;
    }
    return result;
}

MotiveResult motive_from_musicxml(const char *data, size_t size, const MotiveOptions *options, Motive *out,
                                  MotiveError *error) {
    if (!data || !out) {
        motive_set_error(error, "", "input or output is NULL");
        return MOTIVE_ERR_NULL;
    }
    Motive motive;
    MotiveDocument document;
    begin(&motive, &document, error);
    MotiveResult result = motive_read_musicxml(data, size, &motive, &document, error);
    if (result == MOTIVE_OK) {
        result = motive_complete(&motive, &document, options, error);
    }
    if (result == MOTIVE_OK) {
        *out = motive;
    }
    return result;
}

MotiveResult motive_load(const void *data, size_t size, MotiveFormat format, const MotiveOptions *options, Motive *out,
                         MotiveError *error) {
    if (!data || !out) {
        motive_set_error(error, "", "input or output is NULL");
        return MOTIVE_ERR_NULL;
    }
    if (format == MOTIVE_FORMAT_AUTO) {
        format = motive_detect_format(data, size);
    }
    switch (format) {
    case MOTIVE_FORMAT_MIDI:
        return motive_from_midi(data, size, options, out, error);
    case MOTIVE_FORMAT_MUSICXML:
        return motive_from_musicxml(data, size, options, out, error);
    case MOTIVE_FORMAT_NOTES:
    case MOTIVE_FORMAT_DEGREES:
    case MOTIVE_FORMAT_RHYTHM:
    case MOTIVE_FORMAT_CONTOUR:
    case MOTIVE_FORMAT_JSON: {
        char *text = malloc(size + 1);
        if (!text) {
            motive_set_error(error, "", "out of memory");
            return MOTIVE_ERR_ALLOC;
        }
        memcpy(text, data, size);
        text[size] = '\0';
        MotiveResult result = format == MOTIVE_FORMAT_JSON      ? motive_from_json(text, options, out, error)
                              : format == MOTIVE_FORMAT_DEGREES ? motive_from_degrees_text(text, options, out, error)
                              : format == MOTIVE_FORMAT_RHYTHM  ? motive_from_rhythm_text(text, options, out, error)
                              : format == MOTIVE_FORMAT_CONTOUR ? motive_from_contour_text(text, options, out, error)
                                                                : motive_from_notes_text(text, options, out, error);
        free(text);
        return result;
    }
    default:
        motive_set_error(error, "", "unknown format");
        return MOTIVE_ERR_FORMAT;
    }
}

MotiveResult motive_load_file(const char *path, MotiveFormat format, const MotiveOptions *options, Motive *out,
                              MotiveError *error) {
    if (!path || !out) {
        motive_set_error(error, "", "path or output is NULL");
        return MOTIVE_ERR_NULL;
    }
    FILE *file = fopen(path, "rb");
    if (!file) {
        motive_set_error(error, path, "cannot open the file");
        return MOTIVE_ERR_IO;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0 || size > FILE_SIZE_LIMIT) {
        fclose(file);
        motive_set_error(error, path, "the file is empty or larger than %ld bytes", FILE_SIZE_LIMIT);
        return MOTIVE_ERR_IO;
    }
    unsigned char *data = malloc((size_t)size + 1);
    if (!data) {
        fclose(file);
        motive_set_error(error, path, "out of memory");
        return MOTIVE_ERR_ALLOC;
    }
    size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size) {
        free(data);
        motive_set_error(error, path, "cannot read the file");
        return MOTIVE_ERR_IO;
    }
    data[size] = '\0';
    MotiveResult result = size == 0 ? MOTIVE_ERR_EMPTY : motive_load(data, (size_t)size, format, options, out, error);
    if (size == 0) {
        motive_set_error(error, path, "the file is empty");
    }
    free(data);
    return result;
}

/* ------------------------------------------------------------------ value */

bool motive_equal(const Motive *a, const Motive *b) {
    if (!a || !b) {
        return a == b;
    }
    if (a->meter.beats_per_bar != b->meter.beats_per_bar || a->meter.beat_unit != b->meter.beat_unit ||
        a->meter.bars_per_hyperbar != b->meter.bars_per_hyperbar || a->key_source != b->key_source ||
        a->note_count != b->note_count || plan_time_compare(a->length, b->length) != 0 || a->form != b->form ||
        plan_time_compare(motive_pickup(a), motive_pickup(b)) != 0) {
        return false;
    }
    if (a->key_source != MOTIVE_KEY_UNSPECIFIED &&
        (a->key.tonic_pitch_class != b->key.tonic_pitch_class || a->key.mode != b->key.mode)) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].midi_pitch != b->notes[i].midi_pitch ||
            plan_time_compare(a->notes[i].offset, b->notes[i].offset) != 0 ||
            plan_time_compare(a->notes[i].duration, b->notes[i].duration) != 0) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------- JSON */

cJSON *motive_time_json(PlanTime time) {
    cJSON *object = cJSON_CreateObject();
    if (object &&
        !(json_add(object, "num", json_number(time.num)) && json_add(object, "den", json_number((long)time.den)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *meter_json(const PlanMeter *meter) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "beats_per_bar", json_number(meter->beats_per_bar)) &&
                    json_add(object, "beat_unit", json_number(meter->beat_unit)) &&
                    json_add(object, "bars_per_hyperbar", json_number(meter->bars_per_hyperbar)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *key_json(const PlanKey *key) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "tonic_pitch_class", json_number(key->tonic_pitch_class)) &&
                    json_add(object, "mode", cJSON_CreateString(key->mode == PLAN_MODE_MINOR ? "minor" : "major")))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *notes_json(const Motive *motive) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < motive->note_count; i++) {
        const PlanNote *note = &motive->notes[i];
        cJSON *object = cJSON_CreateObject();
        bool ok = object && json_add(object, "midi_pitch", json_number(note->midi_pitch)) &&
                  json_add(object, "offset", motive_time_json(note->offset)) &&
                  json_add(object, "duration", motive_time_json(note->duration));
        if (!ok) {
            cJSON_Delete(object);
            cJSON_Delete(array);
            return NULL;
        }
        /* json_append frees the object itself when it fails. */
        if (!json_append(array, object)) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

cJSON *motive_json_object(const Motive *motive) {
    cJSON *document = cJSON_CreateObject();
    bool ok = document && json_add(document, "meter", meter_json(&motive->meter));
    /* An inferred key is worked out again on reading, so only a stated one is written. */
    if (ok && motive->key_source == MOTIVE_KEY_GIVEN) {
        ok = json_add(document, "key", key_json(&motive->key));
    }
    ok = ok && json_add(document, "notes", notes_json(motive));
    if (ok && motive->form != MOTIVE_FORM_FULL) {
        ok = json_add(document, "form", cJSON_CreateString(motive_form_name(motive->form)));
    }
    if (ok && motive_pickup(motive).num != 0) {
        ok = json_add(document, "pickup", motive_time_json(motive_pickup(motive)));
    }
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return document;
}

char *motive_json_text(cJSON *document) {
    char *printed = document ? cJSON_Print(document) : NULL;
    cJSON_Delete(document);
    char *text = printed ? json_indent_with_spaces(printed) : NULL;
    cJSON_free(printed);
    return text;
}

char *motive_to_json(const Motive *motive) {
    return motive ? motive_json_text(motive_json_object(motive)) : NULL;
}

static bool read_time(JsonReader *reader, const cJSON *item, const char *path, long min_num, PlanTime *out) {
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, path, "expected an object with num and den");
    }
    long num;
    long den;
    if (!json_read_int_member(reader, item, path, "num", min_num, 1000000000L, &num) ||
        !json_read_int_member(reader, item, path, "den", 1, 1000000000L, &den)) {
        return false;
    }
    return plan_time_make(num, (unsigned long)den, out) == PLAN_OK;
}

static bool read_time_member(JsonReader *reader, const cJSON *object, const char *path, const char *key, long min_num,
                             PlanTime *out) {
    const cJSON *item = json_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[PLAN_JSON_PATH_MAX];
    json_child_path(at, path, key);
    return read_time(reader, item, at, min_num, out);
}

static bool read_meter(JsonReader *reader, const cJSON *item, MotiveDocument *document) {
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, "meter", "expected an object");
    }
    long beats;
    long unit;
    if (!json_read_int_member(reader, item, "meter", "beats_per_bar", 1, 64, &beats) ||
        !json_read_int_member(reader, item, "meter", "beat_unit", 1, 16, &unit)) {
        return false;
    }
    PlanMeter meter = {(int)beats, (int)unit, 0};
    if (!meter_valid(&meter)) {
        return json_bad_format(reader, "meter.beat_unit", "expected 1, 2, 4, 8 or 16");
    }
    const cJSON *hyperbar = cJSON_GetObjectItemCaseSensitive(item, "bars_per_hyperbar");
    if (hyperbar) {
        long bars;
        if (!json_read_long(reader, hyperbar, "meter.bars_per_hyperbar", 0, 1000, &bars)) {
            return false;
        }
        meter.bars_per_hyperbar = (int)bars;
    }
    document->has_meter = true;
    document->meter_declared = true;
    document->meter = meter;
    return true;
}

static bool read_key(JsonReader *reader, const cJSON *item, MotiveDocument *document) {
    if (!cJSON_IsObject(item)) {
        return json_bad_format(reader, "key", "expected an object");
    }
    long tonic;
    if (!json_read_int_member(reader, item, "key", "tonic_pitch_class", 0, 11, &tonic)) {
        return false;
    }
    const cJSON *mode = json_member(reader, item, "key", "mode", true);
    if (!mode) {
        return false;
    }
    if (!cJSON_IsString(mode) || (strcmp(mode->valuestring, "major") != 0 && strcmp(mode->valuestring, "minor") != 0)) {
        return json_bad_format(reader, "key.mode", "expected \"major\" or \"minor\"");
    }
    document->has_key = true;
    document->key.tonic_pitch_class = (int)tonic;
    document->key.mode = strcmp(mode->valuestring, "minor") == 0 ? PLAN_MODE_MINOR : PLAN_MODE_MAJOR;
    return true;
}

static bool read_notes(JsonReader *reader, const cJSON *array, Motive *motive) {
    if (!cJSON_IsArray(array)) {
        return json_bad_format(reader, "notes", "expected an array");
    }
    int count = cJSON_GetArraySize(array);
    if (count == 0) {
        return json_fail(reader, PLAN_ERR_FORMAT, "notes", "the motive has no notes");
    }
    if (count > MOTIVE_MAX_NOTES) {
        return json_fail(reader, PLAN_ERR_FORMAT, "notes", "the motive has %d notes; the most is %d", count,
                         MOTIVE_MAX_NOTES);
    }
    PlanTime end = motive_time(0, 1);
    for (int i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(array, i);
        char path[PLAN_JSON_PATH_MAX];
        json_index_path(path, "notes", (size_t)i);
        if (!cJSON_IsObject(item)) {
            return json_bad_format(reader, path, "expected an object");
        }
        long pitch;
        PlanNote note;
        if (!json_read_int_member(reader, item, path, "midi_pitch", 0, 127, &pitch) ||
            !read_time_member(reader, item, path, "offset", 0, &note.offset) ||
            !read_time_member(reader, item, path, "duration", 1, &note.duration)) {
            return false;
        }
        note.midi_pitch = (int)pitch;
        if (plan_time_compare(note.offset, end) < 0) {
            char at[PLAN_JSON_PATH_MAX];
            json_child_path(at, path, "offset");
            return json_fail(reader, PLAN_ERR_FORMAT, at, "the note starts before the note before it ends");
        }
        end = motive_note_end(&note);
        motive->notes[i] = note;
    }
    motive->note_count = (size_t)count;
    return true;
}

MotiveResult motive_parse_json_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error) {
    const char *end = NULL;
    cJSON *root = cJSON_ParseWithLengthOpts(text, strlen(text) + 1, &end, true);
    if (!root) {
        char location[MOTIVE_ERROR_LOCATION_MAX];
        snprintf(location, sizeof(location), "byte %ld", end ? (long)(end - text) : 0L);
        motive_set_error(error, location, "not valid JSON");
        return MOTIVE_ERR_SYNTAX;
    }
    PlanJsonError detail;
    memset(&detail, 0, sizeof(detail));
    JsonReader reader = {&detail, PLAN_OK};
    bool ok = cJSON_IsObject(root) || json_bad_format(&reader, "", "expected an object");
    const cJSON *meter = ok ? cJSON_GetObjectItemCaseSensitive(root, "meter") : NULL;
    const cJSON *key = ok ? cJSON_GetObjectItemCaseSensitive(root, "key") : NULL;
    ok = ok && (!meter || cJSON_IsNull(meter) || read_meter(&reader, meter, document));
    ok = ok && (!key || cJSON_IsNull(key) || read_key(&reader, key, document));
    const cJSON *form = ok ? cJSON_GetObjectItemCaseSensitive(root, "form") : NULL;
    const cJSON *pickup = ok ? cJSON_GetObjectItemCaseSensitive(root, "pickup") : NULL;
    if (ok && form && !cJSON_IsNull(form)) {
        ok = (cJSON_IsString(form) && motive_form_parse(form->valuestring, &document->form)) ||
             json_bad_format(&reader, "form", "expected \"full\", \"rhythm\" or \"contour\"");
    }
    if (ok && pickup && !cJSON_IsNull(pickup)) {
        ok = read_time(&reader, pickup, "pickup", 0, &document->pickup);
        document->has_pickup = ok;
    }
    ok = ok && read_notes(&reader, json_member(&reader, root, "", "notes", true), motive);
    cJSON_Delete(root);
    if (!ok) {
        motive_set_error(error, detail.path, "%s", detail.message);
        return MOTIVE_ERR_VALUE;
    }
    return MOTIVE_OK;
}

/* ----------------------------------------------------------------- attach */

static PlanResult attach(PiecePlan *plan, NodeId parent, PlanTime start, const Motive *motive, const char *name,
                         const PlanSource *source, NodeId *out_node, LaneSpanId *out_span) {
    if (!plan || !motive) {
        return PLAN_ERR_NULL;
    }
    PlanTime length;
    if (motive_check_notes(motive, &length, NULL) != MOTIVE_OK) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    if (!plan_has_node(plan, parent)) {
        return PLAN_ERR_NOT_FOUND;
    }
    PlanNodeKind kind = plan_node_kind(plan, parent);
    if (kind != PLAN_NODE_SUBPHRASE && kind != PLAN_NODE_IDEA) {
        return PLAN_ERR_BAD_KIND;
    }
    PlanSpan span = {start, length};
    if (!plan_span_contains(plan_node_span(plan, parent), span)) {
        return PLAN_ERR_BAD_SPAN;
    }
    if (plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, NULL, 0) > 0) {
        return PLAN_ERR_LANE_OVERLAP;
    }
    NodeId node;
    PlanResult result = plan_add_node(plan, parent, PLAN_NODE_MOTIVE, span, name, &node);
    if (result != PLAN_OK) {
        return result;
    }
    if (source) {
        /* The node is Given below, and a Given node takes no more constraints. */
        PlanConstraint constraint;
        memset(&constraint, 0, sizeof(constraint));
        constraint.kind = PLAN_CONSTRAINT_SOURCE;
        constraint.params.source = *source;
        result = plan_node_add_constraint(plan, node, &constraint);
        if (result != PLAN_OK) {
            return result;
        }
    }
    PlanLanePayload payload;
    payload.notes.notes = motive->notes;
    payload.notes.note_count = motive->note_count;
    LaneSpanId lane_span;
    result = plan_add_lane_span(plan, node, PLAN_LANE_MELODY, 0, span, &payload, &lane_span);
    if (result != PLAN_OK) {
        return result;
    }
    result = plan_node_set_state(plan, node, PLAN_STATE_GIVEN);
    if (result == PLAN_OK) {
        result = plan_lane_span_set_state(plan, lane_span, PLAN_STATE_GIVEN);
    }
    if (result != PLAN_OK) {
        return result;
    }
    if (out_node) {
        *out_node = node;
    }
    if (out_span) {
        *out_span = lane_span;
    }
    return PLAN_OK;
}

PlanResult motive_attach(PiecePlan *plan, NodeId parent, PlanTime start, const Motive *motive, const char *name,
                         NodeId *out_node, LaneSpanId *out_span) {
    return attach(plan, parent, start, motive, name, NULL, out_node, out_span);
}

PlanResult motive_attach_sourced(PiecePlan *plan, NodeId parent, PlanTime start, const Motive *motive, const char *name,
                                 const PlanSource *source, NodeId *out_node, LaneSpanId *out_span) {
    return attach(plan, parent, start, motive, name, source, out_node, out_span);
}

bool motive_in_key(const Motive *source, const PlanKey *key, Motive *out) {
    if (!source || !key || !out || source->key_source == MOTIVE_KEY_UNSPECIFIED) {
        return false;
    }
    Motive moved = *source;
    for (size_t i = 0; i < source->note_count; i++) {
        long position;
        int alteration;
        motive_scale_position(&source->key, source->notes[i].midi_pitch - 60, &position, &alteration);
        int pitch = 60 + motive_position_pitch(key, position, alteration);
        if (pitch < 0 || pitch > 127) {
            return false;
        }
        moved.notes[i].midi_pitch = pitch;
    }
    moved.key = *key;
    moved.key_source = MOTIVE_KEY_GIVEN;
    *out = moved;
    return true;
}
