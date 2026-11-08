#include "export/score_clefs.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "identity/entity_id.h"
#include "pitch/pitch_spelling.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_rational.h"
#include "time/time_span.h"

/* The price of a change of clef, in ledger line quarter notes: a change is worth it when the other clef saves more
   than this many ledger line quarter notes before the clef changes again. */
#define SCORE_CLEF_CHANGE_COST 4

/* Diatonic positions (octave * 7 + step above C) of the bottom and top line of each staff. */
#define TREBLE_BOTTOM_LINE 30 /* E4 */
#define TREBLE_TOP_LINE 38    /* F5 */
#define BASS_BOTTOM_LINE 18   /* G2 */
#define BASS_TOP_LINE 26      /* A3 */

#define CLEF_COUNT 2 /* index 0 treble, 1 bass */

void score_clef_changes_destroy(ScoreClefChanges *changes) {
    if (changes == NULL) {
        return;
    }
    free(changes->part);
    free(changes->staff);
    free(changes->measure);
    free(changes->clef);
    memset(changes, 0, sizeof(*changes));
}

static ScoreClef clef_of_index(int index) {
    return index == 0 ? SCORE_CLEF_TREBLE : SCORE_CLEF_BASS;
}

/* --- ledger lines --- */

/* The diatonic position of a written pitch: C4 is 28, D4 29, ... PitchLetter counts from A. */
static long staff_position(const PitchSpelling *spelling) {
    long step = ((long)pitch_spelling_letter(spelling) + 5) % 7;
    return (long)pitch_spelling_octave(spelling) * 7 + step;
}

/* The ledger lines a note at position needs in the staff whose lines run from bottom to top: one per ledger line it
   reaches, a note on a ledger line or in the space just beyond it counting that line. */
static long ledger_lines(long position, long bottom, long top) {
    if (position < bottom) {
        return (bottom - position) / 2;
    }
    if (position > top) {
        return (position - top) / 2;
    }
    return 0;
}

/* --- the notes --- */

typedef struct {
    size_t slot; /* the (part, staff) pair, see slot_of */
    Rational start;
    Rational end;
    long lines[CLEF_COUNT];
} Note;

static ScoreClefResult read_time(const ScoreQuantized *notated, size_t i, NoteEvent *event, Rational *start,
                                 Rational *end) {
    if (notated != NULL) {
        if (notated->start_num[i] > (unsigned long long)LLONG_MAX || notated->start_den[i] > (unsigned long long)LLONG_MAX ||
            notated->end_num[i] > (unsigned long long)LLONG_MAX || notated->end_den[i] > (unsigned long long)LLONG_MAX ||
            rational_make((long long)notated->start_num[i], (long long)notated->start_den[i], start) != RATIONAL_OK ||
            rational_make((long long)notated->end_num[i], (long long)notated->end_den[i], end) != RATIONAL_OK) {
            return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
        }
        return rational_cmp(*end, *start) > 0 ? SCORE_CLEFS_OK : SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
    }
    const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(event));
    const Duration *duration = note_event_duration(event);
    if (musical_time_numerator(position) < 0 || duration_numerator(duration) <= 0) {
        return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
    }
    Rational length;
    if (rational_of_musical_time(position, start) != RATIONAL_OK || rational_of_duration(duration, &length) != RATIONAL_OK ||
        rational_add(*start, length, end) != RATIONAL_OK) {
        return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
    }
    return SCORE_CLEFS_OK;
}

/* --- the measures --- */

typedef struct {
    Rational *start;
    Rational *end;
    size_t count;
} Measures;

static void measures_free(Measures *measures) {
    free(measures->start);
    free(measures->end);
    memset(measures, 0, sizeof(*measures));
}

/* The measures of meter that cover everything up to extent, at least one. */
static ScoreClefResult measures_lay(const MeterMap *meter, Rational extent, Measures *measures) {
    MusicalTime *extent_time = NULL;
    if (musical_time_from_rational(extent, &extent_time) != TIME_RATIONAL_OK) {
        return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
    }
    size_t count = 0;
    MeterMapResult status = meter_map_measure_count(meter, extent_time, &count);
    musical_time_destroy(extent_time);
    if (status != METER_MAP_OK) {
        return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
    }
    if (count == 0) {
        count = 1;
    }
    measures->start = (Rational *)calloc(count, sizeof(Rational));
    measures->end = (Rational *)calloc(count, sizeof(Rational));
    if (measures->start == NULL || measures->end == NULL) {
        measures_free(measures);
        return SCORE_CLEFS_ERR_ALLOC;
    }
    int first = meter_map_first_measure_number(meter);
    for (size_t m = 0; m < count; m++) {
        TimeSpan *span = NULL;
        if (meter_map_measure_span(meter, first + (int)m, &span) != METER_MAP_OK) {
            measures_free(measures);
            return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
        }
        bool converted = rational_of_musical_time(time_span_start(span), &measures->start[m]) == RATIONAL_OK &&
                         rational_of_musical_time(time_span_end(span), &measures->end[m]) == RATIONAL_OK;
        time_span_destroy(span);
        if (!converted) {
            measures_free(measures);
            return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
        }
    }
    measures->count = count;
    return SCORE_CLEFS_OK;
}

/* --- the cost per measure --- */

/* One staff's cost of each clef in each measure, and whether any note is on it there. */
typedef struct {
    Rational (*cost)[CLEF_COUNT]; /* measure_count entries */
    bool *has_notes;
} StaffCosts;

/* Adds each note's ledger line quarter notes to the measures it overlaps. */
static ScoreClefResult add_costs(const Note *notes, size_t note_count, const Measures *measures, StaffCosts *staves) {
    for (size_t i = 0; i < note_count; i++) {
        const Note *note = &notes[i];
        /* the first measure that ends after the note starts */
        size_t low = 0;
        size_t high = measures->count;
        while (low < high) {
            size_t middle = low + (high - low) / 2;
            if (rational_cmp(measures->end[middle], note->start) > 0) {
                high = middle;
            } else {
                low = middle + 1;
            }
        }
        for (size_t m = low; m < measures->count && rational_cmp(measures->start[m], note->end) < 0; m++) {
            Rational from = rational_cmp(note->start, measures->start[m]) > 0 ? note->start : measures->start[m];
            Rational to = rational_cmp(note->end, measures->end[m]) < 0 ? note->end : measures->end[m];
            Rational length;
            if (rational_sub(to, from, &length) != RATIONAL_OK || rational_mul_int(length, 4, &length) != RATIONAL_OK) {
                return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
            }
            StaffCosts *staff = &staves[note->slot];
            staff->has_notes[m] = true;
            for (int c = 0; c < CLEF_COUNT; c++) {
                Rational part;
                if (rational_mul_int(length, note->lines[c], &part) != RATIONAL_OK ||
                    rational_add(staff->cost[m][c], part, &staff->cost[m][c]) != RATIONAL_OK) {
                    return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
                }
            }
        }
    }
    return SCORE_CLEFS_OK;
}

/* --- choosing the clefs --- */

typedef struct {
    size_t part;
    int staff;
    size_t measure; /* 1-based */
    ScoreClef clef;
    bool first; /* the measure 1 entry */
} Entry;

typedef struct {
    Entry *items;
    size_t count;
    size_t capacity;
} EntryList;

static ScoreClefResult entry_add(EntryList *list, size_t part, int staff, size_t measure, ScoreClef clef, bool first) {
    if (list->count == list->capacity) {
        size_t capacity = list->capacity > 0 ? list->capacity * 2 : 16;
        Entry *grown = (Entry *)realloc(list->items, capacity * sizeof(Entry));
        if (grown == NULL) {
            return SCORE_CLEFS_ERR_ALLOC;
        }
        list->items = grown;
        list->capacity = capacity;
    }
    list->items[list->count].part = part;
    list->items[list->count].staff = staff;
    list->items[list->count].measure = measure;
    list->items[list->count].clef = clef;
    list->items[list->count].first = first;
    list->count++;
    return SCORE_CLEFS_OK;
}

/* The cost of a change into clef: SCORE_CLEF_CHANGE_COST when it differs from the clef before it. */
static Rational change_cost(int from, int to) {
    return rational_ratio(from == to ? 0 : SCORE_CLEF_CHANGE_COST, 1);
}

static ScoreClefResult add_rationals(Rational a, Rational b, Rational *out) {
    return rational_add(a, b, out) == RATIONAL_OK ? SCORE_CLEFS_OK : SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
}

/*
 * Chooses the clef of each measure with notes on one staff and appends the measure 1 entry and the changes.
 * start_clef is the clef the staff starts in at no cost, or -1 for a staff that starts in either (the cheaper one).
 * to_go and noted are scratch arrays of measure_count entries.
 */
static ScoreClefResult choose_staff(const StaffCosts *staff, size_t measure_count, size_t part, int staff_number,
                                    int start_clef, Rational (*to_go)[CLEF_COUNT], size_t *noted, EntryList *list) {
    size_t noted_count = 0;
    for (size_t m = 0; m < measure_count; m++) {
        if (staff->has_notes[m]) {
            noted[noted_count++] = m;
        }
    }
    if (noted_count == 0) {
        return entry_add(list, part, staff_number, 1, clef_of_index(start_clef < 0 ? 0 : start_clef), true);
    }

    /* to_go[k][c]: the cheapest cost of the noted measures from k on when measure k is in clef c */
    for (size_t k = noted_count; k-- > 0;) {
        for (int c = 0; c < CLEF_COUNT; c++) {
            Rational best = rational_ratio(0, 1);
            if (k + 1 < noted_count) {
                Rational stay = to_go[k + 1][c];
                Rational move;
                if (add_rationals(to_go[k + 1][1 - c], change_cost(1 - c, c), &move) != SCORE_CLEFS_OK) {
                    return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
                }
                best = rational_cmp(move, stay) < 0 ? move : stay;
            }
            if (add_rationals(staff->cost[noted[k]][c], best, &to_go[k][c]) != SCORE_CLEFS_OK) {
                return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
            }
        }
    }

    /* walk forward, taking the cheaper clef and keeping the current one on a tie */
    int current = start_clef;
    for (size_t k = 0; k < noted_count; k++) {
        Rational total[CLEF_COUNT];
        for (int c = 0; c < CLEF_COUNT; c++) {
            total[c] = to_go[k][c];
            if (current >= 0 && add_rationals(total[c], change_cost(current, c), &total[c]) != SCORE_CLEFS_OK) {
                return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
            }
        }
        int pick = current >= 0 ? current : 0;
        int other = 1 - pick;
        if (rational_cmp(total[other], total[pick]) < 0) {
            pick = other;
        }
        size_t measure = noted[k] + 1;
        if (k == 0) {
            /* the clef at measure 1: the pick itself when the notes start there or the staff starts free */
            ScoreClefResult status = entry_add(list, part, staff_number, 1,
                                               clef_of_index(measure == 1 || start_clef < 0 ? pick : start_clef), true);
            if (status != SCORE_CLEFS_OK) {
                return status;
            }
            if (measure != 1 && start_clef >= 0 && pick != start_clef) {
                status = entry_add(list, part, staff_number, measure, clef_of_index(pick), false);
                if (status != SCORE_CLEFS_OK) {
                    return status;
                }
            }
        } else if (pick != current) {
            ScoreClefResult status = entry_add(list, part, staff_number, measure, clef_of_index(pick), false);
            if (status != SCORE_CLEFS_OK) {
                return status;
            }
        }
        current = pick;
    }
    return SCORE_CLEFS_OK;
}

static int compare_entries(const void *left, const void *right) {
    const Entry *a = (const Entry *)left;
    const Entry *b = (const Entry *)right;
    if (a->first != b->first) {
        return a->first ? -1 : 1;
    }
    if (a->measure != b->measure) {
        return a->measure < b->measure ? -1 : 1;
    }
    if (a->part != b->part) {
        return a->part < b->part ? -1 : 1;
    }
    return a->staff - b->staff;
}

/* --- infer --- */

ScoreClefResult score_clefs_infer(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                  const size_t *part_index, const int *staff, const int *part_staff_count,
                                  size_t part_count, const MeterMap *meter_map, const ScoreQuantized *notated,
                                  ScoreClefChanges *out) {
    if (out == NULL) {
        return SCORE_CLEFS_ERR_NULL_ARG;
    }
    memset(out, 0, sizeof(*out));
    if ((events == NULL && count > 0) || (part_count > 0 && part_staff_count == NULL)) {
        return SCORE_CLEFS_ERR_NULL_ARG;
    }
    if (notated != NULL && (notated->count != count || (count > 0 && (notated->start_num == NULL ||
                                                                     notated->start_den == NULL ||
                                                                     notated->end_num == NULL ||
                                                                     notated->end_den == NULL)))) {
        return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
    }
    static const int one_staff = 1;
    if (part_count == 0) {
        part_count = 1;
        part_staff_count = &one_staff;
    }

    ScoreClefResult result = SCORE_CLEFS_OK;
    size_t *slot_start = (size_t *)calloc(part_count + 1, sizeof(size_t));
    if (slot_start == NULL) {
        return SCORE_CLEFS_ERR_ALLOC;
    }
    for (size_t p = 0; p < part_count; p++) {
        if (part_staff_count[p] < 1) {
            free(slot_start);
            return SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
        }
        slot_start[p + 1] = slot_start[p] + (size_t)part_staff_count[p];
    }
    size_t slot_count = slot_start[part_count];

    Note *notes = (Note *)calloc(count > 0 ? count : 1, sizeof(Note));
    if (notes == NULL) {
        free(slot_start);
        return SCORE_CLEFS_ERR_ALLOC;
    }
    Rational extent = rational_ratio(0, 1);
    for (size_t i = 0; i < count && result == SCORE_CLEFS_OK; i++) {
        if (events[i] == NULL || (pitch_values != NULL && pitch_values[i] == NULL)) {
            result = SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
            break;
        }
        size_t part = part_index != NULL ? part_index[i] : 0;
        int staff_number = staff != NULL ? staff[i] : 1;
        const PitchSpelling *spelling = note_event_optional_pitch_spelling(events[i]);
        if (part >= part_count || staff_number < 1 || staff_number > part_staff_count[part] || spelling == NULL) {
            result = SCORE_CLEFS_ERR_INCONSISTENT_INPUT;
            break;
        }
        notes[i].slot = slot_start[part] + (size_t)(staff_number - 1);
        result = read_time(notated, i, events[i], &notes[i].start, &notes[i].end);
        if (result != SCORE_CLEFS_OK) {
            break;
        }
        long position = staff_position(spelling);
        notes[i].lines[0] = ledger_lines(position, TREBLE_BOTTOM_LINE, TREBLE_TOP_LINE);
        notes[i].lines[1] = ledger_lines(position, BASS_BOTTOM_LINE, BASS_TOP_LINE);
        if (rational_cmp(notes[i].end, extent) > 0) {
            extent = notes[i].end;
        }
    }

    Measures measures;
    memset(&measures, 0, sizeof(measures));
    MeterMap *own_meter = NULL;
    StaffCosts *costs = NULL;
    Rational (*cost_store)[CLEF_COUNT] = NULL;
    bool *has_store = NULL;
    Rational (*to_go)[CLEF_COUNT] = NULL;
    size_t *noted = NULL;
    EntryList list;
    memset(&list, 0, sizeof(list));

    if (result == SCORE_CLEFS_OK && count > 0) {
        if (meter_map == NULL) {
            EntityID *id = NULL;
            if (entity_id_create("score-clefs-meter-map", &id) != ENTITY_ID_OK) {
                result = SCORE_CLEFS_ERR_ALLOC;
            } else if (meter_map_create_defaulted(id, &own_meter) != METER_MAP_OK) {
                entity_id_destroy(id);
                result = SCORE_CLEFS_ERR_ALLOC;
            }
            meter_map = own_meter;
        }
        if (result == SCORE_CLEFS_OK) {
            result = measures_lay(meter_map, extent, &measures);
        }
    }
    size_t measure_count = measures.count;

    if (result == SCORE_CLEFS_OK && measure_count > 0) {
        costs = (StaffCosts *)calloc(slot_count, sizeof(StaffCosts));
        cost_store = (Rational (*)[CLEF_COUNT])calloc(slot_count * measure_count, sizeof(*cost_store));
        has_store = (bool *)calloc(slot_count * measure_count, sizeof(bool));
        to_go = (Rational (*)[CLEF_COUNT])calloc(measure_count, sizeof(*to_go));
        noted = (size_t *)calloc(measure_count, sizeof(size_t));
        if (costs == NULL || cost_store == NULL || has_store == NULL || to_go == NULL || noted == NULL) {
            result = SCORE_CLEFS_ERR_ALLOC;
        } else {
            for (size_t s = 0; s < slot_count; s++) {
                costs[s].cost = cost_store + s * measure_count;
                costs[s].has_notes = has_store + s * measure_count;
                for (size_t m = 0; m < measure_count; m++) {
                    costs[s].cost[m][0] = rational_ratio(0, 1);
                    costs[s].cost[m][1] = rational_ratio(0, 1);
                }
            }
            result = add_costs(notes, count, &measures, costs);
        }
    }

    for (size_t p = 0; p < part_count && result == SCORE_CLEFS_OK; p++) {
        for (int s = 1; s <= part_staff_count[p] && result == SCORE_CLEFS_OK; s++) {
            int start_clef = part_staff_count[p] > 1 ? (s == 1 ? 0 : 1) : -1;
            if (measure_count == 0) {
                result = entry_add(&list, p, s, 1, clef_of_index(start_clef < 0 ? 0 : start_clef), true);
            } else {
                result = choose_staff(&costs[slot_start[p] + (size_t)(s - 1)], measure_count, p, s, start_clef, to_go,
                                      noted, &list);
            }
        }
    }

    if (result == SCORE_CLEFS_OK) {
        qsort(list.items, list.count, sizeof(Entry), compare_entries);
        out->part = (size_t *)calloc(list.count, sizeof(size_t));
        out->staff = (int *)calloc(list.count, sizeof(int));
        out->measure = (size_t *)calloc(list.count, sizeof(size_t));
        out->clef = (ScoreClef *)calloc(list.count, sizeof(ScoreClef));
        if (out->part == NULL || out->staff == NULL || out->measure == NULL || out->clef == NULL) {
            score_clef_changes_destroy(out);
            result = SCORE_CLEFS_ERR_ALLOC;
        } else {
            out->count = list.count;
            for (size_t i = 0; i < list.count; i++) {
                out->part[i] = list.items[i].part;
                out->staff[i] = list.items[i].staff;
                out->measure[i] = list.items[i].measure;
                out->clef[i] = list.items[i].clef;
            }
        }
    }

    free(list.items);
    free(noted);
    free(to_go);
    free(has_store);
    free(cost_store);
    free(costs);
    measures_free(&measures);
    meter_map_destroy(own_meter);
    free(notes);
    free(slot_start);
    return result;
}
