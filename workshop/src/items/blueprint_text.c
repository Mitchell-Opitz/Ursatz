#include "items/blueprint_text.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/strings.h>
#include <text/text_tree.h>

#define BLUEPRINT_HEADER "ursatz-text 1 blueprint"

/* The header lines, in the order the text writes them. */
enum { H_KEY, H_METER, H_INTO, H_BARS, H_CADENCE, H_FIRST_CADENCE, H_DEVELOPMENT, H_TEMPO, H_ACCOMPANIMENT, H_CLIMAX, H_BASIC, H_COUNT };
static const char *const kHeaders[H_COUNT] = {
    "key", "meter", "into", "bars", "cadence", "first_cadence", "development", "tempo", "accompaniment", "climax", "basic",
};

/* The brief's members in the order the request writes them, the bar's fields in the order the text writes them. */
static const char *const kBriefOrder[] = {"into", "key", "material", "meter", "bars", "cadence", "first_cadence", "development", "tempo", "motives", "slots"};
static const char *const kBarOrder[] = {"chords", "melody", "rhythm", "job", "anchors", "events", "bass", "inner"};

static bool fail(char *error, size_t size, int line, const char *format, ...) __attribute__((format(printf, 4, 5)));

static bool fail(char *error, size_t size, int line, const char *format, ...) {
    if (error && size > 0) {
        char message[300];
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(message, sizeof message, format, arguments);
        va_end(arguments);
        if (line > 0) {
            snprintf(error, size, "line %d: %s", line, message);
        } else {
            snprintf(error, size, "%s", message);
        }
    }
    return false;
}

/* ---- reading ------------------------------------------------------------------------------------------------------------------------ */

static char *trimmed(const char *text) {
    while (isspace((unsigned char)*text)) {
        text++;
    }
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        length--;
    }
    char *out = malloc(length + 1);
    if (out) {
        memcpy(out, text, length);
        out[length] = '\0';
    }
    return out;
}

/* `text` trimmed, runs of white space one space, a new string. */
static char *collapsed(const char *text) {
    size_t capacity = strlen(text);
    char *out = malloc(capacity + 1);
    if (!out) {
        return NULL;
    }
    size_t length = 0;
    bool gap = false;
    for (const char *p = text; *p; p++) {
        if (isspace((unsigned char)*p)) {
            gap = length > 0;
        } else {
            if (gap) {
                out[length++] = ' ';
                gap = false;
            }
            out[length++] = *p;
        }
    }
    out[length] = '\0';
    return out;
}

static bool is_rational(const cJSON *unit) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(unit, "num");
    const cJSON *d = cJSON_GetObjectItemCaseSensitive(unit, "den");
    return cJSON_IsObject(unit) && cJSON_IsNumber(n) && cJSON_IsNumber(d) && unit->child && unit->child->next && !unit->child->next->next;
}

/* A unit that stands for a word: a string as it is, a number or n/d as written; NULL for the rest (null, true, false, a span, a list). */
static char *unit_word(const cJSON *unit) {
    if (cJSON_IsString(unit)) {
        return strdup(unit->valuestring);
    }
    if (cJSON_IsNumber(unit) || is_rational(unit)) {
        return text_scalar_print(unit);
    }
    return NULL;
}

/* digits, or digits '/' digits. */
static bool is_beats(const char *text) {
    const char *p = text;
    while (isdigit((unsigned char)*p)) {
        p++;
    }
    if (p == text) {
        return false;
    }
    if (*p == '/') {
        const char *den = ++p;
        while (isdigit((unsigned char)*p)) {
            p++;
        }
        return p > den && *p == '\0';
    }
    return *p == '\0';
}

typedef struct {
    char *error;
    size_t size;
    cJSON *brief;      /* the request's members read from the headers */
    cJSON *blueprint; /* climax and accompaniment */
    cJSON *bars;
    bool given[H_COUNT];
} Reader;

static void move_in_order(cJSON *from, const char *const *order, size_t count, cJSON *to) {
    for (size_t i = 0; i < count; i++) {
        cJSON *item = cJSON_DetachItemFromObjectCaseSensitive(from, order[i]);
        if (item) {
            cJSON_AddItemToObject(to, order[i], item);
        }
    }
}

static bool add_string(cJSON *object, const char *name, const char *text) {
    return cJSON_AddStringToObject(object, name, text) != NULL;
}

/* The units of a header's value: `count` of them (or 1 to `count` when `optional`), each a word. */
static cJSON *header_units(Reader *r, const char *rest, int line) {
    TextError text_error = {0, ""};
    cJSON *units = text_units_parse(rest, line, &text_error);
    if (!units) {
        fail(r->error, r->size, line, "%s", text_error.message);
    }
    return units;
}

static bool header_word(Reader *r, const char *name, const cJSON *unit, int line, char **word) {
    *word = unit_word(unit);
    if (!*word) {
        return fail(r->error, r->size, line, "%s takes a word or a number", name);
    }
    return true;
}

/* One header line `<name> <value>`: the value read by the header. */
static bool read_header(Reader *r, size_t index, const char *rest, int line) {
    const char *name = kHeaders[index];
    if (r->given[index]) {
        return fail(r->error, r->size, line, "%s is given twice", name);
    }
    r->given[index] = true;
    if (index == H_BASIC) { /* basic: the rest of the line is the degrees and an optional pickup */
        char *degrees = collapsed(rest);
        if (!degrees) {
            return fail(r->error, r->size, 0, "out of memory");
        }
        char pickup[64] = "0";
        char *mark = NULL;
        for (char *p = strstr(degrees, "pickup"); p; p = strstr(p + 1, "pickup")) {
            if ((p == degrees || p[-1] == ' ') && (p[6] == ' ' || p[6] == '\0')) {
                mark = p;
            }
        }
        if (mark) {
            const char *value = mark[6] == ' ' ? mark + 7 : mark + 6;
            if (!is_beats(value) || strlen(value) >= sizeof pickup) {
                free(degrees);
                return fail(r->error, r->size, line, "pickup takes beats like 1 or 1/2");
            }
            snprintf(pickup, sizeof pickup, "%s", value);
            while (mark > degrees && mark[-1] == ' ') {
                mark--;
            }
            *mark = '\0';
        }
        if (!degrees[0]) {
            free(degrees);
            return fail(r->error, r->size, line, "basic needs the degrees of its motive, like 1:q 3:e 5:h");
        }
        cJSON *motives = cJSON_AddArrayToObject(r->brief, "motives");
        cJSON *motive = motives ? cJSON_CreateObject() : NULL;
        cJSON *slots = cJSON_AddObjectToObject(r->brief, "slots");
        bool ok = motive && slots && cJSON_AddItemToArray(motives, motive) && add_string(motive, "id", "A") && add_string(motive, "form", "full") &&
                  add_string(motive, "degrees", degrees) && add_string(motive, "pickup", pickup) && cJSON_AddNullToObject(motive, "harmony") &&
                  cJSON_AddArrayToObject(motive, "features") && add_string(motive, "role", "melodic") && add_string(slots, "basic", "A");
        free(degrees);
        return ok || fail(r->error, r->size, 0, "out of memory");
    }
    cJSON *units = header_units(r, rest, line);
    if (!units) {
        return false;
    }
    int count = cJSON_GetArraySize(units);
    const cJSON *first = units->child;
    char *word = NULL;
    bool ok = false;
    if (index == H_KEY) { /* key <tonic> [<material>]: the words after the tonic are the material */
        if (count < 1) {
            fail(r->error, r->size, line, "key needs a tonic and the material: key C major");
        } else if (header_word(r, name, first, line, &word)) {
            ok = add_string(r->brief, "key", word);
            free(word);
            word = NULL;
            CommonText material;
            common_text_init(&material);
            for (const cJSON *unit = first->next; ok && unit; unit = unit->next) {
                char *part = NULL;
                ok = header_word(r, name, unit, line, &part) && common_text_appendf(&material, "%s%s", material.length ? " " : "", part);
                free(part);
            }
            char *joined = ok ? common_text_take(&material) : NULL;
            common_text_free(&material);
            if (ok && joined && joined[0]) {
                ok = add_string(r->brief, "material", joined);
            }
            free(joined);
        }
    } else if (index == H_ACCOMPANIMENT) { /* accompaniment <pattern> [voicing <id>] */
        const cJSON *keyword = count == 3 ? first->next : NULL;
        if (count != 1 && !(count == 3 && cJSON_IsString(keyword) && strcmp(keyword->valuestring, "voicing") == 0)) {
            fail(r->error, r->size, line, "accompaniment takes a pattern and optionally voicing <id>: accompaniment block voicing satb");
        } else {
            char *voicing = NULL;
            if (header_word(r, name, first, line, &word) && (count == 1 || header_word(r, name, keyword->next, line, &voicing))) {
                cJSON *object = cJSON_AddObjectToObject(r->blueprint, "accompaniment");
                ok = object && add_string(object, "pattern", word) && (!voicing || add_string(object, "voicing", voicing));
                if (!ok) {
                    fail(r->error, r->size, 0, "out of memory");
                }
            }
            free(voicing);
        }
    } else if (index == H_CLIMAX) { /* climax <bar> <beats> <pitch> */
        char *at = NULL;
        char *pitch = NULL;
        if (count != 3 || !cJSON_IsNumber(first) || first->valuedouble < 1 || first->valuedouble != (double)(int)first->valuedouble) {
            fail(r->error, r->size, line, "climax takes a bar (from 1), a position in beats and a pitch: climax 6 2 A5");
        } else if (header_word(r, name, first->next, line, &at) && header_word(r, name, first->next->next, line, &pitch)) {
            if (!is_beats(at)) {
                fail(r->error, r->size, line, "climax position '%s' is not beats like 2 or 3/2", at);
            } else {
                cJSON *object = cJSON_AddObjectToObject(r->blueprint, "climax");
                ok = object && cJSON_AddNumberToObject(object, "bar", first->valuedouble - 1) && add_string(object, "at", at) &&
                     add_string(object, "pitch", pitch);
                if (!ok) {
                    fail(r->error, r->size, 0, "out of memory");
                }
            }
        }
        free(at);
        free(pitch);
    } else if (count != 1) {
        fail(r->error, r->size, line, "%s takes one value", name);
    } else if (index == H_BARS || index == H_TEMPO) { /* numbers */
        if (cJSON_IsNumber(first) && (index == H_TEMPO || (first->valuedouble >= 1 && first->valuedouble == (double)(int)first->valuedouble))) {
            ok = cJSON_AddNumberToObject(r->brief, name, first->valuedouble) != NULL;
        } else {
            fail(r->error, r->size, line, index == H_BARS ? "bars is a whole number: bars 8" : "tempo is a number of quarter notes a minute: tempo 92");
        }
    } else if (header_word(r, name, first, line, &word)) {
        ok = add_string(r->brief, name, word);
    }
    free(word);
    cJSON_Delete(units);
    return ok || r->error[0] != '\0' ? ok : fail(r->error, r->size, 0, "out of memory");
}

/* The field word and the rest of a field `text`, both new strings. */
static bool split_field(const char *text, char **word, char **rest) {
    size_t length = 0;
    while (text[length] && !isspace((unsigned char)text[length])) {
        length++;
    }
    *word = strndup(text, length);
    *rest = collapsed(text + length);
    return *word && *rest;
}

/* anchor/event: `<beats> <word>`, two tokens. */
static bool read_pair(Reader *r, cJSON *bar, const char *list, const char *second, const char *word, const char *rest, int line, int number) {
    char beats[64], other[64], extra[8];
    if (sscanf(rest, "%63s %63s %7s", beats, other, extra) != 2) {
        return fail(r->error, r->size, line, "bar %d: %s takes a position in beats and %s: %s 0 %s", number, word, strcmp(second, "pitch") == 0 ? "a pitch" : "a type",
                    word, strcmp(second, "pitch") == 0 ? "D5" : "appoggiatura");
    }
    if (!is_beats(beats)) {
        return fail(r->error, r->size, line, "bar %d: %s position '%s' is not beats like 2 or 3/2", number, word, beats);
    }
    cJSON *array = cJSON_GetObjectItemCaseSensitive(bar, list);
    if (!array) {
        array = cJSON_AddArrayToObject(bar, list);
    }
    cJSON *item = array ? cJSON_CreateObject() : NULL;
    if (!item || !cJSON_AddItemToArray(array, item)) {
        cJSON_Delete(item);
        return fail(r->error, r->size, 0, "out of memory");
    }
    return (add_string(item, "at", beats) && add_string(item, second, other)) || fail(r->error, r->size, 0, "out of memory");
}

static bool read_field(Reader *r, cJSON *bar, const char *text, int line, int number) {
    char *word = NULL;
    char *rest = NULL;
    if (!split_field(text, &word, &rest)) {
        free(word);
        free(rest);
        return fail(r->error, r->size, 0, "out of memory");
    }
    bool ok = false;
    static const char *const kTexts[] = {"chords", "melody", "rhythm", "job", "bass"};
    bool is_text = false;
    for (size_t i = 0; i < sizeof kTexts / sizeof kTexts[0]; i++) {
        is_text = is_text || strcmp(word, kTexts[i]) == 0;
    }
    if (is_text) {
        if (!rest[0]) {
            fail(r->error, r->size, line, "bar %d: %s needs its text", number, word);
        } else if (cJSON_GetObjectItemCaseSensitive(bar, word)) {
            fail(r->error, r->size, line, "bar %d: %s is given twice", number, word);
        } else {
            ok = add_string(bar, word, rest) || fail(r->error, r->size, 0, "out of memory");
        }
    } else if (strcmp(word, "anchor") == 0) {
        ok = read_pair(r, bar, "anchors", "pitch", word, rest, line, number);
    } else if (strcmp(word, "event") == 0) {
        ok = read_pair(r, bar, "events", "type", word, rest, line, number);
    } else if (strcmp(word, "inner") == 0) {
        cJSON *inner = cJSON_GetObjectItemCaseSensitive(bar, "inner");
        if (!rest[0]) {
            fail(r->error, r->size, line, "bar %d: inner needs its notes", number);
        } else if ((inner || (inner = cJSON_AddArrayToObject(bar, "inner"))) && cJSON_AddItemToArray(inner, cJSON_CreateString(rest))) {
            ok = true;
        } else {
            fail(r->error, r->size, 0, "out of memory");
        }
    } else {
        fail(r->error, r->size, line, "bar %d: unknown field '%s' (the fields are chords, melody, rhythm, job, anchor, event, bass and inner)", number, word);
    }
    free(word);
    free(rest);
    return ok;
}

/* A bar line `<n>: field ; field`; `expected` is the number it must carry. */
static bool read_bar(Reader *r, const TextLine *line, int expected) {
    char *end = NULL;
    long number = strtol(line->text, &end, 10);
    if (number != expected) {
        return fail(r->error, r->size, line->number, "bar %ld is out of order: the next bar is %d", number, expected);
    }
    cJSON *scratch = cJSON_CreateObject();
    cJSON *bar = cJSON_CreateObject();
    char *fields = trimmed(end + 1);
    bool ok = scratch && bar && fields;
    if (!ok) {
        fail(r->error, r->size, 0, "out of memory");
    }
    char *piece = fields;
    while (ok && fields[0]) {
        char *semicolon = strchr(piece, ';');
        if (semicolon) {
            *semicolon = '\0';
        }
        char *field = trimmed(piece);
        if (!field) {
            ok = fail(r->error, r->size, 0, "out of memory");
        } else if (!field[0]) {
            ok = fail(r->error, r->size, line->number, "bar %d has an empty field: fields are split by one ';'", expected);
        } else {
            ok = read_field(r, scratch, field, line->number, expected);
        }
        free(field);
        if (!semicolon) {
            break;
        }
        piece = semicolon + 1;
    }
    if (ok) {
        move_in_order(scratch, kBarOrder, sizeof kBarOrder / sizeof kBarOrder[0], bar);
        ok = cJSON_AddItemToArray(r->bars, bar) || fail(r->error, r->size, 0, "out of memory");
        bar = ok ? NULL : bar;
    }
    free(fields);
    cJSON_Delete(scratch);
    cJSON_Delete(bar);
    return ok;
}

bool blueprint_text_is(const char *text) {
    while (text && *text) {
        while (*text == ' ' || *text == '\t' || *text == '\r' || *text == '\n') {
            text++;
        }
        if (*text != '#') {
            size_t length = strlen(BLUEPRINT_HEADER);
            return strncmp(text, BLUEPRINT_HEADER, length) == 0 && (text[length] == '\0' || isspace((unsigned char)text[length]));
        }
        while (*text && *text != '\n') {
            text++;
        }
    }
    return false;
}

bool blueprint_text_read(const char *text, cJSON **request, char *error, size_t size) {
    char scratch[8];
    if (!error || size == 0) {
        error = scratch;
        size = sizeof scratch;
    }
    *request = NULL;
    error[0] = '\0';
    TextLines lines;
    TextError text_error = {0, ""};
    if (!text_lines_split(text, &lines, &text_error)) {
        text_lines_free(&lines);
        return fail(error, size, text_error.line, "%s", text_error.message);
    }
    Reader reader = {error, size, cJSON_CreateObject(), cJSON_CreateObject(), cJSON_CreateArray(), {false}};
    bool ok = reader.brief && reader.blueprint && reader.bars;
    if (!ok) {
        fail(error, size, 0, "out of memory");
    } else if (lines.count == 0 || lines.lines[0].indent != 0 || strcmp(lines.lines[0].text, BLUEPRINT_HEADER) != 0) {
        ok = fail(error, size, lines.count ? lines.lines[0].number : 0, "the first line must be %s", BLUEPRINT_HEADER);
    }
    for (size_t i = 1; ok && i < lines.count; i++) {
        const TextLine *line = &lines.lines[i];
        if (line->indent != 0) {
            ok = fail(error, size, line->number, "unexpected indent: a header or a bar starts at the left margin");
        } else if (isdigit((unsigned char)line->text[0])) {
            const char *digits = line->text;
            while (isdigit((unsigned char)*digits)) {
                digits++;
            }
            ok = *digits == ':' ? read_bar(&reader, line, cJSON_GetArraySize(reader.bars) + 1)
                                : fail(error, size, line->number, "a bar line starts with its number and a colon: %d: chords I",
                                       cJSON_GetArraySize(reader.bars) + 1);
        } else {
            size_t length = 0;
            while (line->text[length] && !isspace((unsigned char)line->text[length])) {
                length++;
            }
            size_t index = 0;
            while (index < H_COUNT && (strlen(kHeaders[index]) != length || strncmp(kHeaders[index], line->text, length) != 0)) {
                index++;
            }
            if (cJSON_GetArraySize(reader.bars) > 0) {
                ok = fail(error, size, line->number, "'%.*s' comes after the first bar: the header lines come before the bars", (int)length, line->text);
            } else if (index == H_COUNT) {
                ok = fail(error, size, line->number, "unknown header '%.*s'", (int)length, line->text);
            } else {
                ok = read_header(&reader, index, line->text + length, line->number);
            }
        }
    }
    if (ok && cJSON_GetArraySize(reader.bars) == 0) {
        ok = fail(error, size, 0, "the blueprint has no bar line: write one line a bar, 1: chords I ; rhythm x:w");
    }
    cJSON *out = ok ? cJSON_CreateObject() : NULL;
    if (out) {
        ok = add_string(out, "schema", "ursatz-theme-request") && cJSON_AddNumberToObject(out, "schema_version", 2);
        move_in_order(reader.brief, kBriefOrder, sizeof kBriefOrder / sizeof kBriefOrder[0], out);
        cJSON *blueprint = ok ? cJSON_AddObjectToObject(out, "blueprint") : NULL;
        ok = blueprint != NULL;
        if (ok) {
            cJSON_AddItemToObject(blueprint, "bars", reader.bars);
            reader.bars = NULL;
            static const char *const kBlueprintOrder[] = {"climax", "accompaniment"};
            move_in_order(reader.blueprint, kBlueprintOrder, 2, blueprint);
        }
    }
    text_lines_free(&lines);
    cJSON_Delete(reader.brief);
    cJSON_Delete(reader.blueprint);
    cJSON_Delete(reader.bars);
    if (!ok) {
        cJSON_Delete(out);
        if (!error[0]) {
            fail(error, size, 0, "out of memory");
        }
        return false;
    }
    *request = out;
    return true;
}

/* ---- writing ------------------------------------------------------------------------------------------------------------------------ */

typedef struct {
    CommonText out;
    char *error;
    size_t size;
} Writer;

static bool refuse(Writer *w, const char *format, ...) __attribute__((format(printf, 2, 3)));

static bool refuse(Writer *w, const char *format, ...) {
    if (w->error && w->size > 0) {
        va_list arguments;
        va_start(arguments, format);
        vsnprintf(w->error, w->size, format, arguments);
        va_end(arguments);
    }
    return false;
}

/* Whether `text` reads back as itself when it stands bare: a word, or a whole number or n/d without a leading zero. */
static bool is_bare(const char *text) {
    if (!text[0] || strcmp(text, "null") == 0 || strcmp(text, "true") == 0 || strcmp(text, "false") == 0 || strstr(text, "..")) {
        return false;
    }
    if (isalpha((unsigned char)text[0]) || text[0] == '_') {
        for (const char *p = text; *p; p++) {
            if (!isalnum((unsigned char)*p) && !strchr("_.:#@'+/-", *p)) {
                return false;
            }
        }
        return true;
    }
    const char *p = text;
    for (int part = 0; part < 2; part++) {
        const char *start = p;
        while (isdigit((unsigned char)*p)) {
            p++;
        }
        if (p == start || (*start == '0' && (p - start > 1 || part == 1))) {
            return false;
        }
        if (part == 0 && *p == '/') {
            p++;
        } else {
            break;
        }
    }
    return *p == '\0';
}

static bool put_word(Writer *w, const char *text) {
    return is_bare(text) ? common_text_append(&w->out, text) : common_text_append_json_string(&w->out, text);
}

static bool member_string(Writer *w, const cJSON *object, const char *name, const char *where, const char **value) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(object, name);
    *value = NULL;
    if (!member || cJSON_IsNull(member)) {
        return true;
    }
    if (!cJSON_IsString(member)) {
        return refuse(w, "%s%s is not a string", where, name);
    }
    *value = member->valuestring;
    return true;
}

/* Every member of `object` is one of `allowed` (or null). */
static bool only_members(Writer *w, const cJSON *object, const char *const *allowed, size_t count, const char *where) {
    for (const cJSON *member = object->child; member; member = member->next) {
        bool known = cJSON_IsNull(member);
        for (size_t i = 0; !known && i < count; i++) {
            known = strcmp(member->string, allowed[i]) == 0;
        }
        if (!known) {
            return refuse(w, "%s'%s' has no place in a blueprint text: write the request as JSON", where, member->string);
        }
    }
    return true;
}

/* A whole number or a string position: `at`. */
static bool put_at(Writer *w, const cJSON *member, const char *where) {
    if (cJSON_IsNumber(member) && member->valuedouble >= 0 && member->valuedouble == (double)(long)member->valuedouble) {
        return common_text_appendf(&w->out, "%ld", (long)member->valuedouble);
    }
    if (cJSON_IsString(member) && is_beats(member->valuestring) && is_bare(member->valuestring)) {
        return common_text_append(&w->out, member->valuestring);
    }
    return refuse(w, "%sat is not a position in beats like 2 or 3/2", where);
}

static bool put_number(Writer *w, const cJSON *member, const char *name) {
    char *text = cJSON_IsNumber(member) ? text_scalar_print(member) : NULL;
    bool ok = text && common_text_append(&w->out, text);
    free(text);
    return ok || refuse(w, "%s is not a number", name);
}

static bool write_basic(Writer *w, const cJSON *request) {
    const cJSON *motives = cJSON_GetObjectItemCaseSensitive(request, "motives");
    const cJSON *slots = cJSON_GetObjectItemCaseSensitive(request, "slots");
    if ((!motives || cJSON_IsNull(motives)) && (!slots || cJSON_IsNull(slots))) {
        return true;
    }
    const cJSON *basic = cJSON_IsObject(slots) ? cJSON_GetObjectItemCaseSensitive(slots, "basic") : NULL;
    const cJSON *motive = cJSON_IsArray(motives) && cJSON_GetArraySize(motives) == 1 ? motives->child : NULL;
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(motive, "id");
    if (!cJSON_IsString(basic) || !cJSON_IsObject(motive) || !cJSON_IsString(id) || strcmp(id->valuestring, basic->valuestring) != 0) {
        return refuse(w, "motives and slots hold more than the motive of slots.basic: write the request as JSON");
    }
    static const char *const kSlots[] = {"basic", "contrasting", "continuation", "cadential"};
    for (const cJSON *slot = slots->child; slot; slot = slot->next) {
        bool known = false;
        for (size_t i = 0; i < 4; i++) {
            known = known || strcmp(slot->string, kSlots[i]) == 0;
        }
        if (!known || (slot != cJSON_GetObjectItemCaseSensitive(slots, "basic") && !cJSON_IsNull(slot))) {
            return refuse(w, "slots.%s has no place in a blueprint text: write the request as JSON", slot->string);
        }
    }
    static const char *const kMotive[] = {"id", "form", "degrees", "pickup", "harmony", "features", "role"};
    const cJSON *form = cJSON_GetObjectItemCaseSensitive(motive, "form");
    const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(motive, "degrees");
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(motive, "pickup");
    const cJSON *harmony = cJSON_GetObjectItemCaseSensitive(motive, "harmony");
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(motive, "features");
    const cJSON *role = cJSON_GetObjectItemCaseSensitive(motive, "role");
    if (!only_members(w, motive, kMotive, sizeof kMotive / sizeof kMotive[0], "the motive of slots.basic: ") || !cJSON_IsString(degrees) ||
        !degrees->valuestring[0] || strchr(degrees->valuestring, '\n') || (form && !(cJSON_IsString(form) && strcmp(form->valuestring, "full") == 0)) ||
        (harmony && !cJSON_IsNull(harmony)) || (features && !(cJSON_IsArray(features) && cJSON_GetArraySize(features) == 0)) ||
        (role && !(cJSON_IsString(role) && strcmp(role->valuestring, "melodic") == 0))) {
        return refuse(w, "the motive of slots.basic holds more than degrees and a pickup: write the request as JSON");
    }
    bool zero = !pickup || cJSON_IsNull(pickup) || (cJSON_IsString(pickup) && strcmp(pickup->valuestring, "0") == 0);
    if (!zero && !(cJSON_IsString(pickup) && is_beats(pickup->valuestring))) {
        return refuse(w, "the pickup of the motive of slots.basic is not beats like 1 or 1/2");
    }
    return common_text_appendf(&w->out, "basic %s", degrees->valuestring) && (zero || common_text_appendf(&w->out, " pickup %s", pickup->valuestring)) &&
           common_text_append(&w->out, "\n");
}

static bool text_member(Writer *w, const cJSON *bar, const char *name, int number, bool *first) {
    const cJSON *member = cJSON_GetObjectItemCaseSensitive(bar, name);
    if (!member) {
        return true;
    }
    if (!cJSON_IsString(member) || !member->valuestring[0] || strpbrk(member->valuestring, ";\n\r\t") || isspace((unsigned char)member->valuestring[0]) ||
        isspace((unsigned char)member->valuestring[strlen(member->valuestring) - 1])) {
        return refuse(w, "bar %d: %s is not a text of one line without ';'", number, name);
    }
    bool ok = common_text_appendf(&w->out, "%s%s %s", *first ? " " : " ; ", name, member->valuestring);
    *first = false;
    return ok;
}

static bool pair_members(Writer *w, const cJSON *bar, const char *list, const char *word, const char *second, int number, bool *first) {
    const cJSON *array = cJSON_GetObjectItemCaseSensitive(bar, list);
    if (!array) {
        return true;
    }
    if (!cJSON_IsArray(array)) {
        return refuse(w, "bar %d: %s is not a list", number, list);
    }
    char where[64];
    snprintf(where, sizeof where, "bar %d %s: ", number, word);
    const cJSON *item;
    cJSON_ArrayForEach(item, array) {
        const cJSON *at = cJSON_GetObjectItemCaseSensitive(item, "at");
        const cJSON *other = cJSON_GetObjectItemCaseSensitive(item, second);
        const char *const pair[2] = {"at", second};
        if (!cJSON_IsObject(item) || !only_members(w, item, pair, 2, where) || !at || !cJSON_IsString(other) || !other->valuestring[0] ||
            strpbrk(other->valuestring, " ;\t\r\n") ||
            !common_text_appendf(&w->out, "%s%s ", *first ? " " : " ; ", word) || !put_at(w, at, where) || !common_text_append(&w->out, " ") ||
            !common_text_append(&w->out, other->valuestring)) {
            return w->error[0] ? false : refuse(w, "bar %d: %s needs its at and its %s", number, word, second);
        }
        *first = false;
    }
    return true;
}

static bool write_bar(Writer *w, const cJSON *bar, int number) {
    static const char *const kMembers[] = {"chords", "melody", "rhythm", "job", "anchors", "events", "bass", "inner"};
    char where[32];
    snprintf(where, sizeof where, "bar %d: ", number);
    if (!cJSON_IsObject(bar) || !only_members(w, bar, kMembers, sizeof kMembers / sizeof kMembers[0], where)) {
        return w->error[0] ? false : refuse(w, "bar %d is not an object", number);
    }
    bool first = true;
    if (!common_text_appendf(&w->out, "%d:", number) || !text_member(w, bar, "chords", number, &first) || !text_member(w, bar, "melody", number, &first) ||
        !text_member(w, bar, "rhythm", number, &first) || !text_member(w, bar, "job", number, &first) ||
        !pair_members(w, bar, "anchors", "anchor", "pitch", number, &first) || !pair_members(w, bar, "events", "event", "type", number, &first) ||
        !text_member(w, bar, "bass", number, &first)) {
        return false;
    }
    const cJSON *inner = cJSON_GetObjectItemCaseSensitive(bar, "inner");
    if (inner && !cJSON_IsArray(inner)) {
        return refuse(w, "bar %d: inner is not a list", number);
    }
    const cJSON *voice;
    cJSON_ArrayForEach(voice, inner) {
        if (!cJSON_IsString(voice) || !voice->valuestring[0] || strpbrk(voice->valuestring, ";\n\r\t")) {
            return refuse(w, "bar %d: an inner voice is not a text of one line without ';'", number);
        }
        if (!common_text_appendf(&w->out, "%sinner %s", first ? " " : " ; ", voice->valuestring)) {
            return false;
        }
        first = false;
    }
    return common_text_append(&w->out, "\n");
}

static bool write_line(Writer *w, const char *name, const char *value) {
    return !value || (common_text_appendf(&w->out, "%s ", name) && put_word(w, value) && common_text_append(&w->out, "\n"));
}

static bool write_request(Writer *w, const cJSON *request) {
    static const char *const kMembers[] = {"schema", "schema_version", "into", "key", "material", "meter", "bars", "cadence",
                                           "first_cadence", "development", "tempo", "motives", "slots", "blueprint"};
    if (!cJSON_IsObject(request)) {
        return refuse(w, "a theme request is an object");
    }
    const cJSON *schema = cJSON_GetObjectItemCaseSensitive(request, "schema");
    const cJSON *blueprint = cJSON_GetObjectItemCaseSensitive(request, "blueprint");
    if ((schema && !(cJSON_IsString(schema) && strcmp(schema->valuestring, "ursatz-theme-request") == 0)) || !cJSON_IsObject(blueprint)) {
        return refuse(w, "this is not a blueprint: a theme request with a blueprint member is");
    }
    if (!only_members(w, request, kMembers, sizeof kMembers / sizeof kMembers[0], "the request member ")) {
        return false;
    }
    const char *into, *key, *material, *meter, *cadence, *first_cadence, *development;
    if (!member_string(w, request, "into", "", &into) || !member_string(w, request, "key", "", &key) || !member_string(w, request, "material", "", &material) ||
        !member_string(w, request, "meter", "", &meter) || !member_string(w, request, "cadence", "", &cadence) ||
        !member_string(w, request, "first_cadence", "", &first_cadence) || !member_string(w, request, "development", "", &development)) {
        return false;
    }
    if (material && !key) {
        return refuse(w, "the material needs the key");
    }
    if (!common_text_append(&w->out, BLUEPRINT_HEADER "\n")) {
        return false;
    }
    if (key && (!common_text_append(&w->out, "key ") || !put_word(w, key) || (material && (!common_text_append(&w->out, " ") || !put_word(w, material))) ||
                !common_text_append(&w->out, "\n"))) {
        return w->error[0] ? false : refuse(w, "out of memory");
    }
    const cJSON *bars = cJSON_GetObjectItemCaseSensitive(request, "bars");
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(request, "tempo");
    bool ok = write_line(w, "meter", meter) && write_line(w, "into", into);
    if (ok && bars && !cJSON_IsNull(bars)) {
        ok = common_text_append(&w->out, "bars ") && put_number(w, bars, "bars") && common_text_append(&w->out, "\n");
    }
    ok = ok && write_line(w, "cadence", cadence) && write_line(w, "first_cadence", first_cadence) && write_line(w, "development", development);
    if (ok && tempo && !cJSON_IsNull(tempo)) {
        ok = common_text_append(&w->out, "tempo ") && (cJSON_IsString(tempo) ? put_word(w, tempo->valuestring) : put_number(w, tempo, "tempo")) &&
             common_text_append(&w->out, "\n");
    }
    if (!ok) {
        return w->error[0] ? false : refuse(w, "out of memory");
    }
    static const char *const kBlueprint[] = {"bars", "climax", "accompaniment"};
    if (!only_members(w, blueprint, kBlueprint, 3, "the blueprint member ")) {
        return false;
    }
    const cJSON *accompaniment = cJSON_GetObjectItemCaseSensitive(blueprint, "accompaniment");
    if (accompaniment && !cJSON_IsNull(accompaniment)) {
        static const char *const kAccompaniment[] = {"pattern", "voicing"};
        const char *pattern, *voicing;
        if (!cJSON_IsObject(accompaniment) || !only_members(w, accompaniment, kAccompaniment, 2, "the blueprint's accompaniment ") ||
            !member_string(w, accompaniment, "pattern", "the accompaniment's ", &pattern) || !pattern ||
            !member_string(w, accompaniment, "voicing", "the accompaniment's ", &voicing)) {
            return w->error[0] ? false : refuse(w, "the blueprint's accompaniment needs a pattern");
        }
        if (!common_text_append(&w->out, "accompaniment ") || !put_word(w, pattern) ||
            (voicing && (!common_text_append(&w->out, " voicing ") || !put_word(w, voicing))) || !common_text_append(&w->out, "\n")) {
            return w->error[0] ? false : refuse(w, "out of memory");
        }
    }
    const cJSON *climax = cJSON_GetObjectItemCaseSensitive(blueprint, "climax");
    if (climax && !cJSON_IsNull(climax)) {
        static const char *const kClimax[] = {"bar", "at", "pitch"};
        const cJSON *bar = cJSON_GetObjectItemCaseSensitive(climax, "bar");
        const cJSON *pitch = cJSON_GetObjectItemCaseSensitive(climax, "pitch");
        if (!cJSON_IsObject(climax) || !only_members(w, climax, kClimax, 3, "the climax ") || !cJSON_IsNumber(bar) || bar->valuedouble < 0 ||
            bar->valuedouble != (double)(int)bar->valuedouble || !cJSON_IsString(pitch) || !is_bare(pitch->valuestring) ||
            !common_text_appendf(&w->out, "climax %d ", (int)bar->valuedouble + 1) || !put_at(w, cJSON_GetObjectItemCaseSensitive(climax, "at"), "the climax ") ||
            !common_text_appendf(&w->out, " %s\n", pitch->valuestring)) {
            return w->error[0] ? false : refuse(w, "the climax needs a bar from 0, an at and a pitch");
        }
    }
    if (!write_basic(w, request)) {
        return w->error[0] ? false : refuse(w, "out of memory");
    }
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(blueprint, "bars");
    if (!cJSON_IsArray(list) || cJSON_GetArraySize(list) == 0) {
        return refuse(w, "the blueprint has no bars");
    }
    int number = 1;
    const cJSON *bar;
    cJSON_ArrayForEach(bar, list) {
        if (!write_bar(w, bar, number++)) {
            return w->error[0] ? false : refuse(w, "out of memory");
        }
    }
    return true;
}

bool blueprint_text_write(const cJSON *request, char **text, char *error, size_t size) {
    char scratch[8];
    if (!error || size == 0) {
        error = scratch;
        size = sizeof scratch;
    }
    *text = NULL;
    Writer writer = {{NULL, 0, 0, false}, error, size};
    common_text_init(&writer.out);
    error[0] = '\0';
    bool ok = write_request(&writer, request);
    char *out = ok ? common_text_take(&writer.out) : NULL;
    common_text_free(&writer.out);
    if (!out) {
        if (!error[0]) {
            snprintf(error, size, "out of memory");
        }
        return false;
    }
    *text = out;
    return true;
}
