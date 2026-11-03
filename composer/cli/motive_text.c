#include "motive_text.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>

/* ----------------------------------------------------------------- members */

static const cJSON *member(const cJSON *object, const char *name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

static long number(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    return cJSON_IsNumber(item) ? (long)item->valuedouble : 0;
}

static const char *string(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static bool flag(const cJSON *object, const char *name) {
    return cJSON_IsTrue(member(object, name));
}

static Rational rational(const cJSON *object, const char *name) {
    const cJSON *item = member(object, name);
    Rational value = {number(item, "num"), number(item, "den")};
    if (value.den == 0) {
        value.den = 1;
    }
    return value;
}

/* ------------------------------------------------------------------- texts */

static void key_text(long tonic, const char *mode, char *out, size_t size) {
    static const char *const MAJOR[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
    static const char *const MINOR[12] = {"C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B"};
    bool minor = strcmp(mode, "minor") == 0;
    long pc = ((tonic % 12) + 12) % 12;
    snprintf(out, size, "%s %s", minor ? MINOR[pc] : MAJOR[pc], minor ? "minor" : "major");
}

/* A position in beats, counting from 1: "1", "2+1/2". */
static void beat_text(Rational beat, char *out, size_t size) {
    long whole = (long)(beat.num / beat.den);
    long rest = (long)(beat.num - whole * beat.den);
    if (rest == 0) {
        snprintf(out, size, "%ld", whole + 1);
    } else {
        snprintf(out, size, "%ld+%ld/%lld", whole + 1, rest, beat.den);
    }
}

static void degree_text(const cJSON *degree, char *out, size_t size) {
    snprintf(out, size, "%s%ld", number(degree, "alteration") ? "#" : "", number(degree, "degree"));
}

static void note_name(const cJSON *notes, size_t index, char *out, size_t size) {
    motive_pitch_name((int)number(cJSON_GetArrayItem(notes, (int)index), "midi_pitch"), out, size);
}

/* ------------------------------------------------------------------- parts */

static void print_tones(const cJSON *analysis, const cJSON *notes, FILE *out) {
    const cJSON *tones = member(analysis, "tones");
    char text[64];
    if (cJSON_GetArraySize(tones) == 0) {
        fprintf(out, "Tones: none\n");
        return;
    }
    fprintf(out, "Tones:");
    const cJSON *tone = NULL;
    size_t i = 0;
    cJSON_ArrayForEach(tone, tones) {
        const char *role = string(tone, "role");
        const cJSON *type = member(tone, "type");
        const char *name = strcmp(role, "chord") == 0 || strcmp(role, "unclassified") == 0 ? role
                           : cJSON_IsString(type)                                             ? type->valuestring
                                                                                              : role;
        note_name(notes, i, text, sizeof(text));
        fprintf(out, "%s %zu %s %s", i == 0 ? "" : ",", i + 1, text, name);
        i++;
    }
    fprintf(out, "\n");
    const cJSON *warning = NULL;
    cJSON_ArrayForEach(warning, member(analysis, "warnings")) {
        fprintf(out, "Warning: %s\n", cJSON_IsString(warning) ? warning->valuestring : "");
    }
}

static void print_intervals(const cJSON *analysis, bool key_known, FILE *out) {
    const cJSON *intervals = member(analysis, "intervals");
    if (!cJSON_IsArray(intervals)) {
        return; /* a rhythm or a contour has no intervals */
    }
    int count = cJSON_GetArraySize(intervals);
    fprintf(out, "Intervals:");
    if (count == 0) {
        fprintf(out, " none (one note)");
    }
    int i = 0;
    const cJSON *interval = NULL;
    cJSON_ArrayForEach(interval, intervals) {
        if (cJSON_IsNumber(member(interval, "steps"))) {
            fprintf(out, " %+ld (%+ld steps)", number(interval, "semitones"), number(interval, "steps"));
        } else {
            fprintf(out, " %+ld", number(interval, "semitones"));
        }
        if (++i < count) {
            fputc(',', out);
        }
    }
    fprintf(out, "\n");
    if (!key_known && count > 0) {
        fprintf(out, "  (semitones only: diatonic steps need a key)\n");
    }
}

static void print_contour(const cJSON *analysis, FILE *out) {
    const cJSON *contour = member(analysis, "contour");
    if (!cJSON_IsObject(contour)) {
        return;
    }
    const cJSON *directions = member(contour, "directions");
    int count = cJSON_GetArraySize(directions);
    fprintf(out, "Contour: %s", string(contour, "shape"));
    for (int i = 0; i < count; i++) {
        const cJSON *direction = cJSON_GetArrayItem(directions, i);
        fprintf(out, "%s%s", i == 0 ? " (" : ", ", cJSON_IsString(direction) ? direction->valuestring : "");
        if (i + 1 == count) {
            fprintf(out, ")");
        }
    }
    fprintf(out, "\n");
}

static void print_rhythm(const cJSON *analysis, const cJSON *notes, FILE *out) {
    const cJSON *rhythm = member(analysis, "rhythm");
    if (!cJSON_IsArray(rhythm)) {
        return;
    }
    fprintf(out, "Rhythm:\n");
    char text[64];
    char duration[64];
    char beat[32];
    size_t i = 0;
    const cJSON *note = NULL;
    cJSON_ArrayForEach(note, rhythm) {
        note_name(notes, i++, text, sizeof(text));
        motive_duration_name(rational(note, "duration"), duration, sizeof(duration));
        beat_text(rational(note, "beat"), beat, sizeof(beat));
        fprintf(out, "  %-4s %-4s bar %ld beat %s%s", text, duration, number(note, "bar") + 1, beat,
                flag(note, "on_beat") ? "" : " (off the beat)");
        Rational rest_before = rational(note, "rest_before");
        if (rest_before.num > 0) {
            char rest[32];
            motive_duration_name(rest_before, rest, sizeof(rest));
            fprintf(out, ", rest %s before", rest);
        }
        fprintf(out, "\n");
    }
}

static void print_harmony(const cJSON *analysis, FILE *out) {
    const cJSON *scale = member(analysis, "scale_degrees");
    char text[64];
    char text2[64];
    if (cJSON_IsObject(scale)) {
        fprintf(out, "Scale degrees:");
        const cJSON *degree = NULL;
        cJSON_ArrayForEach(degree, member(scale, "all")) {
            degree_text(degree, text, sizeof(text));
            fprintf(out, " %s", text);
        }
        degree_text(member(scale, "start"), text, sizeof(text));
        degree_text(member(scale, "end"), text2, sizeof(text2));
        fprintf(out, "\n  starts on %s, ends on %s\n", text, text2);
    } else {
        fprintf(out, "Scale degrees: unknown (no key)\n");
    }

    const cJSON *implied = member(analysis, "implied_harmony");
    if (!flag(implied, "key_known")) {
        fprintf(out, "Implied harmony: unknown (no key)\n");
        return;
    }
    fprintf(out, "Implied harmony:\n");
    const cJSON *candidates = member(implied, "candidates");
    if (cJSON_GetArraySize(candidates) == 0) {
        fprintf(out, "  no chord of the key contains any of the notes\n");
    }
    size_t i = 0;
    const cJSON *candidate = NULL;
    cJSON_ArrayForEach(candidate, candidates) {
        const cJSON *fit = member(candidate, "fit");
        fprintf(out, "  %zu. %s (%s) fits %.0f%%: %ld of %ld notes are chord tones\n", ++i,
                string(candidate, "roman_numeral"), string(candidate, "function"),
                (cJSON_IsNumber(fit) ? fit->valuedouble : 0.0) * 100.0, number(candidate, "chord_tones"),
                number(analysis, "note_count"));
    }
}

bool motive_text_print(const char *response_json, FILE *out) {
    cJSON *document = response_json ? cJSON_Parse(response_json) : NULL;
    const cJSON *motive = document ? member(document, "motive") : NULL;
    const cJSON *analysis = document ? member(document, "analysis") : NULL;
    if (!cJSON_IsObject(motive) || !cJSON_IsObject(analysis)) {
        cJSON_Delete(document);
        return false;
    }
    const cJSON *notes = member(motive, "notes");
    char text[64];
    fprintf(out, "Notes:");
    for (int i = 0; i < cJSON_GetArraySize(notes); i++) {
        note_name(notes, (size_t)i, text, sizeof(text));
        fprintf(out, " %s", text);
    }
    fprintf(out, "\n");
    const cJSON *meter = member(motive, "meter");
    fprintf(out, "Meter: %ld/%ld\n", number(meter, "beats_per_bar"), number(meter, "beat_unit"));
    fprintf(out, "Form: %s\n", string(analysis, "form"));
    const char *pickup = string(analysis, "pickup");
    fprintf(out, "Pickup: %s\n", strcmp(pickup, "0") == 0 ? "none" : pickup);
    const cJSON *key = member(analysis, "key");
    if (cJSON_IsObject(key)) {
        key_text(number(key, "tonic_pitch_class"), string(key, "mode"), text, sizeof(text));
        fprintf(out, "Key: %s (%s)\n", text, string(key, "source"));
    } else {
        fprintf(out, "Key: unspecified (no key was given and the pitches do not settle on one)\n");
    }
    Rational bars = rational(analysis, "bars");
    if (bars.den == 1 && bars.num == 1) {
        fprintf(out, "Length: 1 bar\n");
    } else {
        fprintf(out, "Length: %lld/%lld bars\n", bars.num, bars.den);
    }
    if (cJSON_IsString(member(analysis, "harmony_text"))) {
        fprintf(out, "Own harmony: %s\n", string(analysis, "harmony_text"));
    }
    const cJSON *base = member(analysis, "base_harmony");
    if (cJSON_IsObject(base)) {
        fprintf(out, "Base harmony: %s%s\n", string(base, "text"), flag(base, "given") ? " (given)" : " (proposed)");
    } else {
        fprintf(out, "Base harmony: none\n");
    }
    print_tones(analysis, notes, out);
    const cJSON *feature = NULL;
    size_t i = 0;
    cJSON_ArrayForEach(feature, member(motive, "features")) {
        fprintf(out, "%s%s: notes %ld to %ld\n", i++ == 0 ? "Features:\n  " : "  ", string(feature, "label"),
                number(feature, "first"), number(feature, "last"));
    }
    if (strcmp(string(motive, "role"), "accompaniment") == 0) {
        fprintf(out, "Role: accompaniment\n");
    }
    print_intervals(analysis, cJSON_IsObject(key), out);
    print_contour(analysis, out);
    print_rhythm(analysis, notes, out);
    if (cJSON_IsObject(member(analysis, "range"))) {
        const cJSON *range = member(analysis, "range");
        char text2[64];
        motive_pitch_name((int)number(range, "lowest"), text, sizeof(text));
        motive_pitch_name((int)number(range, "highest"), text2, sizeof(text2));
        fprintf(out, "Range: %s to %s (%ld semitones)\n", text, text2, number(range, "semitones"));
        print_harmony(analysis, out);
    }
    cJSON_Delete(document);
    return true;
}
