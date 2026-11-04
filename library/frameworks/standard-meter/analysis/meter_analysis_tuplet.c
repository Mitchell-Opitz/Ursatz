#include "meter_analysis_tuplet.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "core/value.h"
#include "meter_analysis_claim.h"
#include "meter_analysis_time.h"
#include "time/duration.h"
#include "time/meter_arithmetic.h"
#include "time/score_time.h"
#include "time/tuplet.h"

/* The finest grid a tuplet is read on: a twelfth of a whole note is a triplet eighth, a 24th a sixteenth triplet. The
   library's own grid is 1/32, so a time on the finer grid but off the coarser is a third of a step: a tuplet's. */
#define TUPLET_GRID 96
#define NATIVE_STEPS 3 /* TUPLET_GRID steps in one step of the library's 1/32 grid */

/* True when the onset lies a whole number of beats after the base; false when the arithmetic does not fit. */
static bool whole_beats_after(Rational onset, Rational base, Rational beat, bool *whole) {
    Rational offset;
    Rational beats;
    if (rational_sub(onset, base, &offset) != RATIONAL_OK || rational_div(offset, beat, &beats) != RATIONAL_OK) {
        return false;
    }
    *whole = beats.den == 1;
    return true;
}

typedef enum {
    GRID_OFF = 0,
    GRID_ON,
    GRID_OVERFLOW
} GridRead;

/* The time as a whole number of TUPLET_GRID steps, when it lies within a quarter of a step of one: the way a MIDI file's
   rounded times are read. GRID_OFF for a time further from the grid (a performance's rubato), which is then not read at
   all. GRID_OVERFLOW when the arithmetic does not fit. */
static GridRead on_tuplet_grid(Rational time, long long *steps) {
    if (time.num > LLONG_MAX / TUPLET_GRID || time.num < LLONG_MIN / TUPLET_GRID || time.den > LLONG_MAX / 4 ||
        time.den <= 0) {
        return GRID_OVERFLOW;
    }
    long long scaled = time.num * TUPLET_GRID;
    if (scaled > LLONG_MAX - time.den / 2) {
        return GRID_OVERFLOW;
    }
    long long nearest = (scaled + time.den / 2) / time.den;
    if (nearest > LLONG_MAX / time.den) {
        return GRID_OVERFLOW;
    }
    long long off = scaled - nearest * time.den;
    if (off < 0) {
        off = -off;
    }
    *steps = nearest;
    return off * 4 <= time.den ? GRID_ON : GRID_OFF;
}

typedef struct {
    size_t note;
    long long steps;         /* the attack, in steps of the tuplet grid */
    Rational onset;
    long long next_steps;    /* the next attack, or -1 for the last */
    Rational duration; /* the note's length when its release is on the grid, otherwise 0 */
    Rational beat;           /* the length of the beat the note is attacked in */
    int division;            /* 2 (simple) or 3 (compound) */
    bool on_beat;
    size_t attack;           /* the position of the attack among the piece's attacks */
    unsigned actual;         /* the note's ratio actual:normal; 0 when it is not a tuplet note */
    unsigned normal;
} TupletNote;

static int compare_tuplet_notes(const void *a, const void *b) {
    const TupletNote *x = (const TupletNote *)a;
    const TupletNote *y = (const TupletNote *)b;
    if (x->steps != y->steps) {
        return x->steps < y->steps ? -1 : 1;
    }
    return x->note < y->note ? -1 : (x->note > y->note ? 1 : 0);
}

/* Reads a note's attack on the tuplet grid, and its release when that is on the grid too (a performer's articulation
   leaves it off, and the library's own release snap would pull a triplet's onto its 1/32 grid, so the raw release is
   rounded here). GRID_OFF when the attack is off the grid or the meter has no beat to read against. */
static GridRead read_tuplet_note(const MeterMap *map, Rational onset, Rational release, TupletNote *note) {
    long long release_steps = 0;
    int measure = 0;
    StandardMeterMeasureGrid grid;
    GridRead read = on_tuplet_grid(onset, &note->steps);
    if (read != GRID_ON) {
        return read;
    }
    note->duration = rational_ratio(0, 1);
    if (rational_make(note->steps, TUPLET_GRID, &note->onset) != RATIONAL_OK) {
        return GRID_OVERFLOW;
    }
    read = on_tuplet_grid(release, &release_steps);
    if (read == GRID_OVERFLOW) {
        return read;
    }
    if (read == GRID_ON && release_steps > note->steps) {
        if (rational_make(release_steps - note->steps, TUPLET_GRID, &note->duration) != RATIONAL_OK) {
            return GRID_OVERFLOW;
        }
    }
    if (meter_map_measure_of(map, note->onset, &measure) != METER_MAP_OK || !standard_meter_measure_grid(map, measure, &grid) ||
        (grid.division != 2 && grid.division != 3)) {
        return GRID_OFF;
    }
    note->beat = grid.beat;
    note->division = (int)grid.division;
    if (!whole_beats_after(note->onset, grid.base, grid.beat, &note->on_beat)) {
        return GRID_OVERFLOW;
    }
    return GRID_ON;
}

/* Numbers the attacks and finds each note's next attack, from the notes in time order. */
static void link_attacks(TupletNote *notes, size_t count) {
    size_t attack = 0;
    for (size_t k = 0; k < count; k++) {
        if (k > 0 && notes[k].steps != notes[k - 1].steps) {
            attack++;
        }
        notes[k].attack = attack;
        notes[k].next_steps = -1;
        for (size_t j = k + 1; j < count; j++) {
            if (notes[j].steps != notes[k].steps) {
                notes[k].next_steps = notes[j].steps;
                break;
            }
        }
    }
}

/* A note with an attack, or a distance to the next attack, that is not a whole step of the library's 1/32 grid divides a
   step in three: a triplet note (3:2, which includes the sextuplets). */
static void mark_triplets(TupletNote *notes, size_t count) {
    for (size_t k = 0; k < count; k++) {
        bool attack_off = notes[k].steps % NATIVE_STEPS != 0;
        bool distance_off = notes[k].next_steps >= 0 && (notes[k].next_steps - notes[k].steps) % NATIVE_STEPS != 0;
        if (attack_off || distance_off) {
            notes[k].actual = 3;
            notes[k].normal = 2;
        }
    }
}

/* In a compound meter a beat halved in two equal notes is a duplet: a note on the beat whose next attack is half a beat
   later, and a note there that lasts, or is followed by an attack after, half a beat. The notes sound as two dotted values,
   so nothing else tells them apart; a dotted note and a short one do not match. */
static void mark_duplets(TupletNote *notes, size_t count) {
    for (size_t k = 0; k < count; k++) {
        if (notes[k].division != 3 || !notes[k].on_beat) {
            continue;
        }
        Rational half;
        Rational later;
        Rational after_later;
        if (rational_div_int(notes[k].beat, 2, &half) != RATIONAL_OK || rational_add(notes[k].onset, half, &later) != RATIONAL_OK ||
            rational_add(later, half, &after_later) != RATIONAL_OK) {
            continue;
        }
        for (size_t j = k + 1; j < count && rational_cmp(notes[j].onset, later) <= 0; j++) {
            if (rational_cmp(notes[j].onset, later) != 0) {
                continue;
            }
            Rational next_attack;
            Rational own_next;
            bool next_is_half = notes[j].next_steps >= 0 && rational_make(notes[j].next_steps, TUPLET_GRID, &next_attack) == RATIONAL_OK &&
                                rational_cmp(next_attack, after_later) == 0;
            bool lasts_half = rational_cmp(notes[j].duration, half) == 0;
            if (notes[k].next_steps >= 0 && rational_make(notes[k].next_steps, TUPLET_GRID, &own_next) == RATIONAL_OK &&
                rational_cmp(own_next, later) == 0 && (next_is_half || lasts_half)) {
                for (size_t m = 0; m < 2; m++) {
                    TupletNote *marked = m == 0 ? &notes[k] : &notes[j];
                    if (marked->actual == 0) {
                        marked->actual = 2;
                        marked->normal = 3;
                    }
                }
                break;
            }
        }
    }
}

/* One run being built: the consecutive attacks of one ratio. */
typedef struct {
    size_t *notes;
    size_t count;
    unsigned attacks;
    size_t last_attack;      /* the attack number of the latest attack of the run */
    Rational first;          /* the first attack */
    Rational end;            /* where the latest attack's note leads: the next attack, or its release */
    Rational group_end;
    bool has_group_end;
} TupletRun;

static const char *tuplet_name(unsigned actual, unsigned normal) {
    return actual == 3 && normal == 2 ? "triplet" : (actual == 2 && normal == 3 ? "duplet" : "tuplet");
}

/* Names a finished run with the library's rules and appends it. A single attack is no group and is left out. */
static bool emit_run(StandardMeterTuplet **items, size_t *count, size_t *capacity, const AnalysisNoteSet *notes,
                     unsigned actual, unsigned normal, const TupletRun *run) {
    if (run->attacks < 2) {
        return true;
    }
    if (*count == *capacity) {
        size_t grown_capacity = *capacity == 0 ? 16 : *capacity * 2;
        StandardMeterTuplet *grown = (StandardMeterTuplet *)realloc(*items, grown_capacity * sizeof(StandardMeterTuplet));
        if (!grown) {
            return false;
        }
        *items = grown;
        *capacity = grown_capacity;
    }
    StandardMeterTuplet *item = &(*items)[*count];
    memset(item, 0, sizeof(*item));
    item->notes = (size_t *)malloc(run->count * sizeof(size_t));
    if (!item->notes) {
        return false;
    }
    memcpy(item->notes, run->notes, run->count * sizeof(size_t));
    item->note_count = run->count;
    item->actual = actual;
    item->normal = normal;
    item->name = tuplet_name(actual, normal);
    item->attacks = run->attacks;
    item->meter = METER_TYPE_UNCLASSIFIED;
    item->beat = 1.0;
    int measure = 0;
    double beat = 1.0;
    if (standard_meter_beat_of(notes->map, run->first, &measure, &beat)) {
        item->measure = measure;
        item->beat = beat;
    }
    const MeterMapRegion *region = NULL;
    Rational at_q = rational_ratio(0, 1);
    Rational end_q = rational_ratio(0, 1);
    Tuplet *tuplet = NULL;
    if (score_time_span(notes->notes[run->notes[0]].span, &at_q, &end_q)) {
        MusicalTime *at = NULL;
        if (at_q.num >= 0 && musical_time_create((long)at_q.num, (unsigned long)at_q.den, &at) == MUSICAL_TIME_OK) {
            if (meter_map_region_at(notes->map, at, &region) != METER_MAP_OK) {
                region = NULL;
            }
            musical_time_destroy(at);
        }
    }
    if (region != NULL && tuplet_create(actual, normal, &tuplet) == TUPLET_OK) {
        const TimeSignature *signature = meter_map_region_time_signature(region);
        MeterType type = standard_meter_type(signature);
        item->meter = type;
        item->borrowed = standard_meter_tuplet_borrowed(tuplet, type);
        Rational end = run->has_group_end ? run->group_end : run->end;
        Rational span;
        Duration *duration = NULL;
        TupletLevel level;
        if (rational_sub(end, run->first, &span) == RATIONAL_OK && span.num > 0 &&
            duration_create((long)span.num, (unsigned long)span.den, &duration) == DURATION_OK) {
            if (standard_meter_tuplet_level(signature, duration, &level)) {
                item->has_level = true;
                item->level = level;
                item->level_typical = standard_meter_tuplet_level_typical(tuplet, level);
            }
            duration_destroy(duration);
        }
    }
    tuplet_destroy(tuplet);
    (*count)++;
    return true;
}

/* The runs of one ratio, from the notes in time order. A run goes on while its ratio's notes attack one after another (the
   next attack of the piece is the next attack of the run). */
static bool collect_runs(StandardMeterTuplet **items, size_t *count, size_t *capacity, const AnalysisNoteSet *notes,
                         const TupletNote *line_notes, size_t note_count, unsigned actual, unsigned normal,
                         size_t *scratch) {
    TupletRun run;
    memset(&run, 0, sizeof(run));
    run.notes = scratch;
    bool open = false;
    for (size_t k = 0; k < note_count; k++) {
        const TupletNote *note = &line_notes[k];
        if (note->actual != actual || note->normal != normal) {
            continue;
        }
        Rational end;
        if ((note->next_steps >= 0 ? rational_make(note->next_steps, TUPLET_GRID, &end)
                                   : rational_add(note->onset, note->duration, &end)) != RATIONAL_OK) {
            continue;
        }
        if (open && note->attack == run.last_attack) {
            /* another note of the same attack */
        } else if (open && note->attack == run.last_attack + 1) {
            if (run.attacks == actual && !run.has_group_end) {
                run.group_end = note->onset;
                run.has_group_end = true;
            }
            run.attacks++;
            run.last_attack = note->attack;
            run.end = end;
        } else {
            if (open && !emit_run(items, count, capacity, notes, actual, normal, &run)) {
                return false;
            }
            memset(&run, 0, sizeof(run));
            run.notes = scratch;
            open = true;
            run.attacks = 1;
            run.last_attack = note->attack;
            run.first = note->onset;
            run.end = end;
        }
        run.notes[run.count++] = note->note;
    }
    return !open || emit_run(items, count, capacity, notes, actual, normal, &run);
}

static int compare_tuplets(const void *a, const void *b) {
    const StandardMeterTuplet *x = (const StandardMeterTuplet *)a;
    const StandardMeterTuplet *y = (const StandardMeterTuplet *)b;
    if (x->actual != y->actual) {
        return x->actual < y->actual ? -1 : 1;
    }
    if (x->measure != y->measure) {
        return x->measure < y->measure ? -1 : 1;
    }
    return x->beat < y->beat ? -1 : (x->beat > y->beat ? 1 : 0);
}

static void free_items(StandardMeterTuplet *items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(items[i].notes);
    }
    free(items);
}

StandardMeterAnalysisStatus standard_meter_find_tuplets(const AnalysisNoteSet *notes, const bool *in_line,
                                                        StandardMeterTuplets *out) {
    if (notes == NULL || out == NULL || notes->map == NULL || (notes->count > 0 && notes->notes == NULL)) {
        return STANDARD_METER_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardMeterConfig config = standard_meter_config_default();
    TupletNote *stream = (TupletNote *)malloc((notes->count + 1) * sizeof(TupletNote));
    size_t *scratch = (size_t *)malloc((notes->count + 1) * sizeof(size_t));
    StandardMeterTuplet *items = NULL;
    size_t count = 0;
    size_t capacity = 0;
    size_t stream_count = 0;
    StandardMeterAnalysisStatus status =
        stream && scratch ? STANDARD_METER_ANALYSIS_OK : STANDARD_METER_ANALYSIS_ERR_ALLOC;
    /* every note is read as one stream: the outer voice lines split a melody across lines wherever the extraction thins it,
       and a tuplet is a figure of the music, not of a line */
    for (size_t i = 0; status == STANDARD_METER_ANALYSIS_OK && i < notes->count; i++) {
        if (in_line != NULL && !in_line[i]) {
            continue;
        }
        Rational onset;
        Rational release;
        if (!score_time_span(notes->notes[i].span, &onset, &release) || !standard_meter_snap_time(&config, &onset, false, NULL)) {
            continue;
        }
        TupletNote *note = &stream[stream_count];
        memset(note, 0, sizeof(*note));
        note->note = i;
        GridRead read = read_tuplet_note(notes->map, onset, release, note);
        if (read == GRID_ON) {
            stream_count++;
        } else if (read == GRID_OVERFLOW) {
            status = STANDARD_METER_ANALYSIS_ERR_OVERFLOW;
        }
    }
    if (status == STANDARD_METER_ANALYSIS_OK) {
        qsort(stream, stream_count, sizeof(TupletNote), compare_tuplet_notes);
        link_attacks(stream, stream_count);
        mark_triplets(stream, stream_count);
        mark_duplets(stream, stream_count);
        if (!collect_runs(&items, &count, &capacity, notes, stream, stream_count, 2, 3, scratch) ||
            !collect_runs(&items, &count, &capacity, notes, stream, stream_count, 3, 2, scratch)) {
            status = STANDARD_METER_ANALYSIS_ERR_ALLOC;
        }
    }
    free(stream);
    free(scratch);
    if (status != STANDARD_METER_ANALYSIS_OK) {
        free_items(items, count);
        return status;
    }
    if (count > 1) {
        qsort(items, count, sizeof(StandardMeterTuplet), compare_tuplets);
    }
    out->items = items;
    out->count = count;
    return STANDARD_METER_ANALYSIS_OK;
}

void standard_meter_tuplets_destroy(StandardMeterTuplets *tuplets) {
    if (tuplets == NULL) {
        return;
    }
    free_items(tuplets->items, tuplets->count);
    tuplets->items = NULL;
    tuplets->count = 0;
}

bool standard_meter_tuplet_emit(const AnalysisNoteSet *notes, const StandardMeterTuplet *tuplet, const char *key,
                                Structure **shell, Interpretation **interpretation) {
    if (notes == NULL || tuplet == NULL || key == NULL || shell == NULL || interpretation == NULL || tuplet->note_count == 0) {
        return false;
    }
    const char **ids = (const char **)malloc(tuplet->note_count * sizeof(char *));
    if (ids == NULL) {
        return false;
    }
    for (size_t i = 0; i < tuplet->note_count; i++) {
        if (tuplet->notes[i] >= notes->count) {
            free(ids);
            return false;
        }
        ids[i] = notes->notes[tuplet->notes[i]].note_id;
    }
    StandardMeterTupletClaim claim;
    memset(&claim, 0, sizeof(claim));
    claim.note_count = tuplet->note_count;
    claim.actual = tuplet->actual;
    claim.normal = tuplet->normal;
    claim.attacks = tuplet->attacks;
    claim.measure = tuplet->measure;
    claim.beat = tuplet->beat;
    claim.meter = tuplet->meter;
    claim.borrowed = tuplet->borrowed;
    claim.has_level = tuplet->has_level;
    claim.level = tuplet->level;
    claim.level_typical = tuplet->level_typical;
    bool ok = standard_meter_analysis_emit("tuplet", key, ids, tuplet->note_count, STANDARD_METER_TUPLET_CLAIM_TYPE, &claim,
                                           sizeof(claim), 0.0, shell, interpretation);
    free(ids);
    return ok;
}

bool standard_meter_tuplet_claim_read(const Interpretation *interpretation, StandardMeterTupletClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    if (claim == NULL || strcmp(value_type(claim), STANDARD_METER_TUPLET_CLAIM_TYPE) != 0) {
        return false;
    }
    size_t size = 0;
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
