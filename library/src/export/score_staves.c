#include "export/score_staves.h"

#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "time/duration.h"
#include "time/time_rational.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

static const Rational zero = {0, 1};
static const Rational one = {1, 1};

bool score_program_uses_grand_staff(int program) {
    return (program >= 0 && program <= 8) || (program >= 16 && program <= 21) || program == 46;
}

/* --- reading notes --- */

typedef struct {
    Rational start;
    Rational end;
} Span;

/* Reads the span and the MIDI pitch of one note. */
static ScoreStavesResult read_note(NoteEvent *event, PitchNumericValue *pitch_value, Span *span, long *pitch) {
    if (event == NULL) {
        return SCORE_STAVES_ERR_NULL_ARG;
    }
    if (pitch_value == NULL) {
        return SCORE_STAVES_ERR_INCONSISTENT_INPUT;
    }
    const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(event));
    const Duration *duration = note_event_duration(event);
    if (musical_time_numerator(position) < 0 || duration_numerator(duration) <= 0) {
        return SCORE_STAVES_ERR_INCONSISTENT_INPUT;
    }
    Rational length;
    if (rational_of_musical_time(position, &span->start) != RATIONAL_OK ||
        rational_of_duration(duration, &length) != RATIONAL_OK ||
        rational_add(span->start, length, &span->end) != RATIONAL_OK) {
        return SCORE_STAVES_ERR_INCONSISTENT_INPUT;
    }
    *pitch = pitch_numeric_value_position_numerator(pitch_value) /
             (long)pitch_numeric_value_position_denominator(pitch_value);
    return SCORE_STAVES_OK;
}

/* --- staff assignment --- */

#define MIDDLE_C 60
#define OCTAVE 12

typedef struct {
    size_t event;
    Rational start;
    Rational end;
    long pitch;
    int hint; /* 1 or 2 fixes the staff, 0 leaves it to the rule */
    int staff;
} Item;

static int compare_items(const void *left, const void *right) {
    const Item *a = (const Item *)left;
    const Item *b = (const Item *)right;
    int order = rational_cmp(a->start, b->start);
    if (order != 0) {
        return order;
    }
    return a->event < b->event ? -1 : (a->event > b->event ? 1 : 0);
}

static long labs_diff(long a, long b) {
    return a > b ? a - b : b - a;
}

static int staff_by_pitch(long pitch) {
    return pitch >= MIDDLE_C ? 1 : 2;
}

/* The staff for a note that sounds alone at its onset: the staff of the nearer pitch of the last treble note and the
   last bass note that ended within the last whole note before it. A tie or no such note: by pitch. When only one staff
   has such a note, it counts only within an octave of the note, so a bass note entering after a long treble line is
   not pulled up to it. items[0..group_start) are the notes before the group of this note, ascending by start, already
   assigned. */
static int staff_of_lone_note(const Item *items, size_t group_start, size_t index, Rational reach) {
    const Item *note = &items[index];
    int by_pitch = staff_by_pitch(note->pitch);
    const Item *last[3] = {NULL, NULL, NULL}; /* by staff: the latest ending note within the last whole note */
    for (size_t k = group_start; k-- > 0;) {
        Rational limit;
        if (rational_add(items[k].start, reach, &limit) != RATIONAL_OK || rational_cmp(limit, note->start) <= 0) {
            break;
        }
        Rational floor_end;
        if (rational_cmp(items[k].end, note->start) > 0 || rational_add(items[k].end, one, &floor_end) != RATIONAL_OK ||
            rational_cmp(floor_end, note->start) <= 0) {
            continue;
        }
        int staff = items[k].staff;
        if (last[staff] == NULL || rational_cmp(items[k].end, last[staff]->end) > 0) {
            last[staff] = &items[k];
        }
    }
    if (last[1] == NULL && last[2] == NULL) {
        return by_pitch;
    }
    if (last[1] == NULL || last[2] == NULL) {
        const Item *only = last[1] != NULL ? last[1] : last[2];
        return labs_diff(note->pitch, only->pitch) <= OCTAVE ? only->staff : by_pitch;
    }
    long treble = labs_diff(note->pitch, last[1]->pitch);
    long bass = labs_diff(note->pitch, last[2]->pitch);
    return treble < bass ? 1 : (bass < treble ? 2 : by_pitch);
}

/* The gap that divides the sounding notes active[0..m) into bass below and treble above: the largest interval between
   adjacent distinct pitches, a tie going to the gap nearest middle C. A hinted note stays on its staff, so only a gap
   with every hinted bass note at or under it and every hinted treble note over it counts. Returns false when no gap
   counts (all one pitch, or the hints leave none). */
static bool find_split(const Item *items, const size_t *active, size_t m, long *out_below) {
    long hinted_bass = -1;
    long hinted_treble = -1;
    for (size_t a = 0; a < m; a++) {
        const Item *note = &items[active[a]];
        if (note->hint == 2 && note->pitch > hinted_bass) {
            hinted_bass = note->pitch;
        }
        if (note->hint == 1 && (hinted_treble < 0 || note->pitch < hinted_treble)) {
            hinted_treble = note->pitch;
        }
    }
    bool found = false;
    long best_gap = 0;
    long best_centre = 0;
    for (size_t a = 0; a < m; a++) {
        long below = items[active[a]].pitch;
        long above = 0;
        bool has_above = false;
        for (size_t b = 0; b < m; b++) {
            long pitch = items[active[b]].pitch;
            if (pitch > below && (!has_above || pitch < above)) {
                above = pitch;
                has_above = true;
            }
        }
        if (!has_above || below < hinted_bass || (hinted_treble >= 0 && above > hinted_treble)) {
            continue;
        }
        long gap = above - below;
        long centre = labs_diff(below + above, 2 * MIDDLE_C);
        if (!found || gap > best_gap || (gap == best_gap && centre < best_centre)) {
            found = true;
            best_gap = gap;
            best_centre = centre;
            *out_below = below;
        }
    }
    return found;
}

/* Assigns the notes items[from..to) (same start) that have no hint. active[0..m) are the items sounding at that onset
   (these included, ascending by start). */
static void assign_group(Item *items, size_t from, size_t to, const size_t *active, size_t m, Rational reach) {
    bool any_free = false;
    for (size_t k = from; k < to; k++) {
        any_free = any_free || items[k].hint == 0;
    }
    if (!any_free) {
        return;
    }
    long below = 0;
    bool split = m > 1 && find_split(items, active, m, &below);
    bool any_hinted = false;
    for (size_t a = 0; a < m; a++) {
        any_hinted = any_hinted || items[active[a]].hint != 0;
    }
    for (size_t k = from; k < to; k++) {
        if (items[k].hint != 0) {
            continue;
        }
        if (split) {
            items[k].staff = items[k].pitch <= below ? 2 : 1;
        } else if (!any_hinted) {
            items[k].staff = staff_of_lone_note(items, from, k, reach);
        } else {
            /* No gap fits the hints (every hint is on one side of the note): the staff of the nearest hinted note. */
            long best = -1;
            int staff = staff_by_pitch(items[k].pitch);
            for (size_t a = 0; a < m; a++) {
                const Item *other = &items[active[a]];
                long distance = labs_diff(items[k].pitch, other->pitch);
                if (other->hint == 0 || (best >= 0 && distance > best)) {
                    continue;
                }
                staff = (best >= 0 && distance == best && staff != other->hint) ? staff_by_pitch(items[k].pitch)
                                                                                  : other->hint;
                best = distance;
            }
            items[k].staff = staff;
        }
    }
}

/* active is scratch for n entries. */
static void assign_part(Item *items, size_t n, size_t *active, Rational reach) {
    qsort(items, n, sizeof(Item), compare_items);
    size_t m = 0;
    size_t from = 0;
    while (from < n) {
        size_t to = from + 1;
        while (to < n && rational_cmp(items[to].start, items[from].start) == 0) {
            to++;
        }
        size_t kept = 0;
        for (size_t a = 0; a < m; a++) {
            if (rational_cmp(items[active[a]].end, items[from].start) > 0) {
                active[kept++] = active[a];
            }
        }
        m = kept;
        for (size_t k = from; k < to; k++) {
            active[m++] = k;
        }
        assign_group(items, from, to, active, m, reach);
        from = to;
    }
}

ScoreStavesResult score_staves_assign(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                      const size_t *part_index, const int *part_staff_count, size_t part_count,
                                      int *out_staff) {
    return score_staves_assign_hinted(events, pitch_values, count, part_index, part_staff_count, part_count, NULL,
                                      out_staff);
}

ScoreStavesResult score_staves_assign_hinted(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                             size_t count, const size_t *part_index, const int *part_staff_count,
                                             size_t part_count, const int *staff_hint, int *out_staff) {
    if (out_staff == NULL || (count > 0 && events == NULL) || (part_count > 0 && part_staff_count == NULL)) {
        return SCORE_STAVES_ERR_NULL_ARG;
    }
    for (size_t p = 0; p < part_count; p++) {
        if (part_staff_count[p] != 1 && part_staff_count[p] != 2) {
            return SCORE_STAVES_ERR_INCONSISTENT_INPUT;
        }
    }
    for (size_t i = 0; i < count; i++) {
        size_t part = part_index != NULL ? part_index[i] : 0;
        if (events[i] == NULL || part >= part_count) {
            return events[i] == NULL ? SCORE_STAVES_ERR_NULL_ARG : SCORE_STAVES_ERR_INCONSISTENT_INPUT;
        }
        if (staff_hint != NULL && (staff_hint[i] < 0 || staff_hint[i] > 2)) {
            return SCORE_STAVES_ERR_INCONSISTENT_INPUT;
        }
        out_staff[i] = 1;
        if (part_staff_count[part] == 2 && (pitch_values == NULL || pitch_values[i] == NULL)) {
            return pitch_values == NULL ? SCORE_STAVES_ERR_NULL_ARG : SCORE_STAVES_ERR_INCONSISTENT_INPUT;
        }
    }
    Item *items = (Item *)malloc((count > 0 ? count : 1) * sizeof(Item));
    size_t *active = (size_t *)malloc((count > 0 ? count : 1) * sizeof(size_t));
    if (items == NULL || active == NULL) {
        free(items);
        free(active);
        return SCORE_STAVES_ERR_ALLOC;
    }
    ScoreStavesResult result = SCORE_STAVES_OK;
    for (size_t p = 0; p < part_count && result == SCORE_STAVES_OK; p++) {
        if (part_staff_count[p] != 2) {
            continue;
        }
        size_t n = 0;
        Rational longest = zero;
        for (size_t i = 0; i < count && result == SCORE_STAVES_OK; i++) {
            if ((part_index != NULL ? part_index[i] : 0) != p) {
                continue;
            }
            Span span;
            Item *item = &items[n++];
            result = read_note(events[i], pitch_values[i], &span, &item->pitch);
            if (result != SCORE_STAVES_OK) {
                break;
            }
            item->event = i;
            item->start = span.start;
            item->end = span.end;
            item->hint = staff_hint != NULL ? staff_hint[i] : 0;
            item->staff = item->hint != 0 ? item->hint : 1;
            Rational length;
            if (rational_sub(span.end, span.start, &length) != RATIONAL_OK) {
                result = SCORE_STAVES_ERR_INCONSISTENT_INPUT;
                break;
            }
            longest = rational_cmp(length, longest) > 0 ? length : longest;
        }
        Rational reach;
        if (result == SCORE_STAVES_OK && rational_add(longest, one, &reach) != RATIONAL_OK) {
            result = SCORE_STAVES_ERR_INCONSISTENT_INPUT;
        }
        if (result == SCORE_STAVES_OK) {
            assign_part(items, n, active, reach);
        }
        for (size_t k = 0; k < n && result == SCORE_STAVES_OK; k++) {
            out_staff[items[k].event] = items[k].staff;
        }
    }
    free(items);
    free(active);
    return result;
}

/* --- hand pairing --- */


static int compare_spans(const void *left, const void *right) {
    const Span *a = (const Span *)left;
    const Span *b = (const Span *)right;
    int order = rational_cmp(a->start, b->start);
    return order != 0 ? order : rational_cmp(a->end, b->end);
}

static int compare_longs(const void *left, const void *right) {
    long a = *(const long *)left;
    long b = *(const long *)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}

/* Merges the sorted spans in place into disjoint ones (spans that touch join); returns the new count. */
static size_t merge_spans(Span *spans, size_t n) {
    size_t out = 0;
    for (size_t k = 0; k < n; k++) {
        if (out > 0 && rational_cmp(spans[k].start, spans[out - 1].end) <= 0) {
            if (rational_cmp(spans[k].end, spans[out - 1].end) > 0) {
                spans[out - 1].end = spans[k].end;
            }
        } else {
            spans[out++] = spans[k];
        }
    }
    return out;
}

/* How many of the spans (sorted by start) overlap a span of the disjoint, sorted cover. */
static size_t count_overlapping(const Span *spans, size_t n, const Span *cover, size_t cover_count) {
    size_t overlapping = 0;
    size_t j = 0;
    for (size_t k = 0; k < n; k++) {
        while (j < cover_count && rational_cmp(cover[j].end, spans[k].start) <= 0) {
            j++;
        }
        if (j < cover_count && rational_cmp(cover[j].start, spans[k].end) < 0) {
            overlapping++;
        }
    }
    return overlapping;
}

ScoreStavesResult score_staves_pair_hands(NoteEvent *const *events, PitchNumericValue *const *pitch_values,
                                          size_t count, const size_t *stream_index, const ScoreStream *streams,
                                          size_t stream_count, size_t *out_part_of_stream, int *out_staff_of_stream,
                                          size_t *out_part_count) {
    if (out_part_count == NULL || (stream_count > 0 && (streams == NULL || out_part_of_stream == NULL ||
                                                         out_staff_of_stream == NULL)) ||
        (count > 0 && (events == NULL || stream_index == NULL || pitch_values == NULL))) {
        return SCORE_STAVES_ERR_NULL_ARG;
    }
    size_t *offset = (size_t *)calloc(stream_count + 2, sizeof(size_t)); /* notes of stream s: offset[s]..offset[s+1] */
    size_t *cursor = (size_t *)calloc(stream_count + 1, sizeof(size_t));
    size_t *covered = (size_t *)calloc(stream_count + 1, sizeof(size_t));
    long *median2 = (long *)calloc(stream_count + 1, sizeof(long)); /* twice the median pitch */
    Span *spans = (Span *)malloc((count + 1) * sizeof(Span));
    Span *covers = (Span *)malloc((count + 1) * sizeof(Span));
    long *pitches = (long *)malloc((count + 1) * sizeof(long));
    ScoreStavesResult result = SCORE_STAVES_OK;
    if (offset == NULL || cursor == NULL || covered == NULL || median2 == NULL || spans == NULL || covers == NULL ||
        pitches == NULL) {
        result = SCORE_STAVES_ERR_ALLOC;
    }
    for (size_t i = 0; result == SCORE_STAVES_OK && i < count; i++) {
        if (stream_index[i] >= stream_count) {
            result = SCORE_STAVES_ERR_INCONSISTENT_INPUT;
        } else {
            offset[stream_index[i] + 1]++;
        }
    }
    for (size_t s = 0; result == SCORE_STAVES_OK && s < stream_count; s++) {
        if (offset[s + 1] != streams[s].note_count) {
            result = SCORE_STAVES_ERR_INCONSISTENT_INPUT;
        }
        offset[s + 1] += offset[s];
        cursor[s] = offset[s];
    }
    for (size_t i = 0; result == SCORE_STAVES_OK && i < count; i++) {
        size_t at = cursor[stream_index[i]]++;
        result = read_note(events[i], pitch_values[i], &spans[at], &pitches[at]);
    }
    for (size_t s = 0; result == SCORE_STAVES_OK && s < stream_count; s++) {
        size_t n = offset[s + 1] - offset[s];
        if (n == 0) {
            continue;
        }
        qsort(spans + offset[s], n, sizeof(Span), compare_spans);
        qsort(pitches + offset[s], n, sizeof(long), compare_longs);
        median2[s] = n % 2 == 1 ? 2 * pitches[offset[s] + n / 2]
                                : pitches[offset[s] + n / 2 - 1] + pitches[offset[s] + n / 2];
        memcpy(covers + offset[s], spans + offset[s], n * sizeof(Span));
        covered[s] = merge_spans(covers + offset[s], n);
    }

    size_t best_low = 0;
    size_t best_high = 0;
    size_t best_score = 0;
    bool paired = false;
    for (size_t a = 0; result == SCORE_STAVES_OK && a < stream_count; a++) {
        size_t na = offset[a + 1] - offset[a];
        if (na == 0 || !score_program_uses_grand_staff(streams[a].program)) {
            continue;
        }
        for (size_t b = a + 1; b < stream_count; b++) {
            size_t nb = offset[b + 1] - offset[b];
            if (nb == 0 || !score_program_uses_grand_staff(streams[b].program) || median2[a] == median2[b]) {
                continue;
            }
            size_t over_a = count_overlapping(spans + offset[a], na, covers + offset[b], covered[b]);
            size_t over_b = count_overlapping(spans + offset[b], nb, covers + offset[a], covered[a]);
            size_t shorter_over = na <= nb ? over_a : over_b;
            size_t shorter = na <= nb ? na : nb;
            if (2 * shorter_over < shorter) {
                continue;
            }
            if (!paired || over_a + over_b > best_score) {
                paired = true;
                best_low = a;
                best_high = b;
                best_score = over_a + over_b;
            }
        }
    }

    if (result == SCORE_STAVES_OK) {
        size_t parts = 0;
        for (size_t s = 0; s < stream_count; s++) {
            out_part_of_stream[s] = (size_t)-1;
            out_staff_of_stream[s] = 0;
        }
        for (size_t s = 0; s < stream_count; s++) {
            if (out_part_of_stream[s] != (size_t)-1) {
                continue;
            }
            out_part_of_stream[s] = parts;
            if (paired && s == best_low) {
                out_part_of_stream[best_high] = parts;
                bool low_is_higher = median2[best_low] > median2[best_high];
                out_staff_of_stream[best_low] = low_is_higher ? 1 : 2;
                out_staff_of_stream[best_high] = low_is_higher ? 2 : 1;
            }
            parts++;
        }
        *out_part_count = parts;
    }
    free(offset);
    free(cursor);
    free(covered);
    free(median2);
    free(spans);
    free(covers);
    free(pitches);
    return result;
}
