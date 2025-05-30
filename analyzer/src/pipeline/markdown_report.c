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
    const char *format = string_member(input, "format");
    bool score = format && strcmp(format, "musicxml") == 0;
    size_t part_count = (size_t)cJSON_GetArraySize(cJSON_GetObjectItemCaseSensitive(input, "parts"));
    const char *spelling = string_member(input, "spelling");
    bool ok = sb_appendf(sb, "- **Input:** %s", score ? "MusicXML score" : "MIDI file");
    if (score) {
        ok = ok && sb_appendf(sb, ", %zu %s merged into one texture", part_count, plural(part_count, "part", "parts"));
    }
    return ok && sb_appendf(sb, ". Meter: %s. Key signature: %s. Pitch spelling: %s.\n",
                            knowledge_phrase(string_member(input, "meter")),
                            knowledge_phrase(string_member(input, "key")),
                            spelling && strcmp(spelling, "notated") == 0 ? "notated" : "by rule");
}

/* A few plain-language lines for a composer: the piece, its key, how
   much of it resolved to chords, and the chords it leans on most. */
static bool render_summary(StringBuilder *sb, const cJSON *document, const cJSON *metadata) {
    size_t note_count = (size_t)number_member(metadata, "note_count");
    const char *source_file = string_member(metadata, "source_file");
    bool ok = sb_append(sb, "## Summary\n\n") &&
              sb_appendf(sb, "- **File:** %s, %zu %s.\n", source_file ? source_file : "", note_count,
                         plural(note_count, "note", "notes"));

    ok = ok && append_input_line(sb, cJSON_GetObjectItemCaseSensitive(metadata, "input"));

    const cJSON *keys = cJSON_GetObjectItemCaseSensitive(document, "keys");
    const char *home_label = string_member(cJSON_GetObjectItemCaseSensitive(keys, "home_key"), "label");
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
                 sb_appendf(sb, " at m. %d", (int)number_member(change, "measure"));
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
    double beat = number_member(position, "beat") +
                  number_member(subdivision, "num") / number_member(subdivision, "den");
    char beat_text[32];
    snprintf(beat_text, sizeof(beat_text), "%.2f", beat);
    size_t length = strlen(beat_text);
    while (length > 0 && beat_text[length - 1] == '0') {
        beat_text[--length] = '\0';
    }
    if (length > 0 && beat_text[length - 1] == '.') {
        beat_text[--length] = '\0';
    }
    return sb_appendf(sb, "m. %d, beat %s", (int)number_member(position, "measure"), beat_text);
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
    const char *label = string_member(region, "label");
    return sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
           sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
           sb_appendf(sb, " | %s | %s | %s |\n", label ? label : "", uncertainty_text,
                      region_source_phrase(string_member(region, "source")));
}

/* The key regions with the home key, the opening and closing keys, any notated key signatures, and the notes
   on how the notated and detected keys compare. */
static bool render_key(StringBuilder *sb, const cJSON *document, const cJSON *keys) {
    const cJSON *regions = cJSON_GetObjectItemCaseSensitive(keys, "regions");
    if (cJSON_GetArraySize(regions) == 0) {
        return true;
    }
    const cJSON *home = cJSON_GetObjectItemCaseSensitive(keys, "home_key");
    const cJSON *opening = cJSON_GetArrayItem(regions, (int)number_member(keys, "opening_region"));
    const cJSON *closing = cJSON_GetArrayItem(regions, (int)number_member(keys, "closing_region"));

    bool ok = sb_append(sb, "## Key regions\n\n") &&
              sb_append(sb, "Positions are measure and beat. Uncertainty is the library's, from 0 (clearly this key) "
                            "to 1 (another key fits as well or better). The home key is the key with the most time across its regions.\n\n") &&
              sb_appendf(sb, "- **Home key:** %s.\n", string_member(home, "label")) &&
              sb_appendf(sb, "- **Opening key:** %s.\n", string_member(opening, "label")) &&
              sb_appendf(sb, "- **Closing key:** %s.\n", string_member(closing, "label"));

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
                 sb_appendf(sb, "%s or %s at ", string_member(signature, "major_key"),
                            string_member(signature, "minor_key")) &&
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
        const char *kind = string_member(note, "kind");
        if (ok && kind && strcmp(kind, "key_signature_disagreement") == 0) {
            ok = sb_appendf(sb, "\n**Note:** %s\n", string_member(note, "message"));
        }
    }
    return ok && sb_append(sb, "\n");
}

static const char *INVERSION_NAMES[] = {"root position", "first", "second", "third"};

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
             sb_appendf(sb, " | %s | %s |\n", string_member(item, "from_key"), string_member(item, "to_key"));
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
             sb_appendf(sb, " | %s | %s | %s | %s | %s |\n", string_member(item, "from_key"),
                        string_member(item, "to_key"), string_member(item, "extent"), string_member(item, "type"),
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
             sb_appendf(sb, " | %s | %s | %s | %s | %s | %d |\n", string_member(item, "label"),
                        string_member(item, "bass"), inversion_text, string_member(item, "from_key"),
                        string_member(item, "to_key"), (int)number_member(item, "modulation_index") + 1);
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
    double denominator = number_member(rational, "den");
    return denominator == 0.0 ? 0.0 : number_member(rational, "num") / denominator;
}

/* The chord's Roman numeral cell. The key region is named on the first labelled chord and wherever it changes,
   as "V (in G major)"; a chord the library gives no numeral is "unlabelled" with the reason. last_region is the
   region of the last numeral that named its key, -1 before the first. */
static bool append_numeral_cell(StringBuilder *sb, const cJSON *chord, long *last_region) {
    const cJSON *roman = cJSON_GetObjectItemCaseSensitive(chord, "roman_numeral");
    const char *numeral = string_member(roman, "label");
    if (!numeral) {
        const char *reason = string_member(roman, "unlabelled_reason");
        return sb_appendf(sb, "unlabelled: %s | ", reason ? reason : "no reason given");
    }
    const cJSON *region = cJSON_GetObjectItemCaseSensitive(roman, "key_region");
    const char *key = string_member(roman, "key");
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
    const char *name = string_member(function, "name");
    bool ok;
    if (!name) {
        ok = sb_append(sb, "- | ");
    } else {
        const cJSON *applied_to = cJSON_GetObjectItemCaseSensitive(function, "applied_to");
        const char *strength = string_member(function, "predominant");
        ok = sb_append(sb, name);
        if (ok && cJSON_IsNumber(applied_to)) {
            ok = sb_appendf(sb, " of %d", (int)applied_to->valuedouble);
        }
        if (ok && strength) {
            ok = sb_appendf(sb, " (%s)", strength);
        }
        ok = ok && sb_append(sb, " | ");
    }
    const char *six_four = string_member(cJSON_GetObjectItemCaseSensitive(chord, "six_four"), "type");
    return ok && sb_appendf(sb, "%s | ", six_four ? six_four : "-");
}

static bool append_chord_row(StringBuilder *sb, const cJSON *chord, long *last_region) {
    const cJSON *location = cJSON_GetObjectItemCaseSensitive(chord, "location");
    const char *label = string_member(chord, "label");
    const char *root = string_member(chord, "root");
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

    ok = sb_appendf(sb, "%s | %s | %s | %s | %s | %s | ", root, string_member(chord, "quality"),
                    string_member(chord, "bass"), inversion_text, beats_text, confidence_text);
    if (!ok) {
        return false;
    }

    const cJSON *runner_up = cJSON_GetObjectItemCaseSensitive(chord, "runner_up");
    double margin = number_member(chord, "score") - number_member(runner_up, "score");
    if (cJSON_IsObject(runner_up) && margin < RUNNER_UP_MARGIN_THRESHOLD) {
        char score_text[32];
        snprintf(score_text, sizeof(score_text), "%.2f", number_member(runner_up, "score"));
        return sb_appendf(sb, "%s (%s %s, score %s) |\n", string_member(runner_up, "label"),
                          string_member(runner_up, "root"), string_member(runner_up, "quality"), score_text);
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
        const char *agreement = string_member(applied, "tonicization_agreement");
        const char *tonicization_key = string_member(applied, "tonicization_key");
        const char *target_tonic = string_member(applied, "target_tonic");
        const char *kind = string_member(applied, "kind");
        const char *resolution = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(applied, "deceptive"))
                                     ? "deceptive"
                                     : "to the target";
        ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
             sb_appendf(sb, " | %s (%s) | %s (%s) | %s | ", string_member(applied, "numeral"),
                        kind ? kind : "-", string_member(applied, "target_numeral"),
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
    const char *status = string_member(voice, "status");
    const char *pitch = string_member(voice, "pitch");
    const char *degree = string_member(voice, "degree");
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
             sb_appendf(sb, " | %s |", string_member(chord, "key_label")) &&
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
        const char *entry_group = string_member(entry, "group");
        const char *entry_level = string_member(entry, "level");
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
        if (!(sb_appendf(sb, "| %s | %d | %d | %s | %.1f%% | ", string_member(entry, "numeral"),
                         (int)number_member(entry, "count"), (int)number_member(entry, "runs"), beats_text,
                         number_member(entry, "share") * 100.0) &&
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
            const char *group = string_member(entry, "group");
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
            ok = sb_appendf(sb, "### %s (region %d)\n\n", string_member(region, "label"), index) &&
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
    const char *label = string_member(region, "label");
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
        int first = (int)number_member(cJSON_GetObjectItemCaseSensitive(location, "start"), "measure");
        int last = (int)number_member(cJSON_GetObjectItemCaseSensitive(location, "end"), "measure");
        const char *numeral = string_member(prolongation, "numeral");
        ok = (first == last ? sb_appendf(sb, "| m. %d | ", first) : sb_appendf(sb, "| mm. %d to %d | ", first, last)) &&
             sb_appendf(sb, "%s | %s | %s | ", string_member(prolongation, "function"), numeral ? numeral : "-",
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
        const char *type = string_member(tone, "type");
        ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
             sb_appendf(sb, " | %d | %s | %s |\n", (int)cJSON_GetArrayItem(note_ids, 0)->valuedouble,
                        type ? type : "unclassified", string_member(tone, "chord_label"));
    }
    return ok && sb_append(sb, "\n");
}

#define CADENCE_TYPE_MAX 8

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
        const char *type = string_member(cadence, "type");
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
        const char *soprano = string_member(cadence, "soprano_degree");
        ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
             sb_appendf(sb, " | %s | %s | %s | %s | %s |\n", string_member(cadence, "type"),
                        region_label(document, cJSON_GetObjectItemCaseSensitive(cadence, "region")),
                        soprano ? soprano : "none", string_member(cadence, "arrival_beat_strength"),
                        cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(cadence, "boundary")) ? "yes" : "no");
    }
    return ok && sb_append(sb, "\n") && render_cadence_plan(sb, document, cadences);
}

#define PROGRESSION_TABLE_ROWS 10

/* The measure a chord of the document's chords starts in, or -1 when the index or its location is missing. */
/* "m. 8" or "mm. 5 to 8" for a measures object. */
static bool append_measures(StringBuilder *sb, const cJSON *measures) {
    int first = (int)number_member(measures, "first");
    int last = (int)number_member(measures, "last");
    return first == last ? sb_appendf(sb, "m. %d", first) : sb_appendf(sb, "mm. %d to %d", first, last);
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
    return sb_appendf(sb, "%s (", string_member(cadence, "type")) &&
           append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) && sb_append(sb, ")");
}

/* One line per phrase: its ideas with their labels and measures, its subphrases, and its phrase model (the chords the
   library labelled Tb, PD, D and Te, each with where it begins). */
static bool append_phrase_line(StringBuilder *sb, const cJSON *document, const cJSON *phrase) {
    bool ok = sb_appendf(sb, "- **Phrase %d:** ", (int)number_member(phrase, "index") + 1);
    const cJSON *ideas = cJSON_GetObjectItemCaseSensitive(phrase, "ideas");
    if (cJSON_GetArraySize(ideas) == 0) {
        ok = ok && sb_append(sb, "no ideas");
    } else {
        ok = ok && sb_append(sb, "ideas ");
    }
    const cJSON *idea;
    cJSON_ArrayForEach(idea, ideas) {
        ok = ok && sb_appendf(sb, "%s%s (", idea == ideas->child ? "" : ", ", string_member(idea, "label")) &&
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
        const cJSON *chord = cJSON_GetArrayItem(chords, (int)number_member(segment, "chord"));
        const cJSON *start = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(chord, "location"), "start");
        ok = ok && sb_append(sb, segment == model->child ? "; phrase model " : ", ") &&
             sb_appendf(sb, "%s (", string_member(segment, "label")) && append_position(sb, start) && sb_append(sb, ")");
    }
    return ok && sb_append(sb, "\n");
}

static bool render_phrases(StringBuilder *sb, const cJSON *document, const cJSON *phrases) {
    int count = cJSON_GetArraySize(phrases);
    bool ok = sb_append(sb, "## Phrases\n\n") &&
              sb_appendf(sb,
                         "The library's standard-form analysis found %d %s from the soprano, the meter and the "
                         "cadences. Length counts the measures of the phrase proper; Ends is what closed it: its "
                         "cadence, a break in the melody, or the end of the piece; Role is where the phrase stands in "
                         "the piece.\n\n",
                         count, count == 1 ? "phrase" : "phrases") &&
              sb_append(sb, "| Phrase | Measures | Length | Cadence | Ends | Role |\n"
                            "| --- | --- | --- | --- | --- | --- |\n");
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, phrases) {
        if (!ok) {
            break;
        }
        ok = sb_appendf(sb, "| %d | ", (int)number_member(phrase, "index") + 1) &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(phrase, "measures")) &&
             sb_appendf(sb, " | %d | ", (int)number_member(phrase, "length")) &&
             append_phrase_cadence(sb, document, phrase) &&
             sb_appendf(sb, " | %s | %s |\n", string_member(phrase, "signal"), string_member(phrase, "role"));
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
        ok = sb_appendf(sb, "| %d | %s | %s | ", (int)number_member(theme, "index") + 1, string_member(theme, "label"),
                        cJSON_IsString(commonality) ? commonality->valuestring : "-") &&
             append_measures(sb, cJSON_GetObjectItemCaseSensitive(theme, "measures")) &&
             sb_appendf(sb, " | %d | ", (int)number_member(theme, "length")) && append_theme_parts(sb, document, theme) &&
             sb_append(sb, " | ") && append_theme_traits(sb, theme) && sb_append(sb, " |\n");
    }
    ok = ok && sb_append(sb, "\n");
    bool any_compound = false;
    cJSON_ArrayForEach(theme, themes) {
        const cJSON *halves = cJSON_GetObjectItemCaseSensitive(theme, "halves");
        if (ok && cJSON_GetArraySize(halves) == 2) {
            any_compound = true;
            ok = sb_appendf(sb, "- **Theme %d** is a compound period of themes %d and %d.\n",
                            (int)number_member(theme, "index") + 1,
                            (int)cJSON_GetArrayItem(halves, 0)->valuedouble + 1,
                            (int)cJSON_GetArrayItem(halves, 1)->valuedouble + 1);
        }
    }
    return ok && (!any_compound || sb_append(sb, "\n"));
}

static int chord_start_measure(const cJSON *document, const cJSON *index) {
    if (!cJSON_IsNumber(index)) {
        return -1;
    }
    const cJSON *chord = cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(document, "chords"),
                                            (int)index->valuedouble);
    const cJSON *start = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(chord, "location"),
                                                          "start");
    return cJSON_IsObject(start) ? (int)number_member(start, "measure") : -1;
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
        ok = sb_appendf(sb, "| %d | %s | %d | ", rank++, string_member(progression, "label"),
                        (int)number_member(progression, "count"));
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

static bool section_is_empty(const cJSON *section);

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
        ok = sb_appendf(sb, "| %d | m. %d | ", (int)number_member(approach, "cadence") + 1,
                        (int)number_member(approach, "arrival_measure")) &&
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
        int first = (int)number_member(window, "first_measure");
        int last = (int)number_member(window, "last_measure");
        ok = (first == last ? sb_appendf(sb, "| m. %d | ", first) : sb_appendf(sb, "| mm. %d to %d | ", first, last));
        const cJSON *measure;
        cJSON_ArrayForEach(measure, per_measure) {
            int number = (int)number_member(measure, "measure");
            if (ok && number >= first && number <= last) {
                ok = sb_appendf(sb, "%s%d", number == first ? "" : ", ", (int)number_member(measure, "changes"));
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
    {"progressions", render_progression_patterns},
    {"harmonic_rhythm", render_harmonic_rhythm},
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
