#include <ursatz_composer/form_figure.h>

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

/* The chord tone of the mask nearest a pitch (the lower of two equally near); false when the mask is empty. */
static bool nearest_chord_tone(unsigned mask, int pitch, int *out) {
    bool found = false;
    for (int candidate = pitch - 12; candidate <= pitch + 12; candidate++) {
        if (!((mask >> (((candidate % 12) + 12) % 12)) & 1u)) {
            continue;
        }
        if (!found || util_absolute(candidate - pitch) < util_absolute(*out - pitch)) {
            *out = candidate;
            found = true;
        }
    }
    return found;
}

bool form_figure_skeleton(const PiecePlan *plan, NodeId section, FormSkeleton *out) {
    PlanReturn ret;
    AnalysisKey key;
    PhraseChord chords[HARMONY_MAX_CHORDS];
    size_t chord_count;
    PhraseMetre metre;
    int shift;
    size_t note_count;
    if (!plan || !out || !return_target(plan, section, &ret) || ret.variation != PLAN_RETURN_FIGURATE) {
        return false;
    }
    PlanSpan span = plan_node_span(plan, section);
    if (!section_key(plan, span, &key) || !harmony_read_lane(plan, span, key.mode, chords, &chord_count, &metre) ||
        !rational_is_zero(metre.lead) || !rational_is_zero(metre.tail) || !rational_is_zero(metre.offset) ||
        metre.cells == 0 || metre.cells > FORM_SKELETON_MAX || metre.cells_per_bar == 0 ||
        !return_key_shift(plan, plan_node_span(plan, ret.section), plan, span, &shift)) {
        return false;
    }
    SourceNote *notes = read_source_melody(plan, ret.section, shift, &note_count);
    if (!notes) {
        return false;
    }
    /* The chord each cell is under. */
    size_t chord_of[FORM_SKELETON_MAX];
    size_t cell = 0;
    for (size_t c = 0; c < chord_count; c++) {
        for (size_t k = 0; k < chords[c].cells && cell < FORM_SKELETON_MAX; k++) {
            chord_of[cell++] = c;
        }
    }
    bool ok = cell == metre.cells;
    size_t half = metre.bars / 2;
    memset(out, 0, sizeof(*out));
    out->count = metre.cells;
    for (size_t k = 0; ok && k < metre.cells; k++) {
        Rational start;
        Rational end;
        unsigned mask;
        unsigned seventh;
        ok = rational_mul_int(metre.cell_length, (long long)k, &start) == RATIONAL_OK &&
             rational_add(start, metre.cell_length, &end) == RATIONAL_OK &&
             melody_chord_mask(&key, &chords[chord_of[k]], &mask, &seventh);
        if (!ok) {
            break;
        }
        const SourceNote *sounding = NULL;
        const SourceNote *inside = NULL;
        const SourceNote *before = NULL;
        for (size_t n = 0; n < note_count; n++) {
            if (rational_cmp(notes[n].onset, start) <= 0) {
                before = &notes[n];
                if (rational_cmp(start, notes[n].end) < 0) {
                    sounding = &notes[n];
                }
            } else if (rational_cmp(notes[n].onset, end) < 0 && !inside && ((mask >> (((notes[n].pitch % 12) + 12) % 12)) & 1u)) {
                inside = &notes[n];
            }
        }
        int pitch = 0;
        if (sounding && ((mask >> (((sounding->pitch % 12) + 12) % 12)) & 1u)) {
            pitch = sounding->pitch;
        } else if (inside) {
            pitch = inside->pitch;
        } else {
            const SourceNote *reference = sounding ? sounding : before;
            ok = reference && nearest_chord_tone(mask, reference->pitch, &pitch);
        }
        size_t bar = k / metre.cells_per_bar;
        out->pitch[k] = pitch;
        out->onset[k] = start;
        out->kept[k] = bar + 1 == metre.bars || (half > 0 && bar + 1 == half);
    }
    free(notes);
    return ok;
}
