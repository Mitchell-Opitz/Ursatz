#define _POSIX_C_SOURCE 200809L

#include "romantext.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "time/duration.h"
#include "time/metrical_position.h"
#include "time/time_signature.h"

static void set_error(RomanTextAnalysis *out, const char *format, int line, const char *detail) {
    snprintf(out->error_message, sizeof(out->error_message), format, line, detail);
}

static bool is_roman_letter(char c) {
    return c == 'I' || c == 'V' || c == 'i' || c == 'v';
}

static int roman_degree(const char *letters, size_t length) {
    static const char *const NAMES[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    char lower[8] = {0};
    if (length == 0 || length >= sizeof(lower)) {
        return 0;
    }
    bool upper = isupper((unsigned char)letters[0]) != 0;
    for (size_t i = 0; i < length; i++) {
        if ((isupper((unsigned char)letters[i]) != 0) != upper) {
            return 0;
        }
        lower[i] = (char)tolower((unsigned char)letters[i]);
    }
    for (int d = 0; d < 7; d++) {
        if (strcmp(lower, NAMES[d]) == 0) {
            return d + 1;
        }
    }
    return 0;
}

/* "G:", "a:", "Bb:", "f#:", "B-:" -> true with the tonic and mode. */
static bool parse_key_token(const char *token, int *tonic, bool *minor) {
    static const int LETTER_PC[7] = {9, 11, 0, 2, 4, 5, 7}; /* a b c d e f g */
    char letter = token[0];
    if (!isalpha((unsigned char)letter) || tolower((unsigned char)letter) < 'a' || tolower((unsigned char)letter) > 'g') {
        return false;
    }
    int pc = LETTER_PC[tolower((unsigned char)letter) - 'a'];
    const char *p = token + 1;
    while (*p == '#' || *p == 'b' || *p == '-') {
        pc += *p == '#' ? 1 : -1;
        p++;
    }
    if (p[0] != ':' || p[1] != '\0') {
        return false;
    }
    *tonic = ((pc % 12) + 12) % 12;
    *minor = islower((unsigned char)letter) != 0;
    return true;
}

/* "b3", "b2.5": the beat in thousandths. */
static bool parse_beat_token(const char *token, long *thousandths) {
    if (token[0] != 'b' || !isdigit((unsigned char)token[1])) {
        return false;
    }
    const char *p = token + 1;
    long whole = 0;
    while (isdigit((unsigned char)*p)) {
        whole = whole * 10 + (*p - '0');
        p++;
    }
    long frac = 0;
    if (*p == '.') {
        p++;
        long scale = 100;
        while (isdigit((unsigned char)*p)) {
            frac += scale * (*p - '0');
            scale /= 10;
            p++;
        }
    }
    if (*p != '\0') {
        return false;
    }
    *thousandths = whole * 1000 + frac;
    return true;
}

/* A degree with its alterations from the front of *p; advances *p. */
static bool parse_degree(const char **p, int *degree, int *alteration, bool *upper) {
    const char *q = *p;
    int alter = 0;
    while (*q == 'b' || *q == '-' || *q == '#') {
        alter += *q == '#' ? 1 : -1;
        q++;
    }
    const char *start = q;
    while (is_roman_letter(*q)) {
        q++;
    }
    int d = roman_degree(start, (size_t)(q - start));
    if (d == 0) {
        return false;
    }
    *degree = d;
    *alteration = alter;
    *upper = isupper((unsigned char)*start) != 0;
    *p = q;
    return true;
}

/* Reads one chord token into chord (text set by the caller). supported is false when the grammar does not
   cover it. */
static void parse_chord_token(const char *token, RomanTextChord *chord) {
    chord->supported = false;
    const char *p = token;
    if (!parse_degree(&p, &chord->degree, &chord->alteration, &chord->upper)) {
        return;
    }
    chord->marker = 0;
    if (*p == 'o' || *p == '%' || *p == '+') {
        chord->marker = *p++;
    }
    char figure[8] = {0};
    size_t f = 0;
    while (isdigit((unsigned char)*p) || (*p == '/' && isdigit((unsigned char)p[1]))) {
        if (*p == '/') {
            p++;
            continue;
        }
        if (f + 1 >= sizeof(figure)) {
            return;
        }
        figure[f++] = *p++;
    }
    chord->seventh = false;
    chord->inversion = 0;
    if (strcmp(figure, "") == 0) {
    } else if (strcmp(figure, "6") == 0) {
        chord->inversion = 1;
    } else if (strcmp(figure, "64") == 0) {
        chord->inversion = 2;
    } else if (strcmp(figure, "7") == 0) {
        chord->seventh = true;
    } else if (strcmp(figure, "65") == 0) {
        chord->seventh = true;
        chord->inversion = 1;
    } else if (strcmp(figure, "43") == 0) {
        chord->seventh = true;
        chord->inversion = 2;
    } else if (strcmp(figure, "42") == 0 || strcmp(figure, "2") == 0) {
        chord->seventh = true;
        chord->inversion = 3;
    } else {
        return;
    }
    chord->applied = false;
    chord->target_degree = 0;
    chord->target_alteration = 0;
    if (*p == '/') {
        p++;
        if (!parse_degree(&p, &chord->target_degree, &chord->target_alteration, &chord->target_upper)) {
            return;
        }
        chord->applied = true;
    }
    if (*p != '\0') {
        return; /* a nested target, a bracket, or anything else the grammar does not read */
    }
    chord->supported = true;
}

static bool is_ignored_token(const char *token) {
    return strchr(token, '|') != NULL || strncmp(token, "var", 3) == 0 || strncmp(token, "Phrase", 6) == 0 ||
           strcmp(token, ":") == 0;
}

typedef struct {
    RomanTextAnalysis *out;
    size_t key_capacity;
    size_t chord_capacity;
} Builder;

static bool push_key(Builder *b, MusicalTime *time, int measure, int tonic, bool minor) {
    RomanTextAnalysis *out = b->out;
    if (out->key_count == b->key_capacity) {
        size_t capacity = b->key_capacity == 0 ? 8 : b->key_capacity * 2;
        RomanTextKey *grown = (RomanTextKey *)realloc(out->keys, capacity * sizeof(RomanTextKey));
        if (grown == NULL) {
            return false;
        }
        out->keys = grown;
        b->key_capacity = capacity;
    }
    RomanTextKey *key = &out->keys[out->key_count++];
    key->time = time;
    key->measure = measure;
    key->tonic_pitch_class = tonic;
    key->minor = minor;
    return true;
}

static RomanTextChord *push_chord(Builder *b) {
    RomanTextAnalysis *out = b->out;
    if (out->chord_count == b->chord_capacity) {
        size_t capacity = b->chord_capacity == 0 ? 32 : b->chord_capacity * 2;
        RomanTextChord *grown = (RomanTextChord *)realloc(out->chords, capacity * sizeof(RomanTextChord));
        if (grown == NULL) {
            return NULL;
        }
        out->chords = grown;
        b->chord_capacity = capacity;
    }
    RomanTextChord *chord = &out->chords[out->chord_count++];
    memset(chord, 0, sizeof(*chord));
    chord->key_index = SIZE_MAX;
    return chord;
}

static void copy_meta(char *dest, const char *value) {
    while (*value == ' ' || *value == '\t') {
        value++;
    }
    snprintf(dest, ROMANTEXT_META_MAX, "%s", value);
    size_t n = strlen(dest);
    while (n > 0 && (dest[n - 1] == ' ' || dest[n - 1] == '\t' || dest[n - 1] == '\r')) {
        dest[--n] = '\0';
    }
}

static void read_time_signature(const char *value, int *top, int *bottom) {
    int t = 0;
    int b = 0;
    if (sscanf(value, " %d/%d", &t, &b) == 2 && t > 0 && b > 0) {
        *top = t;
        *bottom = b;
    }
}

bool romantext_beat_offset(unsigned top, unsigned bottom, long beat_thousandths, long *numerator,
                           unsigned long *denominator) {
    unsigned long unit_n = 1;
    if (top > 3 && top % 3 == 0 && bottom >= 8) {
        unit_n = 3; /* compound: the beat is a dotted note */
    }
    long beats_after_first = beat_thousandths - 1000;
    if (beats_after_first < 0 || bottom == 0) {
        return false;
    }
    *numerator = beats_after_first * (long)unit_n;
    *denominator = 1000ul * bottom;
    return true;
}

/* The time of (measure, beat in thousandths) on a MeterMap. */
static bool meter_map_time_at(const void *context, int measure, long thousandths, MusicalTime **time) {
    const MeterMap *map = (const MeterMap *)context;
    const TimeSignature *signature = NULL;
    if (metrical_position_reading_signature(map, measure, &signature) != METRICAL_POSITION_OK) {
        return false;
    }
    long numerator = 0;
    unsigned long denominator = 0;
    if (!romantext_beat_offset(time_signature_top_number(signature), time_signature_bottom_number(signature),
                               thousandths, &numerator, &denominator)) {
        return false;
    }
    Duration *offset = NULL;
    if (duration_create(numerator, denominator, &offset) != DURATION_OK) {
        return false;
    }
    MetricalPosition *position = NULL;
    MetricalPositionResult made = metrical_position_create(measure, offset, &position);
    duration_destroy(offset);
    if (made != METRICAL_POSITION_OK) {
        return false;
    }
    MetricalPositionResult converted = metrical_position_to_time(map, position, time);
    metrical_position_destroy(position);
    return converted == METRICAL_POSITION_OK;
}

RomanTextGrid romantext_meter_map_grid(const MeterMap *map) {
    RomanTextGrid grid = {meter_map_time_at, map};
    return grid;
}

static bool before(const MusicalTime *a, const MusicalTime *b) {
    return musical_time_numerator(a) * (long)musical_time_denominator(b) <
           musical_time_numerator(b) * (long)musical_time_denominator(a);
}

static bool starts_with(const char *line, const char *prefix) {
    return strncmp(line, prefix, strlen(prefix)) == 0;
}

RomanTextStatus romantext_parse_metadata(const char *text, RomanTextAnalysis *out) {
    if (text == NULL || out == NULL) {
        return ROMANTEXT_ERR_NULL_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    char *copy = strdup(text);
    if (copy == NULL) {
        return ROMANTEXT_ERR_ALLOC;
    }
    char *save = NULL;
    for (char *line = strtok_r(copy, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
        if (starts_with(line, "Composer:")) {
            copy_meta(out->composer, line + 9);
        } else if (starts_with(line, "Title:")) {
            copy_meta(out->title, line + 6);
        } else if (starts_with(line, "Time Signature:") && out->time_signature_top == 0) {
            read_time_signature(line + 15, &out->time_signature_top, &out->time_signature_bottom);
        } else if (line[0] == 'm' && isdigit((unsigned char)line[1])) {
            break;
        }
    }
    free(copy);
    return ROMANTEXT_OK;
}

RomanTextStatus romantext_parse(const char *text, const RomanTextGrid *grid, RomanTextAnalysis *out) {
    if (text == NULL || grid == NULL || grid->time_at == NULL || out == NULL) {
        return ROMANTEXT_ERR_NULL_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    Builder builder = {out, 0, 0};
    char *copy = strdup(text);
    if (copy == NULL) {
        return ROMANTEXT_ERR_ALLOC;
    }
    RomanTextStatus status = ROMANTEXT_OK;
    bool in_measures = false;
    int line_number = 0;
    char *line_save = NULL;
    for (char *line = strtok_r(copy, "\n", &line_save); line != NULL && status == ROMANTEXT_OK;
         line = strtok_r(NULL, "\n", &line_save)) {
        line_number++;
        size_t length = strlen(line);
        while (length > 0 && (line[length - 1] == '\r' || line[length - 1] == ' ')) {
            line[--length] = '\0';
        }
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }
        if (!(line[0] == 'm' && isdigit((unsigned char)line[1]))) {
            if (starts_with(line, "Composer:")) {
                copy_meta(out->composer, line + 9);
            } else if (starts_with(line, "Title:")) {
                copy_meta(out->title, line + 6);
            } else if (starts_with(line, "Time Signature:")) {
                int top = 0;
                int bottom = 0;
                read_time_signature(line + 15, &top, &bottom);
                if (out->time_signature_top == 0) {
                    out->time_signature_top = top;
                    out->time_signature_bottom = bottom;
                } else if (in_measures && (top != out->time_signature_top || bottom != out->time_signature_bottom)) {
                    out->unsupported_count++;
                }
            }
            continue;
        }
        in_measures = true;

        char *token_save = NULL;
        char *first = strtok_r(line, " \t", &token_save);
        const char *digits = first + 1;
        char *end = NULL;
        long measure_long = strtol(digits, &end, 10);
        if (*end != '\0') {
            out->unsupported_count++; /* "m5-6", "m7a" */
            continue;
        }
        int measure = (int)measure_long;
        long beat = 1000;
        bool repeat_line = false;
        char *tokens[64];
        size_t token_count = 0;
        for (char *t = strtok_r(NULL, " \t", &token_save); t != NULL; t = strtok_r(NULL, " \t", &token_save)) {
            if (strcmp(t, "=") == 0) {
                repeat_line = true;
            }
            if (token_count < 64) {
                tokens[token_count++] = t;
            }
        }
        if (repeat_line) {
            out->unsupported_count++;
            continue;
        }
        if (measure > out->last_measure) {
            out->last_measure = measure;
        }
        for (size_t i = 0; i < token_count && status == ROMANTEXT_OK; i++) {
            const char *token = tokens[i];
            long beat_value = 0;
            int tonic = 0;
            bool minor = false;
            if (parse_beat_token(token, &beat_value)) {
                beat = beat_value;
                continue;
            }
            if (is_ignored_token(token)) {
                continue;
            }
            MusicalTime *time = NULL;
            if (parse_key_token(token, &tonic, &minor)) {
                if (!grid->time_at(grid->context, measure, beat, &time)) {
                    set_error(out, "line %d: position of \"%s\" is outside the score's measures", line_number, token);
                    status = ROMANTEXT_ERR_BAD_POSITION;
                    break;
                }
                if (!push_key(&builder, time, measure, tonic, minor)) {
                    musical_time_destroy(time);
                    status = ROMANTEXT_ERR_ALLOC;
                }
                continue;
            }
            if (!grid->time_at(grid->context, measure, beat, &time)) {
                set_error(out, "line %d: position of \"%s\" is outside the score's measures", line_number, token);
                status = ROMANTEXT_ERR_BAD_POSITION;
                break;
            }
            if (out->chord_count > 0 && before(time, out->chords[out->chord_count - 1].time)) {
                set_error(out, "line %d: \"%s\" is before the chord that precedes it", line_number, token);
                musical_time_destroy(time);
                status = ROMANTEXT_ERR_BAD_POSITION;
                break;
            }
            RomanTextChord *chord = push_chord(&builder);
            if (chord == NULL) {
                musical_time_destroy(time);
                status = ROMANTEXT_ERR_ALLOC;
                break;
            }
            chord->time = time;
            chord->measure = measure;
            snprintf(chord->text, sizeof(chord->text), "%s", token);
            parse_chord_token(token, chord);
            if (out->key_count > 0) {
                const RomanTextKey *key = &out->keys[out->key_count - 1];
                chord->key_index = out->key_count - 1;
                chord->tonic_pitch_class = key->tonic_pitch_class;
                chord->minor = key->minor;
            }
            if (!chord->supported) {
                out->unsupported_count++;
                out->unsupported_chords++;
            }
        }
    }
    free(copy);
    if (status != ROMANTEXT_OK) {
        char message[sizeof(out->error_message)];
        memcpy(message, out->error_message, sizeof(message));
        romantext_destroy(out);
        memcpy(out->error_message, message, sizeof(message));
    }
    return status;
}

RomanTextStatus romantext_load(const char *path, const RomanTextGrid *grid, RomanTextAnalysis *out) {
    if (path == NULL || grid == NULL || out == NULL) {
        return ROMANTEXT_ERR_NULL_ARGUMENT;
    }
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        memset(out, 0, sizeof(*out));
        snprintf(out->error_message, sizeof(out->error_message), "cannot read %s", path);
        return ROMANTEXT_ERR_IO;
    }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        return ROMANTEXT_ERR_ALLOC;
    }
    size_t got = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    buffer[got] = '\0';
    RomanTextStatus status = romantext_parse(buffer, grid, out);
    free(buffer);
    return status;
}

void romantext_destroy(RomanTextAnalysis *analysis) {
    if (analysis == NULL) {
        return;
    }
    for (size_t i = 0; i < analysis->key_count; i++) {
        musical_time_destroy(analysis->keys[i].time);
    }
    for (size_t i = 0; i < analysis->chord_count; i++) {
        musical_time_destroy(analysis->chords[i].time);
    }
    free(analysis->keys);
    free(analysis->chords);
    analysis->keys = NULL;
    analysis->chords = NULL;
    analysis->key_count = 0;
    analysis->chord_count = 0;
}


static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};
static const int MINOR_SCALE[7] = {0, 2, 3, 5, 7, 8, 10};

static int wrap12(int v) {
    return ((v % 12) + 12) % 12;
}

static int scale_step(int tonic, bool minor, int degree, int alteration) {
    const int *scale = minor ? MINOR_SCALE : MAJOR_SCALE;
    return wrap12(tonic + scale[degree - 1] + alteration);
}

size_t romantext_expected_chord(const RomanTextChord *c, int *root, int intervals[4]) {
    int tonic = c->tonic_pitch_class;
    bool minor = c->minor;
    int local_tonic = tonic;
    bool local_minor = minor;
    bool dominant_like = false;
    if (c->applied) {
        local_tonic = scale_step(tonic, minor, c->target_degree, c->target_alteration);
        local_minor = !c->target_upper;
    }
    if (c->applied && (c->degree == 5 || c->degree == 7)) {
        /* V/x and vii/x: the chord as it reads in the tonicized key, whose tonic is a major or minor key's */
        *root = c->degree == 5 ? wrap12(local_tonic + 7) : wrap12(local_tonic + 11);
        dominant_like = c->degree == 5;
    } else {
        *root = scale_step(local_tonic, local_minor, c->degree, c->alteration);
        if (local_minor && !c->applied && c->degree == 7 && !c->upper) {
            *root = wrap12(local_tonic + 11 + c->alteration); /* a lowercase vii in minor: the leading tone */
        }
    }
    static const int TRI_MAJ[3] = {0, 4, 7};
    static const int TRI_MIN[3] = {0, 3, 7};
    static const int TRI_DIM[3] = {0, 3, 6};
    static const int TRI_AUG[3] = {0, 4, 8};
    if (!c->seventh) {
        const int *t = c->marker == 'o' ? TRI_DIM : (c->marker == '+' ? TRI_AUG : (c->upper ? TRI_MAJ : TRI_MIN));
        memcpy(intervals, t, 3 * sizeof(int));
        intervals[3] = 0;
        return 3;
    }
    static const int S_DOM[4] = {0, 4, 7, 10};
    static const int S_MAJ[4] = {0, 4, 7, 11};
    static const int S_MIN[4] = {0, 3, 7, 10};
    static const int S_HALF[4] = {0, 3, 6, 10};
    static const int S_FULL[4] = {0, 3, 6, 9};
    const int *t;
    if (c->marker == 'o') {
        t = S_FULL;
    } else if (c->marker == '%') {
        t = S_HALF;
    } else if (!c->upper) {
        t = S_MIN;
    } else if (dominant_like || (!c->applied && c->degree == 5)) {
        t = S_DOM;
    } else {
        /* a capital seventh chord not on the dominant: the seventh the key's scale gives */
        const int *scale = local_minor ? MINOR_SCALE : MAJOR_SCALE;
        int seventh_degree = ((c->degree - 1) + 6) % 7;
        int seventh = wrap12(scale[seventh_degree] - scale[c->degree - 1]);
        t = seventh == 11 ? S_MAJ : S_DOM;
    }
    memcpy(intervals, t, 4 * sizeof(int));
    return 4;
}


bool romantext_is_raised_sixth_diminished(const RomanTextChord *chord) {
    return chord->minor && !chord->applied && chord->degree == 6 && chord->alteration == 1 && chord->marker == 'o';
}
