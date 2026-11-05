#include <ursatz_analyzer/text_export.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/score_export.h>
#include <ursatz_analyzer/version.h>

#include "export/score_text_reader.h"

/* ---- a growing string ---- */

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
    bool failed;
} Text;

static void text_reserve(Text *text, size_t extra) {
    if (text->failed || text->length + extra + 1 <= text->capacity) {
        return;
    }
    size_t capacity = text->capacity ? text->capacity : 256;
    while (capacity < text->length + extra + 1) {
        capacity *= 2;
    }
    char *grown = realloc(text->data, capacity);
    if (!grown) {
        text->failed = true;
        return;
    }
    text->data = grown;
    text->capacity = capacity;
}

static void text_add(Text *text, const char *piece) {
    size_t length = strlen(piece);
    text_reserve(text, length);
    if (!text->failed) {
        memcpy(text->data + text->length, piece, length + 1);
        text->length += length;
    }
}

static void text_addf(Text *text, const char *format, ...) {
    char small[256];
    va_list args;
    va_start(args, format);
    int needed = vsnprintf(small, sizeof(small), format, args);
    va_end(args);
    if (needed < 0) {
        text->failed = true;
        return;
    }
    if ((size_t)needed < sizeof(small)) {
        text_add(text, small);
        return;
    }
    text_reserve(text, (size_t)needed);
    if (text->failed) {
        return;
    }
    va_start(args, format);
    vsnprintf(text->data + text->length, (size_t)needed + 1, format, args);
    va_end(args);
    text->length += (size_t)needed;
}

AnalyzerTextExportOptions analyzer_text_export_options_default(void) {
    AnalyzerTextExportOptions options = {NULL, 0, 0};
    return options;
}

/* ---- positions in the score ---- */

/* The index of the last measure that starts at or before t (0 when t is before the first measure). */
static size_t measure_at(const ScoreTextScore *score, TextRational t) {
    size_t low = 0;
    size_t high = score->measure_count;
    while (high - low > 1) {
        size_t middle = low + (high - low) / 2;
        if (text_rational_compare(score->measures[middle].start, t) <= 0) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low;
}

/* The index of the measure that holds the instant just before t: the measure a claim ending at t last touches. */
static size_t measure_before(const ScoreTextScore *score, TextRational t) {
    size_t at = measure_at(score, t);
    if (at > 0 && text_rational_compare(score->measures[at].start, t) == 0) {
        return at - 1;
    }
    return at;
}

/* The 1-based beat of time t counted in measure m, a pickup aligned to the end of its bar. */
static TextRational beat_at(const ScoreTextScore *score, size_t m, TextRational t) {
    const ScoreTextMeasure *measure = &score->measures[m];
    TextRational inside = text_rational_sub(t, measure->start);
    if (measure->pickup) {
        inside = text_rational_add(inside, text_rational_sub(text_rational(measure->beats, measure->beat_type), measure->length));
    }
    return text_rational_add(text_rational(1, 1), text_rational_mul(inside, text_rational(measure->beat_type, 1)));
}

/* A beat as exact text: an integer, a terminating decimal ("2.5"), or whole plus fraction ("1+1/3"). */
static void add_beat(Text *text, TextRational beat) {
    if (beat.den == 1) {
        text_addf(text, "%lld", (long long)beat.num);
        return;
    }
    int64_t scale = 1;
    int64_t digits = 0;
    while (scale % beat.den != 0 && digits < 9) {
        scale *= 10;
        digits++;
    }
    if (scale % beat.den == 0) {
        int64_t scaled = beat.num * (scale / beat.den);
        int64_t whole = scaled / scale;
        int64_t fraction = scaled % scale;
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%0*lld", (int)digits, (long long)fraction);
        size_t length = strlen(buffer);
        while (length > 0 && buffer[length - 1] == '0') {
            buffer[--length] = '\0';
        }
        text_addf(text, "%lld.%s", (long long)whole, buffer);
        return;
    }
    int64_t whole = beat.num / beat.den;
    text_addf(text, "%lld+%lld/%lld", (long long)whole, (long long)(beat.num - whole * beat.den), (long long)beat.den);
}

/* "m12", or "m9-m12" for a range of measures. */
static void add_measure_range(Text *text, const ScoreTextScore *score, size_t first, size_t last) {
    text_addf(text, "m%s", score->measures[first].number);
    if (last != first) {
        text_addf(text, "-m%s", score->measures[last].number);
    }
}

/* ---- notes ---- */

static void add_pitch(Text *text, const ScoreTextPitch *pitch) {
    text_addf(text, "%c", pitch->step);
    for (int i = 0; i < pitch->alter; i++) {
        text_add(text, "#");
    }
    for (int i = 0; i > pitch->alter; i--) {
        text_add(text, "b");
    }
    text_addf(text, "%d", pitch->octave);
}

static void add_event(Text *text, const ScoreTextEvent *event) {
    if (event->rest) {
        text_add(text, "r");
    }
    for (size_t i = 0; i < event->pitch_count; i++) {
        if (i > 0) {
            text_add(text, "+");
        }
        add_pitch(text, &event->pitches[i]);
    }
    text_add(text, " @");
    add_beat(text, event->beat_in_measure);
    text_add(text, " ");
    if (event->tuplet_actual > 0 && event->tuplet_normal > 0) {
        text_addf(text, "%d:%d", event->tuplet_actual, event->tuplet_normal);
    }
    text_addf(text, "%c", event->type);
    for (int i = 0; i < event->dots; i++) {
        text_add(text, ".");
    }
    if (event->tie_start) {
        text_add(text, "~");
    }
}

static void add_measure_notes(Text *text, const ScoreTextMeasure *measure) {
    size_t i = 0;
    while (i < measure->event_count) {
        const ScoreTextEvent *first = &measure->events[i];
        text_addf(text, " P%d S%d V%d:", first->part, first->staff, first->voice);
        bool opened = false;
        while (i < measure->event_count && measure->events[i].part == first->part &&
               measure->events[i].staff == first->staff && measure->events[i].voice == first->voice) {
            text_add(text, opened ? ", " : " ");
            add_event(text, &measure->events[i]);
            opened = true;
            i++;
        }
        text_add(text, "\n");
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
    RANK_MOTIVE
} ClaimRank;

typedef struct {
    size_t measure; /* where the claim starts */
    TextRational start;
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

static bool rational_of(const cJSON *rational, TextRational *out) {
    const cJSON *num = member(rational, "num");
    const cJSON *den = member(rational, "den");
    if (!cJSON_IsNumber(num) || !cJSON_IsNumber(den) || den->valuedouble == 0) {
        return false;
    }
    *out = text_rational((int64_t)num->valuedouble, (int64_t)den->valuedouble);
    return true;
}

/* The claim's span: start, and end (the start when the span has none). */
static bool span_of(const cJSON *claim, TextRational *start, TextRational *end) {
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
static void add_confidence(Text *text, const cJSON *claim) {
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
    text_addf(text, " c=%.2f", confidence);
}

static bool claims_add(Claims *claims, const ScoreTextScore *score, TextRational start, ClaimRank rank, Text *line) {
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
static Text line_begin(const char *words) {
    Text line = {NULL, 0, 0, false};
    text_add(&line, " ");
    text_add(&line, words);
    return line;
}

static void add_span_range(Text *line, const ScoreTextScore *score, TextRational start, TextRational end) {
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
        TextRational start, end;
        const char *label = string_of(region, "label");
        if (!label || !span_of(region, &start, &end)) {
            continue;
        }
        Text line = line_begin("key ");
        text_add(&line, label);
        add_confidence(&line, region);
        text_add(&line, " (");
        add_span_range(&line, score, start, end);
        text_add(&line, ")");
        claims_add(claims, score, start, RANK_KEY, &line);
    }
}

static void collect_key_changes(Claims *claims, const ScoreTextScore *score, const cJSON *document, const char *section,
                                const char *word, ClaimRank rank) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, member(document, section)) {
        TextRational start, end;
        const char *to_key = string_of(entry, "to_key");
        const char *from_key = string_of(entry, "from_key");
        if (!span_of(entry, &start, &end)) {
            continue;
        }
        Text line = line_begin(word);
        text_add(&line, " ");
        if (rank == RANK_MODULATION && from_key && to_key) {
            text_addf(&line, "%s to %s", from_key, to_key);
        } else if (to_key) {
            text_add(&line, to_key);
        } else if (string_of(entry, "label")) {
            text_add(&line, string_of(entry, "label"));
        }
        add_confidence(&line, entry);
        text_add(&line, " (");
        add_span_range(&line, score, start, end);
        text_add(&line, ")");
        claims_add(claims, score, start, rank, &line);
    }
}

static const char *quality_suffix(const char *quality) {
    static const struct {
        const char *quality;
        const char *suffix;
    } SUFFIXES[] = {{"major", ""},         {"minor", "m"},     {"dominant7", "7"},
                    {"major7", "maj7"},    {"minor7", "m7"},   {"diminished", "dim"},
                    {"diminished7", "dim7"}, {"half-diminished7", "m7b5"}, {"augmented", "aug"}};
    for (size_t i = 0; i < sizeof(SUFFIXES) / sizeof(SUFFIXES[0]); i++) {
        if (strcmp(quality, SUFFIXES[i].quality) == 0) {
            return SUFFIXES[i].suffix;
        }
    }
    return quality;
}

static void collect_chords(Claims *claims, const ScoreTextScore *score, const cJSON *document) {
    const cJSON *chord;
    cJSON_ArrayForEach(chord, member(document, "chords")) {
        TextRational start, end;
        const char *root = string_of(chord, "root");
        const char *quality = string_of(chord, "quality");
        if (!root || !quality || !span_of(chord, &start, &end)) {
            continue;
        }
        const char *bass = string_of(chord, "bass");
        const char *numeral = string_of(member(chord, "roman_numeral"), "label");
        size_t m = measure_at(score, start);
        Text line = line_begin("chord ");
        text_addf(&line, "%s%s", root, quality_suffix(quality));
        if (bass && strcmp(bass, root) != 0) {
            text_addf(&line, "/%s", bass);
        }
        if (numeral) {
            text_addf(&line, " %s", numeral);
        }
        text_add(&line, " @");
        add_beat(&line, beat_at(score, m, start));
        add_confidence(&line, chord);
        claims_add(claims, score, start, RANK_CHORD, &line);
    }
}

static const char *cadence_name(const char *type) {
    static const struct {
        const char *type;
        const char *name;
    } NAMES[] = {{"perfect-authentic", "PAC"},   {"imperfect-authentic", "IAC"}, {"authentic-unspecified", "AC"},
                 {"half", "HC"},                 {"plagal", "PC"},               {"deceptive", "DC"}};
    for (size_t i = 0; i < sizeof(NAMES) / sizeof(NAMES[0]); i++) {
        if (strcmp(type, NAMES[i].type) == 0) {
            return NAMES[i].name;
        }
    }
    return type;
}

static void collect_cadences(Claims *claims, const ScoreTextScore *score, const cJSON *document) {
    const cJSON *cadence;
    cJSON_ArrayForEach(cadence, member(document, "cadences")) {
        TextRational start, end;
        const char *type = string_of(cadence, "type");
        if (!type) {
            type = string_of(cadence, "label");
        }
        if (!type || !span_of(cadence, &start, &end)) {
            continue;
        }
        /* The cadence starts at its dominant; the line names the measure and beat where it arrives. */
        TextRational arrival = start;
        TextRational ignored;
        span_of(member(cadence, "arrival"), &arrival, &ignored);
        size_t m = measure_at(score, arrival);
        Text line = line_begin("cadence ");
        text_addf(&line, "%s m%s @", cadence_name(type), score->measures[m].number);
        add_beat(&line, beat_at(score, m, arrival));
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
        TextRational start, end;
        const cJSON *index = member(phrase, "index");
        if (!cJSON_IsNumber(index) || !span_of(phrase, &start, &end)) {
            continue;
        }
        Text line = line_begin("phrase ");
        text_addf(&line, "%d ", (int)index->valuedouble + 1);
        add_span_range(&line, score, start, end);
        add_confidence(&line, phrase);
        const char *form = form_section_of_phrase(document, (int)index->valuedouble);
        if (form) {
            text_addf(&line, " | form %s", form);
        }
        claims_add(claims, score, start, RANK_PHRASE, &line);
    }
}

static void collect_labelled(Claims *claims, const ScoreTextScore *score, const cJSON *entries, const char *word,
                             ClaimRank rank) {
    const cJSON *entry;
    cJSON_ArrayForEach(entry, entries) {
        TextRational start, end;
        const char *label = string_of(entry, "label");
        if (!label || !span_of(entry, &start, &end)) {
            continue;
        }
        Text line = line_begin(word);
        text_addf(&line, " %s ", label);
        add_span_range(&line, score, start, end);
        add_confidence(&line, entry);
        claims_add(claims, score, start, rank, &line);
    }
}

/* a, b, ... z, aa, ab, ... for motive 0, 1, ... */
static void add_motive_letter(Text *text, int index) {
    char letters[8];
    size_t length = 0;
    int n = index;
    do {
        letters[length++] = (char)('a' + n % 26);
        n = n / 26 - 1;
    } while (n >= 0 && length < sizeof(letters) - 1);
    while (length > 0) {
        char one[2] = {letters[--length], '\0'};
        text_add(text, one);
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
            TextRational start, end;
            if (!span_of(occurrence, &start, &end)) {
                continue;
            }
            size_t first = measure_at(score, start);
            size_t last = measure_before(score, end);
            if (last < first) {
                last = first;
            }
            Text line = line_begin("motive ");
            add_motive_letter(&line, (int)index->valuedouble);
            text_addf(&line, " m%s @", score->measures[first].number);
            add_beat(&line, beat_at(score, first, start));
            text_add(&line, "-");
            if (last != first) {
                text_addf(&line, "m%s", score->measures[last].number);
            }
            text_add(&line, "@");
            add_beat(&line, beat_at(score, last, end));
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
    int by_start = text_rational_compare(a->start, b->start);
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

static void add_tempo(Text *text, const cJSON *document) {
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
        text_addf(text, "q=%s", buffer);
    } else {
        text_add(text, "not declared");
    }
}

static void add_header(Text *text, const cJSON *document, const ScoreTextScore *score, const char *title,
                       size_t first, size_t last, bool whole) {
    text_addf(text, "# Ursatz export v%d: piece\n", URSATZ_ANALYZER_TEXT_EXPORT_VERSION);
    text_addf(text, "title: %s\n", title);

    const cJSON *regions = member(member(document, "keys"), "regions");
    const char *key = string_of(cJSON_GetArrayItem(regions, 0), "label");
    text_addf(text, "key: %s | meter: %d/%d | tempo: ", key ? key : "unknown", score->measures[0].beats,
              score->measures[0].beat_type);
    add_tempo(text, document);
    text_addf(text, " | measures: %s-%s", score->measures[first].number, score->measures[last].number);
    if (!whole) {
        text_addf(text, " of %s-%s", score->measures[0].number, score->measures[score->measure_count - 1].number);
    }
    text_add(text, "\n");

    char commit[13];
    snprintf(commit, sizeof(commit), "%.12s", ursatz_analyzer_ursatz_commit());
    text_addf(text, "analyzer: %s ursatz %s\n", ursatz_analyzer_version(), commit);
    text_add(text, "notes: <pitch><octave> @<beat> <duration>; r = rest; ~ = tied to next; 3:2 = tuplet; c=<confidence>\n");
}

AnalyzerStatus analyzer_text_export_piece(const ImportedNotes *notes, const char *document_json,
                                          const AnalyzerTextExportOptions *options, char **text) {
    if (text) {
        *text = NULL;
    }
    if (!notes || !document_json || !options || !text) {
        return ANALYZER_ERR_ARGUMENT;
    }
    bool whole_piece = options->first_measure == 0 && options->last_measure == 0;
    if (options->first_measure < 0 || options->last_measure < 0 ||
        (!whole_piece && options->first_measure > options->last_measure)) {
        return ANALYZER_ERR_ARGUMENT;
    }

    cJSON *document = cJSON_Parse(document_json);
    const cJSON *version = member(document, "schema_version");
    if (!document || !cJSON_IsNumber(version) ||
        (int)version->valuedouble != URSATZ_ANALYZER_DOCUMENT_SCHEMA_VERSION) {
        cJSON_Delete(document);
        return ANALYZER_ERR_ARGUMENT;
    }

    char *musicxml = NULL;
    AnalyzerStatus status = analyzer_score_export_musicxml(notes, &musicxml);
    if (status != ANALYZER_OK) {
        cJSON_Delete(document);
        return status;
    }
    char *error = NULL;
    ScoreTextScore *score = score_text_read(musicxml, &error);
    free(musicxml);
    free(error);
    if (!score) {
        cJSON_Delete(document);
        return ANALYZER_ERR_LIBRARY;
    }

    size_t first = score->measure_count;
    size_t last = 0;
    for (size_t m = 0; m < score->measure_count; m++) {
        if (in_range(score, m, options->first_measure, options->last_measure)) {
            if (first == score->measure_count) {
                first = m;
            }
            last = m;
        }
    }
    if (first == score->measure_count) {
        score_text_destroy(score);
        cJSON_Delete(document);
        return ANALYZER_ERR_ARGUMENT;
    }
    bool whole = first == 0 && last == score->measure_count - 1;

    Claims claims = {NULL, 0, 0, 0, false};
    collect_keys(&claims, score, document);
    collect_key_changes(&claims, score, document, "tonicizations", "tonicization", RANK_TONICIZATION);
    collect_key_changes(&claims, score, document, "modulations", "modulation", RANK_MODULATION);
    collect_chords(&claims, score, document);
    collect_cadences(&claims, score, document);
    collect_phrases(&claims, score, document);
    collect_labelled(&claims, score, member(document, "themes"), "theme", RANK_THEME);
    collect_labelled(&claims, score, member(member(document, "form"), "sections"), "form", RANK_FORM);
    collect_motives(&claims, score, document);
    qsort(claims.items, claims.count, sizeof(Claim), claim_compare);

    const char *title = options->title && options->title[0] ? options->title : NULL;
    if (!title) {
        title = string_of(member(document, "metadata"), "source_file");
    }
    Text out = {NULL, 0, 0, claims.failed};
    add_header(&out, document, score, title ? title : "untitled", first, last, whole);

    size_t next_claim = 0;
    while (next_claim < claims.count && claims.items[next_claim].measure < first) {
        next_claim++;
    }
    for (size_t m = first; m <= last; m++) {
        const ScoreTextMeasure *measure = &score->measures[m];
        text_add(&out, "\n");
        text_addf(&out, "m%s", measure->number);
        if (measure->time_changed) {
            text_addf(&out, " %d/%d", measure->beats, measure->beat_type);
        }
        text_add(&out, "\n");
        add_measure_notes(&out, measure);
        while (next_claim < claims.count && claims.items[next_claim].measure == m) {
            text_add(&out, claims.items[next_claim].line);
            text_add(&out, "\n");
            next_claim++;
        }
    }

    claims_free(&claims);
    score_text_destroy(score);
    cJSON_Delete(document);
    if (out.failed) {
        free(out.data);
        return ANALYZER_ERR_ALLOC;
    }
    *text = out.data;
    return ANALYZER_OK;
}
