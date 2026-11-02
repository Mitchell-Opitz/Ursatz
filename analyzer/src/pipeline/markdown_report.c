#include <ursatz_analyzer/markdown_report.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "pipeline/analysis_document_internal.h"
#include "util/json_util.h"

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

static double number_or_zero(const cJSON *object, const char *key) {
    const cJSON *item = json_member(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : 0.0;
}

/* ---- Sections ---- */

/* Renders one report section from the whole document and that
   section's own data (never NULL, null, or empty). */
typedef bool (*SectionRenderer)(StringBuilder *sb, const cJSON *document, const cJSON *section);

static bool render_title(StringBuilder *sb, const cJSON *document, const cJSON *metadata) {
    (void)document;
    const char *source_file = json_string_member(metadata, "source_file");
    return sb_appendf(sb, "# Analysis Report: %s\n\n", source_file ? source_file : "");
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
        const char *label = json_string_member(chord, "label");
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

static const char *knowledge_phrase(const char *knowledge) {
    if (knowledge && strcmp(knowledge, "notated") == 0) {
        return "notated";
    }
    if (knowledge && strcmp(knowledge, "embedded") == 0) {
        return "embedded in the file";
    }
    return "inferred";
}

/* The input type, and whether the meter, key and pitch spelling were notated, embedded in the file, or inferred. */
static bool append_input_line(StringBuilder *sb, const cJSON *input) {
    if (!cJSON_IsObject(input)) {
        return true;
    }
    const char *format = json_string_member(input, "format");
    bool score = format && strcmp(format, "musicxml") == 0;
    size_t part_count = (size_t)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(input, "parts"));
    const char *spelling = json_string_member(input, "spelling");
    bool ok = sb_appendf(sb, "- **Input:** %s", score ? "MusicXML score" : "MIDI file");
    if (score) {
        ok = ok && sb_appendf(sb, ", %zu %s merged into one texture", part_count, plural(part_count, "part", "parts"));
    }
    return ok && sb_appendf(sb, ". Meter: %s. Key signature: %s. Pitch spelling: %s.\n",
                            knowledge_phrase(json_string_member(input, "meter")),
                            knowledge_phrase(json_string_member(input, "key")),
                            spelling && strcmp(spelling, "notated") == 0 ? "notated" : "by rule");
}

/* A few plain-language lines for a composer: the piece, its key, how
   much of it resolved to chords, and the chords it leans on most. */
static bool render_summary(StringBuilder *sb, const cJSON *document, const cJSON *metadata) {
    size_t note_count = (size_t)number_or_zero(metadata, "note_count");
    const char *source_file = json_string_member(metadata, "source_file");
    bool ok = sb_append(sb, "## Summary\n\n") &&
              sb_appendf(sb, "- **File:** %s, %zu %s.\n", source_file ? source_file : "", note_count,
                         plural(note_count, "note", "notes"));

    ok = ok && append_input_line(sb, cJSON_GetObjectItemCaseSensitive(metadata, "input"));

    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(document, "keys");
    const char *home_label = json_string_member(cJSON_GetObjectItemCaseSensitive(keys, "home_key"), "label");
    size_t region_count = (size_t)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(keys, "regions"));
    if (home_label && region_count == 1) {
        ok = ok && sb_appendf(sb, "- **Key:** %s, the only key region.\n", home_label);
    } else if (home_label) {
        ok = ok && sb_appendf(sb, "- **Key:** %s, the key with the most time across %zu key regions.\n", home_label,
                              region_count);
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
    long long num = (long long)number_or_zero(rational, "num");
    long long den = (long long)number_or_zero(rational, "den");
    if (den <= 1) {
        return sb_appendf(sb, "%lld", num);
    }
    return sb_appendf(sb, "%lld/%lld", num, den);
}

static bool append_time_signature(StringBuilder *sb, const cJSON *time_signature) {
    return sb_appendf(sb, "%d/%d", (int)number_or_zero(time_signature, "numerator"),
                      (int)number_or_zero(time_signature, "denominator"));
}

/* The time signature, how many measures there are, a pickup if the piece
   has one, and any later change of signature. A meter the file does not
   declare is shown as inferred. */
static bool render_meter(StringBuilder *sb, const cJSON *document, const cJSON *meter) {
    (void)document;
    const cJSON *time_signature = cJSON_GetObjectItemCaseSensitive(meter, "time_signature");
    const char *source = json_string_member(meter, "source");
    bool inferred = source && strcmp(source, "inferred") == 0;

    bool ok = sb_append(sb, "## Meter\n\n") && sb_append(sb, "- **Time signature:** ") &&
              append_time_signature(sb, time_signature);
    if (inferred) {
        ok = ok && sb_append(sb, " (inferred: the file has no time signature, so 4/4 is assumed)");
    } else if (source && strcmp(source, "notated") == 0) {
        ok = ok && sb_append(sb, " (notated)");
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
                 sb_appendf(sb, " at m. %d", (int)number_or_zero(change, "measure"));
        }
        ok = ok && sb_append(sb, ".\n");
    }
    return ok && sb_append(sb, "\n");
}

/* "m. 3, beat 2.5": the beat is the 1-based beat plus its fraction,
   shown to at most two places. A position that could not be read shows
   as "unknown position". */
static bool append_position(StringBuilder *sb, const cJSON *position) {
    if (!cJSON_IsObject(position)) {
        return sb_append(sb, "unknown position");
    }
    const cJSON *subdivision = cJSON_GetObjectItemCaseSensitive(position, "subdivision");
    double beat = number_or_zero(position, "beat") +
                  number_or_zero(subdivision, "num") / number_or_zero(subdivision, "den");
    char beat_text[32];
    snprintf(beat_text, sizeof(beat_text), "%.2f", beat);
    size_t length = strlen(beat_text);
    while (length > 0 && beat_text[length - 1] == '0') {
        beat_text[--length] = '\0';
    }
    if (length > 0 && beat_text[length - 1] == '.') {
        beat_text[--length] = '\0';
    }
    return sb_appendf(sb, "m. %d, beat %s", (int)number_or_zero(position, "measure"), beat_text);
}

static const char *region_source_phrase(const char *source) {
    if (source && strcmp(source, "key_signature") == 0) {
        return "key signature";
    }
    return "profile";
}

static bool append_key_region_row(StringBuilder *sb, const cJSON *region) {
    const cJSON *location = cJSON_GetObjectItemCaseSensitive(region, "location");
    const cJSON *uncertainty = cJSON_GetObjectItemCaseSensitive(region, "uncertainty");
    char uncertainty_text[32] = "-";
    if (cJSON_IsNumber(uncertainty)) {
        snprintf(uncertainty_text, sizeof(uncertainty_text), "%.2f", uncertainty->valuedouble);
    }
    const char *label = json_string_member(region, "label");
    return sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
           sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
           sb_appendf(sb, " | %s | %s | %s |\n", label ? label : "", uncertainty_text,
                      region_source_phrase(json_string_member(region, "source")));
}

/* The key regions with the home key, the opening and closing keys, any notated key signatures, and the notes
   on how the notated and detected keys compare. */
static bool render_key(StringBuilder *sb, const cJSON *document, const cJSON *keys) {
    const cJSON *regions = cJSON_GetObjectItemCaseSensitive(keys, "regions");
    if (cJSON_GetArraySize(regions) == 0) {
        return true;
    }
    const cJSON *home = cJSON_GetObjectItemCaseSensitive(keys, "home_key");
    const cJSON *opening = cJSON_GetArrayItem(regions, (int)number_or_zero(keys, "opening_region"));
    const cJSON *closing = cJSON_GetArrayItem(regions, (int)number_or_zero(keys, "closing_region"));

    bool ok = sb_append(sb, "## Key regions\n\n") &&
              sb_append(sb, "Positions are measure and beat. Uncertainty is the library's, from 0 (clearly this key) "
                            "to 1 (another key fits as well or better). The home key is the key with the most time across its regions.\n\n") &&
              sb_appendf(sb, "- **Home key:** %s.\n", json_string_member(home, "label")) &&
              sb_appendf(sb, "- **Opening key:** %s.\n", json_string_member(opening, "label")) &&
              sb_appendf(sb, "- **Closing key:** %s.\n", json_string_member(closing, "label"));

    const cJSON *signatures =
        cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(document, "metadata"), "key_signatures");
    if (ok && cJSON_GetArraySize(signatures) > 0) {
        ok = sb_append(sb, "- **Notated key signatures:** ");
        const cJSON *signature;
        cJSON_ArrayForEach(signature, signatures) {
            if (signature != signatures->child) {
                ok = ok && sb_append(sb, ", ");
            }
            ok = ok &&
                 sb_appendf(sb, "%s or %s at ", json_string_member(signature, "major_key"),
                            json_string_member(signature, "minor_key")) &&
                 append_position(sb, cJSON_GetObjectItemCaseSensitive(signature, "location"));
        }
        ok = ok && sb_append(sb, ".\n");
    }

    ok = ok && sb_append(sb, "\n| Position | Key | Uncertainty | Set by |\n| --- | --- | --- | --- |\n");
    const cJSON *region;
    cJSON_ArrayForEach(region, regions) {
        if (!ok) {
            break;
        }
        ok = append_key_region_row(sb, region);
    }

    const cJSON *notes = cJSON_GetObjectItemCaseSensitive(document, "notes");
    const cJSON *note;
    cJSON_ArrayForEach(note, notes) {
        const char *kind = json_string_member(note, "kind");
        if (ok && kind && strcmp(kind, "key_signature_disagreement") == 0) {
            ok = sb_appendf(sb, "\n**Note:** %s\n", json_string_member(note, "message"));
        }
    }
    return ok && sb_append(sb, "\n");
}

static const char *const INVERSION_NAMES[] = {"root position", "first", "second", "third"};

/* "m. 3, beat 1 to m. 9, beat 1" for a claim's location. */
static bool append_location(StringBuilder *sb, const cJSON *claim) {
    const cJSON *location = cJSON_GetObjectItemCaseSensitive(claim, "location");
    return append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) && sb_append(sb, " to ") &&
           append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end"));
}

static bool render_tonicizations(StringBuilder *sb, const cJSON *document, const cJSON *tonicizations) {
    (void)document;
    bool ok = sb_append(sb, "## Tonicizations\n\n") &&
              sb_appendf(sb,
                         "%d brief %s of another key that return%s to the key before. Span is the stretch in the "
                         "target key. The library tells a tonicization from a modulation by how long the new key "
                         "lasts, whether it returns, and whether a cadence arrives in it; without a cadence "
                         "it is a tonicization however long.\n\n",
                         cJSON_GetArraySize(tonicizations),
                         cJSON_GetArraySize(tonicizations) == 1 ? "excursion" : "excursions",
                         cJSON_GetArraySize(tonicizations) == 1 ? "s" : "") &&
              sb_append(sb, "| Span | In | Target |\n| --- | --- | --- |\n");
    const cJSON *item;
    cJSON_ArrayForEach(item, tonicizations) {
        if (!ok) {
            break;
        }
        ok = sb_append(sb, "| ") && append_location(sb, item) &&
             sb_appendf(sb, " | %s | %s |\n", json_string_member(item, "from_key"), json_string_member(item, "to_key"));
    }
    return ok && sb_append(sb, "\n");
}

static bool render_modulations(StringBuilder *sb, const cJSON *document, const cJSON *modulations) {
    (void)document;
    bool ok = sb_append(sb, "## Modulations\n\n") &&
              sb_append(sb, "A modulation is a lasting new key. Position is where the new key arrives. Type is "
                            "pivot when the library found a pivot chord and direct when it did not. A return goes "
                            "back to the key before an excursion. The library gives the change's extent from the "
                            "length of the new key, whether it returns, and whether a cadence arrives in it.\n\n") &&
              sb_append(sb, "| Position | From | To | Extent | Type | Return |\n| --- | --- | --- | --- | --- | --- |\n");
    const cJSON *item;
    cJSON_ArrayForEach(item, modulations) {
        if (!ok) {
            break;
        }
        const cJSON *location = cJSON_GetObjectItemCaseSensitive(item, "location");
        ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_appendf(sb, " | %s | %s | %s | %s | %s |\n", json_string_member(item, "from_key"),
                        json_string_member(item, "to_key"), json_string_member(item, "extent"), json_string_member(item, "type"),
                        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item, "is_return")) ? "yes" : "no");
    }
    return ok && sb_append(sb, "\n");
}

static bool render_pivot_chords(StringBuilder *sb, const cJSON *document, const cJSON *pivots) {
    (void)document;
    bool ok = sb_append(sb, "## Pivot chords\n\n") &&
              sb_append(sb, "The chord at a modulation that is diatonic in both the old and the new key, named by "
                            "root and quality, with no Roman numeral. Modulation is its row in the Modulations "
                            "table, counting from 1.\n\n") &&
              sb_append(sb, "| Position | Chord | Bass | Inversion | From | To | Modulation |\n"
                            "| --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *item;
    cJSON_ArrayForEach(item, pivots) {
        if (!ok) {
            break;
        }
        const cJSON *inversion = cJSON_GetObjectItemCaseSensitive(item, "inversion");
        const char *inversion_text = "-";
        if (cJSON_IsNumber(inversion) && inversion->valueint >= 0 && inversion->valueint < 4) {
            inversion_text = INVERSION_NAMES[inversion->valueint];
        }
        ok = sb_append(sb, "| ") && append_location(sb, item) &&
             sb_appendf(sb, " | %s | %s | %s | %s | %s | %d |\n", json_string_member(item, "label"),
                        json_string_member(item, "bass"), inversion_text, json_string_member(item, "from_key"),
                        json_string_member(item, "to_key"), (int)number_or_zero(item, "modulation_index") + 1);
    }
    return ok && sb_append(sb, "\n");
}

/* The runner-up is shown on a chord's row only when the library's scores for the two are this close
   or closer: best score minus runner-up score below this. Documented in README.md. */
#define RUNNER_UP_MARGIN_THRESHOLD 0.10

/* A number with up to two decimals, trailing zeros trimmed ("1.5", "3"). */
static void format_number(double value, char *buffer, size_t size) {
    snprintf(buffer, size, "%.2f", value);
    size_t length = strlen(buffer);
    while (length > 0 && buffer[length - 1] == '0') {
        buffer[--length] = '\0';
    }
    if (length > 0 && buffer[length - 1] == '.') {
        buffer[--length] = '\0';
    }
}

static double rational_member(const cJSON *object, const char *key) {
    const cJSON *rational = cJSON_GetObjectItemCaseSensitive(object, key);
    double denominator = number_or_zero(rational, "den");
    return denominator == 0.0 ? 0.0 : number_or_zero(rational, "num") / denominator;
}

/* The chord's Roman numeral cell. The key region is named on the first labelled chord and wherever it changes,
   as "V (in G major)"; a chord the library gives no numeral is "unlabelled" with the reason. last_region is the
   region of the last numeral that named its key, -1 before the first. */
static bool append_numeral_cell(StringBuilder *sb, const cJSON *chord, long *last_region) {
    const cJSON *roman = cJSON_GetObjectItemCaseSensitive(chord, "roman_numeral");
    const char *numeral = json_string_member(roman, "label");
    if (!numeral) {
        const char *reason = json_string_member(roman, "unlabelled_reason");
        return sb_appendf(sb, "unlabelled: %s | ", reason ? reason : "no reason given");
    }
    const cJSON *region = cJSON_GetObjectItemCaseSensitive(roman, "key_region");
    const char *key = json_string_member(roman, "key");
    if (cJSON_IsNumber(region) && key && (long)region->valuedouble != *last_region) {
        *last_region = (long)region->valuedouble;
        return sb_appendf(sb, "%s (in %s) | ", numeral, key);
    }
    return sb_appendf(sb, "%s | ", numeral);
}

/* The chord's Function and Six-four cells: the library's function ("dominant of 5" for an applied chord, with the
   strength of a predominant) and six-four type, or "-" where it gives none. */
static bool append_function_cells(StringBuilder *sb, const cJSON *chord) {
    const cJSON *function = cJSON_GetObjectItemCaseSensitive(chord, "function");
    const char *name = json_string_member(function, "name");
    bool ok;
    if (!name) {
        ok = sb_append(sb, "- | ");
    } else {
        const cJSON *applied_to = cJSON_GetObjectItemCaseSensitive(function, "applied_to");
        const char *strength = json_string_member(function, "predominant");
        ok = sb_append(sb, name);
        if (ok && cJSON_IsNumber(applied_to)) {
            ok = sb_appendf(sb, " of %d", (int)applied_to->valuedouble);
        }
        if (ok && strength) {
            ok = sb_appendf(sb, " (%s)", strength);
        }
        ok = ok && sb_append(sb, " | ");
    }
    const char *six_four = json_string_member(cJSON_GetObjectItemCaseSensitive(chord, "six_four"), "type");
    return ok && sb_appendf(sb, "%s | ", six_four ? six_four : "-");
}

static bool append_chord_row(StringBuilder *sb, const cJSON *chord, long *last_region) {
    const cJSON *location = cJSON_GetObjectItemCaseSensitive(chord, "location");
    const char *label = json_string_member(chord, "label");
    const char *root = json_string_member(chord, "root");
    bool matched = root != NULL;

    bool ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
              sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
              sb_appendf(sb, " | %s | ", label ? label : "") && append_numeral_cell(sb, chord, last_region) &&
              append_function_cells(sb, chord);
    if (!ok) {
        return false;
    }

    if (!matched) {
        return sb_append(sb, "- | - | - | - | - | - | - |\n");
    }

    const cJSON *inversion = cJSON_GetObjectItemCaseSensitive(chord, "inversion");
    const char *inversion_text = "-";
    if (cJSON_IsNumber(inversion) && inversion->valueint >= 0 && inversion->valueint < 4) {
        inversion_text = INVERSION_NAMES[inversion->valueint];
    }
    const cJSON *duration = cJSON_GetObjectItemCaseSensitive(chord, "duration");
    const cJSON *beats = cJSON_GetObjectItemCaseSensitive(duration, "beats");
    char beats_text[32] = "-";
    if (cJSON_IsObject(beats)) {
        format_number(rational_member(duration, "beats"), beats_text, sizeof(beats_text));
    }
    const cJSON *uncertainty = cJSON_GetObjectItemCaseSensitive(chord, "uncertainty");
    char confidence_text[32] = "-";
    if (cJSON_IsNumber(uncertainty)) {
        snprintf(confidence_text, sizeof(confidence_text), "%.2f", uncertainty->valuedouble);
    }

    ok = sb_appendf(sb, "%s | %s | %s | %s | %s | %s | ", root, json_string_member(chord, "quality"),
                    json_string_member(chord, "bass"), inversion_text, beats_text, confidence_text);
    if (!ok) {
        return false;
    }

    const cJSON *runner_up = cJSON_GetObjectItemCaseSensitive(chord, "runner_up");
    double margin = number_or_zero(chord, "score") - number_or_zero(runner_up, "score");
    if (cJSON_IsObject(runner_up) && margin < RUNNER_UP_MARGIN_THRESHOLD) {
        char score_text[32];
        snprintf(score_text, sizeof(score_text), "%.2f", number_or_zero(runner_up, "score"));
        return sb_appendf(sb, "%s (%s %s, score %s) |\n", json_string_member(runner_up, "label"),
                          json_string_member(runner_up, "root"), json_string_member(runner_up, "quality"), score_text);
    }
    return sb_append(sb, "- |\n");
}

static bool render_chords(StringBuilder *sb, const cJSON *document, const cJSON *chords) {
    bool ok = sb_append(sb, "## Chords\n\n") && sb_append(sb, "Positions are measure and beat.");
    if (ok && cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(
                  cJSON_GetObjectItemCaseSensitive(document, "meter"), "pickup"))) {
        ok = sb_append(sb, " The pickup is measure 0.");
    }
    ok = ok &&
         sb_appendf(sb,
                    " Numeral is the Roman numeral in the local key region, which is named where it changes. "
                    "Function is the library's harmonic function of the chord: an applied chord is a dominant "
                    "toward its target degree, and a predominant is strong (IV, ii) or weak (iii, vi). Six-four "
                    "is the library's type for a chord in second inversion. Duration is in beats. Confidence is "
                    "the library's confidence in the chord. The runner-up is shown only when its score is "
                    "within %.2f of the best chord's.\n\n",
                    RUNNER_UP_MARGIN_THRESHOLD) &&
         sb_append(sb, "| Position | Chord | Numeral | Function | Six-four | Root | Quality | Bass | Inversion | Beats | "
                       "Confidence | Runner-up |\n"
                       "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
    long last_region = -1;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        if (!ok) {
            break;
        }
        ok = append_chord_row(sb, chord, &last_region);
    }
    return ok;
}

/* The applied chords, one row each, with the check against the tonicization claims. A disagreement is stated, not
   resolved. Nothing is written when no chord is applied. */
static bool render_applied_chords(StringBuilder *sb, const cJSON *document, const cJSON *chords) {
    (void)document;
    bool any = false;
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        any = any || cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(
                         cJSON_GetObjectItemCaseSensitive(chord, "roman_numeral"), "applied"));
    }
    if (!any) {
        return true;
    }
    bool ok = sb_append(sb, "\n## Applied chords\n\n") &&
              sb_append(sb, "Chords the library reads as applied (secondary) chords: the chord as it reads in the "
                            "tonicized key, the chord it tonicizes and that chord's tonic. The last column checks "
                            "each against the tonicization claims above for the same span; where they disagree both "
                            "are shown and neither is changed.\n\n") &&
              sb_append(sb, "| Position | Applied chord | Target | Resolution | Tonicization |\n"
                            "| --- | --- | --- | --- | --- |\n");
    cJSON_ArrayForEach(chord, chords) {
        const cJSON *applied =
            cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(chord, "roman_numeral"), "applied");
        if (!ok) {
            break;
        }
        if (!cJSON_IsObject(applied)) {
            continue;
        }
        const cJSON *location = cJSON_GetObjectItemCaseSensitive(chord, "location");
        const char *agreement = json_string_member(applied, "tonicization_agreement");
        const char *tonicization_key = json_string_member(applied, "tonicization_key");
        const char *target_tonic = json_string_member(applied, "target_tonic");
        const char *kind = json_string_member(applied, "kind");
        const char *resolution = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(applied, "deceptive"))
                                     ? "deceptive"
                                     : "to the target";
        ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
             sb_appendf(sb, " | %s (%s) | %s (%s) | %s | ", json_string_member(applied, "numeral"),
                        kind ? kind : "-", json_string_member(applied, "target_numeral"),
                        target_tonic ? target_tonic : "-", resolution);
        if (!ok) {
            break;
        }
        if (agreement && strcmp(agreement, "agrees") == 0) {
            ok = sb_appendf(sb, "agrees: tonicization of %s |\n", tonicization_key ? tonicization_key : "-");
        } else if (agreement && strcmp(agreement, "target_differs") == 0) {
            ok = sb_appendf(sb, "DISAGREES: the tonicization is of %s |\n", tonicization_key ? tonicization_key : "-");
        } else {
            ok = sb_append(sb, "DISAGREES: no tonicization is reported here |\n");
        }
    }
    return ok;
}

/* One voice's cells in the outer-voices table: its pitch and its degree, or what the library made of its
   absence. Appends " pitch | degree |". */
static bool append_outer_voice_cells(StringBuilder *sb, const cJSON *voice) {
    const char *status = json_string_member(voice, "status");
    const char *pitch = json_string_member(voice, "pitch");
    const char *degree = json_string_member(voice, "degree");
    if (pitch) {
        return sb_appendf(sb, " %s | %s |", pitch, degree ? degree : "-");
    }
    const char *text = "rest";
    if (status && strcmp(status, "bridged") == 0) {
        text = "rest (bridged)";
    } else if (status && strcmp(status, "guarded") == 0) {
        text = "rest (register guard)";
    }
    return sb_appendf(sb, " %s | - |", text);
}

/* The outer voices, one row per chord: the table is separate from the chord table to keep that one readable. */
static bool render_outer_voices(StringBuilder *sb, const cJSON *document, const cJSON *chords) {
    (void)document;
    bool ok = sb_append(sb, "## Outer voices\n\n") &&
              sb_append(sb, "The highest (soprano) and lowest (bass) sounding note of each chord's span, with "
                            "its scale degree in that chord's local key. A flat or sharp sign marks a chromatic "
                            "alteration. A degree is \"-\" where the span lies in no key region. A rest means "
                            "the library found no note for that line in the span, a bridged one a short gap "
                            "inside the line, and a register guard a note the library dropped rather than "
                            "adopt an accompaniment note. Degrees carry the key region's uncertainty in the "
                            "analysis document.\n\n") &&
              sb_append(sb, "| Position | Key | Soprano | Degree | Bass | Degree |\n"
                            "| --- | --- | --- | --- | --- | --- |\n");
    const cJSON *chord;
    cJSON_ArrayForEach(chord, chords) {
        if (!ok) {
            break;
        }
        const cJSON *location = cJSON_GetObjectItemCaseSensitive(chord, "location");
        const cJSON *outer = cJSON_GetObjectItemCaseSensitive(chord, "outer");
        ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
             sb_appendf(sb, " | %s |", json_string_member(chord, "key_label")) &&
             append_outer_voice_cells(sb, cJSON_GetObjectItemCaseSensitive(outer, "soprano")) &&
             append_outer_voice_cells(sb, cJSON_GetObjectItemCaseSensitive(outer, "bass")) && sb_append(sb, "\n");
    }
    return ok;
}

/* One table of vocabulary entries sharing a scope, group and level. */
static bool append_vocabulary_table(StringBuilder *sb, const cJSON *entries, const char *scope_key, const char *group,
                                    const char *level, const char *first_column) {
    bool any = false;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, entries) {
        const char *entry_group = json_string_member(entry, "group");
        const char *entry_level = json_string_member(entry, "level");
        const cJSON *key_region = cJSON_GetObjectItemCaseSensitive(entry, "key_region");
        bool in_scope = scope_key ? (cJSON_IsNumber(key_region) && (long)key_region->valuedouble == atol(scope_key))
                                  : !cJSON_IsNumber(key_region);
        if (!in_scope || !entry_group || strcmp(entry_group, group) != 0 || strcmp(entry_level, level) != 0) {
            continue;
        }
        if (!any) {
            if (!sb_appendf(sb, "| %s | Count | Runs | Beats | Share | First |\n| --- | --- | --- | --- | --- | --- |\n",
                            first_column)) {
                return false;
            }
            any = true;
        }
        const cJSON *duration = cJSON_GetObjectItemCaseSensitive(entry, "duration");
        char beats_text[32] = "-";
        if (cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(duration, "beats"))) {
            format_number(rational_member(duration, "beats"), beats_text, sizeof(beats_text));
        }
        const cJSON *first = cJSON_GetObjectItemCaseSensitive(entry, "first");
        if (!(sb_appendf(sb, "| %s | %d | %d | %s | %.1f%% | ", json_string_member(entry, "numeral"),
                         (int)number_or_zero(entry, "count"), (int)number_or_zero(entry, "runs"), beats_text,
                         number_or_zero(entry, "share") * 100.0) &&
              append_position(sb, cJSON_GetObjectItemCaseSensitive(first, "position")) && sb_append(sb, " |\n"))) {
            return false;
        }
    }
    return !any || sb_append(sb, "\n");
}

/* The groups and both levels for one scope. */
static bool append_vocabulary_scope(StringBuilder *sb, const cJSON *entries, const char *scope_key) {
    static const struct {
        const char *group;
        const char *title;
    } GROUPS[] = {{"diatonic", "Diatonic chords"}, {"applied", "Applied chords"}, {"mixture", "Borrowed chords"}};
    for (size_t g = 0; g < sizeof(GROUPS) / sizeof(GROUPS[0]); g++) {
        bool present = false;
        const cJSON *entry;
        cJSON_ArrayForEach(entry, entries) {
            const cJSON *key_region = cJSON_GetObjectItemCaseSensitive(entry, "key_region");
            const char *group = json_string_member(entry, "group");
            if (group && strcmp(group, GROUPS[g].group) == 0 &&
                (scope_key ? (cJSON_IsNumber(key_region) && (long)key_region->valuedouble == atol(scope_key))
                           : !cJSON_IsNumber(key_region))) {
                present = true;
            }
        }
        if (!present) {
            continue;
        }
        if (!(sb_appendf(sb, "**%s**\n\n", GROUPS[g].title) &&
              append_vocabulary_table(sb, entries, scope_key, GROUPS[g].group, "numeral", "Numeral") &&
              sb_append(sb, "By base numeral (inversions together):\n\n") &&
              append_vocabulary_table(sb, entries, scope_key, GROUPS[g].group, "base_numeral", "Base numeral"))) {
            return false;
        }
    }
    return true;
}

static bool render_chord_vocabulary(StringBuilder *sb, const cJSON *document, const cJSON *entries) {
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(document, "keys");
    const cJSON *regions = cJSON_GetObjectItemCaseSensitive(keys, "regions");
    bool ok = sb_append(sb, "\n## Chord vocabulary\n\n") &&
              sb_append(sb, "Each distinct Roman numeral of the chords the library numbered, for the whole piece and "
                            "for each key region, sorted by count, then total length, then first appearance. Count "
                            "is the number of chord rows with that numeral (every row in the Chords table is one). "
                            "Runs counts a stretch of adjacent identical numerals in one key region once. Beats is "
                            "the total length, Share its fraction of the scope's numbered chord length, and First "
                            "where it first appears. Applied chords are listed on their own. Chords without a "
                            "numeral are not counted.\n\n### Overall\n\n") &&
              append_vocabulary_scope(sb, entries, NULL);
    if (cJSON_GetArraySize(regions) < 2) {
        return ok;
    }
    int index = 0;
    const cJSON *region;
    cJSON_ArrayForEach(region, regions) {
        char scope_key[16];
        snprintf(scope_key, sizeof(scope_key), "%d", index);
        bool present = false;
        const cJSON *entry;
        cJSON_ArrayForEach(entry, entries) {
            const cJSON *key_region = cJSON_GetObjectItemCaseSensitive(entry, "key_region");
            present = present || (cJSON_IsNumber(key_region) && (int)key_region->valuedouble == index);
        }
        if (ok && present) {
            ok = sb_appendf(sb, "### %s (region %d)\n\n", json_string_member(region, "label"), index) &&
                 append_vocabulary_scope(sb, entries, scope_key);
        }
        index++;
    }
    return ok;
}

/* The label of the key region at an index of the document's keys, or "none" outside every region. */
static const char *region_label(const cJSON *document, const cJSON *index) {
    if (!cJSON_IsNumber(index)) {
        return "none";
    }
    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(document, "keys");
    const cJSON *region = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(keys, "regions"), (int)index->valuedouble);
    const char *label = json_string_member(region, "label");
    return label ? label : "none";
}

/* The reasons of a prolongation, each once, in order of first appearance. */
static bool append_reasons(StringBuilder *sb, const cJSON *reasons) {
    bool ok = true;
    const cJSON *reason;
    cJSON_ArrayForEach(reason, reasons) {
        bool seen = false;
        for (const cJSON *earlier = reasons->child; earlier != reason && !seen; earlier = earlier->next) {
            seen = cJSON_IsString(earlier) && cJSON_IsString(reason) && strcmp(earlier->valuestring, reason->valuestring) == 0;
        }
        if (ok && !seen && cJSON_IsString(reason)) {
            ok = sb_appendf(sb, "%s%s", reason == reasons->child ? "" : ", ", reason->valuestring);
        }
    }
    return ok;
}

static bool render_prolongations(StringBuilder *sb, const cJSON *document, const cJSON *prolongations) {
    bool ok = sb_append(sb, "## Prolongations\n\n") &&
              sb_appendf(sb,
                         "The library's standard-harmony analysis found %d %s: a tonic or dominant that lasts "
                         "through the chords between it and the harmony that closes it. Measures run from the "
                         "prolonged chord to the closing chord. Numeral is the prolonged chord. Reason is why the "
                         "library reads the chords between as a prolongation.\n\n",
                         cJSON_GetArraySize(prolongations),
                         cJSON_GetArraySize(prolongations) == 1 ? "prolongation" : "prolongations") &&
              sb_append(sb, "| Measures | Function | Numeral | Key | Reason |\n"
                            "| --- | --- | --- | --- | --- |\n");
    const cJSON *prolongation;
    cJSON_ArrayForEach(prolongation, prolongations) {
        if (!ok) {
            break;
        }
        const cJSON *location = cJSON_GetObjectItemCaseSensitive(prolongation, "location");
        int first = (int)number_or_zero(cJSON_GetObjectItemCaseSensitive(location, "start"), "measure");
        int last = (int)number_or_zero(cJSON_GetObjectItemCaseSensitive(location, "end"), "measure");
        const char *numeral = json_string_member(prolongation, "numeral");
        ok = (first == last ? sb_appendf(sb, "| m. %d | ", first) : sb_appendf(sb, "| mm. %d to %d | ", first, last)) &&
             sb_appendf(sb, "%s | %s | %s | ", json_string_member(prolongation, "function"), numeral ? numeral : "-",
                        region_label(document, cJSON_GetObjectItemCaseSensitive(prolongation, "region"))) &&
             append_reasons(sb, cJSON_GetObjectItemCaseSensitive(prolongation, "reasons")) && sb_append(sb, " |\n");
    }
    return ok && sb_append(sb, "\n");
}

static bool render_non_chord_tones(StringBuilder *sb, const cJSON *document, const cJSON *tones) {
    (void)document;
    size_t unclassified = 0;
    const cJSON *tone;
    cJSON_ArrayForEach(tone, tones) {
        if (!cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(tone, "classified"))) {
            unclassified++;
        }
    }
    bool ok = sb_append(sb, "## Non-chord tones\n\n") &&
              sb_appendf(sb, "%d notes sound against a chord without being one of its tones; %zu the library "
                             "left unclassified. Type is the library's name for the figure. Note is the "
                             "imported note's index.\n\n",
                         cJSON_GetArraySize(tones), unclassified) &&
              sb_append(sb, "| Position | Note | Type | Against |\n| --- | --- | --- | --- |\n");
    cJSON_ArrayForEach(tone, tones) {
        if (!ok) {
            break;
        }
        const cJSON *location = cJSON_GetObjectItemCaseSensitive(tone, "location");
        const cJSON *note_ids = cJSON_GetObjectItemCaseSensitive(tone, "note_ids");
        const char *type = json_string_member(tone, "type");
        ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
             sb_appendf(sb, " | %d | %s | %s |\n", (int)cJSON_GetArrayItem(note_ids, 0)->valuedouble,
                        type ? type : "unclassified", json_string_member(tone, "chord_label"));
    }
    return ok && sb_append(sb, "\n");
}

#define CADENCE_TYPE_MAX 8

/* The report's word for a cadence type: the library's "subverted" is "evaded" here (the JSON keeps the library's). */
static const char *report_cadence_type_label(const char *type) {
    return type && strcmp(type, "subverted") == 0 ? "evaded" : type;
}

/* "m23 b1" for a position object; "unknown position" when the meter cannot place it. */
static bool append_short_position(StringBuilder *sb, const cJSON *position) {
    if (!cJSON_IsObject(position)) {
        return sb_append(sb, "unknown position");
    }
    return sb_appendf(sb, "m%d b%d", (int)number_or_zero(position, "measure"), (int)number_or_zero(position, "beat"));
}

/* A share in thousandths as a percentage: "60%" or "62.5%". */
static bool append_share(StringBuilder *sb, double share) {
    long rounded = (long)(share + 0.5);
    return rounded % 10 == 0 ? sb_appendf(sb, "%ld%%", rounded / 10) : sb_appendf(sb, "%.1f%%", (double)rounded / 10.0);
}

/* A cadence type as a sentence word: "perfect-authentic" is "perfect authentic cadence". */
static bool append_cadence_phrase(StringBuilder *sb, const char *type) {
    for (const char *c = report_cadence_type_label(type); c && *c; c++) {
        if (!sb_appendf(sb, "%c", *c == '-' ? ' ' : *c)) {
            return false;
        }
    }
    return sb_append(sb, " cadence");
}

/* The cadences that have a competing reading, one line each, the primary first: "m23 b1 half cadence (60%), or
   perfect authentic cadence at m24 b1 (40%)" and "m4 b1 evaded cadence (55%), or tonic prolongation (45%)". Read
   from the document's cadences; prints nothing when none has an alternative. */
static bool render_cadence_readings(StringBuilder *sb, const cJSON *cadences) {
    bool heading = false;
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        const cJSON *alternative = cJSON_GetObjectItemCaseSensitive(cadence, "alternative");
        if (!cJSON_IsObject(alternative)) {
            continue;
        }
        const cJSON *location = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cadence, "arrival"),
                                                                 "location");
        bool prolongation = json_string_member(alternative, "reading") &&
                            strcmp(json_string_member(alternative, "reading"), "prolongation") == 0;
        bool ok = (heading || sb_append(sb, "### Competing readings\n\n"
                                            "A cadence the evidence does not settle has two readings, the likelier "
                                            "first, each with its share of the evidence.\n\n")) &&
                  sb_append(sb, "- ") && append_short_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
                  sb_append(sb, " ") && append_cadence_phrase(sb, json_string_member(cadence, "type")) &&
                  sb_append(sb, " (") && append_share(sb, number_or_zero(cadence, "share")) && sb_append(sb, "), or ");
        heading = true;
        if (ok && prolongation) {
            ok = sb_append(sb, "tonic prolongation");
        } else if (ok) {
            const cJSON *measure = cJSON_GetObjectItemCaseSensitive(alternative, "measure");
            const cJSON *beat = cJSON_GetObjectItemCaseSensitive(alternative, "beat");
            ok = append_cadence_phrase(sb, json_string_member(alternative, "type")) && sb_append(sb, " at ") &&
                 (cJSON_IsNumber(measure) && cJSON_IsNumber(beat)
                      ? sb_appendf(sb, "m%d b%d", (int)measure->valuedouble, (int)beat->valuedouble)
                      : sb_append(sb, "unknown position"));
        }
        if (!(ok && sb_append(sb, " (") && append_share(sb, number_or_zero(alternative, "share")) &&
              sb_append(sb, ")\n"))) {
            return false;
        }
    }
    return !heading || sb_append(sb, "\n");
}

typedef struct {
    const char *type;
    int count;
} TypeCount;

/* The cadence plan: the cadences in time order as the piece's plan of arrivals, and how many of each type, in the
   order each type first appears. Read from the document's cadences, so the plan is the ordered array itself. */
static bool render_cadence_plan(StringBuilder *sb, const cJSON *document, const cJSON *cadences) {
    TypeCount counts[CADENCE_TYPE_MAX] = {{NULL, 0}};
    size_t distinct = 0;
    bool ok = sb_append(sb, "### Cadence plan\n\n");
    int number = 1;
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        if (!ok) {
            break;
        }
        const cJSON *location = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cadence, "arrival"),
                                                                 "location");
        const char *type = report_cadence_type_label(json_string_member(cadence, "type"));
        ok = sb_appendf(sb, "%d. ", number++) &&
             append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_appendf(sb, ": %s in %s\n", type, region_label(document, cJSON_GetObjectItemCaseSensitive(cadence, "region")));
        size_t t = 0;
        while (t < distinct && strcmp(counts[t].type, type) != 0) {
            t++;
        }
        if (t == distinct && distinct < CADENCE_TYPE_MAX) {
            counts[distinct++] = (TypeCount){type, 0};
        }
        if (t < distinct) {
            counts[t].count++;
        }
    }
    ok = ok && sb_append(sb, "\n**Counts:** ");
    for (size_t t = 0; ok && t < distinct; t++) {
        ok = sb_appendf(sb, "%s%s %d", t > 0 ? ", " : "", counts[t].type, counts[t].count);
    }
    return ok && sb_append(sb, ".\n\n");
}

static bool render_cadences(StringBuilder *sb, const cJSON *document, const cJSON *cadences) {
    bool ok = sb_append(sb, "## Cadences\n\n") &&
              sb_appendf(sb,
                         "The library's standard-harmony analysis found %d %s from the chords, the meter and the "
                         "soprano. Position is where the arrival begins. Key is the key region of the arrival. "
                         "Soprano is the soprano's scale degree at the arrival. Boundary marks a half cadence added "
                         "before a section start.\n\n",
                         cJSON_GetArraySize(cadences), cJSON_GetArraySize(cadences) == 1 ? "cadence" : "cadences") &&
              sb_append(sb, "| Position | Type | Key | Soprano | Beat strength | Boundary |\n"
                            "| --- | --- | --- | --- | --- | --- |\n");
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, cadences) {
        if (!ok) {
            break;
        }
        const cJSON *arrival = cJSON_GetObjectItemCaseSensitive(cadence, "arrival");
        const cJSON *location = cJSON_GetObjectItemCaseSensitive(arrival, "location");
        const char *soprano = json_string_member(cadence, "soprano_degree");
        ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_appendf(sb, " | %s | %s | %s | %s | %s |\n", report_cadence_type_label(json_string_member(cadence, "type")),
                        region_label(document, cJSON_GetObjectItemCaseSensitive(cadence, "region")),
                        soprano ? soprano : "none", json_string_member(cadence, "arrival_beat_strength"),
                        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(cadence, "boundary")) ? "yes" : "no");
    }
    return ok && sb_append(sb, "\n") && render_cadence_readings(sb, cadences) &&
           render_cadence_plan(sb, document, cadences);
}

#define PROGRESSION_TABLE_ROWS 10

/* ---- Motives ---- */

/* The measures the occurrences of a family start in, as "1, 29, 70", each measure once. */
static bool append_motive_measures(StringBuilder *sb, const cJSON *occurrences) {
    int previous = -1;
    bool first = true;
    bool ok = true;
    const cJSON *occurrence;
    cJSON_ArrayForEach(occurrence, occurrences) {
        int measure = (int)number_or_zero(cJSON_GetObjectItemCaseSensitive(occurrence, "measures"), "first");
        if (ok && (first || measure != previous)) {
            ok = sb_appendf(sb, "%s%d", first ? "" : ", ", measure);
            first = false;
        }
        previous = measure;
    }
    return ok;
}

/* The phrases the occurrences of a family lie in, as "1, 2, 5" in order, each once; occurrences that cross a phrase
   boundary lie in none. "-" when there are none. */
static bool append_motive_phrases(StringBuilder *sb, const cJSON *occurrences) {
    bool ok = true;
    bool any = false;
    const cJSON *occurrence;
    cJSON_ArrayForEach(occurrence, occurrences) {
        const cJSON *phrase = cJSON_GetObjectItemCaseSensitive(occurrence, "phrase");
        if (!ok || !cJSON_IsNumber(phrase)) {
            continue;
        }
        bool seen = false;
        const cJSON *earlier;
        cJSON_ArrayForEach(earlier, occurrences) {
            if (earlier == occurrence) {
                break;
            }
            const cJSON *other = cJSON_GetObjectItemCaseSensitive(earlier, "phrase");
            seen = seen || (cJSON_IsNumber(other) && other->valuedouble == phrase->valuedouble);
        }
        if (!seen) {
            ok = sb_appendf(sb, "%s%d", any ? ", " : "", (int)phrase->valuedouble + 1);
            any = true;
        }
    }
    return ok && (any || sb_append(sb, "-"));
}

/* How many occurrences of a family are of each kind, as "exact 1, tonal-transposed 3", in the order the kinds first
   appear. The kind is the library's name of the occurrence's transformation relative to the prototype. */
static bool append_motive_kinds(StringBuilder *sb, const cJSON *occurrences) {
    const cJSON *occurrence;
    bool ok = true;
    bool first = true;
    cJSON_ArrayForEach(occurrence, occurrences) {
        const char *kind = json_string_member(occurrence, "label");
        bool counted = false;
        const cJSON *earlier;
        cJSON_ArrayForEach(earlier, occurrences) {
            if (earlier == occurrence) {
                break;
            }
            counted = counted || strcmp(json_string_member(earlier, "label"), kind) == 0;
        }
        if (!ok || counted) {
            continue;
        }
        int count = 0;
        cJSON_ArrayForEach(earlier, occurrences) {
            count += strcmp(json_string_member(earlier, "label"), kind) == 0 ? 1 : 0;
        }
        ok = sb_appendf(sb, "%s%s %d", first ? "" : ", ", kind, count);
        first = false;
    }
    return ok;
}

/* The prototype's degrees joined by spaces, or a dash for a family without features. */
static bool append_motive_degrees(StringBuilder *sb, const cJSON *features) {
    const cJSON *degrees = cJSON_GetObjectItemCaseSensitive(features, "degrees");
    if (!cJSON_IsArray(degrees) || cJSON_GetArraySize(degrees) == 0) {
        return sb_append(sb, "-");
    }
    bool ok = true;
    bool first = true;
    const cJSON *degree;
    cJSON_ArrayForEach(degree, degrees) {
        ok = ok && sb_appendf(sb, "%s%s", first ? "" : " ", cJSON_IsString(degree) ? degree->valuestring : "?");
        first = false;
    }
    return ok;
}

static bool render_motives(StringBuilder *sb, const cJSON *document, const cJSON *motives) {
    (void)document;
    int count = cJSON_GetArraySize(motives);
    bool ok = sb_append(sb, "## Motives\n\n") &&
              sb_appendf(sb,
                         "The library's motive analysis found %d motive %s in the soprano, the highest sounding note "
                         "at each attack. A family is a short figure that recurs; Notes is the length of the figure, "
                         "Pattern whether its occurrences match by scale steps or by semitones, Measures the "
                         "measures the occurrences start in, Phrases the phrases they lie in (an occurrence "
                         "across a phrase boundary lies in none), and Transformations how many occurrences are of each "
                         "kind the library names against the first occurrence (exact, real-transposed, "
                         "tonal-transposed, inverted, retrograde, augmented, diminished, fragmented, varied).\n\n",
                         count, count == 1 ? "family" : "families") &&
              sb_append(sb, "| Family | Notes | Degrees | Pattern | Occurrences | Measures | Phrases | Transformations |\n"
                            "| --- | --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *motive;
    cJSON_ArrayForEach(motive, motives) {
        if (!ok) {
            break;
        }
        const cJSON *occurrences = cJSON_GetObjectItemCaseSensitive(motive, "occurrences");
        ok = sb_appendf(sb, "| %d | %d | ", (int)number_or_zero(motive, "index") + 1,
                        (int)number_or_zero(motive, "note_count")) &&
             append_motive_degrees(sb, cJSON_GetObjectItemCaseSensitive(motive, "features")) &&
             sb_appendf(sb, " | %s | %d | ",
                        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(motive, "generic")) ? "scale steps" : "semitones",
                        cJSON_GetArraySize(occurrences)) &&
             append_motive_measures(sb, occurrences) && sb_append(sb, " | ") &&
             append_motive_phrases(sb, occurrences) && sb_append(sb, " | ") && append_motive_kinds(sb, occurrences) && sb_append(sb, " |\n");
    }
    return ok && sb_append(sb, "\n");
}

/* The development of each family: its occurrences in time order as "m1 original > m3 real-transposed > m5
   fragmented". The original is the prototype, which the transformation null marks; every other step is the library's
   kind of that occurrence against it. A document without developments (an older one) renders nothing. */
static bool render_motive_development(StringBuilder *sb, const cJSON *document, const cJSON *motives) {
    (void)document;
    bool any = false;
    const cJSON *motive;
    cJSON_ArrayForEach(motive, motives) {
        any = any || cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(motive, "development")) > 0;
    }
    if (!any) {
        return true;
    }
    bool ok = sb_append(sb, "### Development\n\n") &&
              sb_append(sb, "Each family's occurrences in time order, as the measure they start in and the kind the "
                            "library names against the family's prototype, which is the original.\n\n");
    cJSON_ArrayForEach(motive, motives) {
        const cJSON *steps = cJSON_GetObjectItemCaseSensitive(motive, "development");
        if (!ok || cJSON_GetArraySize(steps) == 0) {
            continue;
        }
        ok = sb_appendf(sb, "- **Family %d:** ", (int)number_or_zero(motive, "index") + 1);
        bool first = true;
        const cJSON *step;
        cJSON_ArrayForEach(step, steps) {
            const cJSON *transformation = cJSON_GetObjectItemCaseSensitive(step, "transformation");
            ok = ok && sb_appendf(sb, "%sm%d %s", first ? "" : " > ", (int)number_or_zero(step, "measure"),
                                  cJSON_IsString(transformation) ? transformation->valuestring : "original");
            first = false;
        }
        ok = ok && sb_append(sb, "\n");
    }
    return ok && sb_append(sb, "\n");
}

/* The measure a chord of the document's chords starts in, or -1 when the index or its location is missing. */
/* "m. 8" or "mm. 5 to 8" for a measures object. */
static bool append_measures(StringBuilder *sb, const cJSON *measures) {
    int first = (int)number_or_zero(measures, "first");
    int last = (int)number_or_zero(measures, "last");
    return first == last ? sb_appendf(sb, "m. %d", first) : sb_appendf(sb, "mm. %d to %d", first, last);
}

/* ---- Melody ---- */

static bool render_melody(StringBuilder *sb, const cJSON *document, const cJSON *melody) {
    (void)document;
    bool ok = sb_append(sb, "## Melody\n\n") &&
              sb_append(sb, "The library's melody adapter read the soprano of each phrase as a line of notes: the soprano "
                            "notes that start in the phrase. Range is the lowest and highest note and the semitones between "
                            "them; Climax is the first note at the highest pitch, with how many notes sound it when it is "
                            "not one; Contour counts the intervals between consecutive notes that go up, go down and "
                            "repeat, and how many of them are steps and leaps.\n\n") &&
              sb_append(sb, "| Phrase | Measures | Notes | Range | Climax | Up | Down | Same | Steps | Leaps |\n"
                            "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *entry;
    cJSON_ArrayForEach(entry, melody) {
        if (!ok) {
            break;
        }
        const cJSON *range = cJSON_GetObjectItemCaseSensitive(entry, "range");
        const cJSON *climax = cJSON_GetObjectItemCaseSensitive(entry, "climax");
        const cJSON *contour = cJSON_GetObjectItemCaseSensitive(entry, "contour");
        const cJSON *highest = cJSON_GetObjectItemCaseSensitive(range, "highest");
        int occurrences = (int)number_or_zero(climax, "occurrences");
        ok = sb_appendf(sb, "| %d | ", (int)number_or_zero(entry, "index") + 1) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(entry, "measures")) &&
             sb_appendf(sb, " | %d | %s to %s (%d) | %s, ", (int)number_or_zero(entry, "note_count"),
                        json_string_member(cJSON_GetObjectItemCaseSensitive(range, "lowest"), "name"),
                        json_string_member(highest, "name"), (int)number_or_zero(range, "semitones"),
                        json_string_member(highest, "name")) &&
             append_position(sb, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(climax, "location"),
                                                                  "start")) &&
             (occurrences > 1 ? sb_appendf(sb, ", %d notes", occurrences) : true) &&
             sb_appendf(sb, " | %d | %d | %d | %d | %d |\n", (int)number_or_zero(contour, "up"),
                        (int)number_or_zero(contour, "down"), (int)number_or_zero(contour, "same"),
                        (int)number_or_zero(contour, "steps"), (int)number_or_zero(contour, "leaps"));
    }
    ok = ok && sb_append(sb, "\n") &&
         sb_append(sb, "### Leaps\n\n") &&
         sb_append(sb, "A leap is an interval of a third or larger. The library's rule asks a leap of a fourth or "
                       "larger to be followed by a step in the opposite direction: such a leap is Recovered when one "
                       "follows, Unrecovered when a step the same way, a repeated note or another leap does, and Not "
                       "judged when nothing follows it; a third asks nothing (Not required). Consecutive counts the "
                       "pairs of leaps in a row the library's rule finds fault with.\n\n") &&
         sb_append(sb, "| Phrase | Leaps | Recovered | Unrecovered | Not required | Not judged | Consecutive |\n"
                       "| --- | --- | --- | --- | --- | --- | --- |\n");
    cJSON_ArrayForEach(entry, melody) {
        if (!ok) {
            break;
        }
        const cJSON *leaps = cJSON_GetObjectItemCaseSensitive(entry, "leaps");
        ok = sb_appendf(sb, "| %d | %d | %d | %d | %d | %d | %d |\n", (int)number_or_zero(entry, "index") + 1,
                        cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(leaps, "items")),
                        (int)number_or_zero(leaps, "recovered"), (int)number_or_zero(leaps, "unrecovered"),
                        (int)number_or_zero(leaps, "not_required"), (int)number_or_zero(leaps, "not_judged"),
                        cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(entry, "consecutive_leaps")));
    }
    ok = ok && sb_append(sb, "\n") &&
         sb_append(sb, "### Non-chord tones\n\n") &&
         sb_append(sb, "Each melody note is set against the chord of the segment it starts in, as the library's chord "
                       "identification reads it: a Chord tone is one of the chord's tones, a Non-chord tone is one it "
                       "left over and named, and Unclassified notes are in a segment with no chord or were left "
                       "unnamed. By type counts the non-chord tones under the library's names.\n\n") &&
         sb_append(sb, "| Phrase | Notes | Chord tones | Non-chord tones | Unclassified | By type |\n"
                       "| --- | --- | --- | --- | --- | --- |\n");
    cJSON_ArrayForEach(entry, melody) {
        if (!ok) {
            break;
        }
        const cJSON *tones = cJSON_GetObjectItemCaseSensitive(entry, "non_chord_tones");
        const cJSON *by_type = cJSON_GetObjectItemCaseSensitive(tones, "by_type");
        ok = sb_appendf(sb, "| %d | %d | %d | %d | %d | ", (int)number_or_zero(entry, "index") + 1,
                        (int)number_or_zero(entry, "note_count"), (int)number_or_zero(tones, "chord_tones"),
                        (int)number_or_zero(tones, "non_chord_tones"), (int)number_or_zero(tones, "unclassified"));
        const cJSON *type;
        cJSON_ArrayForEach(type, by_type) {
            ok = ok && sb_appendf(sb, "%s%s %d", type == by_type->child ? "" : ", ", json_string_member(type, "type"),
                                  (int)number_or_zero(type, "count"));
        }
        ok = ok && sb_append(sb, cJSON_GetArraySize(by_type) == 0 ? "-" : "") && sb_append(sb, " |\n");
    }
    ok = ok && sb_append(sb, "\n") &&
         sb_append(sb, "### Tendencies\n\n") &&
         sb_append(sb, "The library's melodic tendencies are soft preferences a well-formed line usually shows: Pitch "
                       "proximity (steps outnumber leaps, small leaps outnumber large ones), Step declination "
                       "(descending steps outnumber ascending ones), Step inertia (the line continues in its "
                       "direction at least as often as it reverses), Melodic regression (extreme notes turn back "
                       "toward the middle) and Melodic arch (the line rises to a climax in its middle half and ends "
                       "below it); a phrase either shows a tendency or does not. The seventh degree column counts the "
                       "notes the library labels the seventh degree that another note follows, judged by its leading "
                       "tone rule: a leading tone goes up to the tonic by step, and a seventh a whole step below the "
                       "tonic does not. Followed is the count that did, Broken the count that did not, with where "
                       "each broken one stands.\n\n") &&
         sb_append(sb, "| Phrase | Pitch proximity | Step declination | Step inertia | Melodic regression | Melodic arch "
                       "| Seventh degree followed | Seventh degree broken |\n"
                       "| --- | --- | --- | --- | --- | --- | --- | --- |\n");
    cJSON_ArrayForEach(entry, melody) {
        if (!ok) {
            break;
        }
        const cJSON *tendencies = cJSON_GetObjectItemCaseSensitive(entry, "tendencies");
        const cJSON *line = cJSON_GetObjectItemCaseSensitive(tendencies, "line");
        const cJSON *seventh = cJSON_GetObjectItemCaseSensitive(tendencies, "seventh_degree");
        ok = sb_appendf(sb, "| %d | ", (int)number_or_zero(entry, "index") + 1);
        for (int t = 0; ok && t < 5; t++) {
            const cJSON *item = cJSON_GetArrayItem(line, t);
            ok = sb_appendf(sb, "%s | ",
                            item ? (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(item, "holds")) ? "holds" : "does not hold")
                                 : "-");
        }
        if (ok && cJSON_IsObject(seventh)) {
            ok = sb_appendf(sb, "%d | %d", (int)number_or_zero(seventh, "followed"), (int)number_or_zero(seventh, "broken"));
            const cJSON *broken;
            const cJSON *breaks = cJSON_GetObjectItemCaseSensitive(seventh, "breaks");
            cJSON_ArrayForEach(broken, breaks) {
                ok = ok && sb_append(sb, broken == breaks->child ? " (" : "; ") &&
                     append_position(sb, cJSON_GetObjectItemCaseSensitive(
                                             cJSON_GetObjectItemCaseSensitive(broken, "location"), "start")) &&
                     (broken->next ? true : sb_append(sb, ")"));
            }
            ok = ok && sb_append(sb, " |\n");
        } else {
            ok = ok && sb_append(sb, "- | - |\n");
        }
    }
    return ok && sb_append(sb, "\n");
}

/* The cadence that ends a phrase as "perfect-authentic (m. 8)", or "none". */
static bool append_phrase_cadence(StringBuilder *sb, const cJSON *document, const cJSON *phrase) {
    const cJSON *index = cJSON_GetObjectItemCaseSensitive(phrase, "cadence");
    const cJSON *cadence =
        cJSON_IsNumber(index)
            ? cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "cadences"), (int)index->valuedouble)
            : NULL;
    if (!cadence) {
        return sb_append(sb, "none");
    }
    const cJSON *location = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(cadence, "arrival"),
                                                             "location");
    return sb_appendf(sb, "%s (", report_cadence_type_label(json_string_member(cadence, "type"))) &&
           append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) && sb_append(sb, ")");
}

/* One line per phrase: its ideas with their labels and measures, its subphrases, and its phrase model (the chords the
   library labelled Tb, PD, D and Te, each with where it begins). */
static bool append_phrase_line(StringBuilder *sb, const cJSON *document, const cJSON *phrase) {
    bool ok = sb_appendf(sb, "- **Phrase %d:** ", (int)number_or_zero(phrase, "index") + 1);
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(phrase, "ideas");
    if (cJSON_GetArraySize(ideas) == 0) {
        ok = ok && sb_append(sb, "no ideas");
    } else {
        ok = ok && sb_append(sb, "ideas ");
    }
    const cJSON *idea;
    cJSON_ArrayForEach(idea, ideas) {
        ok = ok && sb_appendf(sb, "%s%s (", idea == ideas->child ? "" : ", ", json_string_member(idea, "label")) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(idea, "measures")) && sb_append(sb, ")");
    }
    const cJSON *subphrases = cJSON_GetObjectItemCaseSensitive(phrase, "subphrases");
    const cJSON *subphrase;
    cJSON_ArrayForEach(subphrase, subphrases) {
        ok = ok && sb_append(sb, subphrase == subphrases->child ? "; subphrases " : ", ") &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(subphrase, "measures"));
    }
    const cJSON *model = cJSON_GetObjectItemCaseSensitive(phrase, "phrase_model");
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(document, "chords");
    const cJSON *segment;
    cJSON_ArrayForEach(segment, model) {
        const cJSON *chord = cJSON_GetArrayItem(chords, (int)number_or_zero(segment, "chord"));
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(chord, "location"), "start");
        ok = ok && sb_append(sb, segment == model->child ? "; phrase model " : ", ") &&
             sb_appendf(sb, "%s (", json_string_member(segment, "label")) && append_position(sb, start) && sb_append(sb, ")");
    }
    return ok && sb_append(sb, "\n");
}

/* The phrase's expansions and contractions as "internal expansion +2 (mm. 5 to 8)" or "external codetta +1 (m. 9)", each
   with the measures it adds or removes, or "-" with none. */
static bool append_phrase_expansions(StringBuilder *sb, const cJSON *phrase) {
    const cJSON *expansions = cJSON_GetObjectItemCaseSensitive(phrase, "expansions");
    if (cJSON_GetArraySize(expansions) == 0) {
        return sb_append(sb, "-");
    }
    const cJSON *expansion;
    bool ok = true;
    cJSON_ArrayForEach(expansion, expansions) {
        const cJSON *external = cJSON_GetObjectItemCaseSensitive(expansion, "external_kind");
        bool contraction = strcmp(json_string_member(expansion, "change"), "contraction") == 0;
        ok = ok && sb_appendf(sb, "%s%s %s%s%+d (", expansion == expansions->child ? "" : "; ",
                              json_string_member(expansion, "label"), cJSON_IsString(external) ? external->valuestring : "",
                              cJSON_IsString(external) ? " " : "",
                              contraction ? -(int)number_or_zero(expansion, "measures_changed")
                                          : (int)number_or_zero(expansion, "measures_changed")) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(expansion, "measures")) && sb_append(sb, ")");
    }
    return ok;
}

static bool render_phrases(StringBuilder *sb, const cJSON *document, const cJSON *phrases) {
    int count = cJSON_GetArraySize(phrases);
    bool ok = sb_append(sb, "## Phrases\n\n") &&
              sb_appendf(sb,
                         "The library's standard-form analysis found %d %s from the soprano, the meter and the "
                         "cadences. Length counts the measures of the phrase proper; Ends is what closed it: its "
                         "cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in "
                         "the piece; Expansion is a lengthening or shortening of the phrase's ending, or an extension "
                         "after its cadence, with the measures it adds or removes.\n\n",
                         count, count == 1 ? "phrase" : "phrases") &&
              sb_append(sb, "| Phrase | Measures | Length | Cadence | Ends | Role | Expansion |\n"
                            "| --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, phrases) {
        if (!ok) {
            break;
        }
        ok = sb_appendf(sb, "| %d | ", (int)number_or_zero(phrase, "index") + 1) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(phrase, "measures")) &&
             sb_appendf(sb, " | %d | ", (int)number_or_zero(phrase, "length")) &&
             append_phrase_cadence(sb, document, phrase) &&
             sb_appendf(sb, " | %s | %s | ", json_string_member(phrase, "signal"), json_string_member(phrase, "role")) &&
             append_phrase_expansions(sb, phrase) && sb_append(sb, " |\n");
    }
    ok = ok && sb_append(sb, "\n");
    cJSON_ArrayForEach(phrase, phrases) {
        ok = ok && append_phrase_line(sb, document, phrase);
    }
    return ok && sb_append(sb, "\n");
}

/* The parts of a theme as "presentation (mm. 1 to 4), continuation (mm. 5 to 8, perfect-authentic (m. 8, beat 1))": a
   part's measures, then the cadence that ends it when it has one. A part the library does not name is "part". */
static bool append_theme_parts(StringBuilder *sb, const cJSON *document, const cJSON *theme) {
    const cJSON *parts = cJSON_GetObjectItemCaseSensitive(theme, "parts");
    const cJSON *part;
    bool ok = true;
    cJSON_ArrayForEach(part, parts) {
        const cJSON *name = cJSON_GetObjectItemCaseSensitive(part, "name");
        ok = ok && sb_appendf(sb, "%s%s (", part == parts->child ? "" : ", ",
                              cJSON_IsString(name) ? name->valuestring : "part") &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(part, "measures"));
        if (ok && cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(part, "cadence"))) {
            ok = sb_append(sb, ", ") && append_phrase_cadence(sb, document, part);
        }
        ok = ok && sb_append(sb, ")");
    }
    return ok;
}

/* The continuation traits as "fragmentation", or "-" with none. */
static bool append_theme_traits(StringBuilder *sb, const cJSON *theme) {
    const cJSON *traits = cJSON_GetObjectItemCaseSensitive(theme, "traits");
    if (cJSON_GetArraySize(traits) == 0) {
        return sb_append(sb, "-");
    }
    const cJSON *trait;
    bool ok = true;
    cJSON_ArrayForEach(trait, traits) {
        ok = ok && sb_appendf(sb, "%s%s", trait == traits->child ? "" : ", ",
                              cJSON_IsString(trait) ? trait->valuestring : "");
    }
    return ok;
}

static bool render_themes(StringBuilder *sb, const cJSON *document, const cJSON *themes) {
    int count = cJSON_GetArraySize(themes);
    bool ok = sb_append(sb, "## Themes\n\n") &&
              sb_appendf(sb,
                         "The library's standard-form analysis found %d %s from the phrases, their ideas and the "
                         "cadences. Form is the library's name for the pairing of the two parts; Commonality is how "
                         "common that pairing is; Traits are what the library finds in the continuation.\n\n",
                         count, count == 1 ? "theme" : "themes") &&
              sb_append(sb, "| Theme | Form | Commonality | Measures | Length | Parts | Traits |\n"
                            "| --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *theme;
    cJSON_ArrayForEach(theme, themes) {
        if (!ok) {
            break;
        }
        const cJSON *commonality = cJSON_GetObjectItemCaseSensitive(theme, "commonality");
        ok = sb_appendf(sb, "| %d | %s | %s | ", (int)number_or_zero(theme, "index") + 1, json_string_member(theme, "label"),
                        cJSON_IsString(commonality) ? commonality->valuestring : "-") &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(theme, "measures")) &&
             sb_appendf(sb, " | %d | ", (int)number_or_zero(theme, "length")) && append_theme_parts(sb, document, theme) &&
             sb_append(sb, " | ") && append_theme_traits(sb, theme) && sb_append(sb, " |\n");
    }
    ok = ok && sb_append(sb, "\n");
    bool any_compound = false;
    cJSON_ArrayForEach(theme, themes) {
        const cJSON *halves = cJSON_GetObjectItemCaseSensitive(theme, "halves");
        if (ok && cJSON_GetArraySize(halves) == 2) {
            any_compound = true;
            ok = sb_appendf(sb, "- **Theme %d** is a compound period of themes %d and %d.\n",
                            (int)number_or_zero(theme, "index") + 1,
                            (int)cJSON_GetArrayItem(halves, 0)->valuedouble + 1,
                            (int)cJSON_GetArrayItem(halves, 1)->valuedouble + 1);
        }
    }
    return ok && (!any_compound || sb_append(sb, "\n"));
}

static bool section_is_empty(const cJSON *section);

/* ---- Form plan ---- */

/* The phrases of a section as "1" or "1 to 4": the first and last phrase numbers. */
static bool append_section_phrases(StringBuilder *sb, const cJSON *section) {
    const cJSON *phrases = cJSON_GetObjectItemCaseSensitive(section, "phrases");
    int first = (int)number_or_zero(cJSON_GetArrayItem(phrases, 0), "phrase") + 1;
    int last = (int)number_or_zero(cJSON_GetArrayItem(phrases, cJSON_GetArraySize(phrases) - 1), "phrase") + 1;
    return first == last ? sb_appendf(sb, "%d", first) : sb_appendf(sb, "%d to %d", first, last);
}

/* The section the section returns from as "returns A (similarity 0.83)", or "-" for a section with a new letter. */
static bool append_section_return(StringBuilder *sb, const cJSON *form, const cJSON *section) {
    const cJSON *from = cJSON_GetObjectItemCaseSensitive(section, "returns_from");
    const cJSON *index = cJSON_GetObjectItemCaseSensitive(section, "return");
    if (!cJSON_IsNumber(from) || !cJSON_IsNumber(index)) {
        return sb_append(sb, "-");
    }
    const cJSON *earlier = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(form, "sections"), (int)from->valuedouble);
    const cJSON *item = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(form, "returns"), (int)index->valuedouble);
    return sb_appendf(sb, "returns %s (similarity %.2f)", json_string_member(earlier, "label"),
                      number_or_zero(cJSON_GetObjectItemCaseSensitive(item, "similarity"), "value"));
}

static bool append_outline_line(StringBuilder *sb, int depth, const char *level, const char *text,
                                const cJSON *measures, const char *detail) {
    bool ok = true;
    for (int i = 0; ok && i < depth; i++) {
        ok = sb_append(sb, "  ");
    }
    ok = ok && sb_appendf(sb, "- %s%s%s (", level, text[0] ? " " : "", text) && append_measures(sb, measures);
    return ok && sb_appendf(sb, "%s%s)\n", detail[0] ? ", " : "", detail);
}

static bool append_idea_outline(StringBuilder *sb, int depth, const cJSON *ideas) {
    const cJSON *idea;
    bool ok = true;
    cJSON_ArrayForEach(idea, ideas) {
        ok = ok && append_outline_line(sb, depth, "idea", json_string_member(idea, "label"),
                                       cJSON_GetObjectItemCaseSensitive(idea, "measures"), "");
    }
    return ok;
}

static bool append_phrase_outline(StringBuilder *sb, int depth, const cJSON *phrase) {
    char number[24];
    snprintf(number, sizeof(number), "%d", (int)number_or_zero(phrase, "phrase") + 1);
    bool ok = append_outline_line(sb, depth, "phrase", number, cJSON_GetObjectItemCaseSensitive(phrase, "measures"), "");
    const cJSON *subphrases = cJSON_GetObjectItemCaseSensitive(phrase, "subphrases");
    const cJSON *subphrase;
    cJSON_ArrayForEach(subphrase, subphrases) {
        snprintf(number, sizeof(number), "%d", (int)number_or_zero(subphrase, "index") + 1);
        ok = ok && append_outline_line(sb, depth + 1, "subphrase", number,
                                       cJSON_GetObjectItemCaseSensitive(subphrase, "measures"), "") &&
             append_idea_outline(sb, depth + 2, cJSON_GetObjectItemCaseSensitive(subphrase, "ideas"));
    }
    return ok && append_idea_outline(sb, depth + 1, cJSON_GetObjectItemCaseSensitive(phrase, "ideas"));
}

/* The tree as an indented outline: form, sections, phrases, subphrases and ideas, each with its measures. */
static bool render_form_outline(StringBuilder *sb, const cJSON *form) {
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(form, "sections");
    bool ok = sb_append(sb, "### Outline\n\n") &&
              append_outline_line(sb, 0, "form", "", cJSON_GetObjectItemCaseSensitive(form, "measures"), "");
    const cJSON *section;
    cJSON_ArrayForEach(section, sections) {
        const cJSON *key = cJSON_GetObjectItemCaseSensitive(section, "key");
        ok = ok && append_outline_line(sb, 1, "section", json_string_member(section, "label"),
                                       cJSON_GetObjectItemCaseSensitive(section, "measures"),
                                       cJSON_IsString(key) ? key->valuestring : "");
        const cJSON *phrase;
        cJSON_ArrayForEach(phrase, cJSON_GetObjectItemCaseSensitive(section, "phrases")) {
            ok = ok && append_phrase_outline(sb, 2, phrase);
        }
    }
    return ok && sb_append(sb, "\n");
}

static bool render_thematic_returns(StringBuilder *sb, const cJSON *form, const cJSON *returns) {
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(form, "sections");
    bool ok = sb_append(sb, "### Thematic returns\n\n") &&
              sb_append(sb, "A later theme recalls an earlier one when the kernel's thematic return detection finds "
                            "the same contour and rhythm in a stretch of both. Similarity is the lower of the two "
                            "scores. Themes that share a phrase are not compared.\n\n") &&
              sb_append(sb, "| Later theme | Measures | Section | Earlier theme | Measures | Section | Similarity |\n"
                            "| --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *item;
    cJSON_ArrayForEach(item, returns) {
        const cJSON *later = cJSON_GetArrayItem(sections, (int)number_or_zero(item, "later_section"));
        const cJSON *earlier = cJSON_GetArrayItem(sections, (int)number_or_zero(item, "earlier_section"));
        ok = ok && sb_appendf(sb, "| %d | ", (int)number_or_zero(item, "later_theme") + 1) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(item, "later")) &&
             sb_appendf(sb, " | %s | %d | ", json_string_member(later, "label"),
                        (int)number_or_zero(item, "earlier_theme") + 1) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(item, "earlier")) &&
             sb_appendf(sb, " | %s | %.2f |\n", json_string_member(earlier, "label"),
                        number_or_zero(cJSON_GetObjectItemCaseSensitive(item, "similarity"), "value"));
    }
    return ok && sb_append(sb, "\n");
}

static bool render_form_plan(StringBuilder *sb, const cJSON *document, const cJSON *form) {
    const cJSON *sections = cJSON_GetObjectItemCaseSensitive(form, "sections");
    const cJSON *returns = cJSON_GetObjectItemCaseSensitive(form, "returns");
    int count = cJSON_GetArraySize(sections);
    bool ok = sb_append(sb, "## Form plan\n\n") &&
              sb_appendf(sb,
                         "The kernel's form structure detection grouped the phrases into %d %s, closing a section at "
                         "each phrase that ends on a perfect or imperfect authentic cadence; the phrases after the "
                         "last such cadence form the last section. Sections are lettered by first appearance, and a "
                         "section whose opening theme returns an earlier section's material takes that letter with a "
                         "prime. Key is the key the section opens in.\n\n",
                         count, count == 1 ? "section" : "sections") &&
              sb_append(sb, "| Section | Measures | Key | Phrases | Closing cadence | Returns |\n"
                            "| --- | --- | --- | --- | --- | --- |\n");
    const cJSON *section;
    cJSON_ArrayForEach(section, sections) {
        const cJSON *key = cJSON_GetObjectItemCaseSensitive(section, "key");
        ok = ok && sb_appendf(sb, "| %s | ", json_string_member(section, "label")) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(section, "measures")) &&
             sb_appendf(sb, " | %s | ", cJSON_IsString(key) ? key->valuestring : "-") &&
             append_section_phrases(sb, section) && sb_append(sb, " | ") &&
             append_phrase_cadence(sb, document, section) && sb_append(sb, " | ") &&
             append_section_return(sb, form, section) && sb_append(sb, " |\n");
    }
    ok = ok && sb_append(sb, "\n");
    return ok && (section_is_empty(returns) || render_thematic_returns(sb, form, returns)) &&
           render_form_outline(sb, form);
}

static int chord_start_measure(const cJSON *document, const cJSON *index) {
    if (!cJSON_IsNumber(index)) {
        return -1;
    }
    const cJSON *chord = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "chords"),
                                            (int)index->valuedouble);
    const cJSON *start = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(chord, "location"),
                                                          "start");
    return cJSON_IsObject(start) ? (int)number_or_zero(start, "measure") : -1;
}

/* "m. 3" or "mm. 3 to 4" for the measures an occurrence's first and last chord start in. */
static bool append_occurrence_measures(StringBuilder *sb, const cJSON *document, const cJSON *occurrence) {
    const cJSON *chords = cJSON_GetObjectItemCaseSensitive(occurrence, "chords");
    int first = chord_start_measure(document, cJSON_GetArrayItem(chords, 0));
    int last = chord_start_measure(document, cJSON_GetArrayItem(chords, cJSON_GetArraySize(chords) - 1));
    if (first < 0 || last < 0) {
        return sb_append(sb, "unknown position");
    }
    return first == last ? sb_appendf(sb, "m. %d", first) : sb_appendf(sb, "mm. %d to %d", first, last);
}

/* The progressions in the document's order, which is their rank: how often, then how long, then how early. Read from
   the document's progressions, so the table is the array itself, cut to its first rows. */
static bool render_progression_patterns(StringBuilder *sb, const cJSON *document, const cJSON *progressions) {
    int total = cJSON_GetArraySize(progressions);
    bool ok = sb_append(sb, "## Progression patterns\n\n") &&
              sb_appendf(sb,
                         "%d recurring %s of two to six chords, counted over the library's Roman numerals: a held "
                         "chord counts once, and a progression does not cross an unlabelled chord or a change of "
                         "key. Ranked by count, then length, then first occurrence%s. Measures are where each "
                         "occurrence begins and ends.\n\n",
                         total, total == 1 ? "progression" : "progressions",
                         total > PROGRESSION_TABLE_ROWS ? "; the top 10 are shown" : "") &&
              sb_append(sb, "| Rank | Progression | Count | Measures |\n| --- | --- | --- | --- |\n");
    int rank = 1;
    const cJSON *progression;
    cJSON_ArrayForEach(progression, progressions) {
        if (!ok || rank > PROGRESSION_TABLE_ROWS) {
            break;
        }
        ok = sb_appendf(sb, "| %d | %s | %d | ", rank++, json_string_member(progression, "label"),
                        (int)number_or_zero(progression, "count"));
        const cJSON *occurrence;
        cJSON_ArrayForEach(occurrence, cJSON_GetObjectItemCaseSensitive(progression, "occurrences")) {
            ok = ok && sb_append(sb, occurrence == cJSON_GetObjectItemCaseSensitive(progression, "occurrences")->child
                                         ? ""
                                         : ", ") &&
                 append_occurrence_measures(sb, document, occurrence);
        }
        ok = ok && sb_append(sb, " |\n");
    }
    return ok && sb_append(sb, "\n");
}

/* "1.25" for a rational, to at most two places. */
static bool append_mean(StringBuilder *sb, const cJSON *object, const char *key) {
    char text[32];
    format_number(rational_member(object, key), text, sizeof(text));
    return sb_append(sb, text);
}


/* The mean cell of a cadence approach: a rational, or "none" when the start of the piece clips the window away. */
static bool append_window_mean(StringBuilder *sb, const cJSON *approach, const char *key) {
    return cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(approach, key)) ? append_mean(sb, approach, key)
                                                                           : sb_append(sb, "none");
}

/* How the harmonic rhythm moves into each cadence: the mean changes per measure over the two measures before the
   arrival against the two before those. Arrival is where the cadence arrives, as in the cadence plan. */
static bool render_cadence_approaches(StringBuilder *sb, const cJSON *approaches) {
    bool ok = sb_append(sb, "### Approach to cadences\n\n") &&
              sb_append(sb, "Changes per measure over the two measures before each cadence's arrival (Approach), "
                            "against the two measures before those (Before). Faster is yes when the approach "
                            "changes harmony more often.\n\n") &&
              sb_append(sb, "| Cadence | Arrival | Before | Approach | Faster |\n| --- | --- | --- | --- | --- |\n");
    const cJSON *approach;
    cJSON_ArrayForEach(approach, approaches) {
        if (!ok) {
            break;
        }
        ok = sb_appendf(sb, "| %d | m. %d | ", (int)number_or_zero(approach, "cadence") + 1,
                        (int)number_or_zero(approach, "arrival_measure")) &&
             append_window_mean(sb, approach, "before") && sb_append(sb, " | ") &&
             append_window_mean(sb, approach, "approach") &&
             sb_appendf(sb, " | %s |\n",
                        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(approach, "accelerates")) ? "yes" : "no");
    }
    return ok && sb_append(sb, "\n");
}

/* The harmonic rhythm: the changes of harmony per measure, four measures to a row with the mean of the row, the
   mean over the piece, and the approach to each cadence. Read from the document's harmonic_rhythm. */
static bool render_harmonic_rhythm(StringBuilder *sb, const cJSON *document, const cJSON *rhythm) {
    (void)document;
    const cJSON *per_measure = cJSON_GetObjectItemCaseSensitive(rhythm, "per_measure");
    const cJSON *windows = cJSON_GetObjectItemCaseSensitive(rhythm, "per_window");
    bool ok = sb_append(sb, "## Harmonic rhythm\n\n") &&
              sb_append(sb, "How often the harmony changes. A change is a chord whose base Roman numeral or key "
                            "differs from the chord before it, counted in the measure it starts in. Changes lists the "
                            "measures of each row in order; Mean is changes per measure over the row.\n\n") &&
              sb_append(sb, "| Measures | Changes | Mean |\n| --- | --- | --- |\n");
    const cJSON *window;
    cJSON_ArrayForEach(window, windows) {
        if (!ok) {
            break;
        }
        int first = (int)number_or_zero(window, "first_measure");
        int last = (int)number_or_zero(window, "last_measure");
        ok = (first == last ? sb_appendf(sb, "| m. %d | ", first) : sb_appendf(sb, "| mm. %d to %d | ", first, last));
        const cJSON *measure;
        cJSON_ArrayForEach(measure, per_measure) {
            int number = (int)number_or_zero(measure, "measure");
            if (ok && number >= first && number <= last) {
                ok = sb_appendf(sb, "%s%d", number == first ? "" : ", ", (int)number_or_zero(measure, "changes"));
            }
        }
        ok = ok && sb_append(sb, " | ") && append_mean(sb, window, "mean_per_measure") && sb_append(sb, " |\n");
    }
    ok = ok && sb_append(sb, "\n**Mean:** ") && append_mean(sb, rhythm, "mean_per_measure") &&
         sb_appendf(sb, " changes per measure over %d %s.\n\n", cJSON_GetArraySize(per_measure),
                    cJSON_GetArraySize(per_measure) == 1 ? "measure" : "measures");
    const cJSON *approaches = cJSON_GetObjectItemCaseSensitive(rhythm, "cadence_approaches");
    return ok && (section_is_empty(approaches) || render_cadence_approaches(sb, approaches));
}

/* ---- Rhythm and meter ---- */

/* The rhythm claims are one flat list, each labelled by the result it is. */
static bool has_label(const cJSON *claim, const char *label) {
    const char *own = json_string_member(claim, "label");
    return own && strcmp(own, label) == 0;
}

static bool has_claims(const cJSON *rhythm, const char *label) {
    const cJSON *claim;
    cJSON_ArrayForEach(claim, rhythm) {
        if (has_label(claim, label)) {
            return true;
        }
    }
    return false;
}

/* The durations of a density claim, shortest first, as "1/8 (12), 1/4 (3)". */
static bool append_durations(StringBuilder *sb, const cJSON *claim) {
    const cJSON *durations = cJSON_GetObjectItemCaseSensitive(claim, "durations");
    const cJSON *bin;
    bool ok = true;
    cJSON_ArrayForEach(bin, durations) {
        const cJSON *duration = cJSON_GetObjectItemCaseSensitive(bin, "duration");
        ok = ok && sb_appendf(sb, "%s%d/%d (%d)", bin == durations->child ? "" : ", ", (int)number_or_zero(duration, "num"),
                              (int)number_or_zero(duration, "den"), (int)number_or_zero(bin, "count"));
    }
    const cJSON *truncated = cJSON_GetObjectItemCaseSensitive(claim, "durations_truncated");
    return ok && sb_append(sb, cJSON_GetArraySize(durations) == 0 ? "-" : "") &&
           sb_append(sb, cJSON_IsTrue(truncated) ? ", ..." : "");
}

static bool render_densities(StringBuilder *sb, const cJSON *rhythm) {
    bool ok = sb_append(sb, "### Density\n\n") &&
              sb_append(sb, "The library's density profile of each phrase, for all its notes and for each line that has a "
                            "note in it (the soprano and bass of the outer voices, and Other for the rest). A note is in a "
                            "phrase when its onset is; a chord is one attack. Attacks per beat is read measure by measure "
                            "over the beats the phrase spans in it, as a mean and the largest; Mean gap is the mean "
                            "distance between attacks in whole notes; On the beat is the share of attacks exactly on a "
                            "beat. Durations lists the notes' lengths in whole notes, shortest first, with how many "
                            "notes have each. The patterns that recur are the motive families' rhythms (see Motives).\n\n") &&
              sb_append(sb, "| Phrase | Measures | Line | Notes | Attacks | Per beat | Max per beat | Mean gap | On the beat | "
                            "Durations |\n"
                            "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, rhythm) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "density")) {
            continue;
        }
        const cJSON *per_beat = cJSON_GetObjectItemCaseSensitive(claim, "attacks_per_beat");
        char mean[32];
        char max[32];
        char gap[32];
        format_number(number_or_zero(per_beat, "mean"), mean, sizeof(mean));
        format_number(number_or_zero(per_beat, "max"), max, sizeof(max));
        format_number(number_or_zero(claim, "mean_inter_onset"), gap, sizeof(gap));
        ok = sb_appendf(sb, "| %d | ", (int)number_or_zero(claim, "phrase") + 1) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(claim, "measures")) &&
             sb_appendf(sb, " | %s | %d | %d | %s | %s | %s | %.1f%% | ", json_string_member(claim, "line"),
                        (int)number_or_zero(claim, "notes"), (int)number_or_zero(claim, "attacks"), mean, max, gap,
                        100.0 * number_or_zero(claim, "on_beat_share")) &&
             append_durations(sb, claim) && sb_append(sb, " |\n");
    }
    return ok && sb_append(sb, "\n");
}

static bool render_syncopations(StringBuilder *sb, const cJSON *rhythm) {
    bool ok = sb_append(sb, "### Syncopation\n\n");
    if (!has_claims(rhythm, "syncopation")) {
        return ok && sb_append(sb, "The library found no syncopation.\n\n");
    }
    ok = ok && sb_append(sb, "A note is syncopated when it is attacked at a weaker metric level and held through a "
                             "stronger one that no note of its line attacks. Each line (the soprano and bass of the outer "
                             "voices, and Other for the rest) is read on its own. Strength is the difference of the two "
                             "levels, 1 to 4: an off-beat note held over the beat is 1, a note on a beat held over the "
                             "barline is 2, an off-beat note held over the barline is 3.\n\n") &&
         sb_append(sb, "| Line | Measure | Beat | Attacked at | Held through | Strength |\n"
                       "| --- | --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, rhythm) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "syncopation")) {
            continue;
        }
        const cJSON *through = cJSON_GetObjectItemCaseSensitive(claim, "held_through");
        char beat[32];
        char through_beat[32];
        format_number(number_or_zero(claim, "beat"), beat, sizeof(beat));
        format_number(number_or_zero(through, "beat"), through_beat, sizeof(through_beat));
        ok = sb_appendf(sb, "| %s | %d | %s | %s | %s (m. %d, beat %s) | %d |\n", json_string_member(claim, "line"),
                        (int)number_or_zero(claim, "measure"), beat, json_string_member(claim, "attack_level"),
                        json_string_member(through, "level"), (int)number_or_zero(through, "measure"), through_beat,
                        (int)number_or_zero(claim, "strength"));
    }
    return ok && sb_append(sb, "\n");
}

/* "agrees", "does not" or "unavailable" for a hemiola's evidence value. */
static const char *evidence_text(const cJSON *evidence, const char *key) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(evidence, key);
    return cJSON_IsNull(value) ? "unavailable" : (cJSON_IsTrue(value) ? "agrees" : "does not");
}

static bool render_hemiolas(StringBuilder *sb, const cJSON *rhythm) {
    bool ok = sb_append(sb, "### Hemiola\n\n");
    if (!has_claims(rhythm, "hemiola")) {
        return ok && sb_append(sb, "The library found no hemiola.\n\n");
    }
    ok = ok && sb_append(sb, "A hemiola reads the six pulses of two measures of a simple triple meter, or one of 6/8, as "
                             "three duple units. It is one when at least two of the evidence types that are available "
                             "agree: Duration (the attacks fall on the starts of the duple units), Chords (the changes "
                             "of harmony do) and Accent (long notes begin on the second and third units).\n\n") &&
         sb_append(sb, "| Measures | Duration | Chords | Accent | Agreeing |\n| --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, rhythm) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "hemiola")) {
            continue;
        }
        const cJSON *evidence = cJSON_GetObjectItemCaseSensitive(claim, "evidence");
        ok = sb_append(sb, "| ") && append_measures(sb, cJSON_GetObjectItemCaseSensitive(claim, "measures")) &&
             sb_appendf(sb, " | %s | %s | %s | %d of %d |\n", evidence_text(evidence, "duration"),
                        evidence_text(evidence, "chord"), evidence_text(evidence, "accent"),
                        (int)number_or_zero(evidence, "agreeing"), (int)number_or_zero(evidence, "available"));
    }
    return ok && sb_append(sb, "\n");
}

/* The hypermeasure kinds of a hypermeter claim, as "3 regular, 1 short". */
static bool append_hypermeasure_kinds(StringBuilder *sb, const cJSON *hypermeasures) {
    static const char *const KINDS[] = {"regular", "short", "expanded", "extension", "contracted"};
    bool first = true;
    bool ok = true;
    for (size_t k = 0; ok && k < sizeof(KINDS) / sizeof(KINDS[0]); k++) {
        int count = 0;
        const cJSON *item;
        cJSON_ArrayForEach(item, hypermeasures) {
            const char *kind = json_string_member(item, "kind");
            count += kind && strcmp(kind, KINDS[k]) == 0 ? 1 : 0;
        }
        if (count > 0) {
            ok = sb_appendf(sb, "%s%d %s", first ? "" : ", ", count, KINDS[k]);
            first = false;
        }
    }
    return ok;
}

static bool render_hypermeters(StringBuilder *sb, const cJSON *rhythm) {
    bool ok = sb_append(sb, "### Hypermeter\n\n") &&
              sb_append(sb, "The library reads each phrase in hypermeasures of two or four measures: the length that "
                            "fills the phrase and holds the cadence's arrival in the last one. The first measure of each "
                            "hypermeasure is a hyperdownbeat. A phrase that does not fill its hypermeasures ends in a "
                            "short one, an internal expansion makes the last one longer, an external expansion adds one "
                            "of its own and a contraction shortens the last. Score is the library's confidence in the "
                            "reading. The form adapter does not report elided cadences, so no phrase is read as "
                            "elided.\n\n") &&
              sb_append(sb, "| Phrase | Measures | Length | Hyperdownbeats | Hypermeasures | Cadence arrives | Score |\n"
                            "| --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, rhythm) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "hypermeter")) {
            continue;
        }
        const cJSON *downbeats = cJSON_GetObjectItemCaseSensitive(claim, "hyperdownbeats");
        const cJSON *arrival = cJSON_GetObjectItemCaseSensitive(claim, "arrival_measure");
        char score[32];
        format_number(number_or_zero(claim, "score"), score, sizeof(score));
        ok = sb_appendf(sb, "| %d | ", (int)number_or_zero(claim, "phrase") + 1) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(claim, "measures")) &&
             sb_appendf(sb, " | %d | ", (int)number_or_zero(claim, "length"));
        const cJSON *downbeat;
        cJSON_ArrayForEach(downbeat, downbeats) {
            ok = ok && sb_appendf(sb, "%s%d", downbeat == downbeats->child ? "" : ", ", (int)downbeat->valuedouble);
        }
        ok = ok && sb_append(sb, " | ") &&
             append_hypermeasure_kinds(sb, cJSON_GetObjectItemCaseSensitive(claim, "hypermeasures")) &&
             (cJSON_IsNumber(arrival) ? sb_appendf(sb, " | m. %d | ", (int)arrival->valuedouble)
                                      : sb_append(sb, " | none | ")) &&
             sb_appendf(sb, "%s |\n", score);
    }
    return ok && sb_append(sb, "\n");
}

static bool render_tuplets(StringBuilder *sb, const cJSON *rhythm) {
    bool ok = sb_append(sb, "### Tuplets and borrowed divisions\n\n");
    if (!has_claims(rhythm, "tuplet")) {
        return ok && sb_append(sb, "No tuplets were found.\n\n");
    }
    ok = ok && sb_append(sb, "A meter divides its beat two ways (simple) or three (compound). A tuplet divides it the other "
                             "way: a triplet in a simple meter or a duplet in a compound one is a borrowed division. The "
                             "library names those two and says at which level a group occurs (the whole Beat, a "
                             "Subdivision of it, or several beats) and whether that is typical. Only triplets (sextuplets "
                             "included) and duplets are looked for, read from the notes' times alone, rounded to a grid "
                             "of 96 to the whole note. A run is two or more consecutive attacks of one ratio.\n\n") &&
         sb_append(sb, "| Ratio | Name | Runs | Attacks | Borrowed |\n| --- | --- | --- | --- | --- |\n");
    /* one row per ratio; the claims of a ratio are together, the smaller actual count first */
    const cJSON *claim;
    long previous = -1;
    cJSON_ArrayForEach(claim, rhythm) {
        if (!ok) {
            break;
        }
        const cJSON *ratio = cJSON_GetObjectItemCaseSensitive(claim, "ratio");
        if (!has_label(claim, "tuplet") || (long)number_or_zero(ratio, "actual") == previous) {
            continue;
        }
        previous = (long)number_or_zero(ratio, "actual");
        int runs = 0;
        int attacks = 0;
        int borrowed = 0;
        const cJSON *other;
        cJSON_ArrayForEach(other, rhythm) {
            if (has_label(other, "tuplet") &&
                (long)number_or_zero(cJSON_GetObjectItemCaseSensitive(other, "ratio"), "actual") == previous) {
                runs++;
                attacks += (int)number_or_zero(other, "attacks");
                borrowed += cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(other, "borrowed")) ? 1 : 0;
            }
        }
        ok = sb_appendf(sb, "| %d:%d | %s | %d | %d | %s |\n", (int)previous,
                        (int)number_or_zero(ratio, "normal"), json_string_member(claim, "name"), runs, attacks,
                        borrowed == 0 ? "no" : (borrowed == runs ? "yes" : "some"));
    }
    ok = ok && sb_append(sb, "\n| Measure | Beat | Ratio | Name | Meter | Borrowed | Level | Attacks |\n"
                             "| --- | --- | --- | --- | --- | --- | --- | --- |\n");
    cJSON_ArrayForEach(claim, rhythm) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "tuplet")) {
            continue;
        }
        const cJSON *ratio = cJSON_GetObjectItemCaseSensitive(claim, "ratio");
        const cJSON *level = cJSON_GetObjectItemCaseSensitive(claim, "level");
        const cJSON *typical = cJSON_GetObjectItemCaseSensitive(claim, "level_typical");
        char beat[32];
        format_number(number_or_zero(claim, "beat"), beat, sizeof(beat));
        ok = sb_appendf(sb, "| %d | %s | %d:%d | %s | %s | %s | ", (int)number_or_zero(claim, "measure"), beat, (int)number_or_zero(ratio, "actual"),
                        (int)number_or_zero(ratio, "normal"), json_string_member(claim, "name"),
                        json_string_member(claim, "meter"),
                        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(claim, "borrowed")) ? "yes" : "no") &&
             (cJSON_IsString(level) ? sb_appendf(sb, "%s%s", level->valuestring, cJSON_IsTrue(typical) ? "" : " (not typical)")
                                    : sb_append(sb, "not placed")) &&
             sb_appendf(sb, " | %d |\n", (int)number_or_zero(claim, "attacks"));
    }
    return ok && sb_append(sb, "\n");
}

/* "3/4, scale 2, pickup 1/4" for a meter-fit reading, with its score. */
static bool append_meter_reading(StringBuilder *sb, const cJSON *reading) {
    const cJSON *meter = cJSON_GetObjectItemCaseSensitive(reading, "meter");
    const cJSON *scale = cJSON_GetObjectItemCaseSensitive(reading, "scale");
    const cJSON *pickup = cJSON_GetObjectItemCaseSensitive(reading, "pickup");
    bool ok = sb_appendf(sb, "%d/%d, scale %d", (int)number_or_zero(meter, "numerator"),
                         (int)number_or_zero(meter, "denominator"), (int)number_or_zero(scale, "num"));
    if ((int)number_or_zero(scale, "den") != 1) {
        ok = ok && sb_appendf(sb, "/%d", (int)number_or_zero(scale, "den"));
    }
    ok = ok && sb_append(sb, ", ");
    if ((int)number_or_zero(pickup, "num") == 0) {
        ok = ok && sb_append(sb, "no pickup");
    } else {
        ok = ok && sb_appendf(sb, "pickup %d/%d", (int)number_or_zero(pickup, "num"), (int)number_or_zero(pickup, "den"));
    }
    return ok && sb_appendf(sb, " (fit %.2f)", number_or_zero(reading, "score"));
}

/* The meter check: how well the meter used carries the harmonic rhythm and cadences, and a better reading if there is
   one. Offered only, never applied. */
static bool render_meter_fit(StringBuilder *sb, const cJSON *rhythm) {
    const cJSON *claim;
    cJSON_ArrayForEach(claim, rhythm) {
        if (!has_label(claim, "meter-fit")) {
            continue;
        }
        const cJSON *alternative = cJSON_GetObjectItemCaseSensitive(claim, "alternative");
        bool ok = sb_append(sb, "**Meter fit:** ") &&
                  append_meter_reading(sb, cJSON_GetObjectItemCaseSensitive(claim, "declared")) &&
                  sb_append(sb, cJSON_IsObject(alternative) ? "; a better reading: " : "; no better reading found");
        if (ok && cJSON_IsObject(alternative)) {
            ok = append_meter_reading(sb, alternative);
        }
        return ok && sb_append(sb, "\n\n");
    }
    return true;
}

static bool render_rhythm(StringBuilder *sb, const cJSON *document, const cJSON *rhythm) {
    (void)document;
    return sb_append(sb, "## Rhythm and meter\n\n") && render_meter_fit(sb, rhythm) &&
           (!has_claims(rhythm, "density") || render_densities(sb, rhythm)) &&
           render_syncopations(sb, rhythm) && render_hemiolas(sb, rhythm) &&
           (!has_claims(rhythm, "hypermeter") || render_hypermeters(sb, rhythm)) && render_tuplets(sb, rhythm);
}

/* ---- Texture ---- */

/* The texture claims are one flat list: the passages, then the changes between them. */
static bool render_texture(StringBuilder *sb, const cJSON *document, const cJSON *texture) {
    (void)document;
    bool ok = sb_append(sb, "## Texture\n\n") &&
              sb_append(sb, "The library's texture adapter read every measure of the piece from the voices the kernel "
                            "separated in each track or part, classified it, and merged consecutive measures of the same "
                            "texture into passages. Category is the main texture (homorhythm and melody and accompaniment "
                            "are homophony). Voices is the most sounding at once, Synchrony the share of attack times "
                            "where the voices attack together, Pattern the score of a repeated cell (0 to 1: evidence for "
                            "an accompaniment figure, not a name for it). Confidence is the texture's margin from its "
                            "thresholds times the voice separation's.\n\n") &&
              sb_append(sb, "| Measures | Texture | Category | Voices | Synchrony | Pattern | Confidence |\n"
                            "| --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, texture) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "passage")) {
            continue;
        }
        const cJSON *features = cJSON_GetObjectItemCaseSensitive(claim, "features");
        const cJSON *confidence = cJSON_GetObjectItemCaseSensitive(claim, "confidence");
        char synchrony[32];
        char pattern[32];
        char overall[32];
        format_number(number_or_zero(features, "synchrony"), synchrony, sizeof(synchrony));
        format_number(number_or_zero(features, "pattern"), pattern, sizeof(pattern));
        format_number(number_or_zero(confidence, "overall"), overall, sizeof(overall));
        ok = sb_append(sb, "| ") && append_measures(sb, cJSON_GetObjectItemCaseSensitive(claim, "measures")) &&
             sb_appendf(sb, " | %s | %s | %d | %s | %s | %s |\n", json_string_member(claim, "texture"),
                        json_string_member(claim, "category"), (int)number_or_zero(features, "voices_max"), synchrony, pattern,
                        overall);
    }
    ok = ok && sb_append(sb, "\n");
    if (!has_claims(texture, "change")) {
        return ok && sb_append(sb, "The texture does not change.\n\n");
    }
    ok = ok && sb_append(sb, "### Changes\n\n") &&
         sb_append(sb, "| Measure | From | To | Category changes |\n| --- | --- | --- | --- |\n");
    cJSON_ArrayForEach(claim, texture) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "change")) {
            continue;
        }
        ok = sb_appendf(sb, "| %d | %s | %s | %s |\n", (int)number_or_zero(claim, "boundary_measure"),
                        json_string_member(claim, "from_texture"), json_string_member(claim, "to_texture"),
                        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(claim, "category_changed")) ? "yes" : "no");
    }
    return ok && sb_append(sb, "\n");
}

/* ---- Voice leading ---- */

static const char *const MOTION_NAMES[] = {"contrary", "similar", "parallel", "oblique", "static"};

static const cJSON *find_claim(const cJSON *claims, const char *label) {
    const cJSON *claim;
    cJSON_ArrayForEach(claim, claims) {
        if (has_label(claim, label)) {
            return claim;
        }
    }
    return NULL;
}

static bool render_motions(StringBuilder *sb, const cJSON *claim) {
    const cJSON *totals = cJSON_GetObjectItemCaseSensitive(claim, "totals");
    bool ok = sb_appendf(sb, "%d %s over %d chord segments, %d motions between consecutive segments: ",
                         (int)number_or_zero(claim, "voices"),
                         (int)number_or_zero(claim, "voices") == 1 ? "voice" : "voices",
                         (int)number_or_zero(claim, "segments"), (int)number_or_zero(claim, "motion_count"));
    for (size_t type = 0; ok && type < sizeof(MOTION_NAMES) / sizeof(MOTION_NAMES[0]); type++) {
        ok = sb_appendf(sb, "%s%d %s", type == 0 ? "" : ", ", (int)number_or_zero(totals, MOTION_NAMES[type]),
                        MOTION_NAMES[type]);
    }
    ok = ok && sb_append(sb, ".\n\n") &&
         sb_append(sb, "| Voices | Contrary | Similar | Parallel | Oblique | Static |\n"
                       "| --- | --- | --- | --- | --- | --- |\n");
    const cJSON *pair;
    cJSON_ArrayForEach(pair, cJSON_GetObjectItemCaseSensitive(claim, "pairs")) {
        if (!ok) {
            break;
        }
        if ((int)number_or_zero(pair, "total") == 0) {
            continue;
        }
        const cJSON *counts = cJSON_GetObjectItemCaseSensitive(pair, "counts");
        ok = sb_appendf(sb, "| %d and %d |", (int)number_or_zero(pair, "upper_voice"),
                        (int)number_or_zero(pair, "lower_voice"));
        for (size_t type = 0; ok && type < sizeof(MOTION_NAMES) / sizeof(MOTION_NAMES[0]); type++) {
            ok = sb_appendf(sb, " %d |", (int)number_or_zero(counts, MOTION_NAMES[type]));
        }
        ok = ok && sb_append(sb, "\n");
    }
    return ok && sb_append(sb, "\n");
}

/* The faults and unresolved tones, by the library's rule name. */
static int count_claims_with_rule(const cJSON *claims, const char *label, const char *rule) {
    int count = 0;
    const cJSON *claim;
    cJSON_ArrayForEach(claim, claims) {
        const char *own = json_string_member(claim, "rule");
        count += has_label(claim, label) && own && strcmp(own, rule) == 0 ? 1 : 0;
    }
    return count;
}

static bool render_voice_faults(StringBuilder *sb, const cJSON *voice_leading) {
    bool ok = sb_appendf(sb, "%d parallel %s, %d parallel %s, %d direct fifths or octaves in the outer voices, "
                             "%d unresolved %s, %d unresolved %s.\n\n",
                         count_claims_with_rule(voice_leading, "violation", "parallel-fifths"),
                         count_claims_with_rule(voice_leading, "violation", "parallel-fifths") == 1 ? "fifth" : "fifths",
                         count_claims_with_rule(voice_leading, "violation", "parallel-octaves"),
                         count_claims_with_rule(voice_leading, "violation", "parallel-octaves") == 1 ? "octave" : "octaves",
                         count_claims_with_rule(voice_leading, "violation", "direct-perfects"),
                         count_claims_with_rule(voice_leading, "resolution", "seventh-resolves-down"),
                         count_claims_with_rule(voice_leading, "resolution", "seventh-resolves-down") == 1 ? "seventh" : "sevenths",
                         count_claims_with_rule(voice_leading, "resolution", "leading-tone-resolves"),
                         count_claims_with_rule(voice_leading, "resolution", "leading-tone-resolves") == 1 ? "leading tone" : "leading tones");
    if (!has_claims(voice_leading, "violation") && !has_claims(voice_leading, "resolution")) {
        return ok;
    }
    ok = ok && sb_append(sb, "| Location | Fault | Voices |\n| --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, voice_leading) {
        if (!ok) {
            break;
        }
        if (has_label(claim, "violation")) {
            ok = sb_append(sb, "| ") && append_location(sb, claim) &&
                 sb_appendf(sb, " | %s | %d and %d |\n", json_string_member(claim, "rule"),
                            (int)number_or_zero(claim, "upper_voice"), (int)number_or_zero(claim, "lower_voice"));
        } else if (has_label(claim, "resolution")) {
            ok = sb_append(sb, "| ") && append_location(sb, claim) &&
                 sb_appendf(sb, " | %s | %d |\n", json_string_member(claim, "rule"), (int)number_or_zero(claim, "voice"));
        }
    }
    return ok && sb_append(sb, "\n");
}

/* How many chords double each member, and the chords whose spacing the library finds too wide. */
static bool render_voice_chords(StringBuilder *sb, const cJSON *voice_leading) {
    static const char *const MEMBERS[] = {"root", "third", "fifth", "seventh"};
    int chords = 0;
    int doubled[4] = {0};
    int none = 0;
    int judged = 0;
    int wide = 0;
    const cJSON *claim;
    cJSON_ArrayForEach(claim, voice_leading) {
        if (!has_label(claim, "doubling")) {
            continue;
        }
        chords++;
        const cJSON *member = cJSON_GetObjectItemCaseSensitive(claim, "doubled");
        const char *name = json_string_member(member, "member");
        int index = -1;
        for (int m = 0; name && m < 4; m++) {
            index = strcmp(name, MEMBERS[m]) == 0 ? m : index;
        }
        if (index >= 0) {
            doubled[index]++;
        } else {
            none++;
        }
        const cJSON *spacing = cJSON_GetObjectItemCaseSensitive(claim, "spacing");
        if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(spacing, "judged"))) {
            judged++;
            wide += cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(spacing, "valid")) ? 1 : 0;
        }
    }
    if (chords == 0) {
        return true;
    }
    bool ok = sb_appendf(sb, "%d chords with three or more voices sounding: the root is doubled in %d, the third in %d, "
                             "the fifth in %d, the seventh in %d, no member doubled in %d. Doubling is counted, not judged.",
                         chords, doubled[0], doubled[1], doubled[2], doubled[3], none);
    if (judged == 0) {
        return ok && sb_append(sb, " Spacing is judged only when four voices sound together; no chord has them.\n\n");
    }
    ok = ok && sb_appendf(sb, " Spacing is judged by the library for the %d chords with four voices all sounding: %d too wide.\n\n",
                          judged, wide);
    if (wide == 0) {
        return ok;
    }
    ok = ok && sb_append(sb, "| Location | Chord | Gap too wide |\n| --- | --- | --- |\n");
    static const char *const PAIRS[] = {"voices 0 and 1", "voices 1 and 2", "voices 2 and 3"};
    cJSON_ArrayForEach(claim, voice_leading) {
        if (!ok) {
            break;
        }
        const cJSON *spacing = cJSON_GetObjectItemCaseSensitive(claim, "spacing");
        if (!has_label(claim, "doubling") || !cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(spacing, "valid"))) {
            continue;
        }
        int upper = (int)number_or_zero(spacing, "wide_voice");
        ok = sb_append(sb, "| ") && append_location(sb, claim) &&
             sb_appendf(sb, " | %d | %s |\n", (int)number_or_zero(claim, "chord"), PAIRS[upper < 0 || upper > 2 ? 0 : upper]);
    }
    return ok && sb_append(sb, "\n");
}

static bool render_voice_leading(StringBuilder *sb, const cJSON *document, const cJSON *voice_leading) {
    (void)document;
    bool ok = sb_append(sb, "## Voice leading\n\n") &&
              sb_append(sb, "The kernel separated the notes of the whole piece into voices (voice 0 is the top) and "
                            "the library's counterpoint adapter classified how each pair of voices moves from one "
                            "chord segment to the next. Parallel motion keeps the interval type, similar motion "
                            "changes it; a voice that rests in either segment gives no motion. Faults are the library's: parallel "
                            "fifths and octaves between any pair of voices, direct fifths and octaves in the outer "
                            "pair, and a seventh or leading tone left unresolved when its chord changes (voice 0 is "
                            "the top).\n\n");
    const cJSON *motions = find_claim(voice_leading, "motions");
    return ok && (!motions || render_motions(sb, motions)) && render_voice_faults(sb, voice_leading) &&
           render_voice_chords(sb, voice_leading);
}

/* ---- Schemas and sequences ---- */

/* A galant degree as it reads in a score: the letter distance from the tonic with a flat or sharp for its alteration
   against the major scale ("3", "b3", "#4"); "-" for a null degree. */
static bool append_galant_degree(StringBuilder *sb, const cJSON *degree) {
    if (!cJSON_IsObject(degree)) {
        return sb_append(sb, "-");
    }
    int alteration = (int)number_or_zero(degree, "alteration");
    const char *mark = alteration < 0 ? "b" : (alteration > 0 ? "#" : "");
    return sb_appendf(sb, "%s%d", mark, (int)number_or_zero(degree, "degree"));
}

/* "m. 1 1/1, m. 2 7/5": the measure, soprano degree and bass degree of each stage (a two-note stage shows both
   soprano degrees, a bass-only stage "-"). */
static bool append_schema_stages(StringBuilder *sb, const cJSON *stages) {
    bool ok = true;
    bool first = true;
    const cJSON *stage;
    cJSON_ArrayForEach(stage, stages) {
        const cJSON *second = cJSON_GetObjectItemCaseSensitive(stage, "second_soprano");
        ok = ok && (first || sb_append(sb, ", ")) && sb_appendf(sb, "m. %d ", (int)number_or_zero(stage, "measure")) &&
             append_galant_degree(sb, cJSON_GetObjectItemCaseSensitive(stage, "soprano")) &&
             (!cJSON_IsObject(second) || (sb_append(sb, "+") && append_galant_degree(sb, second))) &&
             sb_append(sb, "/") && append_galant_degree(sb, cJSON_GetObjectItemCaseSensitive(stage, "bass"));
        first = false;
    }
    return ok;
}

static bool render_schema_matches(StringBuilder *sb, const cJSON *schemas) {
    bool ok = sb_append(sb, "| Measures | Schema | Function | Stages (soprano/bass) | Frame | Confidence |\n"
                            "| --- | --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, schemas) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "schema")) {
            continue;
        }
        char overall[32];
        format_number(number_or_zero(cJSON_GetObjectItemCaseSensitive(claim, "confidence"), "overall"), overall,
                      sizeof(overall));
        ok = sb_append(sb, "| ") && append_measures(sb, cJSON_GetObjectItemCaseSensitive(claim, "measures")) &&
             sb_appendf(sb, " | %s | %s | ", json_string_member(claim, "name"), json_string_member(claim, "function")) &&
             append_schema_stages(sb, cJSON_GetObjectItemCaseSensitive(claim, "stages")) && sb_append(sb, " | ") &&
             append_galant_degree(sb, cJSON_GetObjectItemCaseSensitive(claim, "frame")) &&
             sb_appendf(sb, " | %s |\n", overall);
    }
    return ok && sb_append(sb, "\n");
}

/* "m. 1 1/b3": the measure and the bass, soprano and root degrees of each stage of a sequence, in the home key. */
static bool append_sequence_stages(StringBuilder *sb, const cJSON *stages) {
    bool ok = true;
    bool first = true;
    const cJSON *stage;
    cJSON_ArrayForEach(stage, stages) {
        ok = ok && (first || sb_append(sb, ", ")) && sb_appendf(sb, "m. %d ", (int)number_or_zero(stage, "measure")) &&
             append_galant_degree(sb, cJSON_GetObjectItemCaseSensitive(stage, "soprano")) && sb_append(sb, "/") &&
             append_galant_degree(sb, cJSON_GetObjectItemCaseSensitive(stage, "bass"));
        first = false;
    }
    return ok;
}

static bool render_sequence_matches(StringBuilder *sb, const cJSON *schemas) {
    bool ok = sb_append(sb, "### Sequences\n\n") &&
              sb_append(sb, "The library's scale-harmonization adapter found a model of one or more chords repeated, "
                            "transposed by a constant diatonic interval, at least three times. Kind is the library's "
                            "name for it when its rules for that pattern hold over whole units (rule-backed); the "
                            "cycle of fifths has no rule in the library yet, so it is found from the Roman numerals "
                            "alone. Units is how often the model sounds and, in brackets, how many chords it has; Steps the bass's "
                            "diatonic steps from one unit to the next (up is positive). Stages read as in the "
                            "schemas, in the home key.\n\n") &&
              sb_append(sb, "| Measures | Kind | Rule-backed | Direction | Units (chords each) | Steps | Stages (soprano/bass) | "
                            "Confidence |\n"
                            "| --- | --- | --- | --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, schemas) {
        if (!ok) {
            break;
        }
        if (!has_label(claim, "sequence")) {
            continue;
        }
        const char *direction = json_string_member(claim, "direction");
        char overall[32];
        format_number(number_or_zero(cJSON_GetObjectItemCaseSensitive(claim, "confidence"), "overall"), overall,
                      sizeof(overall));
        ok = sb_append(sb, "| ") && append_measures(sb, cJSON_GetObjectItemCaseSensitive(claim, "measures")) &&
             sb_appendf(sb, " | %s | %s | %s | %d (%d) | %+d | ", json_string_member(claim, "kind"),
                        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(claim, "rule_backed")) ? "yes" : "no",
                        direction ? direction : "-", (int)number_or_zero(claim, "units"),
                        (int)number_or_zero(claim, "unit_length"), (int)number_or_zero(claim, "transposition_steps")) &&
             append_sequence_stages(sb, cJSON_GetObjectItemCaseSensitive(claim, "stages")) &&
             sb_appendf(sb, " | %s |\n", overall);
    }
    return ok && sb_append(sb, "\n");
}

/* The schema claims and the sequence claims are one flat list, each labelled by the result it is. */
static bool render_schemas(StringBuilder *sb, const cJSON *document, const cJSON *schemas) {
    (void)document;
    bool ok = sb_append(sb, "## Schemas and sequences\n\n") &&
              sb_append(sb, "The library's galant-schemas adapter matched its schema prototypes against the outer "
                            "voices: the soprano and bass degree of each chord in its local key, with the chord's "
                            "Roman numeral and the strength of its beat. Each match lists its stages by the measure "
                            "they start in and the soprano and bass degrees found there (b is lowered, # raised; a "
                            "stage with no soprano reads \"-\" and matches on the bass alone). Frame is the key the "
                            "stages are read in, as a degree of the home key (1 is the home key). Confidence is the "
                            "library's: the share of stages whose voices match, times the metric fit, times one minus "
                            "the mean uncertainty of the chords.\n\n");
    ok = ok && (has_claims(schemas, "schema") ? render_schema_matches(sb, schemas)
                                               : sb_append(sb, "No galant schema was found.\n\n"));
    return ok && (!has_claims(schemas, "sequence") || render_sequence_matches(sb, schemas));
}

/* ---- Markings ---- */

/* The part's name from the metadata, else its number: the markings name the part by index. */
static bool append_part(StringBuilder *sb, const cJSON *document, const cJSON *claim) {
    int index = (int)number_or_zero(claim, "part");
    const cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
    const cJSON *input = cJSON_GetObjectItemCaseSensitive(metadata, "input");
    const cJSON *part = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(input, "parts"), index);
    const char *name = json_string_member(part, "name");
    return name && name[0] ? sb_append(sb, name) : sb_appendf(sb, "part %d", index + 1);
}

static bool render_tempo_markings(StringBuilder *sb, const cJSON *tempo) {
    bool ok = sb_append(sb, "### Tempo\n\n") &&
              sb_append(sb, "| From | Text | Metronome | Group | Change |\n| --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, tempo) {
        if (!ok) {
            break;
        }
        const cJSON *metronome = cJSON_GetObjectItemCaseSensitive(claim, "metronome");
        const char *text = json_string_member(claim, "text");
        const char *group = json_string_member(claim, "group");
        ok = sb_append(sb, "| ") &&
             append_position(sb, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(claim, "location"),
                                                                  "start")) &&
             sb_appendf(sb, " | %s | ", text ? text : "-");
        if (ok && cJSON_IsObject(metronome)) {
            ok = append_rational(sb, cJSON_GetObjectItemCaseSensitive(metronome, "beat_unit")) && sb_append(sb, " = ") &&
                 append_rational(sb, cJSON_GetObjectItemCaseSensitive(metronome, "per_minute"));
        } else {
            ok = ok && sb_append(sb, "-");
        }
        ok = ok && sb_appendf(sb, " | %s | %s |\n", group ? group : "-", json_string_member(claim, "change"));
    }
    return ok && sb_append(sb, "\n");
}

static bool render_dynamics(StringBuilder *sb, const cJSON *document, const cJSON *dynamics) {
    bool ok = sb_append(sb, "### Dynamics\n\n") &&
              sb_append(sb, "| From | Marking | Part | Change | Against the previous level |\n"
                            "| --- | --- | --- | --- | --- |\n");
    const cJSON *claim;
    cJSON_ArrayForEach(claim, dynamics) {
        if (!ok) {
            break;
        }
        const char *relation = json_string_member(claim, "relation");
        ok = sb_append(sb, "| ") &&
             append_position(sb, cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(claim, "location"),
                                                                  "start")) &&
             sb_appendf(sb, " | %s | ", json_string_member(claim, "marking")) && append_part(sb, document, claim) &&
             sb_appendf(sb, " | %s | %s |\n", json_string_member(claim, "change"), relation ? relation : "-");
    }
    return ok && sb_append(sb, "\n");
}

static bool render_markings(StringBuilder *sb, const cJSON *document, const cJSON *markings) {
    const cJSON *tempo = cJSON_GetObjectItemCaseSensitive(markings, "tempo");
    const cJSON *dynamics = cJSON_GetObjectItemCaseSensitive(markings, "dynamics");
    const char *source = json_string_member(markings, "source");
    bool ok = sb_append(sb, "## Markings\n\n");
    if (source && strcmp(source, "none") == 0) {
        return ok && sb_append(sb, "A MIDI file carries no tempo or dynamics markings.\n\n");
    }
    ok = ok && sb_append(sb, "The tempo and dynamics markings the score notates, read with the library's "
                             "standard-markings rules. A tempo holds until the next one, a dynamic until the next in its "
                             "part. Group and the loudness comparison are the library's; a text it does not recognise "
                             "has none. Wedges are not read, and articulations are not imported.\n\n");
    ok = ok && (section_is_empty(tempo) ? sb_append(sb, "The score has no tempo marking.\n\n")
                                        : render_tempo_markings(sb, tempo));
    return ok && (section_is_empty(dynamics) ? sb_append(sb, "The score has no dynamics marking.\n\n")
                                             : render_dynamics(sb, document, dynamics));
}

/* ---- Counterpoint check ---- */

static const char *const SPECIES_NAMES[6] = {"", "first", "second", "third", "fourth", "fifth"};

static bool render_counterpoint_check(StringBuilder *sb, const cJSON *document, const cJSON *check) {
    (void)document;
    const cJSON *violations = cJSON_GetObjectItemCaseSensitive(check, "violations");
    const cJSON *valid = cJSON_GetObjectItemCaseSensitive(check, "valid");
    const char *shape_error = json_string_member(check, "shape_error");
    int species = (int)number_or_zero(check, "species");
    bool ok = sb_append(sb, "## Counterpoint check\n\n") &&
              sb_appendf(sb,
                         "The two voices checked as a %s-species exercise with the %s voice as the cantus firmus, "
                         "against the library's standard-counterpoint rules. The library judges each rule group over "
                         "the whole exercise and does not say where a rule broke, so every violation covers the "
                         "exercise.\n\n",
                         species >= 1 && species <= 5 ? SPECIES_NAMES[species] : "?", json_string_member(check, "cantus"));
    if (!ok) {
        return false;
    }
    if (shape_error) {
        return sb_appendf(sb, "**Not checked:** %s.\n\n", shape_error);
    }
    ok = sb_appendf(sb, "**Verdict:** %s.\n\n", cJSON_IsTrue(valid) ? "valid, no hard rule is broken" : "not valid");
    if (!ok || section_is_empty(violations)) {
        return ok && (!section_is_empty(violations) || sb_append(sb, "No rule is broken.\n\n"));
    }
    ok = sb_append(sb, "| Group | Severity | Span |\n| --- | --- | --- |\n");
    const cJSON *violation;
    cJSON_ArrayForEach(violation, violations) {
        if (!ok) {
            break;
        }
        ok = sb_appendf(sb, "| %s | %s | ", json_string_member(violation, "group"), json_string_member(violation, "severity")) &&
             append_location(sb, violation) && sb_append(sb, " |\n");
    }
    return ok && sb_append(sb, "\n");
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
    {"tonicizations", render_tonicizations},
    {"modulations", render_modulations},
    {"pivot_chords", render_pivot_chords},
    {"chords", render_chords},
    {"chords", render_applied_chords},
    {"chords", render_outer_voices},
    {"chord_vocabulary", render_chord_vocabulary},
    {"prolongations", render_prolongations},
    {"non_chord_tones", render_non_chord_tones},
    {"cadences", render_cadences},
    {"phrases", render_phrases},
    {"themes", render_themes},
    {"form", render_form_plan},
    {"motives", render_motives},
    {"melody", render_melody},
    {"motives", render_motive_development},
    {"progressions", render_progression_patterns},
    {"harmonic_rhythm", render_harmonic_rhythm},
    {"rhythm", render_rhythm},
    {"texture", render_texture},
    {"voice_leading", render_voice_leading},
    {"schemas", render_schemas},
    {"markings", render_markings},
    {"counterpoint_check", render_counterpoint_check},
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
    /* Every section ends with a blank line; the report ends with one newline. */
    if (sb.length >= 2 && sb.data[sb.length - 1] == '\n' && sb.data[sb.length - 2] == '\n') {
        sb.data[--sb.length] = '\0';
    }
    return sb.data;
}

char *markdown_report_render_result(const char *input_path, const AnalyzerResult *result) {
    AnalysisDocument *document = analysis_document_build_from_result(input_path, result);
    if (!document) {
        return NULL;
    }
    char *markdown = markdown_report_render(document);
    analysis_document_destroy(document);
    return markdown;
}
