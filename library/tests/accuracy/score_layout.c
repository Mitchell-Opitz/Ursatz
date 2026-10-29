#define _POSIX_C_SOURCE 200809L
#include "score_layout.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "identity/entity_id.h"
#include "time/note_value.h"
#include "time/time_signature.h"

typedef struct {
    ScoreLayout *layout;
    size_t capacity;
} Reader;

static bool reader_push(Reader *r, int number, Rational start) {
    ScoreLayout *layout = r->layout;
    if (layout->measure_count == r->capacity) {
        size_t capacity = r->capacity == 0 ? 32 : r->capacity * 2;
        ScoreMeasure *grown = (ScoreMeasure *)realloc(layout->measures, capacity * sizeof(ScoreMeasure));
        if (grown == NULL) {
            return false;
        }
        layout->measures = grown;
        r->capacity = capacity;
    }
    ScoreMeasure *m = &layout->measures[layout->measure_count++];
    m->number = number;
    m->start = start;
    m->length = rational_ratio(0, 1);
    return true;
}

/* The value of attribute `name` in the tag text [tag, tag_end), copied into out; false when absent. */
static bool tag_attribute(const char *tag, const char *tag_end, const char *name, char *out, size_t size) {
    size_t name_length = strlen(name);
    for (const char *p = tag; p + name_length + 2 < tag_end; p++) {
        if ((p == tag || isspace((unsigned char)p[-1])) && strncmp(p, name, name_length) == 0 && p[name_length] == '=') {
            const char quote = p[name_length + 1];
            const char *value = p + name_length + 2;
            const char *close = memchr(value, quote, (size_t)(tag_end - value));
            if (close == NULL || (size_t)(close - value) >= size) {
                return false;
            }
            memcpy(out, value, (size_t)(close - value));
            out[close - value] = '\0';
            return true;
        }
    }
    return false;
}

static bool tag_is(const char *tag, size_t length, const char *name) {
    return length == strlen(name) && strncmp(tag, name, length) == 0;
}

/* A non-negative integer from the text after a tag, up to the next '<'. */
static long text_value(const char *after, const char *end) {
    char buffer[24];
    size_t n = 0;
    while (after < end && *after != '<' && n + 1 < sizeof(buffer)) {
        if (!isspace((unsigned char)*after)) {
            buffer[n++] = *after;
        }
        after++;
    }
    buffer[n] = '\0';
    return strtol(buffer, NULL, 10);
}

ScoreLayoutStatus score_layout_read(const char *musicxml, size_t size, ScoreLayout *out) {
    if (musicxml == NULL || out == NULL) {
        return SCORE_LAYOUT_ERR_NULL_ARGUMENT;
    }
    memset(out, 0, sizeof(*out));
    Reader reader = {out, 0};
    const char *end = musicxml + size;

    bool in_first_part = false;
    bool first_part_done = false;
    bool in_measure = false;
    bool saw_time = false;
    long divisions = 1;
    long cursor = 0;
    long measure_max = 0;
    long measure_start_units = 0; /* the running position of the next measure, in division units */
    char number_text[32] = "";
    /* note / backup / forward state */
    enum { CTX_NONE, CTX_NOTE, CTX_BACKUP, CTX_FORWARD } context = CTX_NONE;
    bool chord = false;
    bool grace = false;
    long duration = 0;
    int last_number = -1;
    bool last_number_set = false;

    /* Units are division units of the divisions in force; a change of divisions rescales what is stored, so the
       layout is accumulated in whole notes as exact rationals once a measure closes. */
    Rational position = rational_ratio(0, 1);

    const char *p = musicxml;
    while (p < end && (p = memchr(p, '<', (size_t)(end - p))) != NULL) {
        const char *tag_end = memchr(p, '>', (size_t)(end - p));
        if (tag_end == NULL) {
            break;
        }
        const char *name = p + 1;
        bool closing = *name == '/';
        if (closing) {
            name++;
        }
        if (*name == '?' || *name == '!') {
            p = tag_end + 1;
            continue;
        }
        const char *name_end = name;
        while (name_end < tag_end && !isspace((unsigned char)*name_end) && *name_end != '/' && *name_end != '>') {
            name_end++;
        }
        size_t name_length = (size_t)(name_end - name);
        bool self_closing = tag_end > p && tag_end[-1] == '/';

        if (!closing) {
            if (tag_is(name, name_length, "part")) {
                in_first_part = !first_part_done;
            } else if (in_first_part && tag_is(name, name_length, "measure")) {
                in_measure = true;
                cursor = 0;
                measure_max = 0;
                if (!tag_attribute(p, tag_end, "number", number_text, sizeof(number_text))) {
                    number_text[0] = '\0';
                }
            } else if (in_first_part && in_measure) {
                if (tag_is(name, name_length, "divisions")) {
                    long d = text_value(tag_end + 1, end);
                    if (d <= 0) {
                        score_layout_destroy(out);
                        return SCORE_LAYOUT_ERR_BAD_VALUE;
                    }
                    divisions = d;
                } else if (tag_is(name, name_length, "note")) {
                    context = CTX_NOTE;
                    chord = false;
                    grace = false;
                    duration = 0;
                } else if (tag_is(name, name_length, "backup")) {
                    context = CTX_BACKUP;
                    duration = 0;
                } else if (tag_is(name, name_length, "forward")) {
                    context = CTX_FORWARD;
                    duration = 0;
                } else if (tag_is(name, name_length, "chord")) {
                    chord = true;
                } else if (tag_is(name, name_length, "grace")) {
                    grace = true;
                } else if (tag_is(name, name_length, "duration") && context != CTX_NONE) {
                    duration = text_value(tag_end + 1, end);
                } else if (tag_is(name, name_length, "beats") && !saw_time) {
                    out->time_top = (unsigned)text_value(tag_end + 1, end);
                } else if (tag_is(name, name_length, "beat-type") && !saw_time) {
                    out->time_bottom = (unsigned)text_value(tag_end + 1, end);
                    saw_time = out->time_top != 0 && out->time_bottom != 0;
                }
            }
            (void)self_closing;
        } else {
            if (tag_is(name, name_length, "part") && in_first_part) {
                in_first_part = false;
                first_part_done = true;
            } else if (in_first_part && in_measure) {
                if (tag_is(name, name_length, "note")) {
                    if (!grace && !chord) {
                        cursor += duration;
                    }
                    if (cursor > measure_max) {
                        measure_max = cursor;
                    }
                    context = CTX_NONE;
                } else if (tag_is(name, name_length, "backup")) {
                    cursor -= duration;
                    context = CTX_NONE;
                } else if (tag_is(name, name_length, "forward")) {
                    cursor += duration;
                    if (cursor > measure_max) {
                        measure_max = cursor;
                    }
                    context = CTX_NONE;
                } else if (tag_is(name, name_length, "measure")) {
                    in_measure = false;
                    /* the measure's length in whole notes: units / (4 * divisions) */
                    Rational length = rational_ratio((unsigned long long)measure_max, 4ull * (unsigned long long)divisions);
                    char *rest = NULL;
                    long digits = strtol(number_text, &rest, 10);
                    bool continuation = last_number_set && rest != NULL && *rest != '\0' && digits == last_number &&
                                        reader.layout->measure_count > 0;
                    if (continuation) {
                        ScoreMeasure *previous = &out->measures[out->measure_count - 1];
                        if (rational_add(previous->length, length, &previous->length) != RATIONAL_OK) {
                            score_layout_destroy(out);
                            return SCORE_LAYOUT_ERR_BAD_VALUE;
                        }
                        out->continued_count++;
                    } else {
                        if (!reader_push(&reader, (int)digits, position)) {
                            score_layout_destroy(out);
                            return SCORE_LAYOUT_ERR_ALLOC;
                        }
                        out->measures[out->measure_count - 1].length = length;
                        last_number = (int)digits;
                        last_number_set = true;
                    }
                    if (rational_add(position, length, &position) != RATIONAL_OK) {
                        score_layout_destroy(out);
                        return SCORE_LAYOUT_ERR_BAD_VALUE;
                    }
                    (void)measure_start_units;
                }
            }
        }
        p = tag_end + 1;
    }
    if (out->measure_count == 0) {
        score_layout_destroy(out);
        return SCORE_LAYOUT_ERR_NO_MEASURES;
    }
    out->end = position;
    if (out->time_top != 0) {
        Rational full = rational_ratio(out->time_top, out->time_bottom);
        out->has_pickup = out->measures[0].number == 0 && rational_cmp(out->measures[0].length, full) < 0;
        for (size_t i = out->has_pickup ? 1 : 0; i + 1 < out->measure_count; i++) {
            if (rational_cmp(out->measures[i].length, full) != 0) {
                out->irregular_count++;
            }
        }
    }
    return SCORE_LAYOUT_OK;
}

void score_layout_destroy(ScoreLayout *layout) {
    if (layout == NULL) {
        return;
    }
    free(layout->measures);
    layout->measures = NULL;
    layout->measure_count = 0;
}

static bool layout_time_at(const void *context, int measure, long thousandths, MusicalTime **out_time) {
    const ScoreLayout *layout = (const ScoreLayout *)context;
    const ScoreMeasure *found = NULL;
    for (size_t i = 0; i < layout->measure_count; i++) {
        if (layout->measures[i].number == measure) {
            found = &layout->measures[i];
            break;
        }
    }
    if (found == NULL || layout->time_top == 0) {
        return false;
    }
    long numerator = 0;
    unsigned long denominator = 0;
    if (!romantext_beat_offset(layout->time_top, layout->time_bottom, thousandths, &numerator, &denominator)) {
        return false;
    }
    /* A pickup's beats are counted from where a full measure would have started, so beat 3 of a one-beat pickup in
       3/4 is its start: shift by the missing part of the measure. */
    Rational offset = rational_ratio((unsigned long long)numerator, denominator);
    if (layout->has_pickup && found == &layout->measures[0]) {
        Rational full = rational_ratio(layout->time_top, layout->time_bottom);
        Rational missing = {0, 1};
        if (rational_sub(full, found->length, &missing) != RATIONAL_OK || rational_cmp(offset, missing) < 0 ||
            rational_sub(offset, missing, &offset) != RATIONAL_OK) {
            return false;
        }
    }
    if (rational_cmp(offset, found->length) >= 0) {
        return false;
    }
    Rational at = {0, 1};
    if (rational_add(found->start, offset, &at) != RATIONAL_OK) {
        return false;
    }
    return musical_time_create((long)at.num, (unsigned long)at.den, out_time) == MUSICAL_TIME_OK;
}

RomanTextGrid score_layout_grid(const ScoreLayout *layout) {
    RomanTextGrid grid = {layout_time_at, layout};
    return grid;
}

MeterMap *score_layout_meter_map(const ScoreLayout *layout, const char *id_text) {
    if (layout == NULL || layout->time_top == 0) {
        return NULL;
    }
    int exponent = 0;
    for (unsigned b = layout->time_bottom; b > 1; b >>= 1) {
        exponent++;
    }
    NoteValue *unit = NULL;
    if (note_value_create(exponent, &unit) != NOTE_VALUE_OK) {
        return NULL;
    }
    TimeSignature *signature = NULL;
    if (time_signature_create(layout->time_top, unit, &signature) != TIME_SIGNATURE_OK) {
        note_value_destroy(unit);
        return NULL;
    }
    note_value_destroy(unit);
    MusicalTime *zero = NULL;
    MusicalTime *later = NULL;
    MeterMap *map = NULL;
    EntityID *id = NULL;
    if (musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK) {
        bool pickup = layout->has_pickup;
        if (!pickup || musical_time_create((long)layout->measures[0].length.num, (unsigned long)layout->measures[0].length.den, &later) == MUSICAL_TIME_OK) {
            MeterMapChangePoint points[2] = {{signature, zero}, {signature, later}};
            if (entity_id_create(id_text, &id) == ENTITY_ID_OK) {
                if (meter_map_create(id, points, pickup ? 2 : 1, &map) != METER_MAP_OK) {
                    entity_id_destroy(id);
                    map = NULL;
                }
            }
        }
    }
    musical_time_destroy(later);
    musical_time_destroy(zero);
    time_signature_destroy(signature);
    return map;
}
