#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "export/text_export_internal.h"

/*
 * The text export's reading of the written score: the library's score layout (io/score_layout.h) gathered into measures of events
 * with exact times. The layout gives ticks; a tick is 1/(4 * divisions) of a whole note.
 */

/* The symbol of a notated value 1 (whole) to 8 (128th). */
static char value_symbol(int value) {
    static const char SYMBOLS[] = {'w', 'h', 'q', 'e', 's', 't', 'x', 'y'};
    return value >= 1 && value <= 8 ? SYMBOLS[value - 1] : '?';
}

/* The notated value of an event with none (a measure rest): the duration undone by the tuplet, as a plain or dotted value. */
static bool derive_type(Rational duration, int actual, int normal, char *type, int *dots) {
    static const char SYMBOLS[] = {'b', 'w', 'h', 'q', 'e', 's', 't', 'x', 'y'};
    Rational notated = duration;
    if (actual > 0 && normal > 0 && (rational_mul(duration, rational_ratio(actual, normal), &notated) != RATIONAL_OK)) {
        return false;
    }
    Rational base = rational_ratio(2, 1);
    for (size_t i = 0; i < sizeof(SYMBOLS); i++) {
        Rational total = base;
        Rational add = base;
        for (int d = 0; d <= 3; d++) {
            if (d > 0) {
                if (rational_mul(add, rational_ratio(1, 2), &add) != RATIONAL_OK || rational_add(total, add, &total) != RATIONAL_OK) {
                    return false;
                }
            }
            if (rational_cmp(total, notated) == 0) {
                *type = SYMBOLS[i];
                *dots = d;
                return true;
            }
        }
        if (rational_mul(base, rational_ratio(1, 2), &base) != RATIONAL_OK) {
            return false;
        }
    }
    *type = '?';
    *dots = 0;
    return true;
}

static int event_compare(const void *left, const void *right) {
    const ScoreTextEvent *a = left;
    const ScoreTextEvent *b = right;
    if (a->part != b->part) {
        return a->part < b->part ? -1 : 1;
    }
    if (a->staff != b->staff) {
        return a->staff < b->staff ? -1 : 1;
    }
    if (a->voice != b->voice) {
        return a->voice < b->voice ? -1 : 1;
    }
    int by_onset = rational_cmp(a->onset, b->onset);
    if (by_onset != 0) {
        return by_onset;
    }
    return a->order < b->order ? -1 : (a->order > b->order ? 1 : 0);
}

void score_text_destroy(ScoreTextScore *score) {
    if (!score) {
        return;
    }
    for (size_t i = 0; i < score->measure_count; i++) {
        free(score->measures[i].events);
        free(score->measures[i].clefs);
    }
    free(score->measures);
    free(score);
}

/* The time of a number of ticks, in whole notes. */
static bool ticks_time(long long ticks, long long divisions, Rational *out) {
    long long unit = 0;
    return !__builtin_mul_overflow(4LL, divisions, &unit) && rational_make(ticks, unit, out) == RATIONAL_OK;
}

/* One measure's events: the layout's, a chord merged into the event before it, with the times placed. */
static bool fill_events(const ScoreLayout *layout, const ScoreLayoutMeasure *source, ScoreTextMeasure *measure, Rational start,
                        Rational lead) {
    if (source->event_count == 0) {
        return true;
    }
    measure->events = calloc(source->event_count, sizeof(ScoreTextEvent));
    if (!measure->events) {
        return false;
    }
    size_t order = 0;
    size_t part = (size_t)-1;
    for (size_t e = 0; e < source->event_count; e++) {
        const ScoreLayoutEvent *event = &source->events[e];
        if (event->part != part) {
            part = event->part;
            order = 0; /* the order counts the part's events from the measure's start */
        }
        ScoreTextPitch pitch = {event->step, event->alter, event->octave};
        if (event->chord && measure->event_count > 0) {
            ScoreTextEvent *previous = &measure->events[measure->event_count - 1];
            if (previous->part == (int)event->part + 1 && !previous->rest && !event->rest) {
                if (previous->pitch_count < SCORE_TEXT_MAX_PITCHES) {
                    previous->pitches[previous->pitch_count++] = pitch;
                }
                previous->tie_start = previous->tie_start || event->tie_start;
                continue;
            }
        }
        ScoreTextEvent *made = &measure->events[measure->event_count++];
        made->part = (int)event->part + 1;
        made->staff = event->staff;
        made->voice = event->voice;
        made->rest = event->rest;
        if (!event->rest) {
            made->pitches[0] = pitch;
            made->pitch_count = 1;
        }
        made->tuplet_actual = (int)event->tuplet_actual;
        made->tuplet_normal = (int)event->tuplet_normal;
        Rational inside_measure;
        Rational duration;
        if (!ticks_time(event->start - source->start, layout->divisions, &inside_measure) ||
            !ticks_time(event->duration, layout->divisions, &duration)) {
            return false;
        }
        if (event->value > 0) {
            made->type = value_symbol(event->value);
            made->dots = event->dots;
        } else if (!derive_type(duration, made->tuplet_actual, made->tuplet_normal, &made->type, &made->dots)) {
            return false;
        }
        made->tie_start = event->tie_start;
        snprintf(made->xml_id, sizeof(made->xml_id), "%s", event->xml_id);
        made->order = order++;
        /* Absolute onsets, and beats counted from the bar's start (a pickup from its end). */
        Rational inside;
        Rational beats;
        if (rational_add(lead, inside_measure, &inside) != RATIONAL_OK ||
            rational_mul(inside, rational_ratio(measure->beat_type, 1), &beats) != RATIONAL_OK ||
            rational_add(rational_ratio(1, 1), beats, &made->beat_in_measure) != RATIONAL_OK ||
            rational_add(start, inside_measure, &made->onset) != RATIONAL_OK) {
            return false;
        }
    }
    qsort(measure->events, measure->event_count, sizeof(ScoreTextEvent), event_compare);
    return true;
}

ScoreTextScore *score_text_from_layout(const ScoreLayout *layout) {
    if (!layout || layout->measure_count == 0 || layout->part_count == 0 || layout->divisions <= 0) {
        return NULL;
    }
    ScoreTextScore *score = calloc(1, sizeof(ScoreTextScore));
    if (!score) {
        return NULL;
    }
    score->part_count = layout->part_count;
    score->measures = calloc(layout->measure_count, sizeof(ScoreTextMeasure));
    if (!score->measures) {
        free(score);
        return NULL;
    }
    score->measure_count = layout->measure_count;
    int beats = 4;
    int beat_type = 4;
    Rational start = rational_ratio(0, 1);
    for (size_t m = 0; m < layout->measure_count; m++) {
        const ScoreLayoutMeasure *source = &layout->measures[m];
        ScoreTextMeasure *measure = &score->measures[m];
        snprintf(measure->number, sizeof(measure->number), "%d", source->number);
        if (source->time_written && source->beats > 0 && source->beat_type > 0) {
            measure->time_changed = m == 0 || source->beats != beats || source->beat_type != beat_type;
            beats = source->beats;
            beat_type = source->beat_type;
        }
        measure->time_changed = measure->time_changed || m == 0;
        measure->beats = beats;
        measure->beat_type = beat_type;
        measure->start = start;
        Rational full = rational_ratio(beats, beat_type);
        Rational extent;
        if (!ticks_time(source->end - source->start, layout->divisions, &extent)) {
            score_text_destroy(score);
            return NULL;
        }
        if (rational_cmp(extent, full) < 0 && (source->implicit || m == 0)) {
            measure->length = extent;
            measure->pickup = m == 0;
        } else {
            measure->length = rational_cmp(extent, full) > 0 ? extent : full;
        }
        if (source->clef_count > 0) {
            measure->clefs = calloc(source->clef_count, sizeof(ScoreTextClef));
            if (!measure->clefs) {
                score_text_destroy(score);
                return NULL;
            }
            for (size_t c = 0; c < source->clef_count; c++) {
                measure->clefs[measure->clef_count++] =
                    (ScoreTextClef){(int)source->clefs[c].part + 1, source->clefs[c].staff, source->clefs[c].bass};
            }
        }
        Rational lead = rational_ratio(0, 1);
        if (measure->pickup && rational_sub(full, measure->length, &lead) != RATIONAL_OK) {
            score_text_destroy(score);
            return NULL;
        }
        if (!fill_events(layout, source, measure, start, lead) || rational_add(start, measure->length, &start) != RATIONAL_OK) {
            score_text_destroy(score);
            return NULL;
        }
    }
    return score;
}
