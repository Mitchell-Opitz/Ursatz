#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>

#include "motive/motive_random_internal.h"

/*
 * Random motives from segments (docs/cli-random-motive.md): a motive is made of one to three segments (Schoenberg's
 * motives), the way a theme starts. Segment a is a rhythm cell with one distinctive feature; a' repeats a's rhythm and
 * contour on the next chord (the dominant form); b is a new cell. A harmony plan puts one chord on each segment, and the
 * last segment slows down to end on a chord tone of the last chord. The defaults below are the whole of what shapes a
 * segment motive; tune them here.
 */

#define SEGMENT_BAR_ONE_BAR_WEIGHT 1 /* a motive of one bar ... */
#define SEGMENT_BAR_TWO_BARS_WEIGHT 1 /* ... and of two bars, as often */
#define SEGMENT_PICKUP_ONE_IN 4       /* a pickup (half a beat or a beat), one note before segment a, in one motive of this many */
#define SEGMENT_MIN_CELL_TICKS 4      /* a cell must hold a feature: the shortest cell is two beats */
#define SEGMENT_MAX 3
#define SEGMENT_LEAP_LOW 5  /* a leap of a fourth (5 semitones) ... */
#define SEGMENT_LEAP_HIGH 9 /* ... to a sixth (9 semitones) */
#define SEGMENT_START_REACH 4 /* rungs either side of the note before, searched for where a new segment starts */
#define SEGMENT_START_DISTANCE 5 /* the most semitones from the note before to where a new segment starts */

/* Shapes: a a' (a cell and its repeat), a b (a cell and a new one), a a' b (two half-bar cells, then a bar). */
typedef enum { SHAPE_REPEAT = 0, SHAPE_CONTRAST, SHAPE_REPEAT_CONTRAST, SHAPE_KINDS } Shape;
static const unsigned long SHAPE_WEIGHTS_ONE_BAR[SHAPE_KINDS] = {3, 2, 0};
static const unsigned long SHAPE_WEIGHTS_TWO_BARS[SHAPE_KINDS] = {3, 2, 2};

/* Harmony plans, one chord (a degree) per segment, and how often each is drawn (ch. V). */
typedef struct {
    size_t count;
    int degrees[SEGMENT_MAX];
    unsigned long weight;
} Plan;
static const Plan PLANS_TWO[] = {{2, {1, 5, 0}, 3}, {2, {1, 4, 0}, 1}, {2, {1, 1, 0}, 1}};
static const Plan PLANS_THREE[] = {{3, {1, 5, 1}, 2}, {3, {1, 4, 1}, 1}};
#define PLAN_KINDS_TWO (sizeof(PLANS_TWO) / sizeof(PLANS_TWO[0]))
#define PLAN_KINDS_THREE (sizeof(PLANS_THREE) / sizeof(PLANS_THREE[0]))

/* The distinctive feature of segment a, drawn with equal weight. */
typedef enum { FEATURE_DOTTED = 0, FEATURE_LEAP, FEATURE_REPEAT, FEATURE_KINDS } Feature;
#define DOTTED_LONG 3  /* a dotted beat, in half-beats ... */
#define DOTTED_SHORT 1 /* ... and what completes the beat after it */

/* ---------------------------------------------------------------- helpers */

static size_t pick_weighted(FillRng *rng, const unsigned long *weights, size_t count) {
    unsigned long sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += weights[i];
    }
    unsigned long pick = (unsigned long)fill_rng_below(rng, sum);
    size_t chosen = 0;
    while (pick >= weights[chosen]) {
        pick -= weights[chosen++];
    }
    return chosen;
}

/* The scale-step offset of a chord's tones from the tones of I: I 0, V down a fourth, IV up a fourth (same octave). */
static long chord_shift(int degree) {
    return degree == 5 ? -3 : degree == 4 ? 3 : 0;
}

static long step_of(long position) {
    return ((position % 7) + 7) % 7;
}

typedef struct {
    long ticks[MAX_NOTES];
    size_t count;
} Cell;

/* Tiles from to to (half-beats from the start of the cell) with the notes of the DURATIONS table. */
static bool tile(FillRng *rng, long from, long to, Cell *cell) {
    for (long position = from; position < to;) {
        long room = to - position;
        bool on_beat = position % 2 == 0;
        unsigned long weights[DURATION_KINDS];
        for (size_t i = 0; i < DURATION_KINDS; i++) {
            bool fits = MOTIVE_RANDOM_DURATIONS[i] <= room && (MOTIVE_RANDOM_DURATIONS[i] == 1 || on_beat);
            weights[i] = fits ? MOTIVE_RANDOM_DURATION_WEIGHTS[i] : 0;
        }
        size_t chosen = pick_weighted(rng, weights, DURATION_KINDS);
        if (cell->count == MAX_NOTES) {
            return false;
        }
        cell->ticks[cell->count++] = MOTIVE_RANDOM_DURATIONS[chosen];
        position += MOTIVE_RANDOM_DURATIONS[chosen];
    }
    return true;
}

/* Segment a's cell of length half-beats for a feature: a dotted figure on a beat, or a cell with two notes on beats. */
static bool draw_cell_a(FillRng *rng, long length, Feature feature, bool long_end, Cell *cell) {
    memset(cell, 0, sizeof(*cell));
    if (feature == FEATURE_DOTTED) {
        long slots = (length - (DOTTED_LONG + DOTTED_SHORT)) / 2 + 1;
        if (slots < 1) {
            return false;
        }
        long start = 2 * (long)fill_rng_below(rng, (uint64_t)slots);
        if (!tile(rng, 0, start, cell) || cell->count + 2 > MAX_NOTES) {
            return false;
        }
        cell->ticks[cell->count++] = DOTTED_LONG;
        cell->ticks[cell->count++] = DOTTED_SHORT;
        if (!tile(rng, start + DOTTED_LONG + DOTTED_SHORT, length, cell)) {
            return false;
        }
    } else {
        if (!tile(rng, 0, length, cell)) {
            return false;
        }
        size_t on_beats = 0;
        long position = 0;
        for (size_t i = 0; i < cell->count; i++) {
            on_beats += position % 2 == 0;
            position += cell->ticks[i];
        }
        if (on_beats < 2) {
            return false;
        }
    }
    return !long_end || cell->ticks[cell->count - 1] >= 2;
}

/* A new cell for the last segment: tiled, then its last notes joined until the last note is at least a beat long. */
static bool draw_cell_b(FillRng *rng, long length, const Cell *not_like, Cell *cell) {
    memset(cell, 0, sizeof(*cell));
    if (!tile(rng, 0, length, cell)) {
        return false;
    }
    while (cell->count >= 2 && cell->ticks[cell->count - 1] < 2) {
        cell->ticks[cell->count - 2] += cell->ticks[cell->count - 1];
        cell->count--;
    }
    if (cell->ticks[cell->count - 1] < 2) {
        return false;
    }
    return cell->count != not_like->count || memcmp(cell->ticks, not_like->ticks, cell->count * sizeof(long)) != 0;
}

/* ----------------------------------------------------------------- layout */

typedef struct {
    Rhythm rhythm;
    size_t segment_count;
    size_t first[SEGMENT_MAX];
    size_t last[SEGMENT_MAX];
    bool repeats[SEGMENT_MAX]; /* the segment repeats segment a's rhythm */
    Plan plan;
    size_t cell_start; /* the first note of segment a's cell: after the pickup note, if any */
} Layout;

static bool add_cell(Layout *layout, const Cell *cell, long *position) {
    for (size_t i = 0; i < cell->count; i++) {
        if (layout->rhythm.count == MAX_NOTES) {
            return false;
        }
        layout->rhythm.on_beat[layout->rhythm.count] = *position % 2 == 0;
        layout->rhythm.ticks[layout->rhythm.count++] = cell->ticks[i];
        *position += cell->ticks[i];
    }
    return true;
}

/* Bars, pickup, shape, plan and the rhythm of every segment. False when this draw does not fit. */
static bool draw_layout(const PlanMeter *meter, FillRng *rng, Layout *layout, Feature *feature_out) {
    memset(layout, 0, sizeof(*layout));
    long beats = meter->beats_per_bar;
    long bar = 2 * beats;
    bool halves = beats % 2 == 0 && beats >= SEGMENT_MIN_CELL_TICKS;
    long bars = 2;
    if (halves) {
        const unsigned long weights[] = {SEGMENT_BAR_ONE_BAR_WEIGHT, SEGMENT_BAR_TWO_BARS_WEIGHT};
        bars = 1 + (long)pick_weighted(rng, weights, 2);
    }
    layout->rhythm.pickup = fill_rng_below(rng, SEGMENT_PICKUP_ONE_IN) == 0 ? 1 + (long)fill_rng_below(rng, 2) : 0;
    Shape shape = (Shape)pick_weighted(rng, bars == 1 ? SHAPE_WEIGHTS_ONE_BAR : SHAPE_WEIGHTS_TWO_BARS,
                                       halves || bars == 1 ? SHAPE_KINDS : SHAPE_KINDS - 1);
    if (shape == SHAPE_REPEAT_CONTRAST && !halves) {
        return false;
    }
    long a_length = shape == SHAPE_REPEAT_CONTRAST ? beats : (bars == 1 ? beats : bar);
    long last_length = shape == SHAPE_REPEAT_CONTRAST ? bar : a_length;
    layout->segment_count = shape == SHAPE_REPEAT_CONTRAST ? 3 : 2;
    if (a_length < SEGMENT_MIN_CELL_TICKS) {
        return false;
    }
    if (layout->segment_count == 2) {
        layout->plan = PLANS_TWO[pick_weighted(rng, (unsigned long[]){PLANS_TWO[0].weight, PLANS_TWO[1].weight, PLANS_TWO[2].weight},
                                               PLAN_KINDS_TWO)];
    } else {
        layout->plan = PLANS_THREE[pick_weighted(rng, (unsigned long[]){PLANS_THREE[0].weight, PLANS_THREE[1].weight},
                                                 PLAN_KINDS_THREE)];
    }
    Feature feature = (Feature)fill_rng_below(rng, FEATURE_KINDS);
    *feature_out = feature;

    Cell a;
    if (!draw_cell_a(rng, a_length, feature, shape == SHAPE_REPEAT, &a)) {
        return false;
    }
    long position = 0;
    if (layout->rhythm.pickup > 0) {
        layout->rhythm.on_beat[0] = layout->rhythm.pickup % 2 == 0;
        layout->rhythm.ticks[0] = layout->rhythm.pickup;
        layout->rhythm.count = 1;
    }
    layout->cell_start = layout->rhythm.count;
    layout->first[0] = 0;
    if (!add_cell(layout, &a, &position)) {
        return false;
    }
    layout->last[0] = layout->rhythm.count - 1;
    if (shape == SHAPE_REPEAT || shape == SHAPE_REPEAT_CONTRAST) {
        layout->first[1] = layout->rhythm.count;
        layout->repeats[1] = true;
        if (!add_cell(layout, &a, &position)) {
            return false;
        }
        layout->last[1] = layout->rhythm.count - 1;
    }
    if (shape != SHAPE_REPEAT) {
        Cell b;
        size_t index = shape == SHAPE_CONTRAST ? 1 : 2;
        if (!draw_cell_b(rng, last_length, &a, &b)) {
            return false;
        }
        layout->first[index] = layout->rhythm.count;
        if (!add_cell(layout, &b, &position)) {
            return false;
        }
        layout->last[index] = layout->rhythm.count - 1;
    }
    layout->rhythm.total = layout->rhythm.pickup + position;
    return layout->rhythm.count >= MIN_NOTES;
}

/* ----------------------------------------------------------------- pitch */

typedef struct {
    long position[MAX_NOTES]; /* scale step, shift included */
    long rung[MAX_NOTES];     /* frame notes: the rung on the ladder of I's chord tones */
    bool frame[MAX_NOTES];
    int segment[MAX_NOTES];
    int alteration[MAX_NOTES];
} Place;

static int pitch_of(const AnalysisKey *key, const Place *place, size_t i) {
    return motive_position_pitch(key, place->position[i], place->alteration[i]);
}

/* A frame note on the ladder of I, moved to the tones of the chord of its segment. In minor, the third of V is raised. */
static bool set_frame(const AnalysisKey *key, const Layout *layout, Place *place, Melody *melody, size_t i, long rung) {
    int degree = layout->plan.degrees[place->segment[i]];
    long shift = chord_shift(degree);
    if (rung < 0) {
        return false;
    }
    place->rung[i] = rung;
    place->position[i] = motive_random_rung_position(rung) + shift;
    place->alteration[i] = key->mode == KEY_STRUCTURE_MODE_MINOR && degree == 5 &&
                                   step_of(place->position[i] - shift) == 2
                               ? 1
                               : 0;
    melody->pitches[i] = pitch_of(key, place, i);
    return motive_random_in_range(melody->pitches[i]);
}

/* The next rung of a walk on the chord tones: one rung most often, two now and then, and a move of two or more comes back. */
static long next_rung(FillRng *rng, long rung, int *step_back) {
    static const int moves[] = {1, 1, 1, 1, 1, 1, 2, 2};
    int move;
    if (*step_back != 0) {
        move = *step_back;
        *step_back = 0;
    } else {
        int size = moves[fill_rng_below(rng, sizeof(moves) / sizeof(moves[0]))];
        move = fill_rng_below(rng, 2) == 0 ? size : -size;
        if (size >= BIG_LEAP_RUNGS) {
            *step_back = move > 0 ? -1 : 1;
        }
    }
    return rung + move;
}

/* Segment a's frame: a walk, with the feature (a leap or a repeated note) between two frame notes. */
static bool draw_frame_a(const AnalysisKey *key, const Layout *layout, FillRng *rng, Feature feature, Place *place,
                         Melody *melody) {
    size_t frames[MAX_NOTES];
    size_t frame_count = 0;
    for (size_t i = layout->first[0]; i <= layout->last[0]; i++) {
        if (place->frame[i]) {
            frames[frame_count++] = i;
        }
    }
    size_t pair = frame_count > 1 ? (size_t)fill_rng_below(rng, frame_count - 1) : 0;
    long rung = BASE_RUNG + (long)fill_rng_below(rng, TRIAD_SIZE);
    int step_back = 0;
    for (size_t f = 0; f < frame_count; f++) {
        if (f > 0) {
            if (feature == FEATURE_REPEAT && f == pair + 1) {
                /* the same pitch */
            } else if (feature == FEATURE_LEAP && f == pair + 1) {
                long options[6];
                size_t option_count = 0;
                int from = melody->pitches[frames[f - 1]];
                for (long move = -3; move <= 3; move++) {
                    if (move == 0 || rung + move < 0) {
                        continue;
                    }
                    int to = motive_position_pitch(key, motive_random_rung_position(rung + move), 0);
                    int leap = abs(to - from);
                    if (leap >= SEGMENT_LEAP_LOW && leap <= SEGMENT_LEAP_HIGH) {
                        options[option_count++] = move;
                    }
                }
                if (option_count == 0) {
                    return false;
                }
                long move = options[fill_rng_below(rng, option_count)];
                rung += move;
                step_back = move > 0 ? -1 : 1;
            } else {
                rung = next_rung(rng, rung, &step_back);
            }
        }
        if (!set_frame(key, layout, place, melody, frames[f], rung)) {
            return false;
        }
    }
    return true;
}

/* The frame of a repeat: a's rungs on the tones of its own chord. A repeat on the same chord is moved one rung. */
static bool draw_frame_repeat(const AnalysisKey *key, const Layout *layout, FillRng *rng, size_t segment, Place *place,
                              Melody *melody) {
    long offset = layout->plan.degrees[segment] == layout->plan.degrees[0] ? (fill_rng_below(rng, 2) == 0 ? 1 : -1) : 0;
    size_t from = layout->cell_start;
    for (size_t i = layout->first[segment]; i <= layout->last[segment]; i++, from++) {
        if (place->frame[i] && !set_frame(key, layout, place, melody, i, place->rung[from] + offset)) {
            return false;
        }
    }
    return true;
}

/* The frame of a new segment: it starts on a chord tone near the last frame note before it, then walks. */
static bool draw_frame_new(const AnalysisKey *key, const Layout *layout, FillRng *rng, size_t segment, Place *place,
                           Melody *melody) {
    size_t first = layout->first[segment];
    size_t previous = first - 1;
    while (!place->frame[previous]) {
        previous--;
    }
    int before = melody->pitches[previous];
    long centre = place->rung[previous];
    long near_rung[2 * SEGMENT_START_REACH + 1];
    size_t near_count = 0;
    long best_rung = -1;
    int best_distance = 1000;
    for (long rung = centre - SEGMENT_START_REACH; rung <= centre + SEGMENT_START_REACH; rung++) {
        if (!set_frame(key, layout, place, melody, first, rung)) {
            continue;
        }
        int distance = abs(melody->pitches[first] - before);
        if (distance <= SEGMENT_START_DISTANCE) {
            near_rung[near_count++] = rung;
        }
        if (distance < best_distance) {
            best_distance = distance;
            best_rung = rung;
        }
    }
    if (best_rung < 0) {
        return false;
    }
    long rung = near_count > 0 ? near_rung[fill_rng_below(rng, near_count)] : best_rung;
    int step_back = 0;
    for (size_t i = first; i <= layout->last[segment]; i++) {
        if (!place->frame[i]) {
            continue;
        }
        if (i > first) {
            rung = next_rung(rng, rung, &step_back);
        }
        if (!set_frame(key, layout, place, melody, i, rung)) {
            return false;
        }
    }
    return true;
}

/*
 * The notes between frame notes: a type is drawn for each (or none), and a note takes a non-chord pitch near the note
 * before it that makes the figure with the notes either side, else a chord tone of its segment's chord near it.
 */
static bool draw_decorations(const AnalysisKey *key, const Layout *layout, FillRng *rng, Place *place, Melody *melody) {
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        if (place->frame[i]) {
            continue;
        }
        if (i == 0 || i + 1 == layout->rhythm.count || !place->frame[i - 1] || !place->frame[i + 1]) {
            return false;
        }
        int degree = layout->plan.degrees[place->segment[i]];
        long shift = chord_shift(degree);
        long before = place->position[i - 1];
        size_t kind = (size_t)fill_rng_below(rng, DECORATION_KINDS + 1);
        long options[2 * OFF_FRAME_REACH + 1];
        size_t option_count = 0;
        if (kind > 0) {
            for (long p = before - OFF_FRAME_REACH; p <= before + OFF_FRAME_REACH; p++) {
                int pitch = motive_position_pitch(key, p, 0);
                if (!motive_random_is_chord_step(p - shift) && motive_random_in_range(pitch) &&
                    motive_random_forms(MOTIVE_RANDOM_DECORATIONS[kind - 1], melody->pitches[i - 1], pitch,
                                        melody->pitches[i + 1])) {
                    options[option_count++] = p;
                }
            }
        }
        if (option_count > 0) {
            place->position[i] = options[fill_rng_below(rng, option_count)];
            place->alteration[i] = 0;
            melody->decorated[i] = true;
            melody->types[i] = MOTIVE_RANDOM_DECORATIONS[kind - 1];
        } else {
            for (long p = before - 4; p <= before + 4; p++) {
                int alteration = key->mode == KEY_STRUCTURE_MODE_MINOR && degree == 5 && step_of(p - shift) == 2 ? 1 : 0;
                if (motive_random_is_chord_step(p - shift) && motive_random_in_range(motive_position_pitch(key, p, alteration))) {
                    options[option_count++] = p;
                }
            }
            if (option_count == 0) {
                return false;
            }
            place->position[i] = options[fill_rng_below(rng, option_count)];
            place->alteration[i] = key->mode == KEY_STRUCTURE_MODE_MINOR && degree == 5 &&
                                           step_of(place->position[i] - shift) == 2
                                       ? 1
                                       : 0;
        }
        melody->pitches[i] = pitch_of(key, place, i);
    }
    return true;
}

/* The frames of every segment, the notes between, and a range of no more than a twelfth. */
static bool draw_melody(const AnalysisKey *key, const Layout *layout, FillRng *rng, Feature feature, Melody *melody) {
    Place place;
    memset(&place, 0, sizeof(place));
    memset(melody, 0, sizeof(*melody));
    for (size_t s = 0; s < layout->segment_count; s++) {
        for (size_t i = layout->first[s]; i <= layout->last[s]; i++) {
            place.segment[i] = (int)s;
            place.frame[i] = layout->rhythm.on_beat[i] || i == layout->first[s];
        }
    }
    if (!draw_frame_a(key, layout, rng, feature, &place, melody)) {
        return false;
    }
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (!(layout->repeats[s] ? draw_frame_repeat(key, layout, rng, s, &place, melody)
                                 : draw_frame_new(key, layout, rng, s, &place, melody))) {
            return false;
        }
    }
    if (!draw_decorations(key, layout, rng, &place, melody)) {
        return false;
    }
    int low = 127;
    int high = 0;
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        low = melody->pitches[i] < low ? melody->pitches[i] : low;
        high = melody->pitches[i] > high ? melody->pitches[i] : high;
    }
    return high - low <= MAX_RANGE_SEMITONES;
}

/* ------------------------------------------------------- the motive itself */

/* The plan as the motive's own harmony (a chord on each segment's first beat) and the segments as its features. */
static bool set_plan_and_segments(const MotiveRandomOptions *options, const Layout *layout, Motive *motive) {
    unsigned long den = 2UL * (unsigned long)options->meter.beat_unit;
    long onset[MAX_NOTES];
    long position = 0;
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        onset[i] = position;
        position += layout->rhythm.ticks[i];
    }
    static const char *const LETTERS = "abcdefgh";
    char names[SEGMENT_MAX][MOTIVE_FEATURE_LABEL_MAX + 1];
    size_t letter = 0;
    for (size_t s = 0; s < layout->segment_count; s++) {
        MotiveSketchChord *chord = &motive->chords[s];
        memset(chord, 0, sizeof(*chord));
        chord->degree = layout->plan.degrees[s];
        if (rational_make(s == 0 ? 0 : onset[layout->first[s]], den, &chord->onset) != RATIONAL_OK) {
            return false;
        }
        names[s][0] = layout->repeats[s] ? LETTERS[0] : LETTERS[letter++];
        names[s][1] = layout->repeats[s] ? '\'' : '\0';
        names[s][2] = '\0';
        MotiveSketchFeature *feature = &motive->features[s];
        memset(feature, 0, sizeof(*feature));
        strcpy(feature->label, names[s]);
        feature->first = layout->first[s];
        feature->last = layout->last[s];
    }
    motive->chord_count = layout->segment_count;
    motive->feature_count = layout->segment_count;
    return true;
}

/*
 * True when the tone roles read the motive as it was drawn: the base harmony is the plan, given, every frame note is a
 * chord tone, and every other note is a non-chord tone of its type that resolves.
 */
static bool tones_as_drawn(const Motive *motive, const Layout *layout, const Melody *melody) {
    MotiveAnalysis analysis;
    if (!motive_analyze(motive, &analysis) || !analysis.base_harmony_known || !analysis.tones_known ||
        !analysis.base_harmony.given || analysis.base_harmony.count != layout->segment_count) {
        return false;
    }
    for (size_t s = 0; s < layout->segment_count; s++) {
        if (analysis.base_harmony.chords[s].degree != layout->plan.degrees[s]) {
            return false;
        }
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        const MotiveToneLabel *label = &analysis.tones[i];
        bool expected = melody->decorated[i] ? label->role == MOTIVE_TONE_NON_CHORD && label->type == melody->types[i] &&
                                                   !label->unresolved
                                             : label->role == MOTIVE_TONE_CHORD;
        if (!expected) {
            return false;
        }
    }
    return true;
}

bool motive_random_segments_candidate(const MotiveRandomOptions *options, const AnalysisKey *key, FillRng *rng, Motive *out,
                                      bool *found) {
    Layout layout;
    Melody melody;
    Feature feature;
    const char *fault = NULL;
    *found = false;
    if (!draw_layout(&options->meter, rng, &layout, &feature) || !draw_melody(key, &layout, rng, feature, &melody) ||
        !motive_random_build(options, key, &layout.rhythm, &melody, out) || !set_plan_and_segments(options, &layout, out) ||
        !tones_as_drawn(out, &layout, &melody)) {
        return true;
    }
    if (!motive_melodic_fault(out, &fault)) {
        return false;
    }
    *found = fault == NULL;
    return true;
}
