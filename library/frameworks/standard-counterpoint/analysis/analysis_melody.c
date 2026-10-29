#include "analysis_melody.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_melody_emit.h"
#include "core/value.h"
#include "identity/entity_id.h"

#define DEFAULT_APPROXIMATE_PENALTY 0.25

StandardCounterpointMelodyConfig standard_counterpoint_melody_config_default(void) {
    StandardCounterpointMelodyConfig config = {DEFAULT_APPROXIMATE_PENALTY};
    return config;
}

/* --- the line --- */

static char *copy_string(const char *text) {
    if (text == NULL) {
        return NULL;
    }
    size_t size = strlen(text) + 1;
    char *copy = (char *)malloc(size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }
    return copy;
}

void standard_counterpoint_melody_line_init(StandardCounterpointMelodyLine *line) {
    if (line != NULL) {
        memset(line, 0, sizeof(*line));
    }
}

void standard_counterpoint_melody_line_destroy(StandardCounterpointMelodyLine *line) {
    if (line == NULL) {
        return;
    }
    for (size_t i = 0; i < line->count; i++) {
        free((void *)line->notes[i].note_id);
        free((void *)line->notes[i].nct_ref);
    }
    free(line->notes);
    memset(line, 0, sizeof(*line));
}

static StandardCounterpointMelodyStatus grow(StandardCounterpointMelodyLine *line) {
    if (line->count < line->capacity) {
        return STANDARD_COUNTERPOINT_MELODY_OK;
    }
    size_t capacity = line->capacity == 0 ? 16 : line->capacity * 2;
    StandardCounterpointMelodyNote *notes =
        (StandardCounterpointMelodyNote *)realloc(line->notes, capacity * sizeof(StandardCounterpointMelodyNote));
    if (notes == NULL) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
    }
    line->notes = notes;
    line->capacity = capacity;
    return STANDARD_COUNTERPOINT_MELODY_OK;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_line_add_rest(StandardCounterpointMelodyLine *line) {
    if (line == NULL) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    StandardCounterpointMelodyStatus status = grow(line);
    if (status != STANDARD_COUNTERPOINT_MELODY_OK) {
        return status;
    }
    memset(&line->notes[line->count++], 0, sizeof(StandardCounterpointMelodyNote));
    return STANDARD_COUNTERPOINT_MELODY_OK;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_line_add_note(StandardCounterpointMelodyLine *line,
                                                                            const StandardCounterpointMelodyNote *note) {
    if (line == NULL || note == NULL) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    if (!note->present || note->note_id == NULL || note->note_id[0] == '\0' || note->pitch == NULL ||
        pitch_numeric_value_position_denominator(note->pitch) != 1 || note->degree < 0 || note->degree > 7 ||
        note->alteration < -1 || note->alteration > 1 || !(note->confidence >= 0.0 && note->confidence <= 1.0) ||
        (note->has_strength && !beat_strength_valid(note->strength)) ||
        (unsigned)note->nct > (unsigned)STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE ||
        (note->nct_ref != NULL && strlen(note->nct_ref) >= 128) ||
        (note->nct == STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE && !non_chord_tone_valid(note->nct_type))) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_BAD_NOTE;
    }
    StandardCounterpointMelodyStatus status = grow(line);
    if (status != STANDARD_COUNTERPOINT_MELODY_OK) {
        return status;
    }
    StandardCounterpointMelodyNote *slot = &line->notes[line->count];
    *slot = *note;
    slot->note_id = copy_string(note->note_id);
    slot->nct_ref = copy_string(note->nct_ref);
    if (slot->note_id == NULL || (note->nct_ref != NULL && slot->nct_ref == NULL)) {
        free((void *)slot->note_id);
        free((void *)slot->nct_ref);
        return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
    }
    line->count++;
    return STANDARD_COUNTERPOINT_MELODY_OK;
}

/* --- generic size --- */

static long semitone_of(const StandardCounterpointMelodyNote *note) {
    return pitch_numeric_value_position_numerator(note->pitch);
}

/* The generic size of the interval a letter-name distance and the semitones imply, from the scale degrees: the letters in
   the direction of motion, plus the octaves the semitones carry beyond the letters. */
static int generic_from_degrees(const StandardCounterpointMelodyNote *a, const StandardCounterpointMelodyNote *b,
                                long semitones) {
    if (semitones == 0) {
        return 1;
    }
    int letters = semitones > 0 ? (((b->degree - a->degree) % 7) + 7) % 7 : (((a->degree - b->degree) % 7) + 7) % 7;
    double distance = (double)labs(semitones);
    long octaves = (long)floor((distance - (double)letters * 12.0 / 7.0) / 12.0 + 0.5);
    if (octaves < 0) {
        octaves = 0;
    }
    return letters + 7 * (int)octaves + 1;
}

/* The generic size read from the semitones alone (used only to decide leap and step when none is known). */
static int generic_from_semitones(long semitones) {
    static const int WITHIN_OCTAVE[12] = {1, 2, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7};
    long size = labs(semitones);
    return WITHIN_OCTAVE[size % 12] + 7 * (int)(size / 12);
}

static StandardCounterpointInterval make_interval(int size) {
    StandardCounterpointInterval interval;
    interval.size = size;
    interval.simple_size = size == 1 ? 1 : ((size - 2) % 7) + 2;
    interval.quality = STANDARD_COUNTERPOINT_QUALITY_PERFECT;
    return interval;
}

/* --- intervals --- */

static void destroy_interval(StandardCounterpointMelodicInterval *item) {
    interpretation_destroy(item->interpretation);
    structure_destroy(item->shell);
    contour_destroy(item->contour);
    interval_destroy(item->interval);
    pitch_identity_destroy(item->pitch_a);
    pitch_identity_destroy(item->pitch_b);
    memset(item, 0, sizeof(*item));
}

void standard_counterpoint_melodic_intervals_destroy(StandardCounterpointMelodicIntervals *intervals) {
    if (intervals == NULL) {
        return;
    }
    for (size_t i = 0; i < intervals->count; i++) {
        destroy_interval(&intervals->items[i]);
    }
    free(intervals->items);
    intervals->items = NULL;
    intervals->count = 0;
}

static bool build_values(const StandardCounterpointMelodyNote *a, const StandardCounterpointMelodyNote *b,
                         StandardCounterpointMelodicInterval *item) {
    PitchIdentity **contour_ids = (PitchIdentity **)calloc(2, sizeof(PitchIdentity *));
    ContourDirection *directions = (ContourDirection *)calloc(1, sizeof(ContourDirection));
    if (contour_ids == NULL || directions == NULL) {
        free(contour_ids);
        free(directions);
        return false;
    }
    directions[0] = (ContourDirection)item->claim.direction;
    unsigned long magnitude = (unsigned long)labs(item->claim.semitones);
    if (pitch_numeric_value_to_identity(a->pitch, &item->pitch_a) != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK ||
        pitch_numeric_value_to_identity(b->pitch, &item->pitch_b) != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK ||
        pitch_numeric_value_to_identity(a->pitch, &contour_ids[0]) != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK ||
        pitch_numeric_value_to_identity(b->pitch, &contour_ids[1]) != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK ||
        interval_create(item->pitch_a, item->pitch_b, true, item->claim.semitones, true, magnitude, false, 0,
                        &item->interval) != INTERVAL_OK) {
        pitch_identity_destroy(contour_ids[0]);
        pitch_identity_destroy(contour_ids[1]);
        free(contour_ids);
        free(directions);
        return false;
    }
    /* contour_create takes the identities and copies the two arrays, which stay ours */
    ContourResult created = contour_create(contour_ids, 2, directions, 1, &item->contour);
    if (created != CONTOUR_OK) {
        pitch_identity_destroy(contour_ids[0]);
        pitch_identity_destroy(contour_ids[1]);
    }
    free(contour_ids);
    free(directions);
    return created == CONTOUR_OK;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_intervals(const StandardCounterpointMelodyLine *line,
                                                                        const StandardCounterpointMelodyConfig *config,
                                                                        StandardCounterpointMelodicIntervals *out) {
    if (line == NULL || out == NULL || (line->count > 0 && line->notes == NULL)) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    StandardCounterpointMelodyConfig defaults = standard_counterpoint_melody_config_default();
    const StandardCounterpointMelodyConfig *effective = config != NULL ? config : &defaults;
    if (!(effective->approximate_penalty >= 0.0 && effective->approximate_penalty <= 1.0)) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    StandardCounterpointMelodicIntervals found = {NULL, 0};
    if (line->count > 1) {
        found.items = (StandardCounterpointMelodicInterval *)calloc(line->count - 1,
                                                                      sizeof(StandardCounterpointMelodicInterval));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
    }
    for (size_t i = 0; i + 1 < line->count; i++) {
        const StandardCounterpointMelodyNote *a = &line->notes[i];
        const StandardCounterpointMelodyNote *b = &line->notes[i + 1];
        if (!a->present || !b->present) {
            continue;
        }
        StandardCounterpointMelodicInterval *item = &found.items[found.count];
        long semitones = semitone_of(b) - semitone_of(a);
        StandardCounterpointMelodicIntervalClaim *claim = &item->claim;
        claim->from = (int)i;
        claim->to = (int)(i + 1);
        claim->semitones = semitones;
        claim->direction = semitones > 0 ? (int)CONTOUR_DIRECTION_UP
                                         : (semitones < 0 ? (int)CONTOUR_DIRECTION_DOWN : (int)CONTOUR_DIRECTION_SAME);

        StandardCounterpointInterval spelled;
        if (a->spelling != NULL && b->spelling != NULL &&
            standard_counterpoint_interval_between(a->spelling, b->spelling, &spelled)) {
            claim->generic_size = spelled.size;
            claim->generic_source = (int)STANDARD_COUNTERPOINT_GENERIC_SPELLING;
            item->effective = spelled;
        } else if (a->degree != 0 && b->degree != 0) {
            claim->generic_size = generic_from_degrees(a, b, semitones);
            claim->generic_source = (int)STANDARD_COUNTERPOINT_GENERIC_DEGREE;
            item->effective = make_interval(claim->generic_size);
        } else {
            claim->generic_size = 0;
            claim->generic_source = (int)STANDARD_COUNTERPOINT_GENERIC_NONE;
            claim->approximate = 1;
            item->effective = make_interval(generic_from_semitones(semitones));
        }
        claim->leap = standard_counterpoint_interval_is_leap(&item->effective) ? 1 : 0;
        claim->step = standard_counterpoint_interval_is_step(&item->effective) ? 1 : 0;

        double lesser = a->confidence < b->confidence ? a->confidence : b->confidence;
        double penalty = claim->approximate ? effective->approximate_penalty : 0.0;
        item->uncertainty = 1.0 - lesser * (1.0 - penalty);

        char key[400];
        snprintf(key, sizeof(key), "%s-%s", a->note_id, b->note_id);
        const char *ids[2] = {a->note_id, b->note_id};
        if (!build_values(a, b, item) ||
            !standard_counterpoint_melody_emit("melodic-interval", key, ids, 2,
                                               STANDARD_COUNTERPOINT_MELODIC_INTERVAL_CLAIM_TYPE, claim,
                                               sizeof(*claim), item->uncertainty, &item->shell,
                                               &item->interpretation)) {
            destroy_interval(item);
            standard_counterpoint_melodic_intervals_destroy(&found);
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
        found.count++;
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_COUNTERPOINT_MELODY_OK;
}

static bool read_claim(const Interpretation *interpretation, const char *type, void *out, size_t size) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t content_size = 0;
    if (claim == NULL || strcmp(value_type(claim), type) != 0) {
        return false;
    }
    const void *content = value_content(claim, &content_size);
    if (content == NULL || content_size != size) {
        return false;
    }
    memcpy(out, content, size);
    return true;
}

bool standard_counterpoint_melodic_interval_claim_read(const Interpretation *interpretation,
                                                       StandardCounterpointMelodicIntervalClaim *out) {
    return read_claim(interpretation, STANDARD_COUNTERPOINT_MELODIC_INTERVAL_CLAIM_TYPE, out, sizeof(*out));
}

/* --- contour summaries --- */

static void destroy_summary(StandardCounterpointContourSummary *summary) {
    interpretation_destroy(summary->interpretation);
    structure_destroy(summary->shell);
    memset(summary, 0, sizeof(*summary));
}

void standard_counterpoint_contour_summaries_destroy(StandardCounterpointContourSummaries *summaries) {
    if (summaries == NULL) {
        return;
    }
    for (size_t i = 0; i < summaries->count; i++) {
        destroy_summary(&summaries->items[i]);
    }
    free(summaries->items);
    summaries->items = NULL;
    summaries->count = 0;
}

StandardCounterpointMelodyStatus standard_counterpoint_melody_contour_summaries(
    const StandardCounterpointMelodyLine *line, const StandardCounterpointMelodicIntervals *intervals,
    const StandardCounterpointMelodySpan *spans, size_t span_count, StandardCounterpointContourSummaries *out) {
    if (line == NULL || intervals == NULL || out == NULL || (span_count > 0 && spans == NULL) ||
        (line->count > 0 && line->notes == NULL)) {
        return STANDARD_COUNTERPOINT_MELODY_ERR_NULL_ARGUMENT;
    }
    StandardCounterpointMelodySpan whole = {"whole-line", 0, line->count > 0 ? line->count - 1 : 0};
    if (spans == NULL) {
        spans = &whole;
        span_count = line->count > 0 ? 1 : 0;
    }
    for (size_t s = 0; s < span_count; s++) {
        if (spans[s].id == NULL || spans[s].id[0] == '\0' || spans[s].first > spans[s].last ||
            spans[s].last >= line->count) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_BAD_SPAN;
        }
    }
    StandardCounterpointContourSummaries found = {NULL, 0};
    if (span_count > 0) {
        found.items = (StandardCounterpointContourSummary *)calloc(span_count,
                                                                     sizeof(StandardCounterpointContourSummary));
        if (found.items == NULL) {
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
    }
    for (size_t s = 0; s < span_count; s++) {
        const StandardCounterpointMelodySpan *span = &spans[s];
        StandardCounterpointContourSummaryClaim claim;
        memset(&claim, 0, sizeof(claim));
        claim.span_first = (int)span->first;
        claim.span_last = (int)span->last;
        const char **ids = (const char **)calloc(span->last - span->first + 1, sizeof(char *));
        if (ids == NULL) {
            standard_counterpoint_contour_summaries_destroy(&found);
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
        size_t first_note = line->count;
        size_t last_note = 0;
        size_t lowest = 0;
        size_t highest = 0;
        double least = 1.0;
        for (size_t i = span->first; i <= span->last; i++) {
            const StandardCounterpointMelodyNote *note = &line->notes[i];
            if (!note->present) {
                continue;
            }
            ids[claim.note_count++] = note->note_id;
            if (first_note == line->count) {
                first_note = i;
                lowest = i;
                highest = i;
            }
            last_note = i;
            if (semitone_of(note) < semitone_of(&line->notes[lowest])) {
                lowest = i;
            }
            if (semitone_of(note) > semitone_of(&line->notes[highest])) {
                highest = i;
            }
            if (note->confidence < least) {
                least = note->confidence;
            }
        }
        if (claim.note_count == 0) {
            free(ids);
            continue;
        }
        for (size_t k = 0; k < intervals->count; k++) {
            const StandardCounterpointMelodicIntervalClaim *step = &intervals->items[k].claim;
            if ((size_t)step->from < span->first || (size_t)step->to > span->last) {
                continue;
            }
            claim.up += step->direction == (int)CONTOUR_DIRECTION_UP;
            claim.down += step->direction == (int)CONTOUR_DIRECTION_DOWN;
            claim.same += step->direction == (int)CONTOUR_DIRECTION_SAME;
            claim.step_count += step->step;
            claim.leap_count += step->leap;
        }
        claim.net_semitones = semitone_of(&line->notes[last_note]) - semitone_of(&line->notes[first_note]);
        claim.net_direction = claim.net_semitones > 0 ? (int)CONTOUR_DIRECTION_UP
                                                      : (claim.net_semitones < 0 ? (int)CONTOUR_DIRECTION_DOWN
                                                                                  : (int)CONTOUR_DIRECTION_SAME);
        claim.range_semitones = semitone_of(&line->notes[highest]) - semitone_of(&line->notes[lowest]);
        claim.lowest_index = (int)lowest;
        claim.highest_index = (int)highest;

        StandardCounterpointContourSummary *summary = &found.items[found.count];
        snprintf(summary->span_id, sizeof(summary->span_id), "%s", span->id);
        summary->claim = claim;
        summary->uncertainty = 1.0 - least;
        bool ok = standard_counterpoint_melody_emit("contour-summary", span->id, ids, (size_t)claim.note_count,
                                                    STANDARD_COUNTERPOINT_CONTOUR_SUMMARY_CLAIM_TYPE, &claim,
                                                    sizeof(claim), summary->uncertainty, &summary->shell,
                                                    &summary->interpretation);
        free(ids);
        if (!ok) {
            standard_counterpoint_contour_summaries_destroy(&found);
            return STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC;
        }
        found.count++;
    }
    if (found.count == 0) {
        free(found.items);
        found.items = NULL;
    }
    *out = found;
    return STANDARD_COUNTERPOINT_MELODY_OK;
}

bool standard_counterpoint_contour_summary_claim_read(const Interpretation *interpretation,
                                                      StandardCounterpointContourSummaryClaim *out) {
    return read_claim(interpretation, STANDARD_COUNTERPOINT_CONTOUR_SUMMARY_CLAIM_TYPE, out, sizeof(*out));
}
