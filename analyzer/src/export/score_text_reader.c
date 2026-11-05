#include "export/score_text_reader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- exact fractions ---- */

static int64_t gcd64(int64_t a, int64_t b) {
    if (a < 0) {
        a = -a;
    }
    if (b < 0) {
        b = -b;
    }
    while (b != 0) {
        int64_t rest = a % b;
        a = b;
        b = rest;
    }
    return a;
}

TextRational text_rational(int64_t num, int64_t den) {
    if (den == 0) {
        TextRational zero = {0, 1};
        return zero;
    }
    if (den < 0) {
        num = -num;
        den = -den;
    }
    int64_t divisor = gcd64(num, den);
    if (divisor > 1) {
        num /= divisor;
        den /= divisor;
    }
    if (num == 0) {
        den = 1;
    }
    TextRational result = {num, den};
    return result;
}

TextRational text_rational_add(TextRational left, TextRational right) {
    return text_rational(left.num * right.den + right.num * left.den, left.den * right.den);
}

TextRational text_rational_sub(TextRational left, TextRational right) {
    return text_rational(left.num * right.den - right.num * left.den, left.den * right.den);
}

TextRational text_rational_mul(TextRational left, TextRational right) {
    return text_rational(left.num * right.num, left.den * right.den);
}

int text_rational_compare(TextRational left, TextRational right) {
    int64_t a = left.num * right.den;
    int64_t b = right.num * left.den;
    return a < b ? -1 : (a > b ? 1 : 0);
}

/* ---- a small XML tree ---- */

typedef struct XmlNode {
    const char *name;
    size_t name_length;
    const char *attributes;
    size_t attributes_length;
    const char *text;
    size_t text_length;
    struct XmlNode *first_child;
    struct XmlNode *last_child;
    struct XmlNode *next;
} XmlNode;

#define XML_MAX_DEPTH 64

static void xml_free(XmlNode *node) {
    while (node) {
        XmlNode *next = node->next;
        xml_free(node->first_child);
        free(node);
        node = next;
    }
}

static bool is_name_char(char c) { return isalnum((unsigned char)c) || c == '_' || c == '-' || c == ':' || c == '.'; }

static char *message(const char *format, const char *detail) {
    size_t size = strlen(format) + (detail ? strlen(detail) : 0) + 1;
    char *text = malloc(size);
    if (text) {
        snprintf(text, size, format, detail ? detail : "");
    }
    return text;
}

static void set_error(char **error, const char *format, const char *detail) {
    if (error) {
        *error = message(format, detail);
    }
}

/* Parses text into a tree under a nameless root. Entities are not decoded: the fields read are numbers and names. */
static XmlNode *xml_parse(const char *text, char **error) {
    XmlNode *root = calloc(1, sizeof(XmlNode));
    if (!root) {
        set_error(error, "out of memory%s", NULL);
        return NULL;
    }
    XmlNode *stack[XML_MAX_DEPTH];
    size_t depth = 0;
    stack[depth++] = root;
    const char *p = text;
    while (*p) {
        if (*p != '<') {
            const char *end = strchr(p, '<');
            if (!end) {
                end = p + strlen(p);
            }
            XmlNode *top = stack[depth - 1];
            if (top != root && top->text_length == 0) {
                const char *a = p;
                const char *b = end;
                while (a < b && isspace((unsigned char)*a)) {
                    a++;
                }
                while (b > a && isspace((unsigned char)b[-1])) {
                    b--;
                }
                if (b > a) {
                    top->text = a;
                    top->text_length = (size_t)(b - a);
                }
            }
            p = end;
            continue;
        }
        if (strncmp(p, "<?", 2) == 0) {
            const char *end = strstr(p, "?>");
            if (!end) {
                set_error(error, "unterminated processing instruction%s", NULL);
                goto fail;
            }
            p = end + 2;
        } else if (strncmp(p, "<!--", 4) == 0) {
            const char *end = strstr(p, "-->");
            if (!end) {
                set_error(error, "unterminated comment%s", NULL);
                goto fail;
            }
            p = end + 3;
        } else if (strncmp(p, "<![CDATA[", 9) == 0) {
            const char *end = strstr(p, "]]>");
            if (!end) {
                set_error(error, "unterminated CDATA section%s", NULL);
                goto fail;
            }
            p = end + 3;
        } else if (strncmp(p, "<!", 2) == 0) {
            const char *end = strchr(p, '>');
            if (!end) {
                set_error(error, "unterminated declaration%s", NULL);
                goto fail;
            }
            p = end + 1;
        } else if (strncmp(p, "</", 2) == 0) {
            const char *name = p + 2;
            const char *end = name;
            while (is_name_char(*end)) {
                end++;
            }
            const char *close = strchr(end, '>');
            if (!close || depth <= 1) {
                set_error(error, "unexpected closing tag%s", NULL);
                goto fail;
            }
            XmlNode *top = stack[depth - 1];
            if (top->name_length != (size_t)(end - name) || strncmp(top->name, name, top->name_length) != 0) {
                set_error(error, "mismatched closing tag%s", NULL);
                goto fail;
            }
            depth--;
            p = close + 1;
        } else {
            const char *name = p + 1;
            const char *end = name;
            while (is_name_char(*end)) {
                end++;
            }
            if (end == name) {
                set_error(error, "malformed tag%s", NULL);
                goto fail;
            }
            /* The tag ends at the first '>' outside quotes. */
            const char *q = end;
            char quote = 0;
            while (*q && (quote || *q != '>')) {
                if (quote) {
                    if (*q == quote) {
                        quote = 0;
                    }
                } else if (*q == '"' || *q == '\'') {
                    quote = *q;
                }
                q++;
            }
            if (*q != '>') {
                set_error(error, "unterminated tag%s", NULL);
                goto fail;
            }
            bool self_closing = q > end && q[-1] == '/';
            XmlNode *node = calloc(1, sizeof(XmlNode));
            if (!node) {
                set_error(error, "out of memory%s", NULL);
                goto fail;
            }
            node->name = name;
            node->name_length = (size_t)(end - name);
            node->attributes = end;
            node->attributes_length = (size_t)(q - end) - (self_closing ? 1u : 0u);
            XmlNode *parent = stack[depth - 1];
            if (parent->last_child) {
                parent->last_child->next = node;
            } else {
                parent->first_child = node;
            }
            parent->last_child = node;
            if (!self_closing) {
                if (depth >= XML_MAX_DEPTH) {
                    set_error(error, "elements nested too deeply%s", NULL);
                    goto fail;
                }
                stack[depth++] = node;
            }
            p = q + 1;
        }
    }
    if (depth != 1) {
        set_error(error, "unclosed element%s", NULL);
        goto fail;
    }
    return root;
fail:
    xml_free(root);
    return NULL;
}

static bool name_is(const XmlNode *node, const char *name) {
    size_t length = strlen(name);
    return node->name_length == length && strncmp(node->name, name, length) == 0;
}

static XmlNode *child_of(const XmlNode *node, const char *name) {
    for (XmlNode *child = node ? node->first_child : NULL; child; child = child->next) {
        if (name_is(child, name)) {
            return child;
        }
    }
    return NULL;
}

/* Copies the value of attribute name into out (size bytes); false when the element has no such attribute. */
static bool attribute_of(const XmlNode *node, const char *name, char *out, size_t size) {
    size_t length = strlen(name);
    const char *p = node->attributes;
    const char *end = node->attributes + node->attributes_length;
    while (p < end) {
        while (p < end && isspace((unsigned char)*p)) {
            p++;
        }
        const char *key = p;
        while (p < end && *p != '=' && !isspace((unsigned char)*p)) {
            p++;
        }
        size_t key_length = (size_t)(p - key);
        while (p < end && isspace((unsigned char)*p)) {
            p++;
        }
        if (p >= end || *p != '=') {
            return false;
        }
        p++;
        while (p < end && isspace((unsigned char)*p)) {
            p++;
        }
        if (p >= end || (*p != '"' && *p != '\'')) {
            return false;
        }
        char quote = *p++;
        const char *value = p;
        while (p < end && *p != quote) {
            p++;
        }
        if (key_length == length && strncmp(key, name, length) == 0) {
            size_t value_length = (size_t)(p - value);
            if (value_length >= size) {
                value_length = size - 1;
            }
            memcpy(out, value, value_length);
            out[value_length] = '\0';
            return true;
        }
        if (p < end) {
            p++;
        }
    }
    return false;
}

static bool text_is(const XmlNode *node, const char *text) {
    size_t length = strlen(text);
    return node && node->text_length == length && strncmp(node->text, text, length) == 0;
}

static bool int_of(const XmlNode *node, long *value) {
    if (!node || node->text_length == 0 || node->text_length > 30) {
        return false;
    }
    char buffer[32];
    memcpy(buffer, node->text, node->text_length);
    buffer[node->text_length] = '\0';
    char *end = NULL;
    long parsed = strtol(buffer, &end, 10);
    if (end == buffer) {
        return false;
    }
    *value = parsed;
    return true;
}

static long child_int(const XmlNode *node, const char *name, long fallback) {
    long value = fallback;
    return int_of(child_of(node, name), &value) ? value : fallback;
}

/* ---- notes ---- */

static char type_char(const XmlNode *type) {
    if (!type) {
        return '?';
    }
    static const struct {
        const char *name;
        char symbol;
    } TYPES[] = {{"breve", 'b'}, {"whole", 'w'}, {"half", 'h'}, {"quarter", 'q'}, {"eighth", 'e'},
                 {"16th", 's'},  {"32nd", 't'},  {"64th", 'x'}, {"128th", 'y'}};
    for (size_t i = 0; i < sizeof(TYPES) / sizeof(TYPES[0]); i++) {
        if (text_is(type, TYPES[i].name)) {
            return TYPES[i].symbol;
        }
    }
    return '?';
}

/* The notated value of a note with no <type>: the duration undone by the tuplet, as a plain or dotted value. */
static void derive_type(TextRational duration, int actual, int normal, char *type, int *dots) {
    static const char SYMBOLS[] = {'b', 'w', 'h', 'q', 'e', 's', 't', 'x', 'y'};
    TextRational notated = duration;
    if (actual > 0 && normal > 0) {
        notated = text_rational_mul(duration, text_rational(actual, normal));
    }
    TextRational base = text_rational(2, 1);
    for (size_t i = 0; i < sizeof(SYMBOLS); i++) {
        TextRational total = base;
        TextRational add = base;
        for (int d = 0; d <= 3; d++) {
            if (d > 0) {
                add = text_rational_mul(add, text_rational(1, 2));
                total = text_rational_add(total, add);
            }
            if (text_rational_compare(total, notated) == 0) {
                *type = SYMBOLS[i];
                *dots = d;
                return;
            }
        }
        base = text_rational_mul(base, text_rational(1, 2));
    }
    *type = '?';
    *dots = 0;
}

static bool pitch_of(const XmlNode *pitch, ScoreTextPitch *out) {
    const XmlNode *step = child_of(pitch, "step");
    long octave = 0;
    if (!step || step->text_length != 1 || !int_of(child_of(pitch, "octave"), &octave)) {
        return false;
    }
    out->step = step->text[0];
    out->alter = (int)child_int(pitch, "alter", 0);
    out->octave = (int)octave;
    return true;
}

/* A part's notes of one measure, with onsets relative to the measure, and where the measure's content ends. */
typedef struct {
    ScoreTextEvent *events;
    size_t count;
    size_t capacity;
    TextRational extent;
} PartMeasure;

static bool event_append(PartMeasure *measure, const ScoreTextEvent *event) {
    if (measure->count == measure->capacity) {
        size_t capacity = measure->capacity ? measure->capacity * 2 : 16;
        ScoreTextEvent *grown = realloc(measure->events, capacity * sizeof(ScoreTextEvent));
        if (!grown) {
            return false;
        }
        measure->events = grown;
        measure->capacity = capacity;
    }
    measure->events[measure->count++] = *event;
    return true;
}

typedef struct {
    int divisions; /* the part's current <divisions> */
} PartState;

/* Reads one <note> into the part's measure, advancing *cursor (a time within the measure in whole notes). */
static bool read_note(const XmlNode *note, int part, const PartState *state, size_t *order, TextRational *cursor,
                      PartMeasure *measure) {
    if (child_of(note, "grace") || state->divisions <= 0) {
        return true;
    }
    long duration_divisions = 0;
    if (!int_of(child_of(note, "duration"), &duration_divisions) || duration_divisions < 0) {
        return true;
    }
    TextRational duration = text_rational(duration_divisions, 4 * (int64_t)state->divisions);
    bool is_chord = child_of(note, "chord") != NULL;
    bool is_rest = child_of(note, "rest") != NULL;

    bool tie_start = false;
    for (const XmlNode *tie = note->first_child; tie; tie = tie->next) {
        char value[16];
        if (name_is(tie, "tie") && attribute_of(tie, "type", value, sizeof(value)) && strcmp(value, "start") == 0) {
            tie_start = true;
        }
    }

    ScoreTextPitch pitch = {'C', 0, 4};
    if (!is_rest && !pitch_of(child_of(note, "pitch"), &pitch)) {
        return true; /* unpitched note: nothing to print */
    }

    if (is_chord && measure->count > 0) {
        ScoreTextEvent *previous = &measure->events[measure->count - 1];
        if (previous->part == part && !previous->rest && !is_rest) {
            if (previous->pitch_count < SCORE_TEXT_MAX_PITCHES) {
                previous->pitches[previous->pitch_count++] = pitch;
            }
            previous->tie_start = previous->tie_start || tie_start;
            return true;
        }
    }

    ScoreTextEvent event;
    memset(&event, 0, sizeof(event));
    event.part = part;
    event.staff = (int)child_int(note, "staff", 1);
    event.voice = (int)child_int(note, "voice", 1);
    event.onset = *cursor; /* relative to the measure until the measures are placed */
    event.rest = is_rest;
    if (!is_rest) {
        event.pitches[0] = pitch;
        event.pitch_count = 1;
    }
    const XmlNode *modification = child_of(note, "time-modification");
    if (modification) {
        event.tuplet_actual = (int)child_int(modification, "actual-notes", 0);
        event.tuplet_normal = (int)child_int(modification, "normal-notes", 0);
    }
    const XmlNode *type = child_of(note, "type");
    if (type) {
        event.type = type_char(type);
        for (const XmlNode *child = note->first_child; child; child = child->next) {
            if (name_is(child, "dot")) {
                event.dots++;
            }
        }
    } else {
        derive_type(duration, event.tuplet_actual, event.tuplet_normal, &event.type, &event.dots);
    }
    event.tie_start = tie_start;
    attribute_of(note, "xml:id", event.xml_id, sizeof(event.xml_id));
    event.order = (*order)++;
    if (!event_append(measure, &event)) {
        return false;
    }
    if (!is_chord) {
        *cursor = text_rational_add(*cursor, duration);
        if (text_rational_compare(*cursor, measure->extent) > 0) {
            measure->extent = *cursor;
        }
    }
    return true;
}

/* ---- parts and measures ---- */

typedef struct {
    char number[24];
    bool implicit;
    bool has_time;
    int beats;
    int beat_type;
    PartMeasure content;
} ParsedMeasure;

typedef struct {
    ParsedMeasure *measures;
    size_t count;
    size_t capacity;
} ParsedPart;

static void parsed_part_free(ParsedPart *part) {
    for (size_t i = 0; i < part->count; i++) {
        free(part->measures[i].content.events);
    }
    free(part->measures);
}

static bool read_part(const XmlNode *part_node, int part, ParsedPart *out) {
    PartState state = {1};
    for (const XmlNode *measure_node = part_node->first_child; measure_node; measure_node = measure_node->next) {
        if (!name_is(measure_node, "measure")) {
            continue;
        }
        if (out->count == out->capacity) {
            size_t capacity = out->capacity ? out->capacity * 2 : 32;
            ParsedMeasure *grown = realloc(out->measures, capacity * sizeof(ParsedMeasure));
            if (!grown) {
                return false;
            }
            out->measures = grown;
            out->capacity = capacity;
        }
        ParsedMeasure *measure = &out->measures[out->count++];
        memset(measure, 0, sizeof(*measure));
        measure->content.extent = text_rational(0, 1);
        char implicit[8];
        if (!attribute_of(measure_node, "number", measure->number, sizeof(measure->number))) {
            snprintf(measure->number, sizeof(measure->number), "%zu", out->count);
        }
        measure->implicit =
            attribute_of(measure_node, "implicit", implicit, sizeof(implicit)) && strcmp(implicit, "yes") == 0;

        TextRational cursor = text_rational(0, 1);
        size_t order = 0;
        for (const XmlNode *item = measure_node->first_child; item; item = item->next) {
            if (name_is(item, "attributes")) {
                long divisions = 0;
                if (int_of(child_of(item, "divisions"), &divisions) && divisions > 0) {
                    state.divisions = (int)divisions;
                }
                const XmlNode *time = child_of(item, "time");
                long beats = 0;
                long beat_type = 0;
                if (time && int_of(child_of(time, "beats"), &beats) && int_of(child_of(time, "beat-type"), &beat_type) &&
                    beats > 0 && beat_type > 0) {
                    measure->has_time = true;
                    measure->beats = (int)beats;
                    measure->beat_type = (int)beat_type;
                }
            } else if (name_is(item, "note")) {
                if (!read_note(item, part, &state, &order, &cursor, &measure->content)) {
                    return false;
                }
            } else if (name_is(item, "backup") || name_is(item, "forward")) {
                long moved = child_int(item, "duration", 0);
                TextRational delta = text_rational(moved, 4 * (int64_t)state.divisions);
                cursor = name_is(item, "backup") ? text_rational_sub(cursor, delta) : text_rational_add(cursor, delta);
                if (text_rational_compare(cursor, text_rational(0, 1)) < 0) {
                    cursor = text_rational(0, 1);
                }
                if (text_rational_compare(cursor, measure->content.extent) > 0) {
                    measure->content.extent = cursor;
                }
            }
        }
    }
    return true;
}

static int event_compare(const void *left, const void *right) {
    const ScoreTextEvent *a = left;
    const ScoreTextEvent *b = right;
    if (a->part != b->part) {
        return a->part < b->part ? -1 : 1;
    }
    if (a->staff != b->staff) {
        return a->staff < b->staff ? -1 : 1;
    }
    if (a->voice != b->voice) {
        return a->voice < b->voice ? -1 : 1;
    }
    int by_onset = text_rational_compare(a->onset, b->onset);
    if (by_onset != 0) {
        return by_onset;
    }
    return a->order < b->order ? -1 : (a->order > b->order ? 1 : 0);
}

void score_text_destroy(ScoreTextScore *score) {
    if (!score) {
        return;
    }
    for (size_t i = 0; i < score->measure_count; i++) {
        free(score->measures[i].events);
    }
    free(score->measures);
    free(score);
}

ScoreTextScore *score_text_read(const char *musicxml, char **error) {
    if (error) {
        *error = NULL;
    }
    if (!musicxml) {
        set_error(error, "no MusicXML%s", NULL);
        return NULL;
    }
    XmlNode *root = xml_parse(musicxml, error);
    if (!root) {
        return NULL;
    }
    const XmlNode *score_node = child_of(root, "score-partwise");
    if (!score_node) {
        set_error(error, "not a score-partwise MusicXML document%s", NULL);
        xml_free(root);
        return NULL;
    }

    size_t part_count = 0;
    for (const XmlNode *node = score_node->first_child; node; node = node->next) {
        if (name_is(node, "part")) {
            part_count++;
        }
    }
    ParsedPart *parts = calloc(part_count ? part_count : 1, sizeof(ParsedPart));
    ScoreTextScore *score = calloc(1, sizeof(ScoreTextScore));
    if (!parts || !score) {
        free(parts);
        free(score);
        xml_free(root);
        set_error(error, "out of memory%s", NULL);
        return NULL;
    }
    bool ok = true;
    size_t index = 0;
    size_t measure_count = 0;
    for (const XmlNode *node = score_node->first_child; node && ok; node = node->next) {
        if (!name_is(node, "part")) {
            continue;
        }
        ok = read_part(node, (int)index + 1, &parts[index]);
        if (parts[index].count > measure_count) {
            measure_count = parts[index].count;
        }
        index++;
    }
    xml_free(root);
    if (!ok) {
        set_error(error, "out of memory%s", NULL);
        goto fail;
    }
    if (part_count == 0 || measure_count == 0) {
        set_error(error, "the score has no measures%s", NULL);
        goto fail;
    }

    score->part_count = part_count;
    score->measure_count = measure_count;
    score->measures = calloc(measure_count, sizeof(ScoreTextMeasure));
    if (!score->measures) {
        set_error(error, "out of memory%s", NULL);
        goto fail;
    }

    int beats = 4;
    int beat_type = 4;
    TextRational start = text_rational(0, 1);
    for (size_t m = 0; m < measure_count; m++) {
        ScoreTextMeasure *measure = &score->measures[m];
        TextRational extent = text_rational(0, 1);
        bool implicit = false;
        bool has_time = false;
        size_t total = 0;
        for (size_t p = 0; p < part_count; p++) {
            if (m >= parts[p].count) {
                continue;
            }
            const ParsedMeasure *parsed = &parts[p].measures[m];
            if (measure->number[0] == '\0') {
                snprintf(measure->number, sizeof(measure->number), "%s", parsed->number);
                implicit = parsed->implicit;
            }
            if (parsed->has_time && !has_time) {
                has_time = true;
                measure->time_changed = m == 0 || parsed->beats != beats || parsed->beat_type != beat_type;
                beats = parsed->beats;
                beat_type = parsed->beat_type;
            }
            if (text_rational_compare(parsed->content.extent, extent) > 0) {
                extent = parsed->content.extent;
            }
            total += parsed->content.count;
        }
        if (m == 0) {
            measure->time_changed = true;
        }
        measure->beats = beats;
        measure->beat_type = beat_type;
        measure->start = start;
        TextRational full = text_rational(beats, beat_type);
        if (text_rational_compare(extent, full) < 0 && (implicit || m == 0)) {
            measure->length = extent;
            measure->pickup = m == 0;
        } else {
            measure->length = text_rational_compare(extent, full) > 0 ? extent : full;
        }

        measure->events = total ? malloc(total * sizeof(ScoreTextEvent)) : NULL;
        if (total && !measure->events) {
            set_error(error, "out of memory%s", NULL);
            goto fail;
        }
        for (size_t p = 0; p < part_count; p++) {
            if (m >= parts[p].count) {
                continue;
            }
            const PartMeasure *content = &parts[p].measures[m].content;
            for (size_t e = 0; e < content->count; e++) {
                measure->events[measure->event_count++] = content->events[e];
            }
        }
        /* Place the events: absolute onsets, and beats counted from the bar's start (a pickup from its end). */
        TextRational lead = measure->pickup ? text_rational_sub(full, measure->length) : text_rational(0, 1);
        for (size_t e = 0; e < measure->event_count; e++) {
            ScoreTextEvent *event = &measure->events[e];
            TextRational inside = text_rational_add(lead, event->onset);
            event->beat_in_measure = text_rational_add(text_rational(1, 1), text_rational_mul(inside, text_rational(beat_type, 1)));
            event->onset = text_rational_add(start, event->onset);
        }
        qsort(measure->events, measure->event_count, sizeof(ScoreTextEvent), event_compare);
        start = text_rational_add(start, measure->length);
    }

    for (size_t p = 0; p < part_count; p++) {
        parsed_part_free(&parts[p]);
    }
    free(parts);
    return score;
fail:
    for (size_t p = 0; p < part_count; p++) {
        parsed_part_free(&parts[p]);
    }
    free(parts);
    score_text_destroy(score);
    return NULL;
}
