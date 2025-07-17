#include <ursatz_composer/motive_analysis.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"
#include "plan/plan_json_internal.h"

static long floor_div(long a, long b) {
    long quotient = a / b;
    return (a % b != 0 && ((a < 0) != (b < 0))) ? quotient - 1 : quotient;
}

/* ------------------------------------------------------------------ names */

static const char *const SHAPE_NAMES[] = {"single note", "level", "ascending", "descending", "arch", "valley", "wave"};

const char *motive_shape_name(MotiveShape shape) {
    return (size_t)shape < sizeof(SHAPE_NAMES) / sizeof(SHAPE_NAMES[0]) ? SHAPE_NAMES[shape] : "unknown";
}

void motive_duration_name(PlanTime duration, char *out, size_t size) {
    static const char LETTERS[] = {'w', 'h', 'q', 'e', 's', 't'};
    for (unsigned k = 0; k < sizeof(LETTERS); k++) {
        /* base 1/2^k, then a dot adds half of the last addition: 3/2 and 7/4 of the base. */
        for (unsigned dots = 0; dots < 3; dots++) {
            long numerator = dots == 0 ? 1 : (dots == 1 ? 3 : 7);
            unsigned long denominator = (1UL << k) * (dots == 0 ? 1UL : (dots == 1 ? 2UL : 4UL));
            PlanTime candidate = motive_time(numerator, denominator);
            if (plan_time_compare(candidate, duration) == 0) {
                snprintf(out, size, "%c%.*s", LETTERS[k], (int)dots, "..");
                return;
            }
        }
    }
    snprintf(out, size, "%ld/%lu", duration.num, duration.den);
}

void motive_pitch_name(int midi_pitch, char *out, size_t size) {
    static const char *const NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    snprintf(out, size, "%s%d", NAMES[((midi_pitch % 12) + 12) % 12], midi_pitch / 12 - 1);
}

/* ---------------------------------------------------------------- degrees */

typedef struct {
    int semitones[8]; /* index 1 to 7: the semitones above the tonic of each degree */
} Scale;

static Scale scale_of(PlanKeyMode mode) {
    Scale scale;
    scale.semitones[0] = -1;
    for (int degree = 1; degree <= 7; degree++) {
        scale.semitones[degree] = harmony_applied_scale_semitone(harmony_mode_of(mode), degree);
    }
    return scale;
}

/* The degree below a pitch outside the scale, raised by a half step. */
static MotiveDegree degree_of(const Scale *scale, int relative) {
    MotiveDegree found = {1, 0};
    for (int degree = 1; degree <= 7; degree++) {
        if (scale->semitones[degree] == relative) {
            found.degree = degree;
            found.alteration = 0;
            return found;
        }
    }
    for (int degree = 1; degree <= 7; degree++) {
        if (scale->semitones[degree] == relative - 1) {
            found.degree = degree;
            found.alteration = 1;
            return found;
        }
    }
    return found;
}

/* The step of the key's scale a pitch is on: octaves of 7 from the tonic's pitch class, and the degree in the octave. */
void motive_scale_position(const PlanKey *key, int pitch, long *position, int *alteration) {
    Scale scale = scale_of(key->mode);
    int relative = pitch - key->tonic_pitch_class;
    long octave = floor_div(relative, 12);
    MotiveDegree degree = degree_of(&scale, (int)(relative - octave * 12));
    *position = octave * 7 + (degree.degree - 1);
    *alteration = degree.alteration;
}

int motive_position_pitch(const PlanKey *key, long position, int alteration) {
    Scale scale = scale_of(key->mode);
    long octave = floor_div(position, 7);
    int degree = (int)(position - octave * 7) + 1;
    return (int)(key->tonic_pitch_class + octave * 12 + scale.semitones[degree] + alteration);
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
    PlanTime bar = motive_time(motive->meter.beats_per_bar, (unsigned long)motive->meter.beat_unit);
    PlanTime previous_end = motive_time(0, 1);
    PlanTime pickup = motive_pickup(motive);
    for (size_t i = 0; i < motive->note_count; i++) {
        const PlanNote *note = &motive->notes[i];
        MotiveRhythmNote *rhythm = &out->rhythm[i];
        /* The first barline is at the pickup, so a note before it is in bar -1. */
        PlanTime at = motive_time_minus(note->offset, pickup);
        long bar_index = floor_div(at.num * (long)bar.den, (long)at.den * bar.num);
        PlanTime inside = motive_time(at.num * (long)bar.den - bar_index * bar.num * (long)at.den, at.den * bar.den);
        rhythm->onset = note->offset;
        rhythm->duration = note->duration;
        rhythm->bar = (int)bar_index;
        rhythm->beat = motive_time(inside.num * motive->meter.beat_unit, inside.den);
        rhythm->on_beat = rhythm->beat.den == 1;
        rhythm->dotted = (note->duration.num == 3 || note->duration.num == 7) && power_of_two(note->duration.den);
        rhythm->rest_before = motive_time_minus(note->offset, previous_end);
        previous_end = motive_note_end(note);
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
    PlanKeyMode mode = motive->key.mode;
    PhraseChord chord;
    if (spec->leading_tone) {
        if (mode != PLAN_MODE_MINOR || !harmony_make_leading_tone(mode, 0, 1, &chord)) {
            return false;
        }
    } else if (!harmony_make_chord(mode, spec->degree, 0, spec->seventh, false, 1, &chord)) {
        return false;
    }
    int intervals[4];
    size_t interval_count;
    if (!harmony_diatonic_intervals(mode, &chord.chord, intervals, &interval_count)) {
        return false;
    }
    int root = spec->leading_tone ? 11 : harmony_applied_scale_semitone(harmony_mode_of(mode), spec->degree);
    bool member[12] = {false};
    for (size_t k = 0; k < interval_count; k++) {
        member[(motive->key.tonic_pitch_class + root + intervals[k]) % 12] = true;
    }

    double total = 0.0;
    double inside = 0.0;
    size_t tones = 0;
    for (size_t i = 0; i < motive->note_count; i++) {
        const PlanNote *note = &motive->notes[i];
        double weight = (double)note->duration.num / (double)note->duration.den;
        if (analysis->rhythm[i].beat.num == 0) {
            weight *= 2.0;
        }
        total += weight;
        if (member[note->midi_pitch % 12]) {
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
    PlanTime length;
    if (!motive || !out || motive_check_notes(motive, &length, NULL) != MOTIVE_OK) {
        return false;
    }
    MotiveAnalysis result;
    memset(&result, 0, sizeof(result));
    result.note_count = motive->note_count;
    result.form = motive->form;
    bool full = motive->form == MOTIVE_FORM_FULL;
    result.intervals_known = full;
    result.contour_known = motive->form != MOTIVE_FORM_RHYTHM;
    result.rhythm_known = motive->form != MOTIVE_FORM_CONTOUR;
    result.range_known = full;
    result.length = length;
    result.key_known = motive->key_source != MOTIVE_KEY_UNSPECIFIED;
    result.key_source = motive->key_source;
    if (result.key_known) {
        result.key = motive->key;
    }
    result.degrees_known = result.key_known && full;
    result.harmony_known = result.key_known && full;

    result.lowest = result.highest = motive->notes[0].midi_pitch;
    for (size_t i = 1; i < motive->note_count; i++) {
        int pitch = motive->notes[i].midi_pitch;
        result.lowest = pitch < result.lowest ? pitch : result.lowest;
        result.highest = pitch > result.highest ? pitch : result.highest;
    }
    result.range_semitones = result.highest - result.lowest;

    long positions[MOTIVE_MAX_NOTES] = {0};
    Scale scale = scale_of(motive->key.mode);
    for (size_t i = 0; i < motive->note_count; i++) {
        if (result.degrees_known) {
            int pitch = motive->notes[i].midi_pitch - motive->key.tonic_pitch_class;
            long octave = floor_div(pitch, 12);
            result.degrees[i] = degree_of(&scale, (int)(pitch - octave * 12));
            positions[i] = octave * 7 + (result.degrees[i].degree - 1);
        }
    }
    result.interval_count = motive->note_count - 1;
    for (size_t i = 0; i < result.interval_count; i++) {
        int semitones = motive->notes[i + 1].midi_pitch - motive->notes[i].midi_pitch;
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
        PlanTime bar = motive_time(motive->meter.beats_per_bar, (unsigned long)motive->meter.beat_unit);
        result.bars = motive_time(motive->length.num * (long)bar.den, motive->length.den * (unsigned long)bar.num);
    }
    if (result.harmony_known) {
        analyze_harmony(motive, &result);
    }
    *out = result;
    return true;
}

/* ------------------------------------------------------------------ print */

static const char *function_name(PlanHarmonicFunction function) {
    switch (function) {
    case PLAN_FUNCTION_TONIC:
        return "tonic";
    case PLAN_FUNCTION_PREDOMINANT:
        return "predominant";
    case PLAN_FUNCTION_DOMINANT:
        return "dominant";
    default:
        return "unknown";
    }
}

static const char *direction_name(ContourDirection direction) {
    return direction == CONTOUR_DIRECTION_UP ? "up" : direction == CONTOUR_DIRECTION_DOWN ? "down" : "same";
}

static void key_text(const PlanKey *key, char *out, size_t size) {
    static const char *const MAJOR[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    static const char *const MINOR[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    snprintf(out, size, "%s %s", key->mode == PLAN_MODE_MINOR ? MINOR[key->tonic_pitch_class] : MAJOR[key->tonic_pitch_class],
             key->mode == PLAN_MODE_MINOR ? "minor" : "major");
}

static const char *key_source_name(MotiveKeySource source) {
    return source == MOTIVE_KEY_GIVEN ? "given" : source == MOTIVE_KEY_INFERRED ? "inferred" : "unspecified";
}

/* A position in beats, counting from 1: "1", "2+1/2". */
static void beat_text(PlanTime beat, char *out, size_t size) {
    long whole = beat.num / (long)beat.den;
    long rest = beat.num - whole * (long)beat.den;
    if (rest == 0) {
        snprintf(out, size, "%ld", whole + 1);
    } else {
        snprintf(out, size, "%ld+%ld/%lu", whole + 1, rest, beat.den);
    }
}

static void degree_text(const MotiveDegree *degree, char *out, size_t size) {
    snprintf(out, size, "%s%d", degree->alteration ? "#" : "", degree->degree);
}

/* The pickup in beats of the meter's beat unit: "0", "1", "1/2". */
static void pickup_beats_text(const Motive *motive, char *out, size_t size) {
    PlanTime pickup = motive_pickup(motive);
    long numerator = pickup.num * motive->meter.beat_unit;
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

void motive_analysis_print(const Motive *motive, const MotiveAnalysis *analysis, FILE *out) {
    char text[64];
    char text2[64];
    fprintf(out, "Notes:");
    for (size_t i = 0; i < analysis->note_count; i++) {
        motive_pitch_name(motive->notes[i].midi_pitch, text, sizeof(text));
        fprintf(out, " %s", text);
    }
    fprintf(out, "\n");
    fprintf(out, "Meter: %d/%d\n", motive->meter.beats_per_bar, motive->meter.beat_unit);
    fprintf(out, "Form: %s\n", motive_form_name(analysis->form));
    char beats[32];
    pickup_beats_text(motive, beats, sizeof(beats));
    fprintf(out, "Pickup: %s\n", motive_pickup(motive).num == 0 ? "none" : beats);
    if (analysis->key_known) {
        key_text(&analysis->key, text, sizeof(text));
        fprintf(out, "Key: %s (%s)\n", text, key_source_name(analysis->key_source));
    } else {
        fprintf(out, "Key: unspecified (no key was given and the pitches do not settle on one)\n");
    }
    if (analysis->bars.den == 1 && analysis->bars.num == 1) {
        fprintf(out, "Length: 1 bar\n");
    } else {
        fprintf(out, "Length: %ld/%lu bars\n", analysis->bars.num, analysis->bars.den);
    }

    if (analysis->intervals_known) {
        fprintf(out, "Intervals:");
    }
    if (!analysis->intervals_known) {
        /* a rhythm or a contour has no intervals */
    } else if (analysis->interval_count == 0) {
        fprintf(out, " none (one note)");
    }
    for (size_t i = 0; analysis->intervals_known && i < analysis->interval_count; i++) {
        const MotiveInterval *interval = &analysis->intervals[i];
        if (interval->has_steps) {
            fprintf(out, " %+d (%+d steps)", interval->semitones, interval->steps);
        } else {
            fprintf(out, " %+d", interval->semitones);
        }
        if (i + 1 < analysis->interval_count) {
            fputc(',', out);
        }
    }
    if (analysis->intervals_known) {
        fprintf(out, "\n");
    }
    if (analysis->intervals_known && !analysis->key_known && analysis->interval_count > 0) {
        fprintf(out, "  (semitones only: diatonic steps need a key)\n");
    }

    if (analysis->contour_known) {
        fprintf(out, "Contour: %s", motive_shape_name(analysis->shape));
        for (size_t i = 0; i < analysis->interval_count; i++) {
            fprintf(out, "%s%s", i == 0 ? " (" : ", ", direction_name(analysis->directions[i]));
            if (i + 1 == analysis->interval_count) {
                fprintf(out, ")");
            }
        }
        fprintf(out, "\n");
    }

    if (analysis->rhythm_known) {
        fprintf(out, "Rhythm:\n");
    }
    for (size_t i = 0; analysis->rhythm_known && i < analysis->note_count; i++) {
        const MotiveRhythmNote *rhythm = &analysis->rhythm[i];
        motive_pitch_name(motive->notes[i].midi_pitch, text, sizeof(text));
        motive_duration_name(rhythm->duration, text2, sizeof(text2));
        char beat[32];
        beat_text(rhythm->beat, beat, sizeof(beat));
        fprintf(out, "  %-4s %-4s bar %d beat %s%s", text, text2, rhythm->bar + 1, beat,
                rhythm->on_beat ? "" : " (off the beat)");
        if (rhythm->rest_before.num > 0) {
            char rest[32];
            motive_duration_name(rhythm->rest_before, rest, sizeof(rest));
            fprintf(out, ", rest %s before", rest);
        }
        fprintf(out, "\n");
    }

    if (!analysis->range_known) {
        return;
    }
    motive_pitch_name(analysis->lowest, text, sizeof(text));
    motive_pitch_name(analysis->highest, text2, sizeof(text2));
    fprintf(out, "Range: %s to %s (%d semitones)\n", text, text2, analysis->range_semitones);

    if (analysis->degrees_known) {
        fprintf(out, "Scale degrees:");
        for (size_t i = 0; i < analysis->note_count; i++) {
            degree_text(&analysis->degrees[i], text, sizeof(text));
            fprintf(out, " %s", text);
        }
        degree_text(&analysis->degrees[0], text, sizeof(text));
        degree_text(&analysis->degrees[analysis->note_count - 1], text2, sizeof(text2));
        fprintf(out, "\n  starts on %s, ends on %s\n", text, text2);
    } else {
        fprintf(out, "Scale degrees: unknown (no key)\n");
    }

    if (!analysis->harmony_known) {
        fprintf(out, "Implied harmony: unknown (no key)\n");
        return;
    }
    fprintf(out, "Implied harmony:\n");
    if (analysis->candidate_count == 0) {
        fprintf(out, "  no chord of the key contains any of the notes\n");
    }
    for (size_t i = 0; i < analysis->candidate_count; i++) {
        const MotiveChordCandidate *candidate = &analysis->candidates[i];
        fprintf(out, "  %zu. %s (%s) fits %.0f%%: %zu of %zu notes are chord tones\n", i + 1, candidate->roman,
                function_name(candidate->function), candidate->fit * 100.0, candidate->chord_tones,
                analysis->note_count);
    }
}

/* ------------------------------------------------------------------- JSON */

static bool add_string(cJSON *object, const char *key, const char *value) {
    return json_add(object, key, cJSON_CreateString(value));
}

static cJSON *degree_json(const MotiveDegree *degree) {
    cJSON *object = cJSON_CreateObject();
    if (object && !(json_add(object, "degree", json_number(degree->degree)) &&
                    json_add(object, "alteration", json_number(degree->alteration)))) {
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
        bool ok = object && json_add(object, "semitones", json_number(interval->semitones)) &&
                  json_add(object, "steps", interval->has_steps ? json_number(interval->steps) : cJSON_CreateNull());
        if (!ok) {
            cJSON_Delete(object);
            cJSON_Delete(array);
            return NULL;
        }
        if (!json_append(array, object)) {
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
        ok = json_append(directions, cJSON_CreateString(direction_name(analysis->directions[i])));
    }
    if (ok) {
        ok = add_string(object, "shape", motive_shape_name(analysis->shape));
    }
    if (ok) {
        ok = json_add(object, "directions", directions);
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
        bool ok = object && json_add(object, "onset", motive_time_json(rhythm->onset)) &&
                  json_add(object, "duration", motive_time_json(rhythm->duration)) &&
                  json_add(object, "bar", json_number(rhythm->bar)) &&
                  json_add(object, "beat", motive_time_json(rhythm->beat)) &&
                  json_add(object, "on_beat", cJSON_CreateBool(rhythm->on_beat)) &&
                  json_add(object, "dotted", cJSON_CreateBool(rhythm->dotted)) &&
                  json_add(object, "rest_before", motive_time_json(rhythm->rest_before));
        if (!ok) {
            cJSON_Delete(object);
            cJSON_Delete(array);
            return NULL;
        }
        if (!json_append(array, object)) {
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
    if (object && !(json_add(object, "lowest", json_number(analysis->lowest)) &&
                    json_add(object, "highest", json_number(analysis->highest)) &&
                    json_add(object, "semitones", json_number(analysis->range_semitones)))) {
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
        ok = json_append(all, degree_json(&analysis->degrees[i]));
    }
    ok = ok && json_add(object, "start", degree_json(&analysis->degrees[0])) &&
         json_add(object, "end", degree_json(&analysis->degrees[analysis->note_count - 1]));
    if (ok) {
        ok = json_add(object, "all", all);
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
    bool ok = object && candidates && json_add(object, "key_known", cJSON_CreateBool(analysis->harmony_known));
    for (size_t i = 0; ok && i < analysis->candidate_count; i++) {
        const MotiveChordCandidate *candidate = &analysis->candidates[i];
        cJSON *entry = cJSON_CreateObject();
        ok = entry && add_string(entry, "roman_numeral", candidate->roman) &&
             add_string(entry, "function", function_name(candidate->function)) &&
             json_add(entry, "fit", cJSON_CreateNumber(candidate->fit)) &&
             json_add(entry, "chord_tones", json_number((long)candidate->chord_tones)) &&
             json_add(entry, "non_chord_tones", json_number((long)candidate->non_chord_tones));
        if (!ok) {
            cJSON_Delete(entry);
            break;
        }
        ok = json_append(candidates, entry);
    }
    if (ok) {
        ok = json_add(object, "candidates", candidates);
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
    bool ok = object && json_add(object, "tonic_pitch_class", json_number(analysis->key.tonic_pitch_class)) &&
              add_string(object, "mode", analysis->key.mode == PLAN_MODE_MINOR ? "minor" : "major") &&
              add_string(object, "source", key_source_name(analysis->key_source));
    if (!ok) {
        cJSON_Delete(object);
        return NULL;
    }
    return object;
}

char *motive_analysis_to_json(const Motive *motive, const MotiveAnalysis *analysis) {
    if (!motive || !analysis) {
        return NULL;
    }
    cJSON *document = cJSON_CreateObject();
    cJSON *body = cJSON_CreateObject();
    bool ok = document && body && json_add(document, "motive", motive_json_object(motive));
    char beats[32];
    if (ok) {
        pickup_beats_text(motive, beats, sizeof(beats));
    }
    char *notes_text = ok ? motive_to_notes_text(motive) : NULL;
    char *degrees_text = ok ? motive_to_degrees_text(motive) : NULL;
    char *rhythm_text = ok && motive->form == MOTIVE_FORM_RHYTHM ? motive_to_rhythm_text(motive) : NULL;
    char *contour_text = ok && motive->form == MOTIVE_FORM_CONTOUR ? motive_to_contour_text(motive) : NULL;
    ok = ok && json_add(body, "form", cJSON_CreateString(motive_form_name(analysis->form))) &&
         json_add(body, "pickup", cJSON_CreateString(beats)) &&
         json_add(body, "notes_text", notes_text ? cJSON_CreateString(notes_text) : cJSON_CreateNull()) &&
         json_add(body, "degrees_text", degrees_text ? cJSON_CreateString(degrees_text) : cJSON_CreateNull());
    if (ok && motive->form == MOTIVE_FORM_RHYTHM) {
        ok = rhythm_text && json_add(body, "rhythm_text", cJSON_CreateString(rhythm_text));
    }
    if (ok && motive->form == MOTIVE_FORM_CONTOUR) {
        ok = contour_text && json_add(body, "contour_text", cJSON_CreateString(contour_text));
    }
    free(notes_text);
    free(degrees_text);
    free(rhythm_text);
    free(contour_text);
    ok = ok && json_add(body, "note_count", json_number((long)analysis->note_count)) &&
         json_add(body, "length", motive_time_json(analysis->length)) &&
         json_add(body, "bars", motive_time_json(analysis->bars)) && json_add(body, "key", key_json(analysis)) &&
         json_add(body, "intervals", intervals_json(analysis)) && json_add(body, "contour", contour_json(analysis)) &&
         json_add(body, "rhythm", rhythm_json(analysis)) && json_add(body, "range", range_json(analysis)) &&
         json_add(body, "scale_degrees", degrees_json(analysis)) &&
         json_add(body, "implied_harmony", harmony_json(analysis));
    if (ok) {
        ok = json_add(document, "analysis", body);
        body = NULL;
    }
    cJSON_Delete(body);
    if (!ok) {
        cJSON_Delete(document);
        return NULL;
    }
    return motive_json_text(document);
}
