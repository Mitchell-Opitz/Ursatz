#include <ursatz_composer/motive.h>

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>
#include <analysis/harmony/key_identification.h>
#include <theory/tonality/common_practice_materials.h>

#include <ursatz_composer/motive_tones.h>
#include <plan/plan_state.h>

#include "motive/motive_internal.h"
#include <common/json.h>
#include "util/util.h"
#include <plan/plan_common.h>

/* ----------------------------------------------------------------- scores */

bool motive_score_from_rational(Rational value, MotiveScore *out) {
    if (value.num < 0 || value.den <= 0) {
        return false;
    }
    out->numerator = (unsigned long)value.num;
    out->denominator = (unsigned long)value.den;
    return true;
}

bool motive_score_make(unsigned long long numerator, unsigned long long denominator, MotiveScore *out) {
    if (denominator == 0) {
        if (numerator != 0) {
            return false;
        }
        out->numerator = 0;
        out->denominator = 0;
        return true;
    }
    Rational value;
    if (numerator > (unsigned long long)LLONG_MAX || denominator > (unsigned long long)LLONG_MAX ||
        rational_make((long long)numerator, (long long)denominator, &value) != RATIONAL_OK) {
        return false;
    }
    return motive_score_from_rational(value, out);
}

Rational motive_score_rational(MotiveScore score) {
    return rational_ratio((long long)score.numerator, (long long)score.denominator);
}

/* ------------------------------------------------------------------ names */

static const char *const RESULT_NAMES[] = {"ok",    "null",       "alloc",     "format",     "syntax",
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

static const char *const FORM_NAMES[] = {"full", "rhythm", "contour"};

bool motive_form_parse(const char *name, MotiveSketchForm *out) {
    if (!name || !out) {
        return false;
    }
    for (size_t i = 0; i < sizeof(FORM_NAMES) / sizeof(FORM_NAMES[0]); i++) {
        if (strcmp(name, FORM_NAMES[i]) == 0) {
            *out = (MotiveSketchForm)i;
            return true;
        }
    }
    return false;
}

const char *motive_form_name(MotiveSketchForm form) {
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

static bool meter_valid(const PlanMeter *meter) {
    int unit = meter->beat_unit;
    return meter->beats_per_bar >= 1 && meter->beats_per_bar <= 64 &&
           (unit == 1 || unit == 2 || unit == 4 || unit == 8 || unit == 16);
}

SpelledKey motive_key_from_signature(const CommonPracticeMaterials *materials, int sharps_flats, CommonPracticeKeyMode mode) {
    const PitchMaterial *material = common_practice_material(materials, mode);
    SpelledKey key;
    if (spelled_key_from_fifths(sharps_flats, material, &key)) {
        return key;
    }
    int major_tonic = util_pitch_class(sharps_flats * 7);
    return plan_key_of_pitch_class(mode == COMMON_PRACTICE_KEY_MINOR ? (major_tonic + 9) % 12 : major_tonic, material);
}

/* ------------------------------------------------------------------- time */

Rational motive_time(long num, unsigned long den) {
    Rational time = {0, 1};
    rational_make(num, den, &time);
    return time;
}

Rational motive_time_plus(Rational a, Rational b) {
    Rational sum = {0, 1};
    rational_add(a, b, &sum);
    return sum;
}

Rational motive_time_minus(Rational a, Rational b) {
    Rational difference = {0, 1};
    rational_sub(a, b, &difference);
    return difference;
}

bool motive_meter_of(const MotiveSketch *sketch, PlanMeter *meter) {
    if (!sketch || !meter || !msk_ok(sketch->bar) || !msk_ok(sketch->beat) || sketch->beat.num != 1) {
        return false;
    }
    Rational beats = msk_div(sketch->bar, sketch->beat);
    if (!msk_ok(beats) || beats.den != 1 || beats.num < 1 || beats.num > 64) {
        return false;
    }
    PlanMeter found = {(int)beats.num, (int)sketch->beat.den, 0};
    if (!meter_valid(&found)) {
        return false;
    }
    *meter = found;
    return true;
}

bool motive_set_meter(MotiveSketch *sketch, PlanMeter meter) {
    if (!sketch || !meter_valid(&meter)) {
        return false;
    }
    sketch->beat = msk_make(1, (unsigned long long)meter.beat_unit);
    sketch->bar = msk_make((unsigned long long)meter.beats_per_bar, (unsigned long long)meter.beat_unit);
    return true;
}

bool motive_key_of(const MotiveSketch *sketch, SpelledKey *key) {
    if (!sketch || !key || !sketch->has_scale || sketch->tonic_pc < 0 || sketch->tonic_pc > 11) {
        return false;
    }
    *key = plan_key_of_pitch_class(sketch->tonic_pc, sketch->material);
    if (sketch->has_tonic_spelling) {
        key->tonic_letter = (PitchLetter)sketch->tonic_letter;
        key->tonic_accidental = sketch->tonic_accidental;
    }
    return true;
}

void motive_set_key(MotiveSketch *sketch, const SpelledKey *key, MotiveSketchKeySource source) {
    if (!sketch || !key) {
        return;
    }
    if (source == MOTIVE_SKETCH_KEY_UNSPECIFIED) {
        /* No key: no scale, so the relations that need one refuse the motive (motive_key reads C major). */
        sketch->has_scale = false;
        sketch->tonic_pc = 0;
        sketch->material = NULL;
        sketch->key_source = MOTIVE_SKETCH_KEY_UNSPECIFIED;
        sketch->has_tonic_spelling = false;
        sketch->tonic_letter = 0;
        sketch->tonic_accidental = 0;
        return;
    }
    sketch->has_scale = true;
    sketch->tonic_pc = spelled_key_tonic_pitch_class(key);
    sketch->material = key->material;
    sketch->key_source = source;
    sketch->has_tonic_spelling = true;
    sketch->tonic_letter = (int)key->tonic_letter;
    sketch->tonic_accidental = key->tonic_accidental;
}

Rational motive_pickup(const Motive *motive) {
    return motive->pickup.den == 0 ? motive_time(0, 1) : motive->pickup;
}

Rational motive_event_end(const MotiveSketchNote *note) {
    return motive_time_plus(note->onset, note->duration);
}

void motive_init(Motive *motive, PlanMeter meter) {
    memset(motive, 0, sizeof(*motive));
    motive_set_meter(motive, meter);
    motive->pickup = motive_time(0, 1);
    motive->length = motive_time(0, 1);
}

void motive_lane_notes(const Motive *motive, PlanNote *out) {
    for (size_t i = 0; i < motive->note_count; i++) {
        out[i] = plan_note(motive_pitch(motive, i), motive->notes[i].onset, motive->notes[i].duration);
    }
}

void motive_set_lane_notes(Motive *motive, const PlanNote *notes, size_t count) {
    motive->note_count = count < MOTIVE_MAX_NOTES ? count : MOTIVE_MAX_NOTES;
    for (size_t i = 0; i < motive->note_count; i++) {
        motive->notes[i].pitch = notes[i].midi;
        motive->notes[i].onset = notes[i].offset;
        motive->notes[i].duration = notes[i].duration;
    }
}

int motive_pitch(const Motive *motive, size_t index) {
    if (motive->notes[index].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN) {
        return motive->notes[index].pitch;
    }
    return 60 + (motive->has_scale ? motive->tonic_pc : 0);
}

SpelledKey motive_key(const Motive *motive) {
    SpelledKey key;
    if (!motive_key_of(motive, &key)) {
        key = plan_key_of_pitch_class(0, NULL); /* the zero key: C, no material (it reads as C major) */
    }
    return key;
}

PlanMeter motive_meter(const Motive *motive) {
    PlanMeter meter = {4, 4, 0};
    if (!motive_meter_of(motive, &meter)) {
        meter.beats_per_bar = 4;
        meter.beat_unit = 4;
    }
    return meter;
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

MotiveResult motive_check_notes(const Motive *motive, Rational *length, MotiveError *error) {
    if (motive->note_count == 0) {
        motive_set_error(error, "", "the motive has no notes");
        return MOTIVE_ERR_EMPTY;
    }
    if (motive->note_count > MOTIVE_MAX_NOTES) {
        motive_set_error(error, "", "the motive has %zu notes; the most is %d", motive->note_count, MOTIVE_MAX_NOTES);
        return MOTIVE_ERR_TOO_MANY_NOTES;
    }
    Rational end = motive_time(0, 1);
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveSketchNote *note = &motive->notes[i];
        char location[MOTIVE_ERROR_LOCATION_MAX];
        snprintf(location, sizeof(location), "notes[%zu]", i);
        if (note->pitch != MOTIVE_SKETCH_PITCH_UNKNOWN && (note->pitch < 0 || note->pitch > 127)) {
            motive_set_error(error, location, "pitch %d is outside the MIDI range 0 to 127", note->pitch);
            return MOTIVE_ERR_VALUE;
        }
        if (note->duration.num <= 0) {
            motive_set_error(error, location, "the duration must be longer than zero");
            return MOTIVE_ERR_VALUE;
        }
        if (note->onset.num < 0) {
            motive_set_error(error, location, "the offset must not be before the start of the motive");
            return MOTIVE_ERR_VALUE;
        }
        if (rational_cmp(note->onset, end) < 0) {
            motive_set_error(error, location, "the note starts at %ld/%lu, before the note before it ends at %ld/%lu",
                             (long)note->onset.num, (unsigned long)note->onset.den, (long)end.num, (unsigned long)end.den);
            return MOTIVE_ERR_VALUE;
        }
        end = motive_event_end(note);
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
static bool infer_key(const Motive *motive, const CommonPracticeMaterials *materials, SpelledKey *out) {
    bool present[12] = {false};
    for (size_t i = 0; i < motive->note_count; i++) {
        if (motive->notes[i].pitch != MOTIVE_SKETCH_PITCH_UNKNOWN) {
            present[motive->notes[i].pitch % 12] = true;
        }
    }
    SpelledKey found[24];
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
        if (common_practice_identify_key_mode(materials, relative, count, &mode)) {
            found[found_count] = plan_key_of_pitch_class(tonic, common_practice_material(materials, mode));
            found_count++;
        }
    }
    if (found_count == 1) {
        *out = found[0];
        return true;
    }
    int last = motive->notes[motive->note_count - 1].pitch % 12;
    size_t match = found_count;
    for (size_t i = 0; i < found_count; i++) {
        if (spelled_key_tonic_pitch_class(&found[i]) == last) {
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
    Rational length;
    MotiveResult checked = motive_check_notes(motive, &length, error);
    if (checked != MOTIVE_OK) {
        return checked;
    }
    motive->length = length;

    bool asked_meter = options && options->has_meter;
    PlanMeter meter = {4, 4, 0};
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
        meter = document->meter;
    } else if (asked_meter) {
        meter = options->meter;
    } else if (document->has_meter) {
        meter = document->meter;
    } else {
        motive_set_error(error, "meter", "the input has no meter; give one");
        return MOTIVE_ERR_METER;
    }
    if (!meter_valid(&meter) || !motive_set_meter(motive, meter)) {
        motive_set_error(error, "meter", "%d/%d is not a meter; use 1 to 64 beats over a beat unit of 1, 2, 4, 8 or 16",
                         meter.beats_per_bar, meter.beat_unit);
        return MOTIVE_ERR_VALUE;
    }

    if (options && options->max_bars > 0) {
        /* The notes end at motive->length; the limit is that many whole bars of the meter. */
        Rational bar = motive->bar;
        Rational limit;
        if (rational_make((long)options->max_bars * (long)bar.num, bar.den, &limit) != RATIONAL_OK ||
            rational_cmp(motive->length, limit) > 0) {
            motive_set_error(error, "", "the notes fill more than %zu bars of %d/%d", options->max_bars,
                             meter.beats_per_bar, meter.beat_unit);
            return MOTIVE_ERR_VALUE;
        }
    }

    Rational pickup = options && options->has_pickup ? options->pickup
                      : document->has_pickup           ? document->pickup
                                                       : motive_time(0, 1);
    if (pickup.den == 0 || pickup.num < 0 || rational_cmp(pickup, motive->bar) >= 0) {
        motive_set_error(error, "pickup", "the pickup must be at least 0 and shorter than one bar of %d/%d",
                         meter.beats_per_bar, meter.beat_unit);
        return MOTIVE_ERR_VALUE;
    }
    motive->pickup = pickup;
    motive->form = document->form;

    SpelledKey key;
    MotiveSketchKeySource source;
    if (options && options->has_key) {
        key = options->key;
        source = MOTIVE_SKETCH_KEY_GIVEN;
    } else if (document->has_key) {
        key = document->key;
        source = MOTIVE_SKETCH_KEY_GIVEN;
    } else if (infer_key(motive, &document->materials, &key)) {
        source = MOTIVE_SKETCH_KEY_INFERRED;
    } else {
        key = plan_key_of_pitch_class(0, document->materials.major);
        source = MOTIVE_SKETCH_KEY_UNSPECIFIED;
    }
    if (spelled_key_tonic_pitch_class(&key) < 0 || spelled_key_tonic_pitch_class(&key) > 11) {
        motive_set_error(error, "key", "the tonic pitch class %d is not from 0 to 11", spelled_key_tonic_pitch_class(&key));
        return MOTIVE_ERR_VALUE;
    }
    /* A motive with no key has no scale: its key reads as C major (motive_key). */
    motive_set_key(motive, &key, source);
    if (motive->form == MOTIVE_SKETCH_FORM_RHYTHM) {
        /* A rhythm has no pitches (the library's rhythm sketch); motive_pitch sounds them on degree 1 of the key. */
        for (size_t i = 0; i < motive->note_count; i++) {
            motive->notes[i].pitch = MOTIVE_SKETCH_PITCH_UNKNOWN;
        }
    }

    /* Own harmony, features and role: the caller's replace the input's. The chords' onsets are in beats, so they are
       read here, once the meter is settled. */
    motive->chord_count = 0;
    motive->feature_count = 0;
    motive->accompaniment = false;
    if (options && options->has_harmony) {
        if (options->harmony_count > MOTIVE_MAX_CHORDS) {
            motive_set_error(error, "harmony", "a motive has at most %d chords", MOTIVE_MAX_CHORDS);
            return MOTIVE_ERR_VALUE;
        }
        memcpy(motive->chords, options->harmony, options->harmony_count * sizeof(MotiveSketchChord));
        motive->chord_count = options->harmony_count;
    } else if (document->has_harmony_text) {
        MotiveError parsed;
        MotiveResult read = motive_harmony_parse(document->harmony_text, &meter, motive->chords, &motive->chord_count,
                                                 &parsed);
        if (read != MOTIVE_OK) {
            motive_set_error(error, "harmony", "%s: %s", parsed.location, parsed.message);
            return read;
        }
    }
    if (options && options->has_features) {
        if (options->feature_count > MOTIVE_MAX_FEATURES) {
            motive_set_error(error, "features", "a motive has at most %d features", MOTIVE_MAX_FEATURES);
            return MOTIVE_ERR_VALUE;
        }
        memcpy(motive->features, options->features, options->feature_count * sizeof(MotiveSketchFeature));
        motive->feature_count = options->feature_count;
    } else if (document->has_features) {
        memcpy(motive->features, document->features, document->feature_count * sizeof(MotiveSketchFeature));
        motive->feature_count = document->feature_count;
    }
    if (options && options->has_role) {
        motive->accompaniment = options->accompaniment;
    } else if (document->has_role) {
        motive->accompaniment = document->accompaniment;
    }
    return motive_check_extras(motive, error);
}

MotiveResult motive_check_extras(const Motive *motive, MotiveError *error) {
    if (motive->chord_count > MOTIVE_MAX_CHORDS) {
        motive_set_error(error, "harmony", "a motive has at most %d chords", MOTIVE_MAX_CHORDS);
        return MOTIVE_ERR_VALUE;
    }
    for (size_t i = 0; i < motive->chord_count; i++) {
        const MotiveSketchChord *chord = &motive->chords[i];
        char location[MOTIVE_ERROR_LOCATION_MAX];
        snprintf(location, sizeof(location), "harmony[%zu]", i);
        if (chord->degree < 1 || chord->degree > 7 || chord->alteration < -1 || chord->alteration > 1 ||
            chord->inversion < 0 || chord->inversion > 3 || (chord->inversion == 3 && !chord->seventh)) {
            motive_set_error(error, location,
                             "a chord is degree 1 to 7, altered by -1 to 1, in inversion 0 to 2 (3 only on a seventh)");
            return MOTIVE_ERR_VALUE;
        }
        if (chord->onset.den == 0 || chord->onset.num < 0 || rational_cmp(chord->onset, motive->length) >= 0) {
            motive_set_error(error, location, "the chord must start inside the motive, before %ld/%lu",
                             motive->length.num, motive->length.den);
            return MOTIVE_ERR_VALUE;
        }
        if (i > 0 && rational_cmp(chord->onset, motive->chords[i - 1].onset) <= 0) {
            motive_set_error(error, location, "the chord must come after the one before it");
            return MOTIVE_ERR_VALUE;
        }
    }
    if (motive->feature_count > MOTIVE_MAX_FEATURES) {
        motive_set_error(error, "features", "a motive has at most %d features", MOTIVE_MAX_FEATURES);
        return MOTIVE_ERR_VALUE;
    }
    for (size_t i = 0; i < motive->feature_count; i++) {
        const MotiveSketchFeature *feature = &motive->features[i];
        char location[MOTIVE_ERROR_LOCATION_MAX];
        snprintf(location, sizeof(location), "features[%zu]", i);
        size_t label_size = 0;
        while (label_size < sizeof(feature->label) && feature->label[label_size] != '\0') {
            label_size++;
        }
        if (label_size < 1 || label_size > MOTIVE_FEATURE_LABEL_MAX) {
            motive_set_error(error, location, "a feature label is 1 to %d characters", MOTIVE_FEATURE_LABEL_MAX);
            return MOTIVE_ERR_VALUE;
        }
        if (feature->first > feature->last || feature->last >= motive->note_count) {
            motive_set_error(error, location, "the feature '%s' covers notes %zu to %zu; the motive has notes 0 to %zu",
                             feature->label, feature->first, feature->last, motive->note_count - 1);
            return MOTIVE_ERR_VALUE;
        }
        for (size_t j = 0; j < i; j++) {
            if (strcmp(motive->features[j].label, feature->label) == 0) {
                motive_set_error(error, location, "the label '%s' is used twice", feature->label);
                return MOTIVE_ERR_VALUE;
            }
        }
    }
    return MOTIVE_OK;
}

/* --------------------------------------------------------------- loading */

static bool begin(Motive *motive, MotiveDocument *document, const MotiveOptions *options, MotiveError *error) {
    memset(motive, 0, sizeof(*motive));
    memset(document, 0, sizeof(*document));
    if (error) {
        memset(error, 0, sizeof(*error));
    }
    if (!options || !common_practice_materials_resolve(options->knowledge, &document->materials, NULL, 0)) {
        motive_set_error(error, "", "the options carry no knowledge snapshot with the common-practice materials");
        return false;
    }
    document->knowledge = options->knowledge;
    document->key = plan_key_of_pitch_class(0, document->materials.major);
    motive_set_meter(motive, (PlanMeter){4, 4, 0});
    motive->length = motive_time(0, 1);
    motive->pickup = motive_time(0, 1);
    return true;
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
    if (!begin(&motive, &document, options, error)) {
        return MOTIVE_ERR_NULL;
    }
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
    if (!begin(&motive, &document, options, error)) {
        return MOTIVE_ERR_NULL;
    }
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
    if (!begin(&motive, &document, options, error)) {
        return MOTIVE_ERR_NULL;
    }
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

/* ------------------------------------------------------------------ value */

bool motive_equal(const Motive *a, const Motive *b) {
    if (!a || !b) {
        return a == b;
    }
    if (rational_cmp(a->bar, b->bar) != 0 || rational_cmp(a->beat, b->beat) != 0 || a->key_source != b->key_source ||
        a->note_count != b->note_count || rational_cmp(a->length, b->length) != 0 || a->form != b->form ||
        rational_cmp(motive_pickup(a), motive_pickup(b)) != 0) {
        return false;
    }
    if (a->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED &&
        (a->has_scale != b->has_scale || (a->has_scale && (a->tonic_pc != b->tonic_pc || motive_sketch_minor(a) != motive_sketch_minor(b))))) {
        return false;
    }
    for (size_t i = 0; i < a->note_count; i++) {
        if (a->notes[i].pitch != b->notes[i].pitch ||
            rational_cmp(a->notes[i].onset, b->notes[i].onset) != 0 ||
            rational_cmp(a->notes[i].duration, b->notes[i].duration) != 0) {
            return false;
        }
    }
    if (a->chord_count != b->chord_count || a->feature_count != b->feature_count || a->accompaniment != b->accompaniment) {
        return false;
    }
    for (size_t i = 0; i < a->chord_count; i++) {
        const MotiveSketchChord *x = &a->chords[i];
        const MotiveSketchChord *y = &b->chords[i];
        if (rational_cmp(x->onset, y->onset) != 0 || x->degree != y->degree || x->alteration != y->alteration ||
            x->inversion != y->inversion || x->seventh != y->seventh) {
            return false;
        }
    }
    for (size_t i = 0; i < a->feature_count; i++) {
        if (strcmp(a->features[i].label, b->features[i].label) != 0 || a->features[i].first != b->features[i].first ||
            a->features[i].last != b->features[i].last) {
            return false;
        }
    }
    return true;
}

/* ------------------------------------------------------------------- JSON */

static cJSON *meter_json(const PlanMeter *meter) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "beats_per_bar", common_json_number(meter->beats_per_bar)) &&
                    common_json_add(object, "beat_unit", common_json_number(meter->beat_unit)) &&
                    common_json_add(object, "bars_per_hyperbar", common_json_number(meter->bars_per_hyperbar)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *key_json(const SpelledKey *key) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "tonic_pitch_class", common_json_number(spelled_key_tonic_pitch_class(key))) &&
                    common_json_add(object, "material", cJSON_CreateString(plan_key_material_id(key))))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *notes_json(const Motive *motive) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < motive->note_count; i++) {
        const MotiveSketchNote *note = &motive->notes[i];
        cJSON *object = cJSON_CreateObject();
        bool ok = object && common_json_add(object, "midi_pitch", common_json_number(motive_pitch(motive, i))) &&
                  common_json_add(object, "offset", common_json_rational_object(note->onset.num, note->onset.den)) &&
                  common_json_add(object, "duration", common_json_rational_object(note->duration.num, note->duration.den));
        if (!ok) {
            cJSON_Delete(object);
            cJSON_Delete(array);
            return NULL;
        }
        /* common_json_append frees the object itself when it fails. */
        if (!common_json_append(array, object)) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

static cJSON *features_json(const Motive *motive) {
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < motive->feature_count; i++) {
        const MotiveSketchFeature *feature = &motive->features[i];
        cJSON *object = cJSON_CreateObject();
        bool ok = object && common_json_add(object, "label", cJSON_CreateString(feature->label)) &&
                  common_json_add(object, "first", common_json_number((long)feature->first)) &&
                  common_json_add(object, "last", common_json_number((long)feature->last));
        if (!ok) {
            cJSON_Delete(object);
            cJSON_Delete(array);
            return NULL;
        }
        if (!common_json_append(array, object)) {
            cJSON_Delete(array);
            return NULL;
        }
    }
    return array;
}

cJSON *motive_json_object(const Motive *motive) {
    cJSON *document = cJSON_CreateObject();
    PlanMeter meter = motive_meter(motive);
    bool ok = document && common_json_add(document, "meter", meter_json(&meter));
    /* An inferred key is worked out again on reading, so only a stated one is written. */
    if (ok && motive->key_source == MOTIVE_SKETCH_KEY_GIVEN) {
        SpelledKey key = motive_key(motive);
        ok = common_json_add(document, "key", key_json(&key));
    }
    ok = ok && common_json_add(document, "notes", notes_json(motive));
    if (ok && motive->form != MOTIVE_SKETCH_FORM_FULL) {
        ok = common_json_add(document, "form", cJSON_CreateString(motive_form_name(motive->form)));
    }
    if (ok && motive_pickup(motive).num != 0) {
        ok = common_json_add(document, "pickup", common_json_rational_object(motive_pickup(motive).num, motive_pickup(motive).den));
    }
    if (ok && motive->chord_count > 0) {
        char *harmony = motive_harmony_to_text(motive);
        ok = harmony && common_json_add(document, "harmony", cJSON_CreateString(harmony));
        free(harmony);
    }
    if (ok && motive->feature_count > 0) {
        ok = common_json_add(document, "features", features_json(motive));
    }
    if (ok && motive->accompaniment) {
        ok = common_json_add(document, "role", cJSON_CreateString(motive_role_name(motive->accompaniment)));
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
    char *text = printed ? common_json_indent(printed) : NULL;
    cJSON_free(printed);
    return text;
}

/* ------------------------------------------------------------------ reading a motive document */

#define READER_PATH_MAX 128

/* The first failure of a read: where in the document and what was wrong (UrsatzError path and message). */
typedef struct {
    UrsatzError error;
    bool failed;
} MotiveReader;

/* Records the first failure only; always returns false. */
static bool reader_fail(MotiveReader *reader, const char *path, const char *format, ...) {
    if (!reader->failed) {
        reader->failed = true;
        va_list args;
        va_start(args, format);
        vsnprintf(reader->error.message, sizeof(reader->error.message), format, args);
        va_end(args);
        snprintf(reader->error.path, sizeof(reader->error.path), "%s", path);
    }
    return false;
}

static bool reader_bad(MotiveReader *reader, const char *path, const char *message) {
    return reader_fail(reader, path, "%s", message);
}

static void child_path(char *out, const char *parent, const char *key) {
    if (parent[0] == '\0') {
        snprintf(out, READER_PATH_MAX, "%s", key);
    } else {
        snprintf(out, READER_PATH_MAX, "%s.%s", parent, key);
    }
}

static void index_path(char *out, const char *parent, size_t index) {
    snprintf(out, READER_PATH_MAX, "%s[%zu]", parent, index);
}

/* The member of object named key. NULL when absent, after reporting it if required. */
static const cJSON *reader_member(MotiveReader *reader, const cJSON *object, const char *path, const char *key, bool required) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item && required) {
        char at[READER_PATH_MAX];
        child_path(at, path, key);
        reader_bad(reader, at, "missing required field");
    }
    return item;
}

/* The whole-number member key of object, from min to max. */
static bool reader_int_member(MotiveReader *reader, const cJSON *object, const char *path, const char *key, long min, long max,
                              long *out) {
    const cJSON *item = reader_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[READER_PATH_MAX];
    child_path(at, path, key);
    if (!cJSON_IsNumber(item)) {
        return reader_bad(reader, at, "expected a number");
    }
    long long value;
    if (!common_json_long(object, key, &value) || value < min || value > max) {
        return reader_fail(reader, at, "expected an integer from %ld to %ld", min, max);
    }
    *out = (long)value;
    return true;
}

static bool read_time(MotiveReader *reader, const cJSON *item, const char *path, long min_num, Rational *out) {
    if (!cJSON_IsObject(item)) {
        return reader_bad(reader, path, "expected an object with num and den");
    }
    long num;
    long den;
    if (!reader_int_member(reader, item, path, "num", min_num, 1000000000L, &num) ||
        !reader_int_member(reader, item, path, "den", 1, 1000000000L, &den)) {
        return false;
    }
    return rational_make(num, (long long)den, out) == RATIONAL_OK;
}

static bool read_time_member(MotiveReader *reader, const cJSON *object, const char *path, const char *key, long min_num,
                             Rational *out) {
    const cJSON *item = reader_member(reader, object, path, key, true);
    if (!item) {
        return false;
    }
    char at[READER_PATH_MAX];
    child_path(at, path, key);
    return read_time(reader, item, at, min_num, out);
}

static bool read_meter(MotiveReader *reader, const cJSON *item, MotiveDocument *document) {
    if (!cJSON_IsObject(item)) {
        return reader_bad(reader, "meter", "expected an object");
    }
    long beats;
    long unit;
    if (!reader_int_member(reader, item, "meter", "beats_per_bar", 1, 64, &beats) ||
        !reader_int_member(reader, item, "meter", "beat_unit", 1, 16, &unit)) {
        return false;
    }
    PlanMeter meter = {(int)beats, (int)unit, 0};
    if (!meter_valid(&meter)) {
        return reader_bad(reader, "meter.beat_unit", "expected 1, 2, 4, 8 or 16");
    }
    if (cJSON_GetObjectItemCaseSensitive(item, "bars_per_hyperbar")) {
        long bars;
        if (!reader_int_member(reader, item, "meter", "bars_per_hyperbar", 0, 1000, &bars)) {
            return false;
        }
        meter.bars_per_hyperbar = (int)bars;
    }
    document->has_meter = true;
    document->meter_declared = true;
    document->meter = meter;
    return true;
}

static bool read_key(MotiveReader *reader, const cJSON *item, MotiveDocument *document) {
    if (!cJSON_IsObject(item)) {
        return reader_bad(reader, "key", "expected an object");
    }
    long tonic;
    if (!reader_int_member(reader, item, "key", "tonic_pitch_class", 0, 11, &tonic)) {
        return false;
    }
    /* "material" (a K1 id, name, key word or alias) or the old "mode" (major or minor); the two together are refused. */
    const cJSON *material = cJSON_GetObjectItemCaseSensitive(item, "material");
    const cJSON *old_mode = cJSON_GetObjectItemCaseSensitive(item, "mode");
    if (material && old_mode) {
        return reader_bad(reader, "key.mode", "give \"material\" or the old \"mode\", not both");
    }
    const PitchMaterial *picked = NULL;
    if (material) {
        if (!cJSON_IsString(material)) {
            return reader_bad(reader, "key.material", "expected a material id, name, key word or alias");
        }
        picked = knowledge_snapshot_material_named(document->knowledge, material->valuestring);
        if (!picked || !composer_material_supported(&document->materials, picked)) {
            return reader_bad(reader, "key.material", "expected a material the Composer writes keys in (major or minor)");
        }
    } else {
        const cJSON *mode = reader_member(reader, item, "key", "mode", true);
        if (!mode) {
            return false;
        }
        if (!cJSON_IsString(mode) || (strcmp(mode->valuestring, "major") != 0 && strcmp(mode->valuestring, "minor") != 0)) {
            return reader_bad(reader, "key.mode", "expected \"major\" or \"minor\"");
        }
        picked = common_practice_material(&document->materials, strcmp(mode->valuestring, "minor") == 0 ? COMMON_PRACTICE_KEY_MINOR
                                                                                                           : COMMON_PRACTICE_KEY_MAJOR);
    }
    document->has_key = true;
    document->key = plan_key_of_pitch_class((int)tonic, picked);
    return true;
}

static bool read_notes(MotiveReader *reader, const cJSON *array, Motive *motive) {
    if (!cJSON_IsArray(array)) {
        return reader_bad(reader, "notes", "expected an array");
    }
    int count = cJSON_GetArraySize(array);
    if (count == 0) {
        return reader_fail(reader, "notes", "the motive has no notes");
    }
    if (count > MOTIVE_MAX_NOTES) {
        return reader_fail(reader, "notes", "the motive has %d notes; the most is %d", count,
                         MOTIVE_MAX_NOTES);
    }
    Rational end = motive_time(0, 1);
    for (int i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(array, i);
        char path[READER_PATH_MAX];
        index_path(path, "notes", (size_t)i);
        if (!cJSON_IsObject(item)) {
            return reader_bad(reader, path, "expected an object");
        }
        long pitch;
        MotiveSketchNote note;
        if (!reader_int_member(reader, item, path, "midi_pitch", 0, 127, &pitch) ||
            !read_time_member(reader, item, path, "offset", 0, &note.onset) ||
            !read_time_member(reader, item, path, "duration", 1, &note.duration)) {
            return false;
        }
        note.pitch = (int)pitch;
        if (rational_cmp(note.onset, end) < 0) {
            char at[READER_PATH_MAX];
            child_path(at, path, "offset");
            return reader_fail(reader, at, "the note starts before the note before it ends");
        }
        end = motive_event_end(&note);
        motive->notes[i] = note;
    }
    motive->note_count = (size_t)count;
    return true;
}

static bool read_features(MotiveReader *reader, const cJSON *array, MotiveDocument *document) {
    if (!cJSON_IsArray(array)) {
        return reader_bad(reader, "features", "expected an array");
    }
    int count = cJSON_GetArraySize(array);
    if (count > MOTIVE_MAX_FEATURES) {
        return reader_fail(reader, "features", "the motive has %d features; the most is %d", count,
                         MOTIVE_MAX_FEATURES);
    }
    for (int i = 0; i < count; i++) {
        const cJSON *item = cJSON_GetArrayItem(array, i);
        char path[READER_PATH_MAX];
        index_path(path, "features", (size_t)i);
        if (!cJSON_IsObject(item)) {
            return reader_bad(reader, path, "expected an object");
        }
        const cJSON *label = reader_member(reader, item, path, "label", true);
        long first;
        long last;
        if (!label || !reader_int_member(reader, item, path, "first", 0, 1000000L, &first) ||
            !reader_int_member(reader, item, path, "last", 0, 1000000L, &last)) {
            return false;
        }
        size_t label_size = cJSON_IsString(label) ? strlen(label->valuestring) : 0;
        if (label_size < 1 || label_size > MOTIVE_FEATURE_LABEL_MAX) {
            char at[READER_PATH_MAX];
            child_path(at, path, "label");
            return reader_bad(reader, at, "expected text of 1 to 8 characters");
        }
        MotiveSketchFeature *feature = &document->features[i];
        memset(feature, 0, sizeof(*feature));
        memcpy(feature->label, label->valuestring, label_size);
        feature->first = (size_t)first;
        feature->last = (size_t)last;
    }
    document->feature_count = (size_t)count;
    document->has_features = true;
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
    MotiveReader reader;
    memset(&reader, 0, sizeof(reader));
    bool ok = cJSON_IsObject(root) || reader_bad(&reader, "", "expected an object");
    const cJSON *meter = ok ? cJSON_GetObjectItemCaseSensitive(root, "meter") : NULL;
    const cJSON *key = ok ? cJSON_GetObjectItemCaseSensitive(root, "key") : NULL;
    ok = ok && (!meter || cJSON_IsNull(meter) || read_meter(&reader, meter, document));
    ok = ok && (!key || cJSON_IsNull(key) || read_key(&reader, key, document));
    const cJSON *form = ok ? cJSON_GetObjectItemCaseSensitive(root, "form") : NULL;
    const cJSON *pickup = ok ? cJSON_GetObjectItemCaseSensitive(root, "pickup") : NULL;
    if (ok && form && !cJSON_IsNull(form)) {
        ok = (cJSON_IsString(form) && motive_form_parse(form->valuestring, &document->form)) ||
             reader_bad(&reader, "form", "expected \"full\", \"rhythm\" or \"contour\"");
    }
    if (ok && pickup && !cJSON_IsNull(pickup)) {
        ok = read_time(&reader, pickup, "pickup", 0, &document->pickup);
        document->has_pickup = ok;
    }
    const cJSON *harmony = ok ? cJSON_GetObjectItemCaseSensitive(root, "harmony") : NULL;
    const cJSON *features = ok ? cJSON_GetObjectItemCaseSensitive(root, "features") : NULL;
    const cJSON *role = ok ? cJSON_GetObjectItemCaseSensitive(root, "role") : NULL;
    if (ok && harmony && !cJSON_IsNull(harmony)) {
        ok = (cJSON_IsString(harmony) && strlen(harmony->valuestring) < sizeof(document->harmony_text)) ||
             reader_bad(&reader, "harmony", "expected chord text such as \"I V6@2 I@3\"");
        if (ok) {
            snprintf(document->harmony_text, sizeof(document->harmony_text), "%s", harmony->valuestring);
            document->has_harmony_text = true;
        }
    }
    ok = ok && (!features || cJSON_IsNull(features) || read_features(&reader, features, document));
    if (ok && role && !cJSON_IsNull(role)) {
        ok = (cJSON_IsString(role) && motive_role_parse(role->valuestring, &document->accompaniment)) ||
             reader_bad(&reader, "role", "expected \"melodic\" or \"accompaniment\"");
        document->has_role = ok;
    }
    ok = ok && read_notes(&reader, reader_member(&reader, root, "", "notes", true), motive);
    cJSON_Delete(root);
    if (!ok) {
        motive_set_error(error, reader.error.path, "%s", reader.error.message);
        return MOTIVE_ERR_VALUE;
    }
    return MOTIVE_OK;
}

/* ----------------------------------------------------------------- attach */

static PlanResult attach(Plan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                         const PlanSource *source, PlanState state, NodeId *out_node, LaneSpanId *out_span) {
    if (!plan || !motive) {
        return PLAN_ERR_NULL;
    }
    Rational length;
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
    /* An accompaniment motive is not placed on chords by the melody operator, so it carries no tones. */
    PlanConstraint tones;
    if (!motive->accompaniment && motive_tones_constraint(motive, &tones)) {
        result = plan_node_add_constraint(plan, node, &tones);
        if (result != PLAN_OK) {
            return result;
        }
    }
    PlanNote lane_notes[MOTIVE_MAX_NOTES];
    motive_lane_notes(motive, lane_notes);
    PlanLanePayload payload;
    payload.notes.notes = lane_notes;
    payload.notes.note_count = motive->note_count;
    LaneSpanId lane_span;
    result = plan_add_lane_span(plan, node, PLAN_LANE_MELODY, 0, span, &payload, &lane_span);
    if (result != PLAN_OK) {
        return result;
    }
    result = plan_node_set_state(plan, node, state);
    if (result == PLAN_OK) {
        result = plan_lane_span_set_state(plan, lane_span, state);
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

PlanResult motive_attach(Plan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                         NodeId *out_node, LaneSpanId *out_span) {
    return attach(plan, parent, start, motive, name, NULL, PLAN_STATE_GIVEN, out_node, out_span);
}

PlanResult motive_attach_as(Plan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                            PlanState state, NodeId *out_node, LaneSpanId *out_span) {
    return attach(plan, parent, start, motive, name, NULL, state, out_node, out_span);
}

PlanResult motive_attach_sourced(Plan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                                 const PlanSource *source, NodeId *out_node, LaneSpanId *out_span) {
    return attach(plan, parent, start, motive, name, source, PLAN_STATE_GIVEN, out_node, out_span);
}

bool motive_in_key(const Motive *source, const SpelledKey *key, Motive *out) {
    if (!source || !key || !out || source->key_source == MOTIVE_SKETCH_KEY_UNSPECIFIED) {
        return false;
    }
    Motive moved = *source;
    SpelledKey source_key = motive_key(source);
    for (size_t i = 0; i < source->note_count; i++) {
        if (source->notes[i].pitch == MOTIVE_SKETCH_PITCH_UNKNOWN) {
            continue;
        }
        long position;
        int alteration;
        motive_scale_position(&source_key, source->notes[i].pitch - 60, &position, &alteration);
        int pitch = 60 + motive_position_pitch(key, position, alteration);
        if (pitch < 0 || pitch > 127) {
            return false;
        }
        moved.notes[i].pitch = pitch;
    }
    motive_set_key(&moved, key, MOTIVE_SKETCH_KEY_GIVEN);
    *out = moved;
    return true;
}
