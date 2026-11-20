#include "motive_text.h"

#include "cli_tools.h"

#include <cJSON.h>
#include <stdlib.h>
#include <string.h>


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

/* ------------------------------------------------------------------- texts */

/* The key's name, "D minor": the tonic's spelling and the key word of the material, both from the Composer's vocabulary. */
static void key_text(long tonic, const char *material, char *out, size_t size) {
    CliResponse response;
    cJSON *vocabulary = cli_composer("vocabulary", NULL, NULL, &response) == URSATZ_OK ? cJSON_Parse(response.json) : NULL;
    const char *spelling = NULL;
    const char *word = NULL;
    const cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, member(vocabulary, "pitch_class_names")) {
        if (number(entry, "value") == ((tonic % 12) + 12) % 12) {
            const cJSON *name = member(member(entry, "names"), material);
            spelling = cJSON_IsString(name) ? name->valuestring : NULL;
        }
    }
    cJSON_ArrayForEach(entry, member(vocabulary, "materials")) {
        if (strcmp(string(entry, "value"), material) == 0) {
            word = string(entry, "label");
        }
    }
    if (spelling && word) {
        snprintf(out, size, "%s %s", spelling, word);
    } else {
        snprintf(out, size, "%ld %s", tonic, material);
    }
    cJSON_Delete(vocabulary);
    cli_response_destroy(&response);
}

/* A position in beats, counting from 1: "1", "2+1/2". */
static void beat_text(long long num, long long den, char *out, size_t size) {
    long whole = (long)(num / den);
    long rest = (long)(num - whole * den);
    if (rest == 0) {
        snprintf(out, size, "%ld", whole + 1);
    } else {
        snprintf(out, size, "%ld+%ld/%lld", whole + 1, rest, den);
    }
}

static void degree_text(const cJSON *degree, char *out, size_t size) {
    snprintf(out, size, "%s%ld", number(degree, "alteration") ? "#" : "", number(degree, "degree"));
}

/* The name of a duration as the analysis writes it: w h q e s t with up to two dots, else "n/d". */
static void duration_name(long long num, long long den, char *out, size_t size) {
    static const char LETTERS[] = {'w', 'h', 'q', 'e', 's', 't'};
    for (unsigned k = 0; k < sizeof LETTERS; k++) {
        for (unsigned dots = 0; dots < 3; dots++) {
            long long numerator = dots == 0 ? 1 : (dots == 1 ? 3 : 7);
            long long denominator = (1LL << k) * (dots == 0 ? 1 : (dots == 1 ? 2 : 4));
            if (num * denominator == numerator * den) {
                snprintf(out, size, "%c%.*s", LETTERS[k], (int)dots, "..");
                return;
            }
        }
    }
    snprintf(out, size, "%lld/%lld", num, den);
}

/* The names of the notes of the document and of the two ends of its range: one pitch-names call (cli_tools.h) gives them, the printers read them. */
typedef struct {
    char (*names)[8]; /* the notes in order, then the lowest and the highest pitch of the range */
    size_t notes;
} PitchNames;

static bool gather_names(const cJSON *notes, const cJSON *range, PitchNames *out) {
    size_t count = (size_t)cJSON_GetArraySize(notes);
    int *midi = calloc(count + 2, sizeof *midi);
    out->names = calloc(count + 2, sizeof *out->names);
    out->notes = count;
    if (!midi || !out->names) {
        free(midi);
        free(out->names);
        out->names = NULL;
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        midi[i] = (int)number(cJSON_GetArrayItem(notes, (int)i), "midi_pitch");
    }
    midi[count] = (int)number(range, "lowest");
    midi[count + 1] = (int)number(range, "highest");
    cli_pitch_names(midi, count + 2, out->names);
    free(midi);
    return true;
}

static const char *note_name(const PitchNames *names, size_t index) {
    return index < names->notes ? names->names[index] : "?";
}

/* ------------------------------------------------------------------- parts */

static void print_tones(const cJSON *analysis, const PitchNames *names, FILE *out) {
    const cJSON *tones = member(analysis, "tones");
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
        fprintf(out, "%s %zu %s %s", i == 0 ? "" : ",", i + 1, note_name(names, i), name);
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

static void print_rhythm(const cJSON *analysis, const PitchNames *names, FILE *out) {
    const cJSON *rhythm = member(analysis, "rhythm");
    if (!cJSON_IsArray(rhythm)) {
        return;
    }
    fprintf(out, "Rhythm:\n");
    char duration[64];
    char beat[32];
    size_t i = 0;
    const cJSON *note = NULL;
    cJSON_ArrayForEach(note, rhythm) {
        long long numerator, denominator;
        cli_fraction(note, "duration", &numerator, &denominator);
        duration_name(numerator, denominator, duration, sizeof(duration));
        cli_fraction(note, "beat", &numerator, &denominator);
        beat_text(numerator, denominator, beat, sizeof(beat));
        fprintf(out, "  %-4s %-4s bar %ld beat %s%s", note_name(names, i++), duration, number(note, "bar") + 1, beat,
                flag(note, "on_beat") ? "" : " (off the beat)");
        cli_fraction(note, "rest_before", &numerator, &denominator);
        if (numerator > 0) {
            char rest[32];
            duration_name(numerator, denominator, rest, sizeof(rest));
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
    PitchNames names;
    if (!gather_names(notes, member(analysis, "range"), &names)) {
        cJSON_Delete(document);
        return false;
    }
    fprintf(out, "Notes:");
    for (size_t i = 0; i < names.notes; i++) {
        fprintf(out, " %s", note_name(&names, i));
    }
    fprintf(out, "\n");
    const cJSON *meter = member(motive, "meter");
    fprintf(out, "Meter: %ld/%ld\n", number(meter, "beats_per_bar"), number(meter, "beat_unit"));
    fprintf(out, "Form: %s\n", string(analysis, "form"));
    const char *pickup = string(analysis, "pickup");
    fprintf(out, "Pickup: %s\n", strcmp(pickup, "0") == 0 ? "none" : pickup);
    const cJSON *key = member(analysis, "key");
    if (cJSON_IsObject(key)) {
        key_text(number(key, "tonic_pitch_class"), string(key, "material"), text, sizeof(text));
        fprintf(out, "Key: %s (%s)\n", text, string(key, "source"));
    } else {
        fprintf(out, "Key: unspecified (no key was given and the pitches do not settle on one)\n");
    }
    long long bars_num, bars_den;
    cli_fraction(analysis, "bars", &bars_num, &bars_den);
    if (bars_den == 1 && bars_num == 1) {
        fprintf(out, "Length: 1 bar\n");
    } else {
        fprintf(out, "Length: %lld/%lld bars\n", bars_num, bars_den);
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
    print_tones(analysis, &names, out);
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
    print_rhythm(analysis, &names, out);
    if (cJSON_IsObject(member(analysis, "range"))) {
        const cJSON *range = member(analysis, "range");
        fprintf(out, "Range: %s to %s (%ld semitones)\n", names.names[names.notes], names.names[names.notes + 1],
                number(range, "semitones"));
        print_harmony(analysis, out);
    }
    free(names.names);
    cJSON_Delete(document);
    return true;
}
