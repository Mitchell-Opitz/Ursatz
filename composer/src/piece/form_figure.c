#include <ursatz_composer/form_figure.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"
#include "piece/form_return_internal.h"
#include "util/util.h"

/*
 * The motive of variation (R-497): the skeleton of a figurate return, and the figure written over it. Time in this
 * file is Rational, from the start of the section.
 */

/* A note of the returned section's melody, moved by the key difference: where it starts and ends, from its section's start. */
typedef struct {
    Rational onset;
    Rational end;
    int pitch;
} SourceNote;

static int compare_notes(const void *a, const void *b) {
    return rational_cmp(((const SourceNote *)a)->onset, ((const SourceNote *)b)->onset);
}

/* The melody of the section the target returns, read as SourceNote, in order of onset. NULL when it holds no note. */
static SourceNote *read_source_melody(const PiecePlan *plan, NodeId source, int shift, size_t *count) {
    PlanSpan span = plan_node_span(plan, source);
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, NULL, 0);
    LaneSpanId *ids = spans ? calloc(spans, sizeof(LaneSpanId)) : NULL;
    size_t total = 0;
    *count = 0;
    if (!spans || !ids) {
        free(ids);
        return NULL;
    }
    plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, spans);
    for (size_t s = 0; s < spans; s++) {
        total += plan_lane_span_payload(plan, ids[s])->notes.note_count;
    }
    SourceNote *notes = total ? calloc(total, sizeof(SourceNote)) : NULL;
    bool ok = notes != NULL;
    size_t used = 0;
    for (size_t s = 0; ok && s < spans; s++) {
        PlanSpan lane_span = plan_lane_span_span(plan, ids[s]);
        const PlanNotes *held = &plan_lane_span_payload(plan, ids[s])->notes;
        for (size_t n = 0; ok && n < held->note_count; n++) {
            Rational at;
            Rational onset;
            ok = rational_add(lane_span.start, held->notes[n].offset, &at) == RATIONAL_OK &&
                 rational_sub(at, span.start, &onset) == RATIONAL_OK &&
                 rational_add(onset, held->notes[n].duration, &notes[used].end) == RATIONAL_OK;
            notes[used].onset = onset;
            notes[used].pitch = held->notes[n].midi + shift;
            used++;
        }
    }
    free(ids);
    if (!ok) {
        free(notes);
        return NULL;
    }
    qsort(notes, used, sizeof(SourceNote), compare_notes);
    *count = used;
    return notes;
}

/* The key of the section: its key lane holds one span over it. */
static bool section_key(const PiecePlan *plan, PlanSpan span, AnalysisKey *key) {
    LaneSpanId ids[2];
    if (plan_lane_find(plan, PLAN_LANE_KEY, 0, span, ids, 2) != 1) {
        return false;
    }
    *key = plan_lane_span_payload(plan, ids[0])->key;
    return true;
}

static int pitch_class(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

static bool has_pitch_class(unsigned mask, int pitch) {
    return (mask >> pitch_class(pitch)) & 1u;
}

/* The chord tone of the mask nearest a pitch (the lower of two equally near); false when the mask is empty. */
static bool nearest_chord_tone(unsigned mask, int pitch, int *out) {
    bool found = false;
    for (int candidate = pitch - 12; candidate <= pitch + 12; candidate++) {
        if (has_pitch_class(mask, candidate) && (!found || util_absolute(candidate - pitch) < util_absolute(*out - pitch))) {
            *out = candidate;
            found = true;
        }
    }
    return found;
}

/*
 * The cell of the figure: a beat of the meter, a quarter in 2/4, 3/4 and 4/4 and a dotted quarter in 6/8. The harmony
 * operators' own cell is longer in 2/4, 3/4 and 4/4 (a bar in 2/4 and 3/4, a half in 4/4): a chord covers whole cells of
 * the figure.
 */
static bool figure_cell_length(const PlanMeter *meter, Rational *cell, size_t *per_bar) {
    if (meter->beat_unit == 8 && meter->beats_per_bar == 6) {
        *per_bar = 2;
        return rational_make(3, 8, cell) == RATIONAL_OK;
    }
    if (meter->beat_unit == 4 && meter->beats_per_bar >= 2 && meter->beats_per_bar <= 4) {
        *per_bar = (size_t)meter->beats_per_bar;
        return rational_make(1, 4, cell) == RATIONAL_OK;
    }
    return false;
}

/* A section as the figure reads it: its key, chords and cells, the returned melody, and the skeleton. */
typedef struct {
    AnalysisKey key;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t chord_count;
    PhraseMetre metre;
    Rational cell_length;             /* the cell of the figure: a beat (a quarter, a dotted quarter in 6/8), from the section's start */
    size_t cells_per_bar;
    unsigned mask[FORM_SKELETON_MAX]; /* the pitch classes of the chord over each cell */
    SourceNote *notes;                /* the returned melody, moved, in order of onset */
    size_t note_count;
    FormSkeleton skeleton;
} Reading;

static void reading_free(Reading *reading) {
    if (reading) {
        free(reading->notes);
        free(reading);
    }
}

static bool cell_bounds(const Reading *reading, size_t cell, Rational *start, Rational *end) {
    return rational_mul_int(reading->cell_length, (long long)cell, start) == RATIONAL_OK &&
           rational_add(*start, reading->cell_length, end) == RATIONAL_OK;
}

static Reading *read_section(const PiecePlan *plan, NodeId section) {
    PlanReturn ret;
    int shift;
    if (!plan || !return_target(plan, section, &ret) || ret.variation != PLAN_RETURN_FIGURATE) {
        return NULL;
    }
    Reading *reading = calloc(1, sizeof(*reading));
    if (!reading) {
        return NULL;
    }
    PlanSpan span = plan_node_span(plan, section);
    PhraseMetre *metre = &reading->metre;
    if (!section_key(plan, span, &reading->key) ||
        !harmony_read_lane(plan, span, reading->key.mode, reading->chords, &reading->chord_count, metre) ||
        !rational_is_zero(metre->lead) || !rational_is_zero(metre->tail) || !rational_is_zero(metre->offset) ||
        metre->cells == 0 || metre->cells_per_bar == 0 || !figure_cell_length(&metre->meter, &reading->cell_length, &reading->cells_per_bar) ||
        metre->bars * reading->cells_per_bar > FORM_SKELETON_MAX ||
        !return_key_shift(plan, plan_node_span(plan, ret.section), plan, span, &shift)) {
        reading_free(reading);
        return NULL;
    }
    reading->notes = read_source_melody(plan, ret.section, shift, &reading->note_count);
    if (!reading->notes) {
        reading_free(reading);
        return NULL;
    }
    /* The chord each cell of the harmony is under, and the cells of the figure. */
    size_t chord_of[FORM_SKELETON_MAX];
    size_t harmony_cell = 0;
    for (size_t c = 0; c < reading->chord_count; c++) {
        for (size_t k = 0; k < reading->chords[c].cells && harmony_cell < FORM_SKELETON_MAX; k++) {
            chord_of[harmony_cell++] = c;
        }
    }
    size_t cells = metre->bars * reading->cells_per_bar;
    bool ok = harmony_cell == metre->cells && cells > 0;
    size_t half = metre->bars / 2;
    FormSkeleton *skeleton = &reading->skeleton;
    skeleton->count = cells;
    for (size_t k = 0; ok && k < cells; k++) {
        Rational start;
        Rational end;
        Rational in_harmony;
        long long harmony_index;
        unsigned seventh;
        ok = cell_bounds(reading, k, &start, &end) && rational_div(start, metre->cell_length, &in_harmony) == RATIONAL_OK &&
             rational_floor(in_harmony, &harmony_index) == RATIONAL_OK && harmony_index >= 0 &&
             (size_t)harmony_index < metre->cells &&
             melody_chord_mask(&reading->key, &reading->chords[chord_of[harmony_index]], &reading->mask[k], &seventh);
        if (!ok) {
            break;
        }
        unsigned mask = reading->mask[k];
        const SourceNote *sounding = NULL;
        const SourceNote *inside = NULL;
        const SourceNote *before = NULL;
        for (size_t n = 0; n < reading->note_count; n++) {
            const SourceNote *note = &reading->notes[n];
            if (rational_cmp(note->onset, start) <= 0) {
                before = note;
                if (rational_cmp(start, note->end) < 0) {
                    sounding = note;
                }
            } else if (rational_cmp(note->onset, end) < 0 && !inside && has_pitch_class(mask, note->pitch)) {
                inside = note;
            }
        }
        int pitch = 0;
        if (sounding && has_pitch_class(mask, sounding->pitch)) {
            pitch = sounding->pitch;
        } else if (inside) {
            pitch = inside->pitch;
        } else {
            const SourceNote *reference = sounding ? sounding : before;
            ok = reference && nearest_chord_tone(mask, reference->pitch, &pitch);
        }
        size_t bar = k / reading->cells_per_bar;
        skeleton->pitch[k] = pitch;
        skeleton->onset[k] = start;
        skeleton->kept[k] = bar + 1 == metre->bars || (half > 0 && bar + 1 == half);
    }
    if (!ok) {
        reading_free(reading);
        return NULL;
    }
    return reading;
}

bool form_figure_skeleton(const PiecePlan *plan, NodeId section, FormSkeleton *out) {
    Reading *reading = out ? read_section(plan, section) : NULL;
    if (!reading) {
        return false;
    }
    *out = reading->skeleton;
    reading_free(reading);
    return true;
}

/* ------------------------------------------------------------- the figure */

/* A note of the written melody: from the start of the section. */
typedef struct {
    int pitch;
    Rational onset;
    Rational duration;
} Out;

static bool find_figure(const PiecePlan *plan, NodeId section, PlanFigure *out) {
    for (size_t i = 0; i < plan_node_constraint_count(plan, section); i++) {
        const PlanConstraint *constraint = plan_node_constraint_at(plan, section, i);
        if (constraint->kind == PLAN_CONSTRAINT_FIGURE) {
            *out = constraint->params.figure;
            return true;
        }
    }
    return false;
}

/* The notes a figure has per cell: the cell over the note length; 0 when it is not a whole number from 2 to 8. */
static size_t notes_per_cell(const Reading *reading, PlanFigureRhythm rhythm, Rational *length) {
    Rational per;
    if (rational_make(1, rhythm == PLAN_FIGURE_EIGHTHS ? 8 : 16, length) != RATIONAL_OK ||
        rational_div(reading->cell_length, *length, &per) != RATIONAL_OK || per.den != 1 || per.num < 2 || per.num > 8) {
        return 0;
    }
    return (size_t)per.num;
}

/*
 * The notes a kept cell holds, added to out (capacity for the returned melody's notes and one more): the returned
 * melody's notes that start in the cell, moved by octave, and the note that sounds into the cell from a cell before it
 * that is not kept, as a note that starts at the cell. A note ends where the next cell that is not kept begins.
 */
static bool kept_notes(const Reading *reading, size_t cell, int octave, Out *out, size_t *count) {
    const FormSkeleton *skeleton = &reading->skeleton;
    Rational start;
    Rational end;
    Rational ignored;
    Rational limit;
    size_t last = cell;
    while (last + 1 < skeleton->count && skeleton->kept[last + 1]) {
        last++;
    }
    if (!cell_bounds(reading, cell, &start, &end) || !cell_bounds(reading, last, &ignored, &limit)) {
        return false;
    }
    bool carry_in = cell == 0 || !skeleton->kept[cell - 1];
    for (size_t n = 0; n < reading->note_count; n++) {
        const SourceNote *note = &reading->notes[n];
        bool inside = rational_cmp(note->onset, start) >= 0 && rational_cmp(note->onset, end) < 0;
        bool carried = carry_in && rational_cmp(note->onset, start) < 0 && rational_cmp(start, note->end) < 0;
        Rational from = inside ? note->onset : start;
        Rational to = rational_cmp(note->end, limit) < 0 ? note->end : limit;
        if ((!inside && !carried) || rational_cmp(to, from) <= 0) {
            continue;
        }
        Out *added = &out[(*count)++];
        added->pitch = note->pitch + octave;
        added->onset = from;
        if (rational_sub(to, from, &added->duration) != RATIONAL_OK) {
            return false;
        }
    }
    return true;
}

/* The scale a cell is figured in: the key's, with the raised seventh in minor over a chord that holds the leading tone. */
static unsigned scale_pitch_classes(const AnalysisKey *key, unsigned chord_mask) {
    static const int MAJOR[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int MINOR[7] = {0, 2, 3, 5, 7, 8, 10};
    int tonic = analysis_key_tonic_pitch_class(key);
    bool minor = key->mode == KEY_STRUCTURE_MODE_MINOR;
    bool raised = minor && ((chord_mask >> ((tonic + 11) % 12)) & 1u);
    unsigned scale = 0;
    for (size_t i = 0; i < 7; i++) {
        int degree = minor ? MINOR[i] : MAJOR[i];
        degree = raised && degree == 10 ? 11 : degree;
        scale |= 1u << ((tonic + degree) % 12);
    }
    return scale;
}

/* The tone of a pitch-class mask next to a pitch, above (direction 1) or below (-1). */
static int next_tone(unsigned mask, int pitch, int direction) {
    int next = pitch + direction;
    for (int tries = 0; tries < 12 && !has_pitch_class(mask, next); tries++) {
        next += direction;
    }
    return next;
}

static bool within(const int *pitches, size_t count, const MelodyFillParams *range) {
    for (size_t i = 0; i < count; i++) {
        if (pitches[i] < range->range_low || pitches[i] > range->range_high) {
            return false;
        }
    }
    return true;
}

static bool neighbor_cell(unsigned scale, int tone, int side, size_t per, int *out) {
    int neighbor = next_tone(scale, tone, side);
    if (util_absolute(neighbor - tone) > 2) {
        return false;
    }
    for (size_t i = 0; i < per; i++) {
        out[i] = i % 2 == 0 ? tone : neighbor;
    }
    return true;
}

/* Up (direction 1) or down from the tone, half the notes away and the rest back. */
static void arch_cell(unsigned scale, int tone, int direction, size_t per, int *out) {
    out[0] = tone;
    for (size_t i = 1; i < per; i++) {
        out[i] = next_tone(scale, out[i - 1], i <= per / 2 ? direction : -direction);
    }
}

/* Stepwise toward the target (not the tone), turning back once it is reached. */
static void toward_cell(unsigned scale, int tone, int target, size_t per, int *out) {
    int direction = target > tone ? 1 : -1;
    bool turned = false;
    out[0] = tone;
    for (size_t i = 1; i < per; i++) {
        int next = next_tone(scale, out[i - 1], direction);
        if (!turned && ((direction > 0 && next > target) || (direction < 0 && next < target))) {
            next = target;
        }
        out[i] = next;
        if (!turned && next == target) {
            turned = true;
            direction = -direction;
        }
    }
}

static void broken_cell(unsigned mask, int tone, int direction, size_t per, int *out) {
    out[0] = tone;
    for (size_t i = 1; i < per; i++) {
        out[i] = next_tone(mask, out[i - 1], direction);
    }
}

/* The first pitch of the next cell: the next skeleton tone, or the first note of a kept cell. */
static int next_cell_pitch(const Reading *reading, size_t cell) {
    const FormSkeleton *skeleton = &reading->skeleton;
    Rational start;
    Rational end;
    if (cell + 1 >= skeleton->count) {
        return skeleton->pitch[cell];
    }
    if (skeleton->kept[cell + 1] && cell_bounds(reading, cell + 1, &start, &end)) {
        /* The notes are in order of onset: the first that sounds at the cell's start, else starts inside it. */
        for (size_t n = 0; n < reading->note_count; n++) {
            const SourceNote *note = &reading->notes[n];
            bool sounding = rational_cmp(note->onset, start) <= 0 && rational_cmp(start, note->end) < 0;
            bool inside = rational_cmp(note->onset, start) > 0 && rational_cmp(note->onset, end) < 0;
            if (sounding || inside) {
                return note->pitch;
            }
        }
    }
    return skeleton->pitch[cell + 1];
}

/* The pitches of one figured cell, made as the figure asks, else the other way; false when both leave the range. */
static bool figure_cell(const Reading *reading, size_t cell, const PlanFigure *figure, int draw, size_t figured, size_t per,
                        const MelodyFillParams *range, int *out) {
    int tone = reading->skeleton.pitch[cell];
    unsigned scale = scale_pitch_classes(&reading->key, reading->mask[cell]);
    for (int attempt = 0; attempt < 2; attempt++) {
        int turn = attempt == 0 ? 1 : -1;
        bool made = false;
        if (figure->kind == PLAN_FIGURE_NEIGHBOR) {
            made = neighbor_cell(scale, tone, draw * (figured % 2 == 0 ? 1 : -1) * turn, per, out);
        } else if (figure->kind == PLAN_FIGURE_SCALE) {
            int target = next_cell_pitch(reading, cell);
            if (target != tone) {
                if (attempt == 0) {
                    toward_cell(scale, tone, target, per, out);
                } else {
                    arch_cell(scale, tone, target > tone ? -1 : 1, per, out);
                }
            } else {
                arch_cell(scale, tone, draw * turn, per, out);
            }
            made = true;
        } else {
            broken_cell(reading->mask[cell], tone, draw * turn, per, out);
            made = true;
        }
        if (made && within(out, per, range)) {
            return true;
        }
    }
    return false;
}

/* The melody of a section, from its skeleton and figure: the notes in order of onset. */
static bool build_melody(const Reading *reading, const PlanFigure *figure, const MelodyFillParams *range, FillRng *rng,
                         Out *list, size_t *count) {
    const FormSkeleton *skeleton = &reading->skeleton;
    Rational length;
    size_t per = notes_per_cell(reading, figure->rhythm, &length);
    int draw = fill_rng_below(rng, 2) == 0 ? 1 : -1; /* the side or the direction, drawn once for the section */
    size_t figured = 0;
    *count = 0;
    if (per == 0) {
        return false;
    }
    for (size_t k = 0; k < skeleton->count; k++) {
        if (skeleton->kept[k]) {
            if (!kept_notes(reading, k, 0, list, count)) {
                return false;
            }
            continue;
        }
        int pitches[8];
        if (!figure_cell(reading, k, figure, draw, figured++, per, range, pitches)) {
            return false;
        }
        for (size_t i = 0; i < per; i++) {
            Rational offset;
            Out *added = &list[(*count)++];
            added->pitch = pitches[i];
            added->duration = length;
            if (rational_mul_int(length, (long long)i, &offset) != RATIONAL_OK ||
                rational_add(skeleton->onset[k], offset, &added->onset) != RATIONAL_OK) {
                return false;
            }
        }
    }
    /* The climax: the whole melody an octave up, when every note stays inside the range. */
    if (figure->climax) {
        bool fits = *count > 0;
        for (size_t n = 0; n < *count; n++) {
            fits = fits && list[n].pitch + 12 >= range->range_low && list[n].pitch + 12 <= range->range_high;
        }
        for (size_t n = 0; fits && n < *count; n++) {
            list[n].pitch += 12;
        }
    }
    return true;
}

/* Writes the notes into the melody spans of the section: each span holds the notes whose onsets lie in it. */
static bool write_spans(PiecePlan *plan, NodeId section, const Out *list, size_t count) {
    PlanSpan span = plan_node_span(plan, section);
    size_t spans = plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, NULL, 0);
    LaneSpanId *ids = spans ? calloc(spans, sizeof(LaneSpanId)) : NULL;
    PlanNote *notes = count ? calloc(count, sizeof(PlanNote)) : NULL;
    bool ok = spans > 0 && ids && notes;
    size_t written = 0;
    if (ok) {
        plan_lane_find(plan, PLAN_LANE_MELODY, 0, span, ids, spans);
    }
    for (size_t s = 0; ok && s < spans; s++) {
        PlanSpan lane_span = plan_lane_span_span(plan, ids[s]);
        Rational lane_end;
        size_t used = 0;
        ok = plan_span_end(lane_span, &lane_end) == PLAN_OK;
        for (size_t n = 0; ok && n < count; n++) {
            Rational at;
            ok = rational_add(span.start, list[n].onset, &at) == RATIONAL_OK;
            if (ok && rational_cmp(at, lane_span.start) >= 0 && rational_cmp(at, lane_end) < 0) {
                Rational offset;
                ok = rational_sub(at, lane_span.start, &offset) == RATIONAL_OK;
                notes[used++] = plan_note(list[n].pitch, offset, list[n].duration);
            }
        }
        PlanLanePayload payload;
        payload.notes.notes = notes;
        payload.notes.note_count = used;
        ok = ok && plan_lane_span_write(plan, ids[s], &payload) == PLAN_OK;
        written += used;
    }
    free(ids);
    free(notes);
    return ok && written == count;
}

bool form_figure_write(PiecePlan *plan, NodeId section, const MelodyFillParams *range, FillRng *rng) {
    MelodyFillParams defaults = melody_fill_params_default();
    PlanFigure figure;
    if (!plan || !rng || !find_figure(plan, section, &figure)) {
        return false;
    }
    Reading *reading = read_section(plan, section);
    if (!reading) {
        return false;
    }
    Out *list = calloc(reading->skeleton.count * 9 + reading->note_count + 1, sizeof(Out));
    size_t count = 0;
    bool ok = list != NULL && build_melody(reading, &figure, range ? range : &defaults, rng, list, &count) &&
              write_spans(plan, section, list, count);
    free(list);
    reading_free(reading);
    return ok;
}

/* ------------------------------------------------------------- the check */

static bool fault(char *reason, size_t size, size_t cell, const char *what) {
    if (reason && size > 0) {
        snprintf(reason, size, "cell %zu: %s", cell, what);
    }
    return false;
}

/* Whether a step from one pitch to the next is a step of the figure: 1 or 2 semitones, 3 in minor (the augmented second). */
static bool is_scale_step(const AnalysisKey *key, int from, int to) {
    int move = util_absolute(to - from);
    return (move >= 1 && move <= 2) || (move == 3 && key->mode == KEY_STRUCTURE_MODE_MINOR);
}

bool form_figure_check(const PiecePlan *plan, NodeId section, char *reason, size_t size) {
    PlanFigure figure;
    Reading *reading = plan && find_figure(plan, section, &figure) ? read_section(plan, section) : NULL;
    size_t own_count = 0;
    SourceNote *own = reading ? read_source_melody(plan, section, 0, &own_count) : NULL;
    if (!reading || !own) {
        free(own);
        reading_free(reading);
        return fault(reason, size, 0, "the section has no skeleton or no melody");
    }
    const FormSkeleton *skeleton = &reading->skeleton;
    Rational length;
    size_t per = notes_per_cell(reading, figure.rhythm, &length);
    Out *expected = calloc(reading->note_count + 2, sizeof(Out));
    bool ok = per > 0 && expected != NULL;
    int octave = 0;
    /* A raised climax: the first note of the first cell that is not kept is the skeleton tone an octave up. */
    for (size_t k = 0; ok && figure.climax && k < skeleton->count; k++) {
        if (skeleton->kept[k]) {
            continue;
        }
        for (size_t n = 0; n < own_count; n++) {
            if (rational_cmp(own[n].onset, skeleton->onset[k]) == 0) {
                octave = own[n].pitch == skeleton->pitch[k] + 12 ? 12 : 0;
            }
        }
        break;
    }
    ok = ok || fault(reason, size, 0, "the figure's note length does not divide the cell");
    for (size_t k = 0; ok && k < skeleton->count; k++) {
        Rational start;
        Rational end;
        const SourceNote *cell_notes[8 + FORM_SKELETON_MAX];
        size_t found = 0;
        ok = cell_bounds(reading, k, &start, &end);
        for (size_t n = 0; ok && n < own_count; n++) {
            if (rational_cmp(own[n].onset, start) >= 0 && rational_cmp(own[n].onset, end) < 0 && found < 8 + FORM_SKELETON_MAX) {
                cell_notes[found++] = &own[n];
            }
        }
        if (skeleton->kept[k]) {
            size_t held = 0;
            ok = kept_notes(reading, k, octave, expected, &held);
            if (ok && held != found) {
                ok = fault(reason, size, k, "a kept cell does not hold the returned notes");
            }
            for (size_t n = 0; ok && n < held; n++) {
                Rational duration;
                ok = rational_sub(cell_notes[n]->end, cell_notes[n]->onset, &duration) == RATIONAL_OK &&
                     cell_notes[n]->pitch == expected[n].pitch && rational_cmp(cell_notes[n]->onset, expected[n].onset) == 0 &&
                     rational_cmp(duration, expected[n].duration) == 0;
                ok = ok || fault(reason, size, k, "a kept cell does not hold the returned notes");
            }
            continue;
        }
        int tone = skeleton->pitch[k] + octave;
        if (found != per) {
            ok = fault(reason, size, k, "the cell does not hold the notes of the figure");
            break;
        }
        for (size_t i = 0; ok && i < per; i++) {
            Rational at;
            Rational duration;
            ok = rational_mul_int(length, (long long)i, &at) == RATIONAL_OK && rational_add(start, at, &at) == RATIONAL_OK &&
                 rational_cmp(cell_notes[i]->onset, at) == 0 &&
                 rational_sub(cell_notes[i]->end, cell_notes[i]->onset, &duration) == RATIONAL_OK &&
                 rational_cmp(duration, length) == 0;
            ok = ok || fault(reason, size, k, "the notes of the cell are not equal");
            if (!ok) {
                break;
            }
            int pitch = cell_notes[i]->pitch;
            if (i == 0 && pitch != tone) {
                ok = fault(reason, size, k, "the cell does not start on the skeleton tone");
            } else if (figure.kind == PLAN_FIGURE_NEIGHBOR && i % 2 == 0 && pitch != tone) {
                ok = fault(reason, size, k, "a neighbor figure leaves the skeleton tone on an even note");
            } else if (figure.kind == PLAN_FIGURE_NEIGHBOR && i % 2 == 1 &&
                       (util_absolute(pitch - tone) < 1 || util_absolute(pitch - tone) > 2)) {
                ok = fault(reason, size, k, "a neighbor is not a step from the skeleton tone");
            } else if (figure.kind == PLAN_FIGURE_SCALE && i > 0 && !is_scale_step(&reading->key, cell_notes[i - 1]->pitch, pitch)) {
                ok = fault(reason, size, k, "a scale figure moves by more than a step");
            } else if (figure.kind == PLAN_FIGURE_BROKEN_CHORD && !has_pitch_class(reading->mask[k], pitch)) {
                ok = fault(reason, size, k, "a broken chord holds a note that is not a tone of the chord");
            }
        }
    }
    free(expected);
    free(own);
    reading_free(reading);
    return ok;
}
