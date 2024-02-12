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
                         "lasts and whether it returns; no cadence is looked for.\n\n",
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
                            "length of the new key and whether it returns; no cadence is looked for.\n\n") &&
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

static bool append_chord_row(StringBuilder *sb, const cJSON *chord, long *last_region) {
    const cJSON *location = cJSON_GetObjectItemCaseSensitive(chord, "location");
    const char *label = string_member(chord, "label");
    const char *root = string_member(chord, "root");
    bool matched = root != NULL;

    bool ok = sb_append(sb, "| ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "start")) &&
              sb_append(sb, " to ") && append_position(sb, cJSON_GetObjectItemCaseSensitive(location, "end")) &&
              sb_appendf(sb, " | %s | ", label ? label : "") && append_numeral_cell(sb, chord, last_region);
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
                    "Duration is in beats. Confidence is the library's confidence in the chord. The runner-up "
                    "is shown only when its score is within %.2f of the best chord's.\n\n",
                    RUNNER_UP_MARGIN_THRESHOLD) &&
         sb_append(sb, "| Position | Chord | Numeral | Root | Quality | Bass | Inversion | Beats | Confidence | "
                       "Runner-up |\n"
                       "| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |\n");
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
    {"tonicizations", render_tonicizations},
    {"modulations", render_modulations},
    {"pivot_chords", render_pivot_chords},
    {"chords", render_chords},
    {"chords", render_applied_chords},
    {"chords", render_outer_voices},
    {"non_chord_tones", render_non_chord_tones},
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
