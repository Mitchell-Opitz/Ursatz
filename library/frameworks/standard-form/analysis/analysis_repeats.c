#include "analysis_repeats.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "time/meter_arithmetic.h"
#include "time/score_time.h"

#define DEFAULT_MIN_MEASURES 4u

typedef struct {
    int measure;
    Rational onset; /* from the start of its measure */
    long semitone;
    Rational duration;
} Entry;

typedef struct {
    int first; /* the measure number of slot 0 */
    size_t measures;
    Entry *entries; /* every note, sorted by measure, onset, semitone, duration */
    size_t *offset; /* per measure + 1: where its entries start */
    Entry *soprano; /* the highest semitone at each onset, per measure in onset order */
    size_t *soprano_offset;
} Table;

typedef struct {
    StandardFormRepeat *items;
    size_t count;
    size_t capacity;
} List;

static void table_free(Table *table) {
    free(table->entries);
    free(table->offset);
    free(table->soprano);
    free(table->soprano_offset);
    memset(table, 0, sizeof(*table));
}

static void list_free(List *list) {
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool list_add(List *list, const StandardFormRepeat *item) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity == 0 ? 8 : list->capacity * 2;
        StandardFormRepeat *grown = (StandardFormRepeat *)realloc(list->items, capacity * sizeof(StandardFormRepeat));
        if (grown == NULL) {
            return false;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count++] = *item;
    return true;
}

static int entry_cmp(const void *left, const void *right) {
    const Entry *a = (const Entry *)left;
    const Entry *b = (const Entry *)right;
    if (a->measure != b->measure) {
        return a->measure < b->measure ? -1 : 1;
    }
    int c = rational_cmp(a->onset, b->onset);
    if (c != 0) {
        return c;
    }
    if (a->semitone != b->semitone) {
        return a->semitone < b->semitone ? -1 : 1;
    }
    return rational_cmp(a->duration, b->duration);
}

static int repeat_cmp(const void *left, const void *right) {
    const StandardFormRepeat *a = (const StandardFormRepeat *)left;
    const StandardFormRepeat *b = (const StandardFormRepeat *)right;
    if (a->later_first != b->later_first) {
        return a->later_first < b->later_first ? -1 : 1;
    }
    if (a->earlier_first != b->earlier_first) {
        return a->earlier_first < b->earlier_first ? -1 : 1;
    }
    if (a->kind != b->kind) {
        return a->kind < b->kind ? -1 : 1;
    }
    if (a->later_last != b->later_last) {
        return a->later_last < b->later_last ? -1 : 1;
    }
    return 0;
}

static StandardFormAnalysisStatus table_build(const StandardFormRepeatNote *notes, size_t count, const MeterMap *map,
                                              Table *table) {
    memset(table, 0, sizeof(*table));
    Entry *entries = (Entry *)calloc(count, sizeof(Entry));
    if (entries == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    int first = 0;
    int last = 0;
    for (size_t i = 0; i < count; i++) {
        Rational start;
        Rational end;
        Rational measure_start;
        int measure = 0;
        if (!score_time_span(notes[i].span, &start, &end) || meter_map_measure_of(map, start, &measure) != METER_MAP_OK ||
            meter_map_measure_bounds(map, measure, &measure_start, NULL) != METER_MAP_OK ||
            rational_sub(start, measure_start, &entries[i].onset) != RATIONAL_OK ||
            rational_sub(end, start, &entries[i].duration) != RATIONAL_OK) {
            free(entries);
            return STANDARD_FORM_ANALYSIS_ERR_METER;
        }
        entries[i].measure = measure;
        entries[i].semitone = notes[i].semitone;
        if (i == 0 || measure < first) {
            first = measure;
        }
        if (i == 0 || measure > last) {
            last = measure;
        }
    }
    qsort(entries, count, sizeof(Entry), entry_cmp);
    table->first = first;
    table->measures = (size_t)(last - first) + 1;
    table->entries = entries;
    table->offset = (size_t *)calloc(table->measures + 1, sizeof(size_t));
    table->soprano = (Entry *)calloc(count, sizeof(Entry));
    table->soprano_offset = (size_t *)calloc(table->measures + 1, sizeof(size_t));
    if (table->offset == NULL || table->soprano == NULL || table->soprano_offset == NULL) {
        table_free(table);
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        table->offset[(size_t)(entries[i].measure - first) + 1]++;
    }
    for (size_t m = 0; m < table->measures; m++) {
        table->offset[m + 1] += table->offset[m];
    }
    size_t sopranos = 0;
    for (size_t m = 0; m < table->measures; m++) {
        table->soprano_offset[m] = sopranos;
        for (size_t i = table->offset[m]; i < table->offset[m + 1]; i++) {
            /* sorted by semitone within an onset, so the last note of an onset is its highest */
            bool last_at_onset = i + 1 == table->offset[m + 1] || !rational_equals(entries[i + 1].onset, entries[i].onset);
            if (last_at_onset) {
                table->soprano[sopranos++] = entries[i];
            }
        }
    }
    table->soprano_offset[table->measures] = sopranos;
    return STANDARD_FORM_ANALYSIS_OK;
}

static size_t note_count(const Table *table, size_t m) {
    return table->offset[m + 1] - table->offset[m];
}

/* Equal contents, exactly; a measure with no notes matches nothing. */
static bool content_equal(const Table *table, size_t a, size_t b) {
    size_t n = note_count(table, a);
    if (n == 0 || n != note_count(table, b)) {
        return false;
    }
    const Entry *x = &table->entries[table->offset[a]];
    const Entry *y = &table->entries[table->offset[b]];
    for (size_t i = 0; i < n; i++) {
        if (x[i].semitone != y[i].semitone || !rational_equals(x[i].onset, y[i].onset) ||
            !rational_equals(x[i].duration, y[i].duration)) {
            return false;
        }
    }
    return true;
}

/* Sopranos with equal onsets that differ by one constant semitone count; *shift is that count. */
static bool soprano_shift(const Table *table, size_t a, size_t b, long *shift) {
    size_t n = table->soprano_offset[a + 1] - table->soprano_offset[a];
    if (n == 0 || n != table->soprano_offset[b + 1] - table->soprano_offset[b]) {
        return false;
    }
    const Entry *x = &table->soprano[table->soprano_offset[a]];
    const Entry *y = &table->soprano[table->soprano_offset[b]];
    long step = y[0].semitone - x[0].semitone;
    for (size_t i = 0; i < n; i++) {
        if (!rational_equals(x[i].onset, y[i].onset) || y[i].semitone - x[i].semitone != step) {
            return false;
        }
    }
    *shift = step;
    return true;
}

static StandardFormAnalysisStatus find_repeats(const Table *table, size_t min_measures, List *repeats) {
    size_t measures = table->measures;
    unsigned char *used = (unsigned char *)calloc(measures, sizeof(unsigned char));
    if (used == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
    }
    for (size_t a = 0; a < measures; a++) {
        for (size_t b = a + 1; b < measures; b++) {
            if (!content_equal(table, a, b) || (a > 0 && content_equal(table, a - 1, b - 1))) {
                continue;
            }
            size_t run = 1;
            while (b + run < measures && content_equal(table, a + run, b + run)) {
                run++;
            }
            if (run < min_measures || b - a < run) {
                continue;
            }
            bool taken = false;
            for (size_t k = 0; k < run; k++) {
                taken = taken || used[b + k] != 0;
            }
            if (taken) {
                continue;
            }
            memset(&used[b], 1, run);
            StandardFormRepeat item = {STANDARD_FORM_REPEAT,
                                       table->first + (int)a,
                                       table->first + (int)(a + run) - 1,
                                       table->first + (int)b,
                                       table->first + (int)(b + run) - 1,
                                       0};
            if (!list_add(repeats, &item)) {
                free(used);
                return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
            }
        }
    }
    free(used);
    return STANDARD_FORM_ANALYSIS_OK;
}

static bool returns_from(const Table *table, int earlier_first, int later_first, size_t run, long shift) {
    int e = earlier_first - table->first;
    int l = later_first - table->first;
    if (e < 0 || l < 0 || (size_t)e + run > table->measures || (size_t)l + run > table->measures) {
        return false;
    }
    for (size_t k = 0; k < run; k++) {
        long found = 0;
        if (!soprano_shift(table, (size_t)e + k, (size_t)l + k, &found) || found != shift) {
            return false;
        }
    }
    return true;
}

/* Dropped when implied by a repeat (analysis_repeats.h). */
static bool return_implied(const Table *table, const StandardFormRepeat *item, const List *repeats) {
    size_t run = (size_t)(item->later_last - item->later_first) + 1;
    for (size_t i = 0; i < repeats->count; i++) {
        const StandardFormRepeat *repeat = &repeats->items[i];
        int shift = repeat->later_first - repeat->earlier_first;
        if (item->later_first >= repeat->later_first && item->later_last <= repeat->later_last &&
            returns_from(table, item->earlier_first, item->later_first - shift, run, item->transposition)) {
            return true;
        }
        if (item->earlier_first >= repeat->later_first && item->earlier_last <= repeat->later_last &&
            returns_from(table, item->earlier_first - shift, item->later_first, run, item->transposition)) {
            return true;
        }
    }
    return false;
}

static StandardFormAnalysisStatus find_returns(const Table *table, size_t min_measures, const List *repeats, List *returns) {
    size_t measures = table->measures;
    List candidates = {NULL, 0, 0};
    for (size_t a = 0; a < measures; a++) {
        for (size_t b = a + 1; b < measures; b++) {
            long shift = 0;
            long before = 0;
            if (!soprano_shift(table, a, b, &shift) || (a > 0 && soprano_shift(table, a - 1, b - 1, &before) && before == shift)) {
                continue;
            }
            size_t run = 1;
            bool all_equal = content_equal(table, a, b);
            long next = 0;
            while (b + run < measures && soprano_shift(table, a + run, b + run, &next) && next == shift) {
                all_equal = all_equal && content_equal(table, a + run, b + run);
                run++;
            }
            if (run < min_measures || b - a < run || all_equal) {
                continue;
            }
            StandardFormRepeat item = {STANDARD_FORM_RETURN,
                                       table->first + (int)a,
                                       table->first + (int)(a + run) - 1,
                                       table->first + (int)b,
                                       table->first + (int)(b + run) - 1,
                                       (int)shift};
            if (!list_add(&candidates, &item)) {
                list_free(&candidates);
                return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
            }
        }
    }
    for (size_t i = 0; i < candidates.count; i++) {
        if (!return_implied(table, &candidates.items[i], repeats) && !list_add(returns, &candidates.items[i])) {
            list_free(&candidates);
            return STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
    }
    list_free(&candidates);
    return STANDARD_FORM_ANALYSIS_OK;
}

void standard_form_repeats_destroy(StandardFormRepeats *repeats) {
    if (repeats == NULL) {
        return;
    }
    free(repeats->items);
    repeats->items = NULL;
    repeats->count = 0;
}

StandardFormAnalysisStatus standard_form_find_repeats(const StandardFormRepeatNote *notes, size_t count, const MeterMap *map,
                                                      const StandardFormRepeatConfig *config, StandardFormRepeats *out) {
    if ((notes == NULL && count > 0) || map == NULL || out == NULL) {
        return STANDARD_FORM_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    size_t min_measures = config == NULL ? DEFAULT_MIN_MEASURES : config->min_measures;
    if (min_measures == 0) {
        return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
    }
    for (size_t i = 0; i < count; i++) {
        if (notes[i].span == NULL) {
            return STANDARD_FORM_ANALYSIS_ERR_BAD_INPUT;
        }
    }
    StandardFormRepeats none = {NULL, 0};
    if (count == 0) {
        *out = none;
        return STANDARD_FORM_ANALYSIS_OK;
    }
    Table table;
    StandardFormAnalysisStatus status = table_build(notes, count, map, &table);
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        return status;
    }
    List repeats = {NULL, 0, 0};
    List all = {NULL, 0, 0};
    status = find_repeats(&table, min_measures, &repeats);
    if (status == STANDARD_FORM_ANALYSIS_OK) {
        status = find_returns(&table, min_measures, &repeats, &all);
    }
    for (size_t i = 0; status == STANDARD_FORM_ANALYSIS_OK && i < repeats.count; i++) {
        if (!list_add(&all, &repeats.items[i])) {
            status = STANDARD_FORM_ANALYSIS_ERR_ALLOC;
        }
    }
    list_free(&repeats);
    table_free(&table);
    if (status != STANDARD_FORM_ANALYSIS_OK) {
        list_free(&all);
        return status;
    }
    if (all.count > 1) {
        qsort(all.items, all.count, sizeof(StandardFormRepeat), repeat_cmp);
    }
    out->items = all.items;
    out->count = all.count;
    return STANDARD_FORM_ANALYSIS_OK;
}
