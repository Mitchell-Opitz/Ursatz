#include <ursatz_composer/motive_analysis.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <model/pitch/midi_pitch.h>

#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"
#include "motive/motive_segments.h"
#include <common/json.h>
#include "util/util.h"
#include <plan/plan_common.h>

static long floor_div(long a, long b) {
    long quotient = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? quotient - 1 : quotient;
}

/* ------------------------------------------------------------------ names */

static const char *const SHAPE_NAMES[] = {"single note", "level", "ascending", "descending", "arch", "valley", "wave"};

/* "arch", "ascending" and so on. */
static const char *motive_shape_name(MotiveShape shape) {
    return (size_t)shape < sizeof(SHAPE_NAMES) / sizeof(SHAPE_NAMES[0]) ? SHAPE_NAMES[shape] : "unknown";
}

void motive_pitch_name(int midi, char *out, size_t size) {
    util_pitch_name(midi, false, out, size);
}

/* ---------------------------------------------------------------- degrees */

typedef struct {
    int semitones[8]; /* index 1 to 7: the semitones above the tonic of each degree */
} Scale;

static Scale scale_of(const PitchMaterial *material) {
    Scale scale;
    scale.semitones[0] = -1;
    for (int degree = 1; degree <= 7; degree++) {
        scale.semitones[degree] = util_scale_semitone(material, degree);
    }
    return scale;
}

/* The degree below a pitch outside the scale, raised by a half step. */
static MotiveDegree degree_of(const PitchMaterial *material, int relative) {
    MotiveDegree found = {1, 0};
    util_scale_degree(0, material, relative, &found.degree, &found.alteration);
    return found;
}

/* The step of the key's scale a pitch is on: octaves of 7 from the tonic's pitch class, and the degree in the octave. */
void motive_scale_position(const SpelledKey *key, int pitch, long *position, int *alteration) {
    int relative = pitch - spelled_key_tonic_pitch_class(key);
    long octave = floor_div(relative, 12);
    MotiveDegree degree = degree_of(key->material, (int)(relative - octave * 12));
    *position = octave * 7 + (degree.degree - 1);
    *alteration = degree.alteration;
}

int motive_position_pitch(const SpelledKey *key, long position, int alteration) {
    Scale scale = scale_of(key->material);
    long octave = floor_div(position, 7);
    int degree = (int)(position - octave * 7) + 1;
    return (int)(spelled_key_tonic_pitch_class(key) + octave * 12 + scale.semitones[degree] + alteration);
}

/* ---------------------------------------------------------------- contour */

static MotiveShape shape_of(const ContourDirection *directions, size_t count, size_t note_count) {
    if (note_count < 2) {
        return MOTIVE_SHAPE_SINGLE;
    }
    int changes = 0;
    int first = 0; /* 1 for up, -1 for down, 0 when none yet */
    int last = 0;
    for (size_t i = 0; i < count; i++) {
        if (directions[i] == CONTOUR_DIRECTION_SAME) {
            continue;
        }
        int direction = directions[i] == CONTOUR_DIRECTION_UP ? 1 : -1;
        if (first == 0) {
            first = direction;
        } else if (direction != last) {
            changes++;
        }
        last = direction;
    }
    if (first == 0) {
        return MOTIVE_SHAPE_LEVEL;
    }
    if (changes == 0) {
        return first > 0 ? MOTIVE_SHAPE_ASCENDING : MOTIVE_SHAPE_DESCENDING;
    }
    if (changes == 1) {
        return first > 0 ? MOTIVE_SHAPE_ARCH : MOTIVE_SHAPE_VALLEY;
    }
    return MOTIVE_SHAPE_WAVE;
}

/* ----------------------------------------------------------------- rhythm */

static bool power_of_two(unsigned long value) {
    return value != 0 && (value & (value - 1)) == 0;
}

static void analyze_rhythm(const Motive *motive, MotiveAnalysis *out) {
    Rational bar;
    PlanMeter bar_meter = motive_meter(motive);
    plan_meter_bar_length(&bar_meter, &bar);
    Rational previous_end = motive_time(0, 1);
    Rational pickup = motive_pickup(motive);
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveSketchNote *note = &motive->notes[i];
        MotiveRhythmNote *rhythm = &out->rhythm[i];
        /* The first barline is at the pickup, so a note before it is in bar -1. */
        Rational at = motive_time_minus(note->onset, pickup);
        long bar_index = floor_div(at.num * (long)bar.den, (long)at.den * bar.num);
        Rational inside = motive_time(at.num * (long)bar.den - bar_index * bar.num * (long)at.den, at.den * bar.den);
        rhythm->onset = note->onset;
        rhythm->duration = note->duration;
        rhythm->bar = (int)bar_index;
        rhythm->beat = motive_time(inside.num * motive_meter(motive).beat_unit, inside.den);
        rhythm->on_beat = rhythm->beat.den == 1;
        rhythm->dotted = (note->duration.num == 3 || note->duration.num == 7) && power_of_two(note->duration.den);
        rhythm->rest_before = motive_time_minus(note->onset, previous_end);
        previous_end = motive_event_end(note);
    }
    out->bars = motive_time(motive->length.num * (long)bar.den, motive->length.den * (unsigned long)bar.num);
}

/* ---------------------------------------------------------------- harmony */

typedef struct {
    int degree;
    bool leading_tone;
    bool seventh;
} ChordSpec;

/* The order of the candidates, which breaks ties. */
static const ChordSpec SPECS[] = {{1, false, false}, {5, false, false}, {4, false, false}, {6, false, false},
                                  {2, false, false}, {3, false, false}, {7, false, false}, {7, true, false},
                                  {5, false, true}};
#define SPEC_COUNT (sizeof(SPECS) / sizeof(SPECS[0]))

typedef struct {
    MotiveChordCandidate candidate;
    double raw_fit;
} Scored;

static bool score_chord(const Motive *motive, const MotiveAnalysis *analysis, const ChordSpec *spec, Scored *out) {
    HarmonyMode mode = harmony_mode_of_material(motive->material);
    /* A modal key's chords are stacked from its material (R-725): its seventh chords and leading-tone chord are not candidates. */
    HarmonyKind kind = {mode, pitch_material_is_modal_key(NULL, motive->material) ? motive->material : NULL, NULL};
    PhraseChord chord;
    if (spec->leading_tone) {
        if (mode != HARMONY_MODE_MINOR || kind.modal != NULL || !harmony_make_leading_tone(mode, 0, 1, &chord)) {
            return false;
        }
    } else if (!harmony_make_chord_in(&kind, spec->degree, 0, spec->seventh, false, 1, &chord)) {
        return false;
    }
    int intervals[4];
    size_t interval_count;
    int root;
    /* A Motive carries no snapshot: its chord tones are read from the fallback tables (R-728), which are the entries' own semitones. */
    if (!composer_chord_intervals(NULL, mode, kind.modal, &chord.chord, &root, intervals, &interval_count)) {
        return false;
    }
    bool member[12] = {false};
    for (size_t k = 0; k < interval_count; k++) {
        member[(motive->tonic_pc + root + intervals[k]) % 12] = true;
    }

    double total = 0.0;
    double inside = 0.0;
    size_t tones = 0;
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveSketchNote *note = &motive->notes[i];
        double weight = (double)note->duration.num / (double)note->duration.den;
        if (analysis->rhythm[i].beat.num == 0) {
            weight *= 2.0;
        }
        total += weight;
        if (member[motive_pitch(motive, i) % 12]) {
            inside += weight;
            tones++;
        }
    }
    if (tones == 0 || total <= 0.0) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    snprintf(out->candidate.roman, sizeof(out->candidate.roman), "%s", chord.roman);
    out->candidate.function = chord.function;
    out->candidate.has_function = chord.has_function;
    out->candidate.chord_tones = tones;
    out->candidate.non_chord_tones = motive->note_count - tones;
    out->raw_fit = inside / total;
    out->candidate.fit = floor(out->raw_fit * 10000.0 + 0.5) / 10000.0;
    return true;
}

static void analyze_harmony(const Motive *motive, MotiveAnalysis *out) {
    Scored scored[SPEC_COUNT];
    size_t count = 0;
    for (size_t i = 0; i < SPEC_COUNT; i++) {
        if (score_chord(motive, out, &SPECS[i], &scored[count])) {
            count++;
        }
    }
    /* Best fit first; an insertion sort keeps the order of the specs among equal fits. */
    for (size_t i = 1; i < count; i++) {
        Scored held = scored[i];
        size_t at = i;
        while (at > 0 && scored[at - 1].raw_fit < held.raw_fit - 1e-9) {
            scored[at] = scored[at - 1];
            at--;
        }
        scored[at] = held;
    }
    for (size_t i = 0; i < count; i++) {
        out->candidates[i] = scored[i].candidate;
    }
    out->candidate_count = count;
}

/* ----------------------------------------------------------------- analyze */

bool motive_analyze(const Motive *motive, MotiveAnalysis *out) {
    return motive_analyze_with_tones(motive, NULL, 0, out);
}

bool motive_analyze_with_tones(const Motive *motive, const MotiveToneOverride *overrides, size_t override_count,
                               MotiveAnalysis *out) {
    Rational length;
    if (!motive || !out || motive_check_notes(motive, &length, NULL) != MOTIVE_OK) {
        return false;
    }
    MotiveAnalysis result;
    memset(&result, 0, sizeof(result));
    result.note_count = motive->note_count;
    result.form = motive->form;
    bool full = motive->form == MOTIVE_SKETCH_FORM_FULL;
    result.intervals_known = full;
    result.contour_known = motive->form != MOTIVE_SKETCH_FORM_RHYTHM;
    result.rhythm_known = motive->form != MOTIVE_SKETCH_FORM_CONTOUR;
    result.range_known = full;
    result.length = length;
    result.beat_length = motive_time(1, (unsigned long)motive_meter(motive).beat_unit);
    result.key_known = motive->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED;
    result.key_source = motive->key_source;
    if (result.key_known) {
        result.key = motive_key(motive);
    }
    result.degrees_known = result.key_known && full;
    result.harmony_known = result.key_known && full;

    result.lowest = result.highest = motive_pitch(motive, 0);
    for (size_t i = 1; i < motive->note_count; i++) {
        int pitch = motive_pitch(motive, i);
        result.lowest = pitch < result.lowest ? pitch : result.lowest;
        result.highest = pitch > result.highest ? pitch : result.highest;
    }
    result.range_semitones = result.highest - result.lowest;

    long positions[MOTIVE_MAX_NOTES] = {0};
    for (size_t i = 0; i < motive->note_count; i++) {
        if (result.degrees_known) {
            int pitch = motive_pitch(motive, i) - motive->tonic_pc;
            long octave = floor_div(pitch, 12);
            result.degrees[i] = degree_of(motive->material, (int)(pitch - octave * 12));
            positions[i] = octave * 7 + (result.degrees[i].degree - 1);
        }
    }
    result.interval_count = motive->note_count - 1;
    for (size_t i = 0; i < result.interval_count; i++) {
        int semitones = motive_pitch(motive, i + 1) - motive_pitch(motive, i);
        result.intervals[i].semitones = semitones;
        result.intervals[i].has_steps = result.degrees_known;
        result.intervals[i].steps = result.degrees_known ? (int)(positions[i + 1] - positions[i]) : 0;
        result.directions[i] = semitones > 0   ? CONTOUR_DIRECTION_UP
                               : semitones < 0 ? CONTOUR_DIRECTION_DOWN
                                               : CONTOUR_DIRECTION_SAME;
    }
    result.shape = shape_of(result.directions, result.interval_count, motive->note_count);

    if (result.rhythm_known) {
        analyze_rhythm(motive, &result);
    } else {
        Rational bar;
        PlanMeter bar_meter = motive_meter(motive);
        plan_meter_bar_length(&bar_meter, &bar);
        result.bars = motive_time(motive->length.num * (long)bar.den, motive->length.den * (unsigned long)bar.num);
    }
    if (result.harmony_known) {
        analyze_harmony(motive, &result);
    }
    result.segments_given = motive->feature_count > 0;
    if (result.segments_given) {
        memcpy(result.segments, motive->features, motive->feature_count * sizeof(result.segments[0]));
        result.segment_count = motive->feature_count;
    } else {
        result.segment_count = motive_segments_propose(&result, result.segments, MOTIVE_MAX_FEATURES);
    }
    result.base_harmony_known = motive_base_harmony(motive, &result.base_harmony);
    result.tones_known = result.base_harmony_known &&
                         motive_tone_labels(motive, result.base_harmony.chords, result.base_harmony.count, overrides,
                                            override_count, result.tones) == MOTIVE_TONE_OK;
    if (override_count > 0 && !result.tones_known) {
        return false;
    }
    *out = result;
    return true;
}

/* ------------------------------------------------------------------ print */

static const char *direction_name(ContourDirection direction) {
    return direction == CONTOUR_DIRECTION_UP ? "up" : direction == CONTOUR_DIRECTION_DOWN ? "down" : "same";
}

static void key_text(const SpelledKey *key, char *out, size_t size) {
    /* The minor key's tonic is named as a minor key is written (key_tonic_name_minor, R-582: Eb minor, not D# minor). */
    int tonic = spelled_key_tonic_pitch_class(key);
    snprintf(out, size, "%s %s",
             composer_key_is_minor(key) && !pitch_material_is_modal_key(NULL, key->material) ? key_tonic_name_minor(tonic) : key_tonic_name(tonic),
             composer_key_word(key));
}

static const char *key_source_name(MotiveSketchKeySource source) {
    return source == MOTIVE_SKETCH_KEY_GIVEN ? "given" : source == MOTIVE_SKETCH_KEY_INFERRED ? "inferred" : "unspecified";
}

/* The pickup in beats of the meter's beat unit: "0", "1", "1/2". */
static void pickup_beats_text(const Motive *motive, char *out, size_t size) {
    Rational pickup = motive_pickup(motive);
    long numerator = pickup.num * motive_meter(motive).beat_unit;
    unsigned long denominator = pickup.den;
    unsigned long a = (unsigned long)(numerator < 0 ? -numerator : numerator);
    unsigned long b = denominator;
    while (b != 0) {
        unsigned long t = a % b;
        a = b;
        b = t;
    }
    if (a > 1) {
        numerator /= (long)a;
        denominator /= a;
    }
    if (denominator == 1) {
        snprintf(out, size, "%ld", numerator);
    } else {
        snprintf(out, size, "%ld/%lu", numerator, denominator);
    }
}

/* The base harmony in the text syntax of an own harmony; caller frees. */
static char *base_harmony_text(const Motive *motive, const MotiveBaseHarmony *base) {
    Motive copy = *motive;
    memcpy(copy.chords, base->chords, base->count * sizeof(base->chords[0]));
    copy.chord_count = base->count;
    return motive_harmony_to_text(&copy);
}

/* ------------------------------------------------------------------- JSON */

static bool add_string(cJSON *object, const char *key, const char *value) {
    return common_json_add(object, key, cJSON_CreateString(value));
}

static cJSON *degree_json(const MotiveDegree *degree) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "degree", common_json_number(degree->degree)) &&
                    common_json_add(object, "alteration", common_json_number(degree->alteration)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *intervals_json(const MotiveAnalysis *analysis) {
    if (!analysis->intervals_known) {
        return cJSON_CreateNull();
    }
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < analysis->interval_count; i++) {
        const MotiveInterval *interval = &analysis->intervals[i];
        cJSON *object = cJSON_CreateObject();
        bool ok = object && common_json_add(object, "semitones", common_json_number(interval->semitones)) &&
                  common_json_add(object, "steps", interval->has_steps ? common_json_number(interval->steps) : cJSON_CreateNull());
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

static cJSON *contour_json(const MotiveAnalysis *analysis) {
    if (!analysis->contour_known) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    cJSON *directions = cJSON_CreateArray();
    bool ok = object && directions;
    for (size_t i = 0; ok && i < analysis->interval_count; i++) {
        ok = common_json_append(directions, cJSON_CreateString(direction_name(analysis->directions[i])));
    }
    if (ok) {
        ok = add_string(object, "shape", motive_shape_name(analysis->shape));
    }
    if (ok) {
        ok = common_json_add(object, "directions", directions);
        directions = NULL;
    }
    cJSON_Delete(directions);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *rhythm_json(const MotiveAnalysis *analysis) {
    if (!analysis->rhythm_known) {
        return cJSON_CreateNull();
    }
    cJSON *array = cJSON_CreateArray();
    for (size_t i = 0; array && i < analysis->note_count; i++) {
        const MotiveRhythmNote *rhythm = &analysis->rhythm[i];
        cJSON *object = cJSON_CreateObject();
        bool ok = object && common_json_add(object, "onset", common_json_rational_object(rhythm->onset.num, rhythm->onset.den)) &&
                  common_json_add(object, "duration", common_json_rational_object(rhythm->duration.num, rhythm->duration.den)) &&
                  common_json_add(object, "bar", common_json_number(rhythm->bar)) &&
                  common_json_add(object, "beat", common_json_rational_object(rhythm->beat.num, rhythm->beat.den)) &&
                  common_json_add(object, "on_beat", cJSON_CreateBool(rhythm->on_beat)) &&
                  common_json_add(object, "dotted", cJSON_CreateBool(rhythm->dotted)) &&
                  common_json_add(object, "rest_before", common_json_rational_object(rhythm->rest_before.num, rhythm->rest_before.den));
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

static cJSON *range_json(const MotiveAnalysis *analysis) {
    if (!analysis->range_known) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    if (object && !(common_json_add(object, "lowest", common_json_number(analysis->lowest)) &&
                    common_json_add(object, "highest", common_json_number(analysis->highest)) &&
                    common_json_add(object, "semitones", common_json_number(analysis->range_semitones)))) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *degrees_json(const MotiveAnalysis *analysis) {
    if (!analysis->degrees_known) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    cJSON *all = cJSON_CreateArray();
    bool ok = object && all;
    for (size_t i = 0; ok && i < analysis->note_count; i++) {
        ok = common_json_append(all, degree_json(&analysis->degrees[i]));
    }
    ok = ok && common_json_add(object, "start", degree_json(&analysis->degrees[0])) &&
         common_json_add(object, "end", degree_json(&analysis->degrees[analysis->note_count - 1]));
    if (ok) {
        ok = common_json_add(object, "all", all);
        all = NULL;
    }
    cJSON_Delete(all);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *harmony_json(const MotiveAnalysis *analysis) {
    cJSON *object = cJSON_CreateObject();
    cJSON *candidates = cJSON_CreateArray();
    bool ok = object && candidates && common_json_add(object, "key_known", cJSON_CreateBool(analysis->harmony_known));
    for (size_t i = 0; ok && i < analysis->candidate_count; i++) {
        const MotiveChordCandidate *candidate = &analysis->candidates[i];
        cJSON *entry = cJSON_CreateObject();
        ok = entry && add_string(entry, "roman_numeral", candidate->roman) &&
             add_string(entry, "function", util_function_name(candidate->has_function, candidate->function)) &&
             common_json_add(entry, "fit", cJSON_CreateNumber(candidate->fit)) &&
             common_json_add(entry, "chord_tones", common_json_number((long)candidate->chord_tones)) &&
             common_json_add(entry, "non_chord_tones", common_json_number((long)candidate->non_chord_tones));
        if (!ok) {
            cJSON_Delete(entry);
            break;
        }
        ok = common_json_append(candidates, entry);
    }
    if (ok) {
        ok = common_json_add(object, "candidates", candidates);
        candidates = NULL;
    }
    cJSON_Delete(candidates);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *key_json(const MotiveAnalysis *analysis) {
    if (!analysis->key_known) {
        return cJSON_CreateNull();
    }
    cJSON *object = cJSON_CreateObject();
    bool ok = object && common_json_add(object, "tonic_pitch_class", common_json_number(spelled_key_tonic_pitch_class(&analysis->key))) &&
              add_string(object, "material", plan_key_material_id(&analysis->key)) &&
              add_string(object, "source", key_source_name(analysis->key_source));
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* {"key": "G minor", "notes": [{"midi_pitch", "offset", "duration"}]}: the motive as it sounds, from its start. */
static cJSON *rendered_json(const Motive *rendered) {
    cJSON *object = cJSON_CreateObject();
    cJSON *notes = cJSON_CreateArray();
    bool ok = object && notes;
    if (ok && rendered->key_source != MOTIVE_SKETCH_KEY_UNSPECIFIED) {
        char text[32];
        SpelledKey rendered_key = motive_key(rendered);
        key_text(&rendered_key, text, sizeof(text));
        ok = add_string(object, "key", text);
    } else if (ok) {
        ok = common_json_add(object, "key", cJSON_CreateNull());
    }
    for (size_t i = 0; ok && i < rendered->note_count; i++) {
        cJSON *note = cJSON_CreateObject();
        ok = note && common_json_add(note, "midi_pitch", common_json_number(motive_pitch(rendered, i))) &&
             common_json_add(note, "offset", common_json_rational_object(rendered->notes[i].onset.num, rendered->notes[i].onset.den)) &&
             common_json_add(note, "duration", common_json_rational_object(rendered->notes[i].duration.num, rendered->notes[i].duration.den));
        if (ok) {
            cJSON_AddItemToArray(notes, note);
        } else {
            cJSON_Delete(note);
        }
    }
    ok = ok && common_json_add(object, "notes", notes);
    if (!ok) {
        cJSON_Delete(notes);
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static cJSON *base_harmony_json(const Motive *motive, const MotiveAnalysis *analysis) {
    if (!analysis->base_harmony_known) {
        return cJSON_CreateNull();
    }
    char *text = base_harmony_text(motive, &analysis->base_harmony);
    cJSON *object = text ? cJSON_CreateObject() : NULL;
    bool ok = object && add_string(object, "text", text) &&
              common_json_add(object, "given", cJSON_CreateBool(analysis->base_harmony.given));
    free(text);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

/* {"given": bool, "items": [{"label", "first", "last"}]}, notes 0-based and inclusive as the features of a motive. */
static cJSON *segments_json(const MotiveAnalysis *analysis) {
    cJSON *object = cJSON_CreateObject();
    cJSON *items = cJSON_CreateArray();
    bool ok = object && items && common_json_add(object, "given", cJSON_CreateBool(analysis->segments_given));
    for (size_t i = 0; ok && i < analysis->segment_count; i++) {
        cJSON *item = cJSON_CreateObject();
        ok = item && add_string(item, "label", analysis->segments[i].label) &&
             common_json_add(item, "first", common_json_number((long)analysis->segments[i].first)) &&
             common_json_add(item, "last", common_json_number((long)analysis->segments[i].last));
        if (ok) {
            cJSON_AddItemToArray(items, item);
        } else {
            cJSON_Delete(item);
        }
    }
    if (ok) {
        ok = common_json_add(object, "items", items);
        items = NULL;
    }
    cJSON_Delete(items);
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

static const char *tone_role_name(MotiveToneRole role) {
    return role == MOTIVE_TONE_CHORD ? "chord" : (role == MOTIVE_TONE_NON_CHORD ? "non-chord" : "unclassified");
}

static cJSON *tones_json(const Motive *motive, const MotiveAnalysis *analysis, cJSON **warnings_out) {
    cJSON *tones = cJSON_CreateArray();
    cJSON *warnings = cJSON_CreateArray();
    bool ok = tones && warnings;
    for (size_t i = 0; ok && analysis->tones_known && i < analysis->note_count; i++) {
        const MotiveToneLabel *label = &analysis->tones[i];
        cJSON *tone = cJSON_CreateObject();
        const char *type = label->role == MOTIVE_TONE_NON_CHORD ? motive_tone_type_name(label->type) : NULL;
        ok = tone && common_json_add(tone, "note", common_json_number((long)i + 1)) && add_string(tone, "role", tone_role_name(label->role)) &&
             common_json_add(tone, "type", type ? cJSON_CreateString(type) : cJSON_CreateNull()) &&
             common_json_add(tone, "resolves_to", label->resolves_to >= 0 ? common_json_number(label->resolves_to + 1) : cJSON_CreateNull()) &&
             common_json_add(tone, "unresolved", cJSON_CreateBool(label->unresolved)) &&
             common_json_add(tone, "overridden", cJSON_CreateBool(label->overridden));
        if (ok) {
            cJSON_AddItemToArray(tones, tone);
        } else {
            cJSON_Delete(tone);
        }
        if (ok && label->unresolved) {
            char name[16];
            char text[64];
            motive_pitch_name(motive_pitch(motive, i), name, sizeof(name));
            snprintf(text, sizeof(text), "Note %zu (%s) does not resolve", i + 1, name);
            cJSON *warning = cJSON_CreateString(text);
            if (warning) {
                cJSON_AddItemToArray(warnings, warning);
            } else {
                ok = false;
            }
        }
    }
    if (!ok) {
        cJSON_Delete(tones);
        cJSON_Delete(warnings);
        return NULL;
    }
    *warnings_out = warnings;
    return tones;
}

static char *analysis_json(const Motive *motive, const MotiveAnalysis *analysis, const Motive *rendered) {
    if (!motive || !analysis) {
        return NULL;
    }
    cJSON *document = cJSON_CreateObject();
    cJSON *body = cJSON_CreateObject();
    bool ok = document && body && common_json_add(document, "motive", motive_json_object(motive));
    char beats[32];
    if (ok) {
        pickup_beats_text(motive, beats, sizeof(beats));
    }
    char *notes_text = ok ? motive_to_notes_text(motive) : NULL;
    char *degrees_text = ok ? motive_to_degrees_text(motive) : NULL;
    char *rhythm_text = ok && motive->form == MOTIVE_SKETCH_FORM_RHYTHM ? motive_to_rhythm_text(motive) : NULL;
    char *contour_text = ok && motive->form == MOTIVE_SKETCH_FORM_CONTOUR ? motive_to_contour_text(motive) : NULL;
    ok = ok && common_json_add(body, "form", cJSON_CreateString(motive_form_name(analysis->form))) &&
         common_json_add(body, "pickup", cJSON_CreateString(beats)) &&
         common_json_add(body, "notes_text", notes_text ? cJSON_CreateString(notes_text) : cJSON_CreateNull()) &&
         common_json_add(body, "degrees_text", degrees_text ? cJSON_CreateString(degrees_text) : cJSON_CreateNull());
    if (ok && motive->form == MOTIVE_SKETCH_FORM_RHYTHM) {
        ok = rhythm_text && common_json_add(body, "rhythm_text", cJSON_CreateString(rhythm_text));
    }
    if (ok && motive->form == MOTIVE_SKETCH_FORM_CONTOUR) {
        ok = contour_text && common_json_add(body, "contour_text", cJSON_CreateString(contour_text));
    }
    if (ok && motive->chord_count > 0) {
        char *harmony_text = motive_harmony_to_text(motive);
        ok = harmony_text && common_json_add(body, "harmony_text", cJSON_CreateString(harmony_text));
        free(harmony_text);
    }
    free(notes_text);
    free(degrees_text);
    free(rhythm_text);
    free(contour_text);
    ok = ok && common_json_add(body, "note_count", common_json_number((long)analysis->note_count)) &&
         common_json_add(body, "length", common_json_rational_object(analysis->length.num, analysis->length.den)) &&
         common_json_add(body, "bars", common_json_rational_object(analysis->bars.num, analysis->bars.den)) && common_json_add(body, "key", key_json(analysis)) &&
         common_json_add(body, "intervals", intervals_json(analysis)) && common_json_add(body, "contour", contour_json(analysis)) &&
         common_json_add(body, "rhythm", rhythm_json(analysis)) && common_json_add(body, "range", range_json(analysis)) &&
         common_json_add(body, "scale_degrees", degrees_json(analysis)) &&
         common_json_add(body, "implied_harmony", harmony_json(analysis)) &&
         common_json_add(body, "base_harmony", base_harmony_json(motive, analysis)) &&
         common_json_add(body, "segments", segments_json(analysis));
    cJSON *warnings = NULL;
    cJSON *tones = ok ? tones_json(motive, analysis, &warnings) : NULL;
    ok = ok && tones && common_json_add(body, "tones", tones) && common_json_add(body, "warnings", warnings);
    if (ok) {
        ok = common_json_add(document, "analysis", body);
        body = NULL;
    }
    ok = ok && (!rendered || common_json_add(document, "rendered", rendered_json(rendered)));
    cJSON_Delete(body);
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return motive_json_text(document);
}

char *motive_analysis_to_json(const Motive *motive, const MotiveAnalysis *analysis) {
    return analysis_json(motive, analysis, NULL);
}

char *motive_analysis_to_json_rendered(const Motive *motive, const MotiveAnalysis *analysis, const Motive *rendered) {
    return rendered ? analysis_json(motive, analysis, rendered) : NULL;
}
