#include <ursatz_analyzer/markdown_report.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "pipeline/analysis_document_internal.h"

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} StringBuilder;

static bool sb_init(StringBuilder *sb) {
    sb->capacity = 256;
    sb->length = 0;
    sb->data = malloc(sb->capacity);
    if (!sb->data) {
        return false;
    }
    sb->data[0] = '\0';
    return true;
}

static bool sb_append(StringBuilder *sb, const char *text) {
    size_t text_len = strlen(text);
    size_t needed = sb->length + text_len + 1;
    if (needed > sb->capacity) {
        size_t new_capacity = sb->capacity * 2;
        while (new_capacity < needed) {
            new_capacity *= 2;
        }
        char *grown = realloc(sb->data, new_capacity);
        if (!grown) {
            return false;
        }
        sb->data = grown;
        sb->capacity = new_capacity;
    }
    memcpy(sb->data + sb->length, text, text_len + 1);
    sb->length += text_len;
    return true;
}

/* Formats into the builder, sized by a first vsnprintf pass so no line
   is ever truncated. */
static bool sb_appendf(StringBuilder *sb, const char *format, ...) {
    va_list args;
    va_start(args, format);
    va_list sizing_args;
    va_copy(sizing_args, args);
    int length = vsnprintf(NULL, 0, format, sizing_args);
    va_end(sizing_args);
    if (length < 0) {
        va_end(args);
        return false;
    }

    char *text = malloc((size_t)length + 1);
    if (!text) {
        va_end(args);
        return false;
    }
    vsnprintf(text, (size_t)length + 1, format, args);
    va_end(args);

    bool ok = sb_append(sb, text);
    free(text);
    return ok;
}

/* ---- Document access ---- */

static const char *string_member(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static double number_member(const cJSON *object, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : 0.0;
}

/* ---- Sections ---- */

/* Renders one report section from the whole document and that
   section's own data (never NULL, null, or empty). */
typedef bool (*SectionRenderer)(StringBuilder *sb, const cJSON *document, const cJSON *section);

static bool render_title(StringBuilder *sb, const cJSON *document, const cJSON *metadata) {
    (void)document;
    const char *source_file = string_member(metadata, "source_file");
    return sb_appendf(sb, "# Analysis Report: %s\n\n", source_file ? source_file : "");
}

/* How the key was found, in plain words, from keys[].method. */
static const char *key_method_phrase(const char *method) {
    if (method && strcmp(method, "key_signature") == 0) {
        return "read from the key signature";
    }
    if (method && strcmp(method, "bass_fallback") == 0) {
        return "inferred from the bass of the closing or opening chord";
    }
    if (method && strcmp(method, "pitch_class_scan") == 0) {
        return "inferred from the overall pitch content";
    }
    return "found by an unrecorded method";
}

typedef struct {
    const char *label;
    size_t count;
} LabelCount;

/* Higher count first, then label order, so ties are deterministic. */
static int compare_label_counts(const void *a, const void *b) {
    const LabelCount *left = a;
    const LabelCount *right = b;
    if (left->count != right->count) {
        return left->count > right->count ? -1 : 1;
    }
    return strcmp(left->label, right->label);
}

static const char *plural(size_t count, const char *singular, const char *plural_form) {
    return count == 1 ? singular : plural_form;
}

/* Matched chord labels ("no match" excluded) with their counts, sorted
   by compare_label_counts. *out_counts is NULL when there are none. */
static bool count_chord_labels(const cJSON *chords, LabelCount **out_counts, size_t *out_count) {
    *out_counts = NULL;
    *out_count = 0;
    size_t chord_count = (size_t)cJSON_GetArraySize(chords);
    if (chord_count == 0) {
        return true;
    }
    LabelCount *counts = malloc(sizeof(LabelCount) * chord_count);
    if (!counts) {
        return false;
    }
    size_t distinct = 0;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        const char *label = string_member(chord, "label");
        if (!label || strcmp(label, "no match") == 0) {
            continue;
        }
        size_t i = 0;
        while (i < distinct && strcmp(counts[i].label, label) != 0) {
            i++;
        }
        if (i == distinct) {
            counts[distinct++] = (LabelCount){label, 0};
        }
        counts[i].count++;
    }
    qsort(counts, distinct, sizeof(LabelCount), compare_label_counts);
    *out_counts = counts;
    *out_count = distinct;
    return true;
}

/* A few plain-language lines for a composer: the piece, its key, how
   much of it resolved to chords, and the chords it leans on most. */
static bool render_summary(StringBuilder *sb, const cJSON *document, const cJSON *metadata) {
    size_t note_count = (size_t)number_member(metadata, "note_count");
    const char *source_file = string_member(metadata, "source_file");
    bool ok = sb_append(sb, "## Summary\n\n") &&
              sb_appendf(sb, "- **File:** %s, %zu %s.\n", source_file ? source_file : "", note_count,
                         plural(note_count, "note", "notes"));

    const cJSON *key = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "keys"), 0);
    const char *key_label = string_member(key, "label");
    if (key_label) {
        ok = ok && sb_appendf(sb, "- **Key:** %s, %s.\n", key_label,
                              key_method_phrase(string_member(key, "method")));
    } else {
        ok = ok && sb_append(sb, "- **Key:** No key was found.\n");
    }

    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(document, "chords");
    size_t chord_count = (size_t)cJSON_GetArraySize(chords);
    LabelCount *counts = NULL;
    size_t distinct = 0;
    ok = ok && count_chord_labels(chords, &counts, &distinct);
    if (ok && chord_count == 0) {
        ok = sb_append(sb, "- **Chords:** No chord events were found.\n");
    } else if (ok) {
        size_t matched = 0;
        for (size_t i = 0; i < distinct; i++) {
            matched += counts[i].count;
        }
        /* Whole percent, but never "0%" for some matches or "100%" for some misses. */
        size_t percent = (matched * 100 + chord_count / 2) / chord_count;
        char percent_text[8];
        if (matched > 0 && percent == 0) {
            snprintf(percent_text, sizeof(percent_text), "<1%%");
        } else if (matched < chord_count && percent == 100) {
            snprintf(percent_text, sizeof(percent_text), ">99%%");
        } else {
            snprintf(percent_text, sizeof(percent_text), "%zu%%", percent);
        }
        ok = sb_appendf(sb, "- **Chords:** %zu chord %s; %zu matched a chord (%s), %zu did not.\n",
                        chord_count, plural(chord_count, "event", "events"), matched, percent_text,
                        chord_count - matched);
    }
    if (ok && distinct > 0) {
        ok = sb_append(sb, "- **Most frequent chords:** ");
        for (size_t i = 0; i < distinct && i < 3 && ok; i++) {
            ok = sb_appendf(sb, "%s%s (%zu)", i > 0 ? ", " : "", counts[i].label, counts[i].count);
        }
        ok = ok && sb_append(sb, ".\n");
    }
    free(counts);
    return ok && sb_append(sb, "\n");
}

/* "1/3" for a rational, "2" when it is whole. */
static bool append_rational(StringBuilder *sb, const cJSON *rational) {
    long long num = (long long)number_member(rational, "num");
    long long den = (long long)number_member(rational, "den");
    if (den <= 1) {
        return sb_appendf(sb, "%lld", num);
    }
    return sb_appendf(sb, "%lld/%lld", num, den);
}

static bool append_time_signature(StringBuilder *sb, const cJSON *time_signature) {
    return sb_appendf(sb, "%d/%d", (int)number_member(time_signature, "numerator"),
                      (int)number_member(time_signature, "denominator"));
}

/* The time signature, how many measures there are, a pickup if the piece
   has one, and any later change of signature. A meter the file does not
   declare is shown as inferred. */
static bool render_meter(StringBuilder *sb, const cJSON *document, const cJSON *meter) {
    (void)document;
    const cJSON *time_signature = cJSON_GetObjectItemCaseSensitive(meter, "time_signature");
    const char *source = string_member(meter, "source");
    bool inferred = source && strcmp(source, "inferred") == 0;

    bool ok = sb_append(sb, "## Meter\n\n") && sb_append(sb, "- **Time signature:** ") &&
              append_time_signature(sb, time_signature);
    if (inferred) {
        ok = ok && sb_append(sb, " (inferred: the file has no time signature, so 4/4 is assumed)");
    }
    ok = ok && sb_append(sb, "\n");

    const cJSON *last_measure = cJSON_GetObjectItemCaseSensitive(meter, "last_measure");
    if (ok && cJSON_IsNumber(last_measure) && last_measure->valuedouble >= 1) {
        size_t measures = (size_t)last_measure->valuedouble;
        ok = sb_appendf(sb, "- **Measures:** %zu %s.\n", measures, plural(measures, "measure", "measures"));
    } else if (ok) {
        ok = sb_append(sb, "- **Measures:** None.\n");
    }

    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(meter, "pickup");
    if (ok && cJSON_IsObject(pickup)) {
        const cJSON *beats = cJSON_GetObjectItemCaseSensitive(pickup, "beats");
        ok = sb_append(sb, "- **Pickup:** ");
        if (ok && cJSON_IsObject(beats)) {
            ok = append_rational(sb, beats) && sb_append(sb, " beat(s), numbered measure 0.\n");
        } else if (ok) {
            ok = sb_append(sb, "yes, numbered measure 0.\n");
        }
    } else if (ok) {
        ok = sb_append(sb, "- **Pickup:** None.\n");
    }

    const cJSON *changes = cJSON_GetObjectItemCaseSensitive(meter, "changes");
    if (ok && cJSON_GetArraySize(changes) > 0) {
        ok = sb_append(sb, "- **Meter changes:** ");
        const cJSON *change;
        cJSON_ArrayForEach(change, changes) {
            if (change != changes->child) {
                ok = ok && sb_append(sb, ", ");
            }
            ok = ok && append_time_signature(sb, cJSON_GetObjectItemCaseSensitive(change, "time_signature")) &&
                 sb_appendf(sb, " at m. %d", (int)number_member(change, "measure"));
        }
        ok = ok && sb_append(sb, ".\n");
    }
    return ok && sb_append(sb, "\n");
}

static bool render_key(StringBuilder *sb, const cJSON *document, const cJSON *keys) {
    (void)document;
    const char *label = string_member(cJSON_GetArrayItem(keys, 0), "label");
    return sb_appendf(sb, "**Detected key:** %s\n\n", label ? label : "");
}

static bool render_chords(StringBuilder *sb, const cJSON *document, const cJSON *chords) {
    (void)document;
    bool ok = sb_append(sb, "## Chord Regions\n\n") &&
              sb_append(sb, "Times are beats from the start of the file.\n\n");
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        if (!ok) {
            break;
        }
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(chord, "span"), "start");
        double beats = 4.0 * number_member(start, "num") / number_member(start, "den");
        const char *label = string_member(chord, "label");
        ok = sb_appendf(sb, "- Beat %.2f: %s\n", beats, label ? label : "");
    }
    return ok;
}

/* Report sections in document section order. Adding a section means
   adding one row and one render function. */
static const struct {
    const char *document_key;
    SectionRenderer render;
} SECTIONS[] = {
    {"metadata", render_title},
    {"metadata", render_summary},
    {"meter", render_meter},
    {"keys", render_key},
    {"chords", render_chords},
};

static bool section_is_empty(const cJSON *section) {
    if (!section || cJSON_IsNull(section)) {
        return true;
    }
    return (cJSON_IsArray(section) || cJSON_IsObject(section)) && section->child == NULL;
}

char *markdown_report_render(const AnalysisDocument *document) {
    StringBuilder sb;
    if (!sb_init(&sb)) {
        return NULL;
    }

    const cJSON *root = document->root;
    bool ok = true;
    for (size_t i = 0; i < sizeof(SECTIONS) / sizeof(SECTIONS[0]) && ok; i++) {
        const cJSON *section = cJSON_GetObjectItemCaseSensitive(root, SECTIONS[i].document_key);
        if (!section_is_empty(section)) {
            ok = SECTIONS[i].render(&sb, root, section);
        }
    }

    if (!ok) {
        free(sb.data);
        return NULL;
    }
    return sb.data;
}
