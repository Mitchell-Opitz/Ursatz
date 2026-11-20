#include "text/text_tree.h"

#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/strings.h>

/* ---- shared helpers ------------------------------------------------------------------------------------------------------------- */

static void set_error(TextError *error, int line, const char *format, ...) {
    if (!error) {
        return;
    }
    error->line = line;
    va_list args;
    va_start(args, format);
    vsnprintf(error->message, sizeof error->message, format, args);
    va_end(args);
}

static bool is_keyword(const char *s) {
    return strcmp(s, "null") == 0 || strcmp(s, "true") == 0 || strcmp(s, "false") == 0;
}

/* A bare string: [A-Za-z_][A-Za-z0-9_.:#@'+/-]*, not a keyword, no "..". */
static bool is_bare_string(const char *s) {
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) {
        return false;
    }
    for (const char *p = s + 1; *p; p++) {
        if (!(isalnum((unsigned char)*p) || strchr("_.:#@'+/-", *p))) {
            return false;
        }
    }
    return !is_keyword(s) && strstr(s, "..") == NULL;
}

/* A bare key: [A-Za-z_$][A-Za-z0-9_$-]*, not a keyword. */
static bool is_bare_key(const char *s) {
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_' || s[0] == '$')) {
        return false;
    }
    for (const char *p = s + 1; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '$' || *p == '-')) {
            return false;
        }
    }
    return !is_keyword(s);
}

/* A table column: [A-Za-z_][A-Za-z0-9_-]*. */
static bool is_column_name(const char *s) {
    if (!(isalpha((unsigned char)s[0]) || s[0] == '_')) {
        return false;
    }
    for (const char *p = s + 1; *p; p++) {
        if (!(isalnum((unsigned char)*p) || *p == '_' || *p == '-')) {
            return false;
        }
    }
    return true;
}

static char *quoted(const char *s) {
    cJSON *item = cJSON_CreateString(s);
    if (!item) {
        return NULL;
    }
    char *out = cJSON_PrintUnformatted(item);
    cJSON_Delete(item);
    return out;
}

/* ---- printing ------------------------------------------------------------------------------------------------------------------- */

static bool whole(const cJSON *v, long long *out) {
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble != floor(v->valuedouble) || fabs(v->valuedouble) > 9007199254740992.0) {
        return false;
    }
    *out = (long long)v->valuedouble;
    return true;
}

static bool is_rational(const cJSON *o, long long *num, long long *den) {
    if (!cJSON_IsObject(o)) {
        return false;
    }
    const cJSON *n = o->child;
    const cJSON *d = n ? n->next : NULL;
    if (!n || !d || d->next || !n->string || !d->string || strcmp(n->string, "num") != 0 || strcmp(d->string, "den") != 0) {
        return false;
    }
    return whole(n, num) && whole(d, den) && *den > 0;
}

static bool is_span(const cJSON *o, long long v[4]) {
    if (!cJSON_IsObject(o)) {
        return false;
    }
    const cJSON *s = o->child;
    const cJSON *e = s ? s->next : NULL;
    if (!s || !e || e->next || !s->string || !e->string || strcmp(s->string, "start") != 0 || strcmp(e->string, "end") != 0) {
        return false;
    }
    return is_rational(s, &v[0], &v[1]) && is_rational(e, &v[2], &v[3]);
}

static char *dup_string(const char *s) {
    return common_strdup(s);
}

static char *printf_alloc(const char *format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    return dup_string(buffer);
}

/* One scalar token, or NULL. */
static char *scalar_token(const cJSON *v) {
    long long a[4];
    if (cJSON_IsNull(v)) {
        return dup_string("null");
    }
    if (cJSON_IsTrue(v)) {
        return dup_string("true");
    }
    if (cJSON_IsFalse(v)) {
        return dup_string("false");
    }
    if (cJSON_IsNumber(v)) {
        return cJSON_PrintUnformatted(v);
    }
    if (cJSON_IsString(v) && v->valuestring) {
        return is_bare_string(v->valuestring) ? dup_string(v->valuestring) : quoted(v->valuestring);
    }
    if (is_rational(v, &a[0], &a[1])) {
        return printf_alloc("%lld/%lld", a[0], a[1]);
    }
    if (is_span(v, a)) {
        return printf_alloc("%lld/%lld..%lld/%lld", a[0], a[1], a[2], a[3]);
    }
    return NULL;
}

static bool is_empty_object(const cJSON *v) {
    return cJSON_IsObject(v) && v->child == NULL;
}

/* A token: a scalar, an inline array of scalars, or {}. NULL when the value needs lines of its own. */
static char *value_token(const cJSON *v) {
    char *s = scalar_token(v);
    if (s) {
        return s;
    }
    if (is_empty_object(v)) {
        return dup_string("{}");
    }
    if (!cJSON_IsArray(v)) {
        return NULL;
    }
    CommonText out;
    common_text_init(&out);
    common_text_append(&out, "[");
    bool first = true;
    for (const cJSON *x = v->child; x; x = x->next) {
        char *t = scalar_token(x);
        if (!t) {
            common_text_free(&out);
            return NULL;
        }
        if (!first) {
            common_text_append(&out, " ");
        }
        common_text_append(&out, t);
        free(t);
        first = false;
    }
    common_text_append(&out, "]");
    return common_text_take(&out);
}

static bool has_token(const cJSON *v) {
    char *t = value_token(v);
    free(t);
    return t != NULL;
}

/* A member that can lead its line: it has a token and is not {}. */
static bool is_leading(const cJSON *v) {
    return has_token(v) && !is_empty_object(v);
}

static char *key_token(const char *k) {
    return is_bare_key(k) ? dup_string(k) : quoted(k);
}

typedef struct {
    const char *a;
    const char *b; /* NULL for a plain column */
} Column;

typedef struct {
    Column *items;
    size_t count;
} Columns;

static void columns_free(Columns *c) {
    free(c->items);
    c->items = NULL;
    c->count = 0;
}

static bool columns_add(Columns *c, const char *a, const char *b) {
    Column *grown = realloc(c->items, (c->count + 1) * sizeof *grown);
    if (!grown) {
        return false;
    }
    c->items = grown;
    c->items[c->count].a = a;
    c->items[c->count].b = b;
    c->count++;
    return true;
}

/* The columns of one row: every member a cell or a one-level object of cells. false when the row cannot be a table row. */
static bool columns_of(const cJSON *row, Columns *out) {
    out->items = NULL;
    out->count = 0;
    if (!cJSON_IsObject(row) || !row->child) {
        return false;
    }
    for (const cJSON *m = row->child; m; m = m->next) {
        if (!m->string || !is_column_name(m->string)) {
            columns_free(out);
            return false;
        }
        if (is_leading(m)) {
            if (!columns_add(out, m->string, NULL)) {
                columns_free(out);
                return false;
            }
        } else if (cJSON_IsObject(m) && m->child) {
            for (const cJSON *s = m->child; s; s = s->next) {
                if (!s->string || !is_column_name(s->string) || !is_leading(s) || !columns_add(out, m->string, s->string)) {
                    columns_free(out);
                    return false;
                }
            }
        } else {
            columns_free(out);
            return false;
        }
    }
    return true;
}

static bool same_columns(const Columns *x, const Columns *y) {
    if (x->count != y->count) {
        return false;
    }
    for (size_t i = 0; i < x->count; i++) {
        if (strcmp(x->items[i].a, y->items[i].a) != 0 || (x->items[i].b == NULL) != (y->items[i].b == NULL) ||
            (x->items[i].b && strcmp(x->items[i].b, y->items[i].b) != 0)) {
            return false;
        }
    }
    return true;
}

/* The columns of an array that prints as a table, or false. */
static bool table_columns(const cJSON *array, Columns *out) {
    if (cJSON_GetArraySize(array) < 2) {
        return false;
    }
    if (!columns_of(array->child, out)) {
        return false;
    }
    for (const cJSON *row = array->child->next; row; row = row->next) {
        Columns other;
        bool ok = columns_of(row, &other);
        if (ok) {
            ok = same_columns(out, &other);
            columns_free(&other);
        }
        if (!ok) {
            columns_free(out);
            return false;
        }
    }
    return true;
}

static void line_start(CommonText *out, int indent) {
    for (int i = 0; i < indent; i++) {
        common_text_append(out, " ");
    }
}

static void emit_member(CommonText *out, const char *key, const cJSON *v, int indent);
static void emit_item(CommonText *out, const cJSON *x, int indent);

/* The leading scalar members of an object as " k v k v", and how many there are. */
static size_t emit_leading(CommonText *out, const cJSON *object) {
    size_t n = 0;
    for (const cJSON *m = object->child; m && is_leading(m); m = m->next) {
        char *k = key_token(m->string ? m->string : "");
        char *t = value_token(m);
        common_text_append(out, " ");
        common_text_append(out, k ? k : "");
        common_text_append(out, " ");
        common_text_append(out, t ? t : "");
        free(k);
        free(t);
        n++;
    }
    return n;
}

static void emit_rest(CommonText *out, const cJSON *object, size_t skip, int indent) {
    const cJSON *m = object->child;
    for (size_t i = 0; m && i < skip; i++) {
        m = m->next;
    }
    for (; m; m = m->next) {
        char *k = key_token(m->string ? m->string : "");
        emit_member(out, k ? k : "", m, indent);
        free(k);
    }
}

static void emit_member(CommonText *out, const char *key, const cJSON *v, int indent) {
    char *t = value_token(v);
    line_start(out, indent);
    common_text_append(out, key);
    if (t) {
        common_text_append(out, " ");
        common_text_append(out, t);
        common_text_append(out, "\n");
        free(t);
        return;
    }
    if (cJSON_IsArray(v)) {
        Columns cols;
        if (table_columns(v, &cols)) {
            common_text_append(out, " |");
            for (size_t i = 0; i < cols.count; i++) {
                common_text_append(out, " ");
                common_text_append(out, cols.items[i].a);
                if (cols.items[i].b) {
                    common_text_append(out, ".");
                    common_text_append(out, cols.items[i].b);
                }
            }
            common_text_append(out, "\n");
            for (const cJSON *row = v->child; row; row = row->next) {
                line_start(out, indent + 2);
                for (size_t i = 0; i < cols.count; i++) {
                    const cJSON *cell = cJSON_GetObjectItemCaseSensitive(row, cols.items[i].a);
                    if (cols.items[i].b) {
                        cell = cJSON_GetObjectItemCaseSensitive(cell, cols.items[i].b);
                    }
                    char *c = value_token(cell);
                    if (i) {
                        common_text_append(out, " ");
                    }
                    common_text_append(out, c ? c : "null");
                    free(c);
                }
                common_text_append(out, "\n");
            }
            columns_free(&cols);
            return;
        }
        common_text_append(out, "\n");
        for (const cJSON *x = v->child; x; x = x->next) {
            emit_item(out, x, indent + 2);
        }
        return;
    }
    /* an object */
    size_t n = emit_leading(out, v);
    common_text_append(out, "\n");
    emit_rest(out, v, n, indent + 2);
}

static void emit_item(CommonText *out, const cJSON *x, int indent) {
    char *t = value_token(x);
    line_start(out, indent);
    if (t) {
        common_text_append(out, "- ");
        common_text_append(out, t);
        common_text_append(out, "\n");
        free(t);
        return;
    }
    if (cJSON_IsObject(x)) {
        common_text_append(out, "-");
        size_t n = emit_leading(out, x);
        common_text_append(out, "\n");
        emit_rest(out, x, n, indent + 2);
        return;
    }
    common_text_append(out, "-\n");
    for (const cJSON *y = x->child; y; y = y->next) {
        emit_item(out, y, indent + 2);
    }
}

char *text_tree_print(const cJSON *object, TextKind kind) {
    if (!cJSON_IsObject(object)) {
        return NULL;
    }
    CommonText out;
    common_text_init(&out);
    common_text_appendf(&out, "ursatz-text %d %s\n", URSATZ_TEXT_GRAMMAR_VERSION, text_kind_name(kind));
    emit_rest(&out, object, 0, 0);
    if (out.failed) {
        common_text_free(&out);
        return NULL;
    }
    return common_text_take(&out);
}

char *text_scalar_print(const cJSON *value) {
    if (is_empty_object(value)) {
        return NULL;
    }
    return value_token(value);
}

/* ---- lines ---------------------------------------------------------------------------------------------------------------------- */

void text_lines_free(TextLines *lines) {
    if (!lines) {
        return;
    }
    for (size_t i = 0; i < lines->count; i++) {
        free(lines->lines[i].text);
    }
    free(lines->lines);
    lines->lines = NULL;
    lines->count = 0;
}

bool text_lines_split(const char *text, TextLines *out, TextError *error) {
    out->lines = NULL;
    out->count = 0;
    size_t capacity = 0;
    int number = 0;
    const char *p = text ? text : "";
    while (*p) {
        number++;
        const char *end = strchr(p, '\n');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        const char *next = end ? end + 1 : p + length;
        while (length > 0 && (p[length - 1] == '\r' || p[length - 1] == ' ' || p[length - 1] == '\t')) {
            length--;
        }
        size_t indent = 0;
        while (indent < length && p[indent] == ' ') {
            indent++;
        }
        if (indent < length && p[indent] == '\t') {
            set_error(error, number, "indent with two spaces, not a tab");
            text_lines_free(out);
            return false;
        }
        if (indent < length && p[indent] != '#') {
            if (indent % 2 != 0) {
                set_error(error, number, "indent with two spaces");
                text_lines_free(out);
                return false;
            }
            if (out->count == capacity) {
                size_t grown = capacity ? capacity * 2 : 64;
                TextLine *lines = realloc(out->lines, grown * sizeof *lines);
                if (!lines) {
                    set_error(error, number, "out of memory");
                    text_lines_free(out);
                    return false;
                }
                out->lines = lines;
                capacity = grown;
            }
            char *copy = malloc(length - indent + 1);
            if (!copy) {
                set_error(error, number, "out of memory");
                text_lines_free(out);
                return false;
            }
            memcpy(copy, p + indent, length - indent);
            copy[length - indent] = '\0';
            out->lines[out->count].indent = (int)indent;
            out->lines[out->count].number = number;
            out->lines[out->count].text = copy;
            out->count++;
        }
        p = next;
    }
    return true;
}

/* ---- units ---------------------------------------------------------------------------------------------------------------------- */

static bool parse_rational_token(const char *s, size_t length, long long *num, long long *den) {
    size_t i = 0;
    if (i < length && s[i] == '-') {
        i++;
    }
    size_t digits = 0;
    while (i < length && isdigit((unsigned char)s[i])) {
        i++;
        digits++;
    }
    if (digits == 0 || i >= length || s[i] != '/') {
        return false;
    }
    size_t slash = i;
    i++;
    size_t den_digits = 0;
    while (i < length && isdigit((unsigned char)s[i])) {
        i++;
        den_digits++;
    }
    if (den_digits == 0 || i != length) {
        return false;
    }
    char buffer[64];
    if (length >= sizeof buffer) {
        return false;
    }
    memcpy(buffer, s, length);
    buffer[length] = '\0';
    buffer[slash] = '\0';
    *num = strtoll(buffer, NULL, 10);
    *den = strtoll(buffer + slash + 1, NULL, 10);
    return true;
}

static bool is_number_token(const char *s, size_t length) {
    size_t i = 0;
    if (i < length && s[i] == '-') {
        i++;
    }
    size_t digits = 0;
    while (i < length && isdigit((unsigned char)s[i])) {
        i++;
        digits++;
    }
    if (digits == 0) {
        return false;
    }
    if (i < length && s[i] == '.') {
        i++;
        size_t fraction = 0;
        while (i < length && isdigit((unsigned char)s[i])) {
            i++;
            fraction++;
        }
        if (fraction == 0) {
            return false;
        }
    }
    if (i < length && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < length && (s[i] == '+' || s[i] == '-')) {
            i++;
        }
        size_t exponent = 0;
        while (i < length && isdigit((unsigned char)s[i])) {
            i++;
            exponent++;
        }
        if (exponent == 0) {
            return false;
        }
    }
    return i == length;
}

static cJSON *rational_object(long long num, long long den) {
    cJSON *o = cJSON_CreateObject();
    if (!o) {
        return NULL;
    }
    if (!cJSON_AddNumberToObject(o, "num", (double)num) || !cJSON_AddNumberToObject(o, "den", (double)den)) {
        cJSON_Delete(o);
        return NULL;
    }
    return o;
}

/* The unit of a bare token. */
static cJSON *bare_unit(const char *s, size_t length, int number, TextError *error) {
    char *token = malloc(length + 1);
    if (!token) {
        set_error(error, number, "out of memory");
        return NULL;
    }
    memcpy(token, s, length);
    token[length] = '\0';
    cJSON *out = NULL;
    bool reported = false;
    long long a, b, c, d;
    const char *dots = strstr(token, "..");
    if (strcmp(token, "null") == 0) {
        out = cJSON_CreateNull();
    } else if (strcmp(token, "true") == 0) {
        out = cJSON_CreateTrue();
    } else if (strcmp(token, "false") == 0) {
        out = cJSON_CreateFalse();
    } else if (strcmp(token, "{}") == 0) {
        out = cJSON_CreateObject();
    } else if (dots && (isdigit((unsigned char)token[0]) || token[0] == '-')) {
        size_t left = (size_t)(dots - token);
        if (!parse_rational_token(token, left, &a, &b) || !parse_rational_token(dots + 2, strlen(dots + 2), &c, &d) || b <= 0 || d <= 0) {
            set_error(error, number, "'%s' is not a span: write it as n/d..n/d", token);
            reported = true;
        } else {
            cJSON *start = rational_object(a, b);
            cJSON *end = rational_object(c, d);
            out = cJSON_CreateObject();
            if (start && end && out) {
                cJSON_AddItemToObject(out, "start", start);
                cJSON_AddItemToObject(out, "end", end);
            } else {
                cJSON_Delete(start);
                cJSON_Delete(end);
                cJSON_Delete(out);
                out = NULL;
            }
        }
    } else if (parse_rational_token(token, length, &a, &b)) {
        if (b <= 0) {
            set_error(error, number, "'%s' has a denominator that is not above 0", token);
            reported = true;
        } else {
            out = rational_object(a, b);
        }
    } else if (is_number_token(token, length)) {
        out = cJSON_CreateNumber(strtod(token, NULL));
    } else {
        out = cJSON_CreateString(token);
    }
    free(token);
    if (!out && !reported) {
        set_error(error, number, "out of memory");
    }
    return out;
}

/* Reads units from *p into `into` until the end of the line (nested false) or the closing ] (nested true). */
static bool lex_units(const char **p, int number, cJSON *into, bool nested, TextError *error) {
    for (;;) {
        while (**p == ' ') {
            (*p)++;
        }
        if (**p == '\0') {
            if (nested) {
                set_error(error, number, "unterminated [");
                return false;
            }
            return true;
        }
        if (**p == ']') {
            if (!nested) {
                set_error(error, number, "unexpected ]");
                return false;
            }
            (*p)++;
            return true;
        }
        cJSON *unit = NULL;
        if (**p == '[') {
            (*p)++;
            unit = cJSON_CreateArray();
            if (!unit || !lex_units(p, number, unit, true, error)) {
                if (!unit) {
                    set_error(error, number, "out of memory");
                }
                cJSON_Delete(unit);
                return false;
            }
        } else if (**p == '"') {
            const char *s = *p;
            const char *q = s + 1;
            while (*q && *q != '"') {
                if (*q == '\\' && q[1]) {
                    q++;
                }
                q++;
            }
            if (*q != '"') {
                set_error(error, number, "unterminated string");
                return false;
            }
            q++;
            unit = cJSON_ParseWithLength(s, (size_t)(q - s));
            if (!unit || !cJSON_IsString(unit)) {
                cJSON_Delete(unit);
                set_error(error, number, "bad string %.*s", (int)(q - s), s);
                return false;
            }
            *p = q;
        } else {
            const char *s = *p;
            const char *q = s;
            while (*q && *q != ' ' && *q != '[' && *q != ']') {
                q++;
            }
            unit = bare_unit(s, (size_t)(q - s), number, error);
            if (!unit) {
                return false;
            }
            *p = q;
        }
        cJSON_AddItemToArray(into, unit);
    }
}

cJSON *text_units_parse(const char *line, int number, TextError *error) {
    cJSON *units = cJSON_CreateArray();
    if (!units) {
        set_error(error, number, "out of memory");
        return NULL;
    }
    const char *p = line ? line : "";
    if (!lex_units(&p, number, units, false, error)) {
        cJSON_Delete(units);
        return NULL;
    }
    return units;
}

/* ---- blocks --------------------------------------------------------------------------------------------------------------------- */

static bool is_item_line(const char *text) {
    return text[0] == '-' && (text[1] == '\0' || text[1] == ' ');
}

static cJSON *block_at(const TextLines *lines, size_t *pos, int indent, TextError *error);

/* Adds `value` as member `key` of `object` (taking it); a repeated key is refused. */
static bool add_member(cJSON *object, const char *key, cJSON *value, int number, TextError *error) {
    if (cJSON_GetObjectItemCaseSensitive(object, key)) {
        cJSON_Delete(value);
        set_error(error, number, "duplicate member '%s'", key);
        return false;
    }
    cJSON_AddItemToObject(object, key, value);
    return true;
}

/* The members of an object written as units: pairs of key and value. false with the line named when they are not. */
static bool pairs_into(cJSON *object, const cJSON *units, int number, TextError *error) {
    int count = cJSON_GetArraySize(units);
    if (count % 2 != 0) {
        set_error(error, number, "a member takes one value or pairs of key and value");
        return false;
    }
    for (const cJSON *k = units->child; k; k = k->next->next) {
        if (!cJSON_IsString(k)) {
            set_error(error, number, "a key is a name or a quoted string");
            return false;
        }
        cJSON *v = cJSON_Duplicate(k->next, true);
        if (!v) {
            set_error(error, number, "out of memory");
            return false;
        }
        if (!add_member(object, k->valuestring, v, number, error)) {
            return false;
        }
    }
    return true;
}

/* Moves the members of `from` (an object) into `into`. */
static bool merge_members(cJSON *into, cJSON *from, int number, TextError *error) {
    while (from->child) {
        cJSON *m = cJSON_DetachItemFromObject(from, from->child->string);
        char *key = m->string;
        m->string = NULL;
        bool ok = add_member(into, key, m, number, error);
        free(key);
        if (!ok) {
            return false;
        }
    }
    return true;
}

/* The members below a line that already carries pairs; list items there are refused. */
static cJSON *member_block(const TextLines *lines, size_t *pos, int indent, TextError *error) {
    if (is_item_line(lines->lines[*pos].text)) {
        set_error(error, lines->lines[*pos].number, "members expected, not list items");
        return NULL;
    }
    return block_at(lines, pos, indent, error);
}

/* The key at the start of a member line, and where its value starts. */
static bool lex_key(const char *text, int number, char **key, const char **rest, TextError *error) {
    const char *p = text;
    if (*p == '"') {
        const char *q = p + 1;
        while (*q && *q != '"') {
            if (*q == '\\' && q[1]) {
                q++;
            }
            q++;
        }
        if (*q != '"') {
            set_error(error, number, "unterminated string");
            return false;
        }
        q++;
        cJSON *s = cJSON_ParseWithLength(p, (size_t)(q - p));
        if (!s || !cJSON_IsString(s)) {
            cJSON_Delete(s);
            set_error(error, number, "bad key %.*s", (int)(q - p), p);
            return false;
        }
        *key = dup_string(s->valuestring);
        cJSON_Delete(s);
        p = q;
    } else {
        const char *q = p;
        while (*q && *q != ' ') {
            q++;
        }
        *key = malloc((size_t)(q - p) + 1);
        if (*key) {
            memcpy(*key, p, (size_t)(q - p));
            (*key)[q - p] = '\0';
            if (!is_bare_key(*key)) {
                set_error(error, number, "a key is a name or a quoted string");
                free(*key);
                *key = NULL;
                return false;
            }
        }
        p = q;
    }
    if (!*key) {
        set_error(error, number, "out of memory");
        return false;
    }
    while (*p == ' ') {
        p++;
    }
    *rest = p;
    return true;
}

static cJSON *table_rows(const TextLines *lines, size_t *pos, int child, const char *header, int number, TextError *error) {
    /* the columns */
    CommonNames names;
    memset(&names, 0, sizeof names);
    const char *p = header;
    for (;;) {
        while (*p == ' ') {
            p++;
        }
        if (!*p) {
            break;
        }
        const char *q = p;
        while (*q && *q != ' ') {
            q++;
        }
        char *name = malloc((size_t)(q - p) + 1);
        if (!name) {
            common_names_free(&names);
            set_error(error, number, "out of memory");
            return NULL;
        }
        memcpy(name, p, (size_t)(q - p));
        name[q - p] = '\0';
        names.items = realloc(names.items, (names.count + 1) * sizeof *names.items);
        if (!names.items) {
            free(name);
            names.count = 0;
            set_error(error, number, "out of memory");
            return NULL;
        }
        names.items[names.count++] = name;
        p = q;
    }
    names.capacity = names.count;
    cJSON *rows = NULL;
    if (names.count == 0) {
        set_error(error, number, "a table needs its column names after |");
        goto done;
    }
    rows = cJSON_CreateArray();
    if (!rows) {
        set_error(error, number, "out of memory");
        goto done;
    }
    while (*pos < lines->count && lines->lines[*pos].indent == child) {
        const TextLine *line = &lines->lines[*pos];
        (*pos)++;
        cJSON *cells = text_units_parse(line->text, line->number, error);
        if (!cells) {
            cJSON_Delete(rows);
            rows = NULL;
            goto done;
        }
        if ((size_t)cJSON_GetArraySize(cells) != names.count) {
            set_error(error, line->number, "a table row has %d cells, its header has %zu", cJSON_GetArraySize(cells), names.count);
            cJSON_Delete(cells);
            cJSON_Delete(rows);
            rows = NULL;
            goto done;
        }
        cJSON *row = cJSON_CreateObject();
        const cJSON *cell = cells->child;
        for (size_t i = 0; row && i < names.count; i++, cell = cell->next) {
            const char *name = names.items[i];
            const char *dot = strchr(name, '.');
            cJSON *value = cJSON_Duplicate(cell, true);
            if (!value) {
                cJSON_Delete(row);
                row = NULL;
                break;
            }
            if (!dot) {
                if (!add_member(row, name, value, line->number, error)) {
                    cJSON_Delete(row);
                    row = NULL;
                }
                continue;
            }
            char *outer = malloc((size_t)(dot - name) + 1);
            if (!outer) {
                cJSON_Delete(value);
                cJSON_Delete(row);
                row = NULL;
                break;
            }
            memcpy(outer, name, (size_t)(dot - name));
            outer[dot - name] = '\0';
            cJSON *group = cJSON_GetObjectItemCaseSensitive(row, outer);
            if (!group) {
                group = cJSON_AddObjectToObject(row, outer);
            }
            free(outer);
            if (!group || !add_member(group, dot + 1, value, line->number, error)) {
                cJSON_Delete(row);
                row = NULL;
            }
        }
        cJSON_Delete(cells);
        if (!row) {
            if (error && error->message[0] == '\0') {
                set_error(error, line->number, "out of memory");
            }
            cJSON_Delete(rows);
            rows = NULL;
            goto done;
        }
        cJSON_AddItemToArray(rows, row);
    }
done:
    common_names_free(&names);
    return rows;
}

/* One member line at `indent`: parses it into `object`. */
static bool member_line(const TextLines *lines, size_t *pos, int indent, cJSON *object, TextError *error) {
    const TextLine *line = &lines->lines[*pos];
    (*pos)++;
    char *key = NULL;
    const char *rest = NULL;
    if (!lex_key(line->text, line->number, &key, &rest, error)) {
        return false;
    }
    int child = indent + 2;
    cJSON *value = NULL;
    if (*rest == '\0') {
        if (*pos >= lines->count || lines->lines[*pos].indent != child) {
            set_error(error, line->number, "expected an indented block below this line");
            free(key);
            return false;
        }
        value = block_at(lines, pos, child, error);
        if (!value) {
            free(key);
            return false;
        }
    } else if (rest[0] == '|' && (rest[1] == '\0' || rest[1] == ' ')) {
        value = table_rows(lines, pos, child, rest + 1, line->number, error);
        if (!value) {
            free(key);
            return false;
        }
    } else {
        cJSON *units = text_units_parse(rest, line->number, error);
        if (!units) {
            free(key);
            return false;
        }
        int count = cJSON_GetArraySize(units);
        if (count == 1) {
            value = cJSON_DetachItemFromArray(units, 0);
        } else {
            value = cJSON_CreateObject();
            if (!value || !pairs_into(value, units, line->number, error)) {
                cJSON_Delete(value);
                cJSON_Delete(units);
                free(key);
                return false;
            }
            if (*pos < lines->count && lines->lines[*pos].indent == child) {
                cJSON *more = member_block(lines, pos, child, error);
                if (!more || !merge_members(value, more, lines->lines[*pos - 1].number, error)) {
                    cJSON_Delete(more);
                    cJSON_Delete(value);
                    cJSON_Delete(units);
                    free(key);
                    return false;
                }
                cJSON_Delete(more);
            }
        }
        cJSON_Delete(units);
    }
    bool ok = add_member(object, key, value, line->number, error);
    free(key);
    return ok;
}

/* One "- ..." item at `indent`: appends it to `array`. */
static bool item_line(const TextLines *lines, size_t *pos, int indent, cJSON *array, TextError *error) {
    const TextLine *line = &lines->lines[*pos];
    (*pos)++;
    int child = indent + 2;
    const char *rest = line->text + 1;
    while (*rest == ' ') {
        rest++;
    }
    cJSON *value = NULL;
    if (*rest == '\0') {
        if (*pos >= lines->count || lines->lines[*pos].indent != child) {
            set_error(error, line->number, "expected an indented block below this item");
            return false;
        }
        value = block_at(lines, pos, child, error);
        if (!value) {
            return false;
        }
    } else {
        cJSON *units = text_units_parse(rest, line->number, error);
        if (!units) {
            return false;
        }
        if (cJSON_GetArraySize(units) == 1) {
            value = cJSON_DetachItemFromArray(units, 0);
        } else {
            value = cJSON_CreateObject();
            if (!value || !pairs_into(value, units, line->number, error)) {
                cJSON_Delete(value);
                cJSON_Delete(units);
                return false;
            }
            if (*pos < lines->count && lines->lines[*pos].indent == child) {
                cJSON *more = member_block(lines, pos, child, error);
                if (!more || !merge_members(value, more, lines->lines[*pos - 1].number, error)) {
                    cJSON_Delete(more);
                    cJSON_Delete(value);
                    cJSON_Delete(units);
                    return false;
                }
                cJSON_Delete(more);
            }
        }
        cJSON_Delete(units);
    }
    cJSON_AddItemToArray(array, value);
    return true;
}

static cJSON *block_at(const TextLines *lines, size_t *pos, int indent, TextError *error) {
    bool items = *pos < lines->count && lines->lines[*pos].indent == indent && is_item_line(lines->lines[*pos].text);
    cJSON *out = items ? cJSON_CreateArray() : cJSON_CreateObject();
    if (!out) {
        set_error(error, 0, "out of memory");
        return NULL;
    }
    while (*pos < lines->count && lines->lines[*pos].indent == indent) {
        bool item = is_item_line(lines->lines[*pos].text);
        if (item != items) {
            set_error(error, lines->lines[*pos].number, items ? "a list item is expected here" : "a member is expected here, not a list item");
            cJSON_Delete(out);
            return NULL;
        }
        if (!(items ? item_line(lines, pos, indent, out, error) : member_line(lines, pos, indent, out, error))) {
            cJSON_Delete(out);
            return NULL;
        }
    }
    return out;
}

cJSON *text_block_parse(const TextLines *lines, size_t *pos, int indent, TextError *error) {
    return block_at(lines, pos, indent, error);
}

/* ---- documents ------------------------------------------------------------------------------------------------------------------ */

cJSON *text_tree_parse(const char *text, TextKind *kind, TextError *error) {
    TextLines lines;
    if (!text_lines_split(text, &lines, error)) {
        return NULL;
    }
    cJSON *out = NULL;
    TextKind found = TEXT_KIND_JSON;
    size_t pos = 1;
    int first = lines.count ? lines.lines[0].number : 1;
    char version[32] = "", name[64] = "", extra[8] = "";
    int n = 0;
    if (lines.count && lines.lines[0].indent == 0) {
        n = sscanf(lines.lines[0].text, "ursatz-text %31s %63s %7s", version, name, extra);
    }
    if (n != 2) {
        set_error(error, first, "the first line must be ursatz-text 1 <kind>");
        goto done;
    }
    bool digits = version[0] != '\0';
    for (const char *p = version; *p; p++) {
        digits = digits && isdigit((unsigned char)*p);
    }
    if (!digits) {
        set_error(error, first, "the first line must be ursatz-text 1 <kind>");
        goto done;
    }
    if (strcmp(version, "1") != 0) {
        set_error(error, first, "grammar %s is not known", version);
        goto done;
    }
    if (!text_kind_find(name, &found)) {
        set_error(error, first, "kind '%s' is not known", name);
        goto done;
    }
    if (pos < lines.count && lines.lines[pos].indent == 0 && is_item_line(lines.lines[pos].text)) {
        set_error(error, lines.lines[pos].number, "a document holds members, not list items");
        goto done;
    }
    out = block_at(&lines, &pos, 0, error);
    if (out && pos < lines.count) {
        set_error(error, lines.lines[pos].number, "unexpected indent");
        cJSON_Delete(out);
        out = NULL;
    }
    if (out && kind) {
        *kind = found;
    }
done:
    text_lines_free(&lines);
    return out;
}
