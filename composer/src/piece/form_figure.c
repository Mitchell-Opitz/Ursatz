#include <ursatz_composer/form_figure.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "harmony/harmony_internal.h"
#include "melody/melody_internal.h"
#include "piece/form_figure_internal.h"
#include "piece/form_return_internal.h"
#include "util/plan_util.h"
#include "util/util.h"

#include <theory/tonality/tonal_key.h>

/*
 * The motive of variation (R-497): the skeleton of a figurate return, and the figure written over it. Time in this
 * file is Rational, from the start of the section.
 */

static int compare_notes(const void *a, const void *b) {
    return rational_cmp(((const FigureSourceNote *)a)->onset, ((const FigureSourceNote *)b)->onset);
}

/* The melody of the section the target returns, read as FigureSourceNote, in order of onset. NULL when it holds no note. */
FigureSourceNote *form_figure_read_melody(const PiecePlan *plan, NodeId source, int shift, bool mode_change, int tonic,
                                          const PitchMaterial *to_material, size_t *count) {
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
    FigureSourceNote *notes = total ? calloc(total, sizeof(FigureSourceNote)) : NULL;
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
            notes[used].pitch = mode_change ? return_map_pitch(tonic, to_material, held->notes[n].midi + shift)
                                            : held->notes[n].midi + shift;
            used++;
        }
    }
    free(ids);
    if (!ok) {
        free(notes);
        return NULL;
    }
    qsort(notes, used, sizeof(FigureSourceNote), compare_notes);
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

bool form_figure_has_pitch_class(unsigned mask, int pitch) {
    return (mask >> util_pitch_class(pitch)) & 1u;
}

/* The chord tone of the mask nearest a pitch (the lower of two equally near); false when the mask is empty. */
static bool nearest_chord_tone(unsigned mask, int pitch, int *out) {
    bool found = false;
    for (int candidate = pitch - 12; candidate <= pitch + 12; candidate++) {
        if (form_figure_has_pitch_class(mask, candidate) && (!found || util_absolute(candidate - pitch) < util_absolute(*out - pitch))) {
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
    TimeSignature *signature = NULL;
    Duration beat;
    if (!plan_meter_signature(*meter, &signature)) {
        return false;
    }
    size_t beats = time_signature_group_count(signature);
    DurationResult beat_made = time_signature_beat_length(signature, &beat);
    time_signature_destroy(signature);
    /* two to four beats of a quarter or a dotted quarter: the figures are written in those cells */
    if (beat_made != DURATION_OK || beats < 2 || beats > 4 ||
        (rational_cmp(beat.value, (Rational){1, 4}) != 0 && rational_cmp(beat.value, (Rational){3, 8}) != 0)) {
        return false;
    }
    *per_bar = beats;
    *cell = beat.value;
    return true;
}

void form_figure_reading_free(FigureReading *reading) {
    if (reading) {
        free(reading->notes);
        free(reading);
    }
}

bool form_figure_cell_bounds(const FigureReading *reading, size_t cell, Rational *start, Rational *end) {
    return rational_mul_int(reading->cell_length, (long long)cell, start) == RATIONAL_OK &&
           rational_add(*start, reading->cell_length, end) == RATIONAL_OK;
}

FigureReading *form_figure_read_section(const PiecePlan *plan, NodeId section) {
    ReturnRef ret;
    int shift;
    if (!plan || !return_target(plan, section, &ret) || ret.variation != SECTION_RETURN_FIGURATE) {
        return NULL;
    }
    FigureReading *reading = calloc(1, sizeof(*reading));
    if (!reading) {
        return NULL;
    }
    PlanSpan span = plan_node_span(plan, section);
    PhraseMetre *metre = &reading->metre;
    if (!section_key(plan, span, &reading->key) ||
        !harmony_read_lane(plan, span, harmony_key_mode(&reading->key), reading->chords, &reading->chord_count, metre) ||
        !rational_is_zero(metre->lead) || !rational_is_zero(metre->tail) || !rational_is_zero(metre->offset) ||
        metre->cells == 0 || metre->cells_per_bar == 0 || !figure_cell_length(&metre->meter, &reading->cell_length, &reading->cells_per_bar) ||
        metre->bars * reading->cells_per_bar > FORM_SKELETON_MAX) {
        form_figure_reading_free(reading);
        return NULL;
    }
    /* A copy across modes (R-499) is moved by 0 and mapped degree by degree, as form-return maps it. */
    bool mode_change = !return_key_shift(plan, plan_node_span(plan, ret.section), plan, span, &shift);
    const PitchMaterial *to_material = NULL;
    if (mode_change) {
        shift = 0;
        if (!return_mode_shift(plan, plan_node_span(plan, ret.section), plan, span, &to_material)) {
            form_figure_reading_free(reading);
            return NULL;
        }
    }
    reading->notes = form_figure_read_melody(plan, ret.section, shift, mode_change, analysis_key_tonic_pitch_class(&reading->key),
                                        to_material, &reading->note_count);
    if (!reading->notes) {
        form_figure_reading_free(reading);
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
        ok = form_figure_cell_bounds(reading, k, &start, &end) && rational_div(start, metre->cell_length, &in_harmony) == RATIONAL_OK &&
             rational_floor(in_harmony, &harmony_index) == RATIONAL_OK && harmony_index >= 0 &&
             (size_t)harmony_index < metre->cells &&
             melody_chord_mask(&reading->key, &reading->chords[chord_of[harmony_index]], &reading->mask[k], &seventh);
        if (!ok) {
            break;
        }
        unsigned mask = reading->mask[k];
        const FigureSourceNote *sounding = NULL;
        const FigureSourceNote *inside = NULL;
        const FigureSourceNote *before = NULL;
        for (size_t n = 0; n < reading->note_count; n++) {
            const FigureSourceNote *note = &reading->notes[n];
            if (rational_cmp(note->onset, start) <= 0) {
                before = note;
                if (rational_cmp(start, note->end) < 0) {
                    sounding = note;
                }
            } else if (rational_cmp(note->onset, end) < 0 && !inside && form_figure_has_pitch_class(mask, note->pitch)) {
                inside = note;
            }
        }
        int pitch = 0;
        if (sounding && form_figure_has_pitch_class(mask, sounding->pitch)) {
            pitch = sounding->pitch;
        } else if (inside) {
            pitch = inside->pitch;
        } else {
            const FigureSourceNote *reference = sounding ? sounding : before;
            ok = reference && nearest_chord_tone(mask, reference->pitch, &pitch);
        }
        size_t bar = k / reading->cells_per_bar;
        skeleton->pitch[k] = pitch;
        skeleton->onset[k] = start;
        skeleton->kept[k] = bar + 1 == metre->bars || (half > 0 && bar + 1 == half);
    }
    if (!ok) {
        form_figure_reading_free(reading);
        return NULL;
    }
    return reading;
}

bool form_figure_skeleton(const PiecePlan *plan, NodeId section, FormSkeleton *out) {
    FigureReading *reading = out ? form_figure_read_section(plan, section) : NULL;
    if (!reading) {
        return false;
    }
    *out = reading->skeleton;
    form_figure_reading_free(reading);
    return true;
}

/* ------------------------------------------------------------- the figure */

bool form_figure_find(const PiecePlan *plan, NodeId section, SectionFigure *out) {
    const SectionDescription *described = plan_section_or_empty(plan, section);
    if (described->has_figure) {
        *out = described->figure;
    }
    return described->has_figure;
}

/* The notes a figure has per cell: the cell over the note length; 0 when it is not a whole number from 2 to 8. */
size_t form_figure_notes_per_cell(const FigureReading *reading, SectionFigureRhythm rhythm, Rational *length) {
    Rational per;
    if (rational_make(1, rhythm == SECTION_FIGURE_EIGHTHS ? 8 : 16, length) != RATIONAL_OK ||
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
bool form_figure_kept_notes(const FigureReading *reading, size_t cell, int octave, FigureNote *out, size_t *count) {
    const FormSkeleton *skeleton = &reading->skeleton;
    Rational start;
    Rational end;
    Rational ignored;
    Rational limit;
    size_t last = cell;
    while (last + 1 < skeleton->count && skeleton->kept[last + 1]) {
        last++;
    }
    if (!form_figure_cell_bounds(reading, cell, &start, &end) || !form_figure_cell_bounds(reading, last, &ignored, &limit)) {
        return false;
    }
    bool carry_in = cell == 0 || !skeleton->kept[cell - 1];
    for (size_t n = 0; n < reading->note_count; n++) {
        const FigureSourceNote *note = &reading->notes[n];
        bool inside = rational_cmp(note->onset, start) >= 0 && rational_cmp(note->onset, end) < 0;
        bool carried = carry_in && rational_cmp(note->onset, start) < 0 && rational_cmp(start, note->end) < 0;
        Rational from = inside ? note->onset : start;
        Rational to = rational_cmp(note->end, limit) < 0 ? note->end : limit;
        if ((!inside && !carried) || rational_cmp(to, from) <= 0) {
            continue;
        }
        FigureNote *added = &out[(*count)++];
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
    int tonic = analysis_key_tonic_pitch_class(key);
    if (!composer_key_is_minor(key)) {
        return util_scale_mask(tonic, key->material);
    }
    bool raised = (chord_mask >> ((tonic + 11) % 12)) & 1u;
    return raised ? util_scale_mask_raised_seventh(tonic, key->material) : util_scale_mask(tonic, key->material);
}

/* The tone of a pitch-class mask next to a pitch, above (direction 1) or below (-1). */
static int next_tone(unsigned mask, int pitch, int direction) {
    int next = pitch + direction;
    for (int tries = 0; tries < 12 && !form_figure_has_pitch_class(mask, next); tries++) {
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

/* The chord's tones from the tone, up or down; a tone outside the range turns the line back (six sixteenths are past two octaves). */
static void broken_cell(unsigned mask, int tone, int direction, size_t per, const MelodyFillParams *range, int *out) {
    out[0] = tone;
    for (size_t i = 1; i < per; i++) {
        int next = next_tone(mask, out[i - 1], direction);
        if (next < range->range_low || next > range->range_high) {
            direction = -direction;
            next = next_tone(mask, out[i - 1], direction);
        }
        out[i] = next;
    }
}

/* The first pitch of the next cell: the next skeleton tone, or the first note of a kept cell. */
static int next_cell_pitch(const FigureReading *reading, size_t cell) {
    const FormSkeleton *skeleton = &reading->skeleton;
    Rational start;
    Rational end;
    if (cell + 1 >= skeleton->count) {
        return skeleton->pitch[cell];
    }
    if (skeleton->kept[cell + 1] && form_figure_cell_bounds(reading, cell + 1, &start, &end)) {
        /* The notes are in order of onset: the first that sounds at the cell's start, else starts inside it. */
        for (size_t n = 0; n < reading->note_count; n++) {
            const FigureSourceNote *note = &reading->notes[n];
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
static bool figure_cell(const FigureReading *reading, size_t cell, const SectionFigure *figure, int draw, size_t figured, size_t per,
                        const MelodyFillParams *range, int *out) {
    int tone = reading->skeleton.pitch[cell];
    unsigned scale = scale_pitch_classes(&reading->key, reading->mask[cell]);
    for (int attempt = 0; attempt < 2; attempt++) {
        int turn = attempt == 0 ? 1 : -1;
        bool made = false;
        if (figure->kind == SECTION_FIGURE_NEIGHBOR) {
            made = neighbor_cell(scale, tone, draw * (figured % 2 == 0 ? 1 : -1) * turn, per, out);
        } else if (figure->kind == SECTION_FIGURE_SCALE) {
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
            broken_cell(reading->mask[cell], tone, draw * turn, per, range, out);
            made = true;
        }
        if (made && within(out, per, range)) {
            return true;
        }
    }
    return false;
}

/* The melody of a section, from its skeleton and figure: the notes in order of onset. */
static bool build_melody(const FigureReading *reading, const SectionFigure *figure, const MelodyFillParams *range, FillRng *rng,
                         FigureNote *list, size_t *count) {
    const FormSkeleton *skeleton = &reading->skeleton;
    Rational length;
    size_t per = form_figure_notes_per_cell(reading, figure->rhythm, &length);
    int draw = fill_rng_below(rng, 2) == 0 ? 1 : -1; /* the side or the direction, drawn once for the section */
    size_t figured = 0;
    *count = 0;
    if (per == 0) {
        return false;
    }
    for (size_t k = 0; k < skeleton->count; k++) {
        if (skeleton->kept[k]) {
            if (!form_figure_kept_notes(reading, k, 0, list, count)) {
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
            FigureNote *added = &list[(*count)++];
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
static bool write_spans(PiecePlan *plan, NodeId section, const FigureNote *list, size_t count) {
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
    SectionFigure figure;
    if (!plan || !rng || !form_figure_find(plan, section, &figure)) {
        return false;
    }
    FigureReading *reading = form_figure_read_section(plan, section);
    if (!reading) {
        return false;
    }
    FigureNote *list = calloc(reading->skeleton.count * 9 + reading->note_count + 1, sizeof(FigureNote));
    size_t count = 0;
    bool ok = list != NULL && build_melody(reading, &figure, range ? range : &defaults, rng, list, &count) &&
              write_spans(plan, section, list, count);
    free(list);
    form_figure_reading_free(reading);
    return ok;
}
