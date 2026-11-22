#include <ursatz_analyzer/analyzer_api.h>
#include <ursatz_analyzer_internal/analysis_document.h>
#include <ursatz_analyzer_internal/version.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"


#include "theory/harmony/harmony_cadence_kinds.h"
#include "theory/tonality/chord_vocabulary.h"
#include "theory/vocabulary/cadence_type.h"
#include "util/misc_util.h"
#include "export/score_export_internal.h"
#include "export/text_export_internal.h"

#include <common/json.h>

AnalyzerTextExportOptions analyzer_text_export_options_default(void) {
    AnalyzerTextExportOptions options = {NULL, 0, 0};
    return options;
}

/* ---- positions in the score ---- */

/* The index of the last measure that starts at or before t (0 when t is before the first measure). */
static size_t measure_at(const ScoreTextScore *score, Rational t) {
    size_t low = 0;
    size_t high = score->measure_count;
    while (high - low > 1) {
        size_t middle = low + (high - low) / 2;
        if (rational_cmp(score->measures[middle].start, t) <= 0) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low;
}

/* The index of the measure that holds the instant just before t: the measure a claim ending at t last touches. */
static size_t measure_before(const ScoreTextScore *score, Rational t) {
    size_t at = measure_at(score, t);
    if (at > 0 && rational_cmp(score->measures[at].start, t) == 0) {
        return at - 1;
    }
    return at;
}

/* The 1-based beat of time t counted in measure m, a pickup aligned to the end of its bar. false when it does not fit a Rational. */
static bool beat_at(const ScoreTextScore *score, size_t m, Rational t, Rational *out) {
    const ScoreTextMeasure *measure = &score->measures[m];
    Rational inside;
    if (rational_sub(t, measure->start, &inside) != RATIONAL_OK) {
        return false;
    }
    if (measure->pickup) {
        Rational gap;
        if (rational_sub(rational_ratio(measure->beats, measure->beat_type), measure->length, &gap) != RATIONAL_OK ||
            rational_add(inside, gap, &inside) != RATIONAL_OK) {
            return false;
        }
    }
    Rational beats;
    return rational_mul(inside, rational_ratio(measure->beat_type, 1), &beats) == RATIONAL_OK &&
           rational_add(rational_ratio(1, 1), beats, out) == RATIONAL_OK;
}

/* The beat of time t in measure m as text; an overflow is an export error (the text is marked failed). */
static void add_beat_at(CommonText *text, const ScoreTextScore *score, size_t m, Rational t);

/* A beat as exact text: an integer, a terminating decimal ("2.5"), or whole plus fraction ("1+1/3"). */
static void add_beat(CommonText *text, Rational beat) {
    if (beat.den == 1) {
        common_text_appendf(text, "%lld", (long long)beat.num);
        return;
    }
    long long scale = 1;
    long long digits = 0;
    while (scale % beat.den != 0 && digits < 9) {
        scale *= 10;
        digits++;
    }
    if (scale % beat.den == 0) {
        long long scaled = beat.num * (scale / beat.den);
        long long whole = scaled / scale;
        long long fraction = scaled % scale;
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%0*lld", (int)digits, (long long)fraction);
        size_t length = strlen(buffer);
        while (length > 0 && buffer[length - 1] == '0') {
            buffer[--length] = '\0';
        }
        common_text_appendf(text, "%lld.%s", (long long)whole, buffer);
        return;
    }
    long long whole = beat.num / beat.den;
    common_text_appendf(text, "%lld+%lld/%lld", (long long)whole, (long long)(beat.num - whole * beat.den), (long long)beat.den);
}

static void add_beat_at(CommonText *text, const ScoreTextScore *score, size_t m, Rational t) {
    Rational beat;
    if (!beat_at(score, m, t, &beat)) {
        text->failed = true;
        return;
    }
    add_beat(text, beat);
}

/* "m12", or "m9-m12" for a range of measures. */
static void add_measure_range(CommonText *text, const ScoreTextScore *score, size_t first, size_t last) {
    common_text_appendf(text, "m%s", score->measures[first].number);
    if (last != first) {
        common_text_appendf(text, "-m%s", score->measures[last].number);
    }
}

/* ---- notes ---- */

static void add_pitch(CommonText *text, const ScoreTextPitch *pitch) {
    common_text_appendf(text, "%c", pitch->step);
    for (int i = 0; i < pitch->alter; i++) {
        common_text_append(text, "#");
    }
    for (int i = 0; i > pitch->alter; i--) {
        common_text_append(text, "b");
    }
    common_text_appendf(text, "%d", pitch->octave);
}

static void add_marks(CommonText *text, const ScoreTextEvent *event, const TextExportMarks *marks, bool close) {
    for (size_t i = 0; marks && i < marks->count; i++) {
        if (marks->items[i].event == event && marks->items[i].close == close) {
            common_text_appendf(text, close ? "[/%s]" : "[%s]", marks->items[i].label);
        }
    }
}

static void add_event(CommonText *text, const ScoreTextEvent *event, const TextExportMarks *marks) {
    if (event->rest) {
        common_text_append(text, "r");
    }
    for (size_t i = 0; i < event->pitch_count; i++) {
        if (i > 0) {
            common_text_append(text, "+");
        }
        add_pitch(text, &event->pitches[i]);
    }
    common_text_append(text, " @");
    add_beat(text, event->beat_in_measure);
    common_text_append(text, " ");
    if (event->tuplet_actual > 0 && event->tuplet_normal > 0) {
        common_text_appendf(text, "%d:%d", event->tuplet_actual, event->tuplet_normal);
    }
    common_text_appendf(text, "%c", event->type);
    for (int i = 0; i < event->dots; i++) {
        common_text_append(text, ".");
    }
    if (event->tie_start) {
        common_text_append(text, "~");
    }
    add_marks(text, event, marks, false);
    add_marks(text, event, marks, true);
}

static void add_measure_notes(CommonText *text, const ScoreTextMeasure *measure, const TextExportMarks *marks) {
    size_t i = 0;
    while (i < measure->event_count) {
        const ScoreTextEvent *first = &measure->events[i];
        common_text_appendf(text, " P%d S%d V%d:", first->part, first->staff, first->voice);
        bool opened = false;
        while (i < measure->event_count && measure->events[i].part == first->part &&
               measure->events[i].staff == first->staff && measure->events[i].voice == first->voice) {
            common_text_append(text, opened ? ", " : " ");
            add_event(text, &measure->events[i], marks);
            opened = true;
            i++;
        }
        common_text_append(text, "\n");
    }
}

/* ---- claims ---- */

typedef enum {
    RANK_KEY,
    RANK_TONICIZATION,
    RANK_MODULATION,
    RANK_CHORD,
    RANK_CADENCE,
    RANK_PHRASE,
    RANK_THEME,
    RANK_FORM,
    RANK_REPEAT,
    RANK_MOTIVE
} ClaimRank;

typedef struct {
    size_t measure; /* where the claim starts */
    Rational start;
    int rank;
    size_t order; /* position in the document, to keep ties stable */
    char *line;
} Claim;

typedef struct {
    Claim *items;
    size_t count;
    size_t capacity;
    size_t order;
    bool failed;
} Claims;

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_IsObject(object) ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
}

static const char *string_of(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    return cJSON_IsString(item) && item->valuestring[0] ? item->valuestring : NULL;
}

static bool rational_of(const cJSON *rational, Rational *out) {
    long long num, den;
    return common_json_rational_read(rational, COMMON_RATIONAL_OBJECT, &num, &den) && rational_make(num, den, out) == RATIONAL_OK;
}

/* The claim's span: start, and end (the start when the span has none). */
static bool span_of(const cJSON *claim, Rational *start, Rational *end) {
    const cJSON *span = member(claim, "span");
    if (!rational_of(member(span, "start"), start)) {
        return false;
    }
    if (!rational_of(member(span, "end"), end)) {
        *end = *start;
    }
    return true;
}

/* " c=0.82" for a claim with an uncertainty; confidence is one minus it. */
static void add_confidence(CommonText *text, const cJSON *claim) {
    const cJSON *uncertainty = member(claim, "uncertainty");
    if (!cJSON_IsNumber(uncertainty)) {
        return;
    }
    double confidence = 1.0 - uncertainty->valuedouble;
    if (confidence < 0.0) {
        confidence = 0.0;
    }
    if (confidence > 1.0) {
        confidence = 1.0;
    }
    common_text_appendf(text, " c=%.2f", confidence);
}

static bool claims_add(Claims *claims, const ScoreTextScore *score, Rational start, ClaimRank rank, CommonText *line) {
    if (line->failed) {
        free(line->data);
        claims->failed = true;
        return false;
    }
    if (claims->count == claims->capacity) {
        size_t capacity = claims->capacity ? claims->capacity * 2 : 64;
        Claim *grown = realloc(claims->items, capacity * sizeof(Claim));
        if (!grown) {
            free(line->data);
            claims->failed = true;
            return false;
        }
        claims->items = grown;
        claims->capacity = capacity;
    }
    Claim *claim = &claims->items[claims->count++];
    claim->measure = measure_at(score, start);
    claim->start = start;
    claim->rank = (int)rank;
    claim->order = claims->order++;
    claim->line = line->data;
    return true;
}

/* A line whose text begins with the given words; the caller adds the rest, then hands it to claims_add. */
static CommonText line_begin(const char *words) {
    CommonText line = {NULL, 0, 0, false};
    common_text_append(&line, " ");
    common_text_append(&line, words);
    return line;
}

static void add_span_range(CommonText *line, const ScoreTextScore *score, Rational start, Rational end) {
    size_t first = measure_at(score, start);
    size_t last = measure_before(score, end);
    if (last < first) {
        last = first;
    }
    add_measure_range(line, score, first, last);
}

static void collect_keys(Claims *claims, const ScoreTextScore *score, const cJSON *document) {
    const cJSON *regions = member(member(document, "keys"), "regions");
    const cJSON *region;
    cJSON_ArrayForEach(region, regions) {
        Rational start, end;
        const char *label = string_of(region, "label");
        if (!label || !span_of(region, &start, &end)) {
            continue;
        }
        CommonText line = line_begin("key ");
        common_text_append(&line, label);
        add_confidence(&line, region);
        common_text_append(&line, " (");
        add_span_range(&line, score, start, end);
        common_text_append(&line, ")");
        claims_add(claims, score, start, RANK_KEY, &line);
    }
}

static void collect_key_changes(Claims *claims, const ScoreTextScore *score, const cJSON *document, const char *section,
                                const char *word, ClaimRank rank) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(document, section)) {
        Rational start, end;
        const char *to_key = string_of(entry, "to_key");
        const char *from_key = string_of(entry, "from_key");
        if (!span_of(entry, &start, &end)) {
            continue;
        }
        CommonText line = line_begin(word);
        common_text_append(&line, " ");
        if (rank == RANK_MODULATION && from_key && to_key) {
            common_text_appendf(&line, "%s to %s", from_key, to_key);
        } else if (to_key) {
            common_text_append(&line, to_key);
        } else if (string_of(entry, "label")) {
            common_text_append(&line, string_of(entry, "label"));
        }
        add_confidence(&line, entry);
        common_text_append(&line, " (");
        add_span_range(&line, score, start, end);
        common_text_append(&line, ")");
        claims_add(claims, score, start, rank, &line);
    }
}

static void collect_chords(Claims *claims, const ScoreTextScore *score, const cJSON *document) {
    const cJSON *chord;
    cJSON_ArrayForEach(chord, member(document, "chords")) {
        Rational start, end;
        const char *root = string_of(chord, "root");
        const char *quality = string_of(chord, "quality");
        if (!root || !quality || !span_of(chord, &start, &end)) {
            continue;
        }
        const char *bass = string_of(chord, "bass");
        const char *numeral = string_of(member(chord, "roman_numeral"), "label");
        size_t m = measure_at(score, start);
        CommonText line = line_begin("chord ");
        common_text_appendf(&line, "%s%s", root, common_practice_chord_symbol_suffix(quality));
        if (bass && strcmp(bass, root) != 0) {
            common_text_appendf(&line, "/%s", bass);
        }
        if (numeral) {
            common_text_appendf(&line, " %s", numeral);
        }
        common_text_append(&line, " @");
        add_beat_at(&line, score, m, start);
        add_confidence(&line, chord);
        claims_add(claims, score, start, RANK_CHORD, &line);
    }
}

/* The abbreviation the library's cadence kinds give (PAC, IAC, AC, HC, PC, DC); a type that is not one of the six, or a
   subverted or unknown one, prints its text. */
static const char *cadence_name(const char *type) {
    const HarmonyCadenceKind *kind = harmony_cadence_kind_find(type);
    if (kind != NULL && kind->core != CADENCE_TYPE_SUBVERTED && kind->core != CADENCE_TYPE_UNKNOWN &&
        strcmp(kind->name, cadence_type_name(kind->core)) == 0) {
        return kind->abbreviation;
    }
    return type;
}

static void collect_cadences(Claims *claims, const ScoreTextScore *score, const cJSON *document) {
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, member(document, "cadences")) {
        Rational start, end;
        const char *type = string_of(cadence, "type");
        if (!type) {
            type = string_of(cadence, "label");
        }
        if (!type || !span_of(cadence, &start, &end)) {
            continue;
        }
        /* The cadence starts at its dominant; the line names the measure and beat where it arrives. */
        Rational arrival = start;
        Rational ignored;
        span_of(member(cadence, "arrival"), &arrival, &ignored);
        size_t m = measure_at(score, arrival);
        CommonText line = line_begin("cadence ");
        common_text_appendf(&line, "%s m%s @", cadence_name(type), score->measures[m].number);
        add_beat_at(&line, score, m, arrival);
        add_confidence(&line, cadence);
        claims_add(claims, score, start, RANK_CADENCE, &line);
    }
}

/* The label of the form section that holds phrase `index`, or NULL. */
static const char *form_section_of_phrase(const cJSON *document, int index) {
    const cJSON *section;
    cJSON_ArrayForEach(section, member(member(document, "form"), "sections")) {
        const cJSON *phrase;
        cJSON_ArrayForEach(phrase, member(section, "phrases")) {
            const cJSON *number = member(phrase, "phrase");
            if (cJSON_IsNumber(number) && (int)number->valuedouble == index) {
                return string_of(section, "label");
            }
        }
    }
    return NULL;
}

static void collect_phrases(Claims *claims, const ScoreTextScore *score, const cJSON *document) {
    const cJSON *phrase;
    cJSON_ArrayForEach(phrase, member(document, "phrases")) {
        Rational start, end;
        const cJSON *index = member(phrase, "index");
        if (!cJSON_IsNumber(index) || !span_of(phrase, &start, &end)) {
            continue;
        }
        CommonText line = line_begin("phrase ");
        common_text_appendf(&line, "%d ", (int)index->valuedouble + 1);
        add_span_range(&line, score, start, end);
        add_confidence(&line, phrase);
        const char *form = form_section_of_phrase(document, (int)index->valuedouble);
        if (form) {
            common_text_appendf(&line, " | form %s", form);
        }
        claims_add(claims, score, start, RANK_PHRASE, &line);
    }
}

static void collect_labelled(Claims *claims, const ScoreTextScore *score, const cJSON *entries, const char *word,
                             ClaimRank rank) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, entries) {
        Rational start, end;
        const char *label = string_of(entry, "label");
        if (!label || !span_of(entry, &start, &end)) {
            continue;
        }
        CommonText line = line_begin(word);
        common_text_appendf(&line, " %s ", label);
        add_span_range(&line, score, start, end);
        add_confidence(&line, entry);
        claims_add(claims, score, start, rank, &line);
    }
}

/* "m12", or "m9-m12" for a run of the library's measure numbers. */
static void add_library_measures(CommonText *text, int first, int last) {
    common_text_appendf(text, "m%d", first);
    if (last != first) {
        common_text_appendf(text, "-m%d", last);
    }
}

/* A repeat or return of the library: at the first measure of the later run, " repeat of m<a>-m<b> (m<c>-m<d>)" or
   " return of m<a>-m<b> (m<c>-m<d>), <t> semitones" (an octave lower or higher for -12 or 12), the earlier run first
   and the later in brackets. */
static void collect_repeats(Claims *claims, const ScoreTextScore *score, const cJSON *document) {
    const cJSON *repeat;
    cJSON_ArrayForEach(repeat, member(document, "repeats")) {
        Rational start, end;
        const char *kind = string_of(repeat, "kind");
        const cJSON *earlier = member(repeat, "earlier");
        const cJSON *later = member(repeat, "later");
        const cJSON *transposition = member(repeat, "transposition");
        if (!kind || !span_of(repeat, &start, &end) || !cJSON_IsNumber(member(earlier, "first")) ||
            !cJSON_IsNumber(member(earlier, "last")) || !cJSON_IsNumber(member(later, "first")) ||
            !cJSON_IsNumber(member(later, "last"))) {
            continue;
        }
        CommonText line = line_begin(kind);
        common_text_append(&line, " of ");
        add_library_measures(&line, (int)member(earlier, "first")->valuedouble,
                             (int)member(earlier, "last")->valuedouble);
        common_text_append(&line, " (");
        add_library_measures(&line, (int)member(later, "first")->valuedouble,
                             (int)member(later, "last")->valuedouble);
        common_text_append(&line, ")");
        if (strcmp(kind, "return") == 0 && cJSON_IsNumber(transposition)) {
            int semitones = (int)transposition->valuedouble;
            if (semitones == 12) {
                common_text_append(&line, ", an octave higher");
            } else if (semitones == -12) {
                common_text_append(&line, ", an octave lower");
            } else {
                common_text_appendf(&line, ", %d semitone%s", semitones, semitones == 1 || semitones == -1 ? "" : "s");
            }
        }
        claims_add(claims, score, start, RANK_REPEAT, &line);
    }
}

/* a, b, ... z, aa, ab, ... for motive 0, 1, ... */
static void add_motive_letter(CommonText *text, int index) {
    char letters[8];
    size_t length = 0;
    int n = index;
    do {
        letters[length++] = (char)('a' + n % 26);
        n = n / 26 - 1;
    } while (n >= 0 && length < sizeof(letters) - 1);
    while (length > 0) {
        char one[2] = {letters[--length], '\0'};
        common_text_append(text, one);
    }
}

static void collect_motives(Claims *claims, const ScoreTextScore *score, const cJSON *document) {
    const cJSON *motive;
    cJSON_ArrayForEach(motive, member(document, "motives")) {
        const cJSON *index = member(motive, "index");
        const cJSON *occurrence;
        if (!cJSON_IsNumber(index)) {
            continue;
        }
        cJSON_ArrayForEach(occurrence, member(motive, "occurrences")) {
            Rational start, end;
            if (!span_of(occurrence, &start, &end)) {
                continue;
            }
            size_t first = measure_at(score, start);
            size_t last = measure_before(score, end);
            if (last < first) {
                last = first;
            }
            CommonText line = line_begin("motive ");
            add_motive_letter(&line, (int)index->valuedouble);
            common_text_appendf(&line, " m%s @", score->measures[first].number);
            add_beat_at(&line, score, first, start);
            common_text_append(&line, "-");
            if (last != first) {
                common_text_appendf(&line, "m%s", score->measures[last].number);
            }
            common_text_append(&line, "@");
            add_beat_at(&line, score, last, end);
            claims_add(claims, score, start, RANK_MOTIVE, &line);
        }
    }
}

static int claim_compare(const void *left, const void *right) {
    const Claim *a = left;
    const Claim *b = right;
    if (a->measure != b->measure) {
        return a->measure < b->measure ? -1 : 1;
    }
    int by_start = rational_cmp(a->start, b->start);
    if (by_start != 0) {
        return by_start;
    }
    if (a->rank != b->rank) {
        return a->rank < b->rank ? -1 : 1;
    }
    return a->order < b->order ? -1 : (a->order > b->order ? 1 : 0);
}

static void claims_free(Claims *claims) {
    for (size_t i = 0; i < claims->count; i++) {
        free(claims->items[i].line);
    }
    free(claims->items);
}

/* ---- the document ---- */

/* Parses a measure number as written ("12", "12a" reads as 12); false when it has no leading digits. */
static bool measure_value(const char *number, long *value) {
    char *end = NULL;
    long parsed = strtol(number, &end, 10);
    if (end == number) {
        return false;
    }
    *value = parsed;
    return true;
}

static bool in_range(const ScoreTextScore *score, size_t m, int first, int last) {
    if (first == 0 && last == 0) {
        return true;
    }
    const ScoreTextMeasure *measure = &score->measures[m];
    if (measure->pickup && m == 0) {
        return first <= 1;
    }
    long value = 0;
    if (!measure_value(measure->number, &value)) {
        return false;
    }
    return value >= first && value <= last;
}

static void add_tempo(CommonText *text, const cJSON *document) {
    const cJSON *tempo = member(member(member(document, "metadata"), "input"), "tempo");
    const cJSON *declared = member(tempo, "declared");
    const cJSON *quarters = member(tempo, "quarters_per_minute");
    if (cJSON_IsTrue(declared) && cJSON_IsNumber(quarters) && quarters->valuedouble > 0) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f", quarters->valuedouble);
        size_t length = strlen(buffer);
        while (length > 0 && buffer[length - 1] == '0') {
            buffer[--length] = '\0';
        }
        if (length > 0 && buffer[length - 1] == '.') {
            buffer[--length] = '\0';
        }
        common_text_appendf(text, "q=%s", buffer);
    } else {
        common_text_append(text, "not declared");
    }
}

void text_export_write_summary(CommonText *text, const cJSON *document, const ScoreTextScore *score, size_t first,
                               size_t last, bool whole) {
    const cJSON *regions = member(member(document, "keys"), "regions");
    const char *key = string_of(cJSON_GetArrayItem(regions, 0), "label");
    common_text_appendf(text, "key: %s | meter: %d/%d | tempo: ", key ? key : "unknown", score->measures[0].beats,
                     score->measures[0].beat_type);
    add_tempo(text, document);
    common_text_appendf(text, " | measures: %s-%s", score->measures[first].number, score->measures[last].number);
    if (!whole) {
        common_text_appendf(text, " of %s-%s", score->measures[0].number,
                         score->measures[score->measure_count - 1].number);
    }
    common_text_append(text, "\n");
}

void text_export_write_legend(CommonText *text) {
    char commit[13];
    snprintf(commit, sizeof(commit), "%.12s", analyzer_producer_key());
    common_text_appendf(text, "analyzer: %s ursatz %s\n", analyzer_version(), commit);
    common_text_append(text,
                    "notes: <pitch><octave> @<beat> <duration>; r = rest; ~ = tied to next; 3:2 = tuplet; c=<confidence>\n");
}

static const char *clef_name(const ScoreTextClef *clef) { return clef->bass ? "bass" : "treble"; }

/* The "clefs:" lines, one per part that has a clef: the clef of each staff in force at the start of measure first
   (those of the first measure, then any change up to and including measure first), for example
   "clefs: P1 S1 treble, S2 bass". Nothing for a score that writes no clef. */
static void add_clefs(CommonText *text, const ScoreTextScore *score, size_t first) {
    size_t capacity = 0;
    for (size_t m = 0; m <= first && m < score->measure_count; m++) {
        capacity += score->measures[m].clef_count;
    }
    ScoreTextClef *state = capacity ? malloc(capacity * sizeof *state) : NULL;
    if (!state) {
        text->failed = text->failed || capacity > 0;
        return;
    }
    size_t count = 0;
    for (size_t m = 0; m <= first && m < score->measure_count; m++) {
        for (size_t c = 0; c < score->measures[m].clef_count; c++) {
            const ScoreTextClef *clef = &score->measures[m].clefs[c];
            size_t at = 0;
            while (at < count && (state[at].part < clef->part ||
                                  (state[at].part == clef->part && state[at].staff < clef->staff))) {
                at++;
            }
            if (at < count && state[at].part == clef->part && state[at].staff == clef->staff) {
                state[at] = *clef;
                continue;
            }
            memmove(&state[at + 1], &state[at], (count - at) * sizeof *state);
            state[at] = *clef;
            count++;
        }
    }
    for (size_t i = 0; i < count; i++) {
        if (i == 0 || state[i].part != state[i - 1].part) {
            common_text_appendf(text, "%sclefs: P%d S%d %s", i == 0 ? "" : "\n", state[i].part, state[i].staff,
                             clef_name(&state[i]));
        } else {
            common_text_appendf(text, ", S%d %s", state[i].staff, clef_name(&state[i]));
        }
    }
    if (count > 0) {
        common_text_append(text, "\n");
    }
    free(state);
}

static void add_header(CommonText *text, const cJSON *document, const ScoreTextScore *score, const char *title,
                       size_t first, size_t last, bool whole) {
    common_text_appendf(text, "# Ursatz export v%d: piece\n", ANALYZER_TEXT_EXPORT_VERSION);
    common_text_appendf(text, "title: %s\n", title);
    text_export_write_summary(text, document, score, first, last, whole);
    add_clefs(text, score, first);
    text_export_write_legend(text);
}

bool text_export_find_range(const ScoreTextScore *score, int first_measure, int last_measure, size_t *first,
                            size_t *last) {
    *first = score->measure_count;
    *last = 0;
    for (size_t m = 0; m < score->measure_count; m++) {
        if (in_range(score, m, first_measure, last_measure)) {
            if (*first == score->measure_count) {
                *first = m;
            }
            *last = m;
        }
    }
    return *first != score->measure_count;
}

/* The measures first to last: each measure line, its notes, then the claims that start in it. false on allocation
   failure. */
bool text_export_write_measures(CommonText *out, const cJSON *document, const ScoreTextScore *score, size_t first,
                                size_t last, const TextExportMarks *marks) {
    Claims claims = {NULL, 0, 0, 0, false};
    collect_keys(&claims, score, document);
    collect_key_changes(&claims, score, document, "tonicizations", "tonicization", RANK_TONICIZATION);
    collect_key_changes(&claims, score, document, "modulations", "modulation", RANK_MODULATION);
    collect_chords(&claims, score, document);
    collect_cadences(&claims, score, document);
    collect_phrases(&claims, score, document);
    collect_labelled(&claims, score, member(document, "themes"), "theme", RANK_THEME);
    collect_labelled(&claims, score, member(member(document, "form"), "sections"), "form", RANK_FORM);
    collect_repeats(&claims, score, document);
    collect_motives(&claims, score, document);
    qsort(claims.items, claims.count, sizeof(Claim), claim_compare);
    if (claims.failed) {
        out->failed = true;
    }

    size_t next_claim = 0;
    while (next_claim < claims.count && claims.items[next_claim].measure < first) {
        next_claim++;
    }
    for (size_t m = first; m <= last; m++) {
        const ScoreTextMeasure *measure = &score->measures[m];
        common_text_append(out, "\n");
        common_text_appendf(out, "m%s", measure->number);
        if (measure->time_changed) {
            common_text_appendf(out, " %d/%d", measure->beats, measure->beat_type);
        }
        /* The clefs of the first measure are the opening ones, in the header; a later <clef> is a change. */
        for (size_t c = 0; m > 0 && c < measure->clef_count; c++) {
            if (score->part_count > 1) {
                common_text_appendf(out, " clef P%d S%d %s", measure->clefs[c].part, measure->clefs[c].staff,
                                 clef_name(&measure->clefs[c]));
            } else {
                common_text_appendf(out, " clef S%d %s", measure->clefs[c].staff, clef_name(&measure->clefs[c]));
            }
        }
        common_text_append(out, "\n");
        add_measure_notes(out, measure, marks);
        while (next_claim < claims.count && claims.items[next_claim].measure == m) {
            common_text_append(out, claims.items[next_claim].line);
            common_text_append(out, "\n");
            next_claim++;
        }
    }
    claims_free(&claims);
    return !out->failed;
}

UrsatzStatus analyzer_text_export_piece_tree(const ImportedScore *notes, const cJSON *document,
                                               const AnalyzerTextExportOptions *options, char **text) {
    if (text) {
        *text = NULL;
    }
    if (!notes || !document || !options || !text) {
        return URSATZ_ERR_REQUEST;
    }
    bool whole_piece = options->first_measure == 0 && options->last_measure == 0;
    if (options->first_measure < 0 || options->last_measure < 0 ||
        (!whole_piece && options->first_measure > options->last_measure)) {
        return URSATZ_ERR_REQUEST;
    }

    ScoreLayout layout;
    UrsatzStatus status = analyzer_score_layout(notes, &layout);
    if (status != URSATZ_OK) {
        return status;
    }
    ScoreTextScore *score = score_text_from_layout(&layout);
    score_layout_destroy(&layout);
    if (!score) {
        return URSATZ_ERR_INTERNAL;
    }

    size_t first = 0;
    size_t last = 0;
    if (!text_export_find_range(score, options->first_measure, options->last_measure, &first, &last)) {
        score_text_destroy(score);
        return URSATZ_ERR_REQUEST;
    }
    bool whole = first == 0 && last == score->measure_count - 1;

    const char *title = options->title && options->title[0] ? options->title : NULL;
    if (!title) {
        title = string_of(member(document, "metadata"), "source_file");
    }
    CommonText out = {NULL, 0, 0, false};
    add_header(&out, document, score, title ? title : "untitled", first, last, whole);
    text_export_write_measures(&out, document, score, first, last, NULL);

    score_text_destroy(score);
    if (out.failed) {
        free(out.data);
        return URSATZ_ERR_ALLOC;
    }
    *text = out.data;
    return URSATZ_OK;
}

static UrsatzStatus text_export_piece(const ImportedScore *notes, const char *document_json,
                                          const AnalyzerTextExportOptions *options, char **text) {
    if (text) {
        *text = NULL;
    }
    if (!notes || !document_json || !options || !text) {
        return URSATZ_ERR_REQUEST;
    }
    cJSON *document = cJSON_Parse(document_json);
    const cJSON *version = member(document, "schema_version");
    if (!document || !cJSON_IsNumber(version) ||
        (int)version->valuedouble != ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        cJSON_Delete(document);
        return URSATZ_ERR_REQUEST;
    }
    UrsatzStatus status = analyzer_text_export_piece_tree(notes, document, options, text);
    cJSON_Delete(document);
    return status;
}

UrsatzStatus analyzer_text_export_piece(const ImportedScore *notes, const char *document_json,                                           const AnalyzerTextExportOptions *options, char **text, UrsatzError *error) {
    return analyzer_status_error(error, text_export_piece(notes, document_json, options, text), "export the piece as text");
}
