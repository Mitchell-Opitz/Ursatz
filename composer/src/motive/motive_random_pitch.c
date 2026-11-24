#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/motive_analysis.h>
#include <ursatz_composer/motive_develop.h>

#include "motive/motive_random_internal.h"
#include "util/util.h"

/* The pitches of the SEGMENTS strategy: the frames, runs, decorations and accents drawn over a layout. */

/* ----------------------------------------------------------------- pitch */

typedef struct {
    long position[MAX_NOTES]; /* scale step, shift included */
    long rung[MAX_NOTES];     /* frame notes: the rung on the ladder of I's chord tones */
    bool frame[MAX_NOTES];
    int segment[MAX_NOTES];
    int alteration[MAX_NOTES];
    bool repeat_pair[MAX_NOTES]; /* the second note of the repeat feature's pair, and the matching note of a repeat of segment a */
} Place;

static int pitch_of(const SpelledKey *key, const Place *place, size_t i) {
    return motive_position_pitch(key, place->position[i], place->alteration[i]);
}

/* A frame note on the ladder of I, moved to the tones of the chord at its onset. In minor, the third of V is raised. */
static bool set_frame(const SpelledKey *key, const Layout *layout, Place *place, Melody *melody, size_t i, long rung) {
    int degree = layout->degree[i];
    long shift = motive_random_chord_shift(degree);
    if (rung < 0) {
        return false;
    }
    place->rung[i] = rung;
    place->position[i] = motive_random_rung_position(rung) + shift;
    place->alteration[i] = motive_random_chord_alteration(key, degree, place->position[i]);
    melody->pitches[i] = pitch_of(key, place, i);
    return motive_random_in_range(melody->pitches[i]);
}

/* The next rung of a walk on the chord tones: one rung most often, two now and then, and a move of two or more comes back. */
static long next_rung(const MotiveRandomPreferences *preferences, FillRng *rng, long rung, int *step_back) {
    int move;
    if (*step_back != 0) {
        move = *step_back;
        *step_back = 0;
    } else {
        int size = 1 + (int)motive_random_pick_weighted(rng, preferences->rung_moves, 2);
        move = fill_rng_below(rng, 2) == 0 ? size : -size;
        if (size >= BIG_LEAP_RUNGS) {
            *step_back = move > 0 ? -1 : 1;
        }
    }
    return rung + move;
}

/* Segment a's frame: a walk, with the feature (a leap or a repeated note) between two frame notes. */
static bool draw_frame_a(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, FillRng *rng, Feature feature, Place *place,
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
                place->repeat_pair[frames[f]] = true;
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
                    if (leap >= preferences->leap_low && leap <= preferences->leap_high) {
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
                rung = next_rung(preferences, rng, rung, &step_back);
            }
        }
        if (!set_frame(key, layout, place, melody, frames[f], rung)) {
            return false;
        }
    }
    return true;
}

/*
 * The frame of a repeat: a's rungs, with the contour of a, on the tones of the repeat's own chord. The whole repeat is moved
 * by a number of rungs (an octave is three) so that it starts near the note before it; on the same chord it is moved at
 * least one rung, so it is not a copy.
 */
static bool draw_frame_repeat(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, FillRng *rng, size_t segment,
                              Place *place, Melody *melody) {
    size_t first = layout->first[segment];
    size_t previous = first - 1;
    while (!place->frame[previous]) {
        previous--;
    }
    int before = melody->pitches[previous];
    bool same_chord = layout->degree[layout->first[segment]] == layout->degree[layout->cell_start];
    long offsets[7];
    size_t offset_count = 0;
    for (long offset = -3; offset <= 3; offset++) {
        if ((same_chord && offset == 0) || place->rung[layout->cell_start] + offset < 0) {
            continue;
        }
        if (set_frame(key, layout, place, melody, first, place->rung[layout->cell_start] + offset) &&
            abs(melody->pitches[first] - before) <= SEGMENT_START_DISTANCE && !(preferences->avoid_repeats && melody->pitches[first] == before)) {
            offsets[offset_count++] = offset;
        }
    }
    if (offset_count == 0) {
        return false;
    }
    long offset = offsets[fill_rng_below(rng, offset_count)];
    size_t from = layout->cell_start;
    for (size_t i = first; i <= layout->last[segment]; i++, from++) {
        if (place->frame[i] && !set_frame(key, layout, place, melody, i, place->rung[from] + offset)) {
            return false;
        }
        place->repeat_pair[i] = place->repeat_pair[from];
    }
    return true;
}

/* The frame of a new segment: it starts on a chord tone near the last frame note before it, then walks. */
static bool draw_frame_new(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, FillRng *rng, size_t segment, Place *place,
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
        if (preferences->avoid_repeats && distance == 0) {
            continue;
        }
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
            rung = next_rung(preferences, rng, rung, &step_back);
        }
        if (!set_frame(key, layout, place, melody, i, rung)) {
            return false;
        }
    }
    return true;
}

/* The positions near the note before where a non-chord tone of a kind (1 to 4 of the decorations list) makes its figure. */
static size_t find_figures(const SpelledKey *key, const Melody *melody, size_t i, long before, long shift, unsigned color_mask, size_t kind,
                           long *options) {
    size_t option_count = 0;
    for (long p = before - OFF_FRAME_REACH; p <= before + OFF_FRAME_REACH; p++) {
        int pitch = motive_position_pitch(key, p, 0);
        if (!motive_random_is_chord_step(p - shift) && !motive_random_is_color_step(color_mask, p - shift) && motive_random_in_range(pitch) &&
            motive_random_forms(MOTIVE_RANDOM_DECORATIONS[kind - 1], melody->pitches[i - 1], pitch, melody->pitches[i + 1])) {
            options[option_count++] = p;
        }
    }
    return option_count;
}

/* A run of notes between two frame notes, walked by step (draw_run). */
typedef struct {
    const SpelledKey *key;
    int degree;
    long shift;
    size_t length;       /* the notes of the run */
    long target;         /* the position of the frame note after the run */
    int target_pitch;
    int start_pitch;     /* the pitch of the frame note before the run */
    long path[MAX_NOTES];
    int alteration[MAX_NOTES];
    int pitch[MAX_NOTES];
    unsigned long seen;  /* complete paths counted so far */
    unsigned long pick;  /* the path to keep */
    long chosen[MAX_NOTES];
    int chosen_alteration[MAX_NOTES];
} RunWalk;

/* Counts (and, at index `pick`, keeps) every path of the run's notes from the position `from` at depth d. */
static void walk_run(RunWalk *walk, size_t d, long from, int from_pitch) {
    if (d == walk->length) {
        if (labs(from - walk->target) == 1 && from_pitch != walk->target_pitch) {
            if (walk->seen == walk->pick) {
                memcpy(walk->chosen, walk->path, walk->length * sizeof(long));
                memcpy(walk->chosen_alteration, walk->alteration, walk->length * sizeof(int));
            }
            walk->seen++;
        }
        return;
    }
    for (long move = d == 0 ? -2 : -1; move <= (d == 0 ? 2 : 1); move++) {
        if (move == 0) {
            continue;
        }
        long position = from + move;
        bool chord_tone = motive_random_is_chord_step(position - walk->shift);
        if (labs(move) == 2 && !chord_tone) {
            continue; /* the first move may be two steps, onto a chord tone */
        }
        int alteration = chord_tone ? motive_random_chord_alteration(walk->key, walk->degree, position) : 0;
        int pitch = motive_position_pitch(walk->key, position, alteration);
        if (!motive_random_in_range(pitch) || pitch == from_pitch) {
            continue;
        }
        walk->path[d] = position;
        walk->alteration[d] = alteration;
        walk_run(walk, d + 1, position, pitch);
    }
}

/*
 * The notes first to last, none of them a frame note, between the frame notes before and after: a path of scale steps from
 * the first to the second in which each move is one step, except that the first may be two steps onto a chord tone of the
 * segment's chord, and the last note is a step from the frame note after. One path is drawn by `fill_rng_below`; none fails
 * the draw.
 */
static bool draw_run(const SpelledKey *key, const Layout *layout, FillRng *rng, Place *place, Melody *melody, size_t first, size_t last) {
    RunWalk walk;
    memset(&walk, 0, sizeof(walk));
    walk.key = key;
    walk.degree = layout->degree[first];
    walk.shift = motive_random_chord_shift(walk.degree);
    walk.length = last - first + 1;
    walk.target = place->position[last + 1];
    walk.target_pitch = melody->pitches[last + 1];
    walk.start_pitch = melody->pitches[first - 1];
    walk.pick = ~0UL;
    walk_run(&walk, 0, place->position[first - 1], walk.start_pitch);
    if (walk.seen == 0) {
        return false;
    }
    walk.pick = (unsigned long)fill_rng_below(rng, walk.seen);
    walk.seen = 0;
    walk_run(&walk, 0, place->position[first - 1], walk.start_pitch);
    for (size_t k = 0; k < walk.length; k++) {
        place->position[first + k] = walk.chosen[k];
        place->alteration[first + k] = walk.chosen_alteration[k];
        melody->pitches[first + k] = pitch_of(key, place, first + k);
        melody->run[first + k] = true;
    }
    return true;
}

/*
 * The notes between frame notes: a type is drawn for each (or none), and a note takes a non-chord pitch near the note
 * before it that makes the figure with the notes either side, else a chord tone of its segment's chord near it. Two or more
 * such notes in a row are a run (draw_run), walked by step.
 */
static bool draw_decorations(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, FillRng *rng, Place *place, Melody *melody) {
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        if (place->frame[i]) {
            continue;
        }
        if (i == 0 || !place->frame[i - 1]) {
            return false;
        }
        size_t last = i;
        while (last + 1 < layout->rhythm.count && !place->frame[last + 1]) {
            last++;
        }
        if (last + 1 == layout->rhythm.count) {
            return false;
        }
        if (last > i) {
            if (!draw_run(key, layout, rng, place, melody, i, last)) {
                return false;
            }
            i = last;
            continue;
        }
        int degree = layout->degree[i];
        long shift = motive_random_chord_shift(degree);
        long before = place->position[i - 1];
        size_t kind = motive_random_pick_weighted(rng, preferences->decorations, DECORATION_KINDS + 1);
        long options[2 * OFF_FRAME_REACH + 1];
        size_t option_count = kind > 0 ? find_figures(key, melody, i, before, shift, layout->color_mask[i], kind, options) : 0;
        if (option_count == 0 && preferences->avoid_repeats && kind > 0) {
            /* the other kinds, in list order after the one drawn, before a chord tone */
            for (size_t step = 1; step < DECORATION_KINDS && option_count == 0; step++) {
                size_t other = 1 + (kind - 1 + step) % DECORATION_KINDS;
                if (preferences->decorations[other] > 0 && (option_count = find_figures(key, melody, i, before, shift, layout->color_mask[i], other, options)) > 0) {
                    kind = other;
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
                int alteration = motive_random_chord_alteration(key, degree, p);
                int pitch = motive_position_pitch(key, p, alteration);
                if ((motive_random_is_chord_step(p - shift) || motive_random_is_color_step(layout->color_mask[i], p - shift)) &&
                    motive_random_in_range(pitch) &&
                    !(preferences->avoid_repeats && (pitch == melody->pitches[i - 1] || pitch == melody->pitches[i + 1]))) {
                    options[option_count++] = p;
                }
            }
            if (option_count == 0) {
                return false;
            }
            place->position[i] = options[fill_rng_below(rng, option_count)];
            place->alteration[i] = motive_random_chord_alteration(key, degree, place->position[i]);
        }
        melody->pitches[i] = pitch_of(key, place, i);
    }
    return true;
}

/* Note i keeps a non-chord tone `type` at position p, and the note after it takes the chord tone (a position and alteration). */
static void put_accent(const SpelledKey *key, Place *place, Melody *melody, size_t i, long position, int alteration, NonChordTone type,
                       long target_position, int target_alteration) {
    place->position[i] = position;
    place->alteration[i] = alteration;
    melody->pitches[i] = pitch_of(key, place, i);
    melody->decorated[i] = true;
    melody->types[i] = type;
    place->position[i + 1] = target_position;
    place->alteration[i + 1] = target_alteration;
    place->rung[i + 1] = place->rung[i];
    place->frame[i + 1] = true;
    melody->pitches[i + 1] = pitch_of(key, place, i + 1);
}

/* An appoggiatura or passing tone on the beat: the chord tone moves one note later, and a step beside it comes first. */
static bool place_step_accent(const SpelledKey *key, const Layout *layout, FillRng *rng, size_t i, NonChordTone type, Place *place, Melody *melody) {
    int degree = layout->degree[i];
    long shift = motive_random_chord_shift(degree);
    long target = place->position[i];
    int target_pitch = melody->pitches[i];
    long options[2];
    size_t option_count = 0;
    for (long p = target - 1; p <= target + 1; p += 2) {
        int pitch = motive_position_pitch(key, p, 0);
        if (!motive_random_is_chord_step(p - shift) && !motive_random_is_color_step(layout->color_mask[i], p - shift) &&
            motive_random_in_range(pitch) && motive_random_forms(type, melody->pitches[i - 1], pitch, target_pitch)) {
            options[option_count++] = p;
        }
    }
    if (option_count == 0) {
        return false;
    }
    long chosen = options[fill_rng_below(rng, option_count)];
    put_accent(key, place, melody, i, chosen, 0, type, target, place->alteration[i]);
    return true;
}

/* A suspension where the chord changes: the note before is held over as a non-chord tone and falls to a chord tone. */
static bool place_suspension(const SpelledKey *key, const Layout *layout, size_t i, Place *place, Melody *melody) {
    if (i == 0 || layout->degree[i] == layout->degree[i - 1]) {
        return false;
    }
    int degree = layout->degree[i];
    long held = place->position[i - 1];
    long resolution = held - 1;
    int resolution_alteration = motive_random_chord_alteration(key, degree, resolution);
    int resolution_pitch = motive_position_pitch(key, resolution, resolution_alteration);
    if (motive_random_is_chord_step(held - motive_random_chord_shift(degree)) || motive_random_is_color_step(layout->color_mask[i], held - motive_random_chord_shift(degree)) || !motive_random_is_chord_step(resolution - motive_random_chord_shift(degree)) ||
        !motive_random_in_range(resolution_pitch) ||
        !motive_random_forms(NON_CHORD_TONE_SUSPENSION, melody->pitches[i - 1], melody->pitches[i - 1], resolution_pitch)) {
        return false;
    }
    put_accent(key, place, melody, i, held, place->alteration[i - 1], NON_CHORD_TONE_SUSPENSION, resolution, resolution_alteration);
    return true;
}

/*
 * Accented non-chord tones (R-425): a note that starts on a beat becomes an appoggiatura, a passing tone or a suspension
 * that resolves to the next note, which takes the chord tone the accented note displaced. One per segment, and never the
 * pickup, the first note, or the last note of a segment. Each eligible note is chosen in one of `accented_one_in`. A
 * segment that already holds a long non-chord tone (`accented`, place_long_accents) is skipped.
 */
static bool place_accents(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, FillRng *rng, Place *place,
                          Melody *melody, bool accented[SEGMENT_MAX]) {
    for (size_t i = 1; i + 1 < layout->rhythm.count; i++) {
        size_t s = (size_t)place->segment[i];
        if (!layout->rhythm.on_beat[i] || !place->frame[i] || i < layout->cell_start || !place->frame[i - 1] ||
            place->segment[i + 1] != place->segment[i] || accented[s]) {
            continue;
        }
        if (fill_rng_below(rng, preferences->accented_one_in) != 0) {
            continue;
        }
        size_t kind = motive_random_pick_weighted(rng, preferences->accented_types, 3);
        bool placed = kind == 2 ? place_suspension(key, layout, i, place, melody)
                                : place_step_accent(key, layout, rng, i, kind == 0 ? NON_CHORD_TONE_APPOGGIATURA : NON_CHORD_TONE_PASSING, place,
                                                    melody);
        accented[s] = accented[s] || placed;
    }
    return true;
}

/* The long non-chord tone types, in the order of the long_nct_types list. */
#define LONG_NCT_TYPES 5
static const NonChordTone LONG_NCT[LONG_NCT_TYPES] = {NON_CHORD_TONE_APPOGGIATURA, NON_CHORD_TONE_SUSPENSION, NON_CHORD_TONE_RETARDATION,
                                                      NON_CHORD_TONE_PASSING, NON_CHORD_TONE_NEIGHBOR};
#define LONG_NCT_CANDIDATES 64

/* One way to make note i a long non-chord tone: its position and alteration, and the chord tone the next note takes. */
typedef struct {
    NonChordTone type;
    long position;
    int alteration;
    long target;
    int target_alteration;
    unsigned long weight;
} LongNct;

/* Adds the figure `type` at position n resolving to r at note i when the rules allow it, weighted by its type and interval. */
static void add_long_nct(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, const Melody *melody,
                         size_t i, size_t type, long n, int n_alteration, long r, LongNct *out, size_t *count) {
    int degree = layout->degree[i];
    long shift = motive_random_chord_shift(degree);
    long step = motive_random_step_of(n - shift);
    if (motive_random_is_chord_step(n - shift) || motive_random_is_color_step(layout->color_mask[i], n - shift) ||
        !motive_random_is_chord_step(r - shift) || *count == LONG_NCT_CANDIDATES) {
        return;
    }
    size_t interval = step == 1 ? 0 : step == 3 ? 1 : step == 5 ? 2 : 3;
    int r_alteration = motive_random_chord_alteration(key, degree, r);
    int n_pitch = motive_position_pitch(key, n, n_alteration);
    int r_pitch = motive_position_pitch(key, r, r_alteration);
    if (!motive_random_in_range(n_pitch) || !motive_random_in_range(r_pitch) ||
        !motive_random_forms(LONG_NCT[type], melody->pitches[i - 1], n_pitch, r_pitch)) {
        return;
    }
    LongNct *c = &out[(*count)++];
    c->type = LONG_NCT[type];
    c->position = n;
    c->alteration = n_alteration;
    c->target = r;
    c->target_alteration = r_alteration;
    c->weight = preferences->long_nct_types[type] * preferences->long_nct_intervals[interval];
}

/*
 * Long non-chord tones (R-441): in one segment of `long_nct_one_in`, a frame note that starts on a beat and lasts a beat or
 * more becomes an appoggiatura, suspension, retardation, passing tone or neighbor that resolves to the next note. The note
 * is drawn by where it sits in the bar (long_nct_beats) and its length (long_nct_lengths), the figure by its type and its
 * interval above the chord root (long_nct_types, long_nct_intervals). A suspension or retardation holds the note before over
 * where the chord changes. A segment with none eligible, or whose weights are all 0, draws nothing.
 */
static bool place_long_accents(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, FillRng *rng, Place *place,
                               Melody *melody, bool accented[SEGMENT_MAX]) {
    long bar = layout->grid.beats * layout->grid.beat;
    long onset[MAX_NOTES];
    long position = 0;
    for (size_t i = layout->cell_start; i < layout->rhythm.count; i++) {
        onset[i] = position;
        position += layout->rhythm.ticks[i];
    }
    for (size_t s = 0; s < layout->segment_count; s++) {
        size_t eligible[MAX_NOTES];
        unsigned long weights[MAX_NOTES];
        size_t eligible_count = 0;
        for (size_t i = layout->first[s]; i <= layout->last[s]; i++) {
            long ticks = layout->rhythm.ticks[i];
            if (!layout->rhythm.on_beat[i] || !place->frame[i] || i < layout->cell_start || i < 1 || !place->frame[i - 1] ||
                i + 1 >= layout->rhythm.count || place->segment[i + 1] != (int)s || ticks < layout->grid.beat) {
                continue;
            }
            long at = onset[i] % bar;
            size_t where = at == 0 ? 0 : layout->grid.beats % 2 == 0 && at == bar / 2 ? 1 : 2;
            size_t length = ticks >= 2 * layout->grid.beat ? 2 : ticks >= 3 * layout->grid.beat / 2 ? 1 : 0;
            weights[eligible_count] = preferences->long_nct_beats[where] * preferences->long_nct_lengths[length];
            eligible[eligible_count++] = i;
        }
        if (eligible_count == 0 || fill_rng_below(rng, preferences->long_nct_one_in) != 0) {
            continue;
        }
        unsigned long total = 0;
        for (size_t k = 0; k < eligible_count; k++) {
            total += weights[k];
        }
        if (total == 0) {
            continue;
        }
        size_t i = eligible[motive_random_pick_weighted(rng, weights, eligible_count)];
        int degree = layout->degree[i];
        LongNct candidates[LONG_NCT_CANDIDATES];
        size_t count = 0;
        for (size_t type = 0; type < LONG_NCT_TYPES; type++) {
            if (type == 1 || type == 2) {
                /* held over from the note before, where the chord changes: down by step, or up (retardation) */
                if (degree != layout->degree[i - 1]) {
                    long held = place->position[i - 1];
                    add_long_nct(preferences, key, layout, melody, i, type, held, place->alteration[i - 1], held + (type == 1 ? -1 : 1), candidates,
                                 &count);
                }
                continue;
            }
            for (long n = place->position[i] - 2; n <= place->position[i] + 2; n++) {
                add_long_nct(preferences, key, layout, melody, i, type, n, 0, n - 1, candidates, &count);
                add_long_nct(preferences, key, layout, melody, i, type, n, 0, n + 1, candidates, &count);
            }
        }
        unsigned long candidate_weights[LONG_NCT_CANDIDATES];
        unsigned long candidate_total = 0;
        for (size_t k = 0; k < count; k++) {
            candidate_weights[k] = candidates[k].weight;
            candidate_total += candidates[k].weight;
        }
        if (candidate_total == 0) {
            continue;
        }
        const LongNct *chosen = &candidates[motive_random_pick_weighted(rng, candidate_weights, count)];
        put_accent(key, place, melody, i, chosen->position, chosen->alteration, chosen->type, chosen->target, chosen->target_alteration);
        accented[s] = true;
    }
    return true;
}

/*
 * The last note of a closed ending is the root or third of its chord, one or two semitones from the note before; of an
 * open ending, degree 2, 5 or 7 of the key.
 */
static bool ending_holds(const Layout *layout, const Place *place, const Melody *melody) {
    size_t last = layout->rhythm.count - 1;
    if (layout->grid.ending == ENDING_CLOSED) {
        long step = motive_random_step_of(place->position[last] - motive_random_chord_shift(layout->degree[last]));
        int move = abs(melody->pitches[last] - melody->pitches[last - 1]);
        return (step == 0 || step == 2) && (move == 1 || move == 2);
    }
    if (layout->grid.ending == ENDING_OPEN) {
        long step = motive_random_step_of(place->position[last]);
        return step == 1 || step == 4 || step == 6;
    }
    return true;
}

/* True when note i starts off the beat and is held across the next one (the beat counted from the first note after the pickup). */
static bool crosses_beat(const Layout *layout, const long *onset, size_t i) {
    long beat = layout->grid.beat;
    long from = onset[i] - layout->rhythm.pickup;
    long offset = ((from % beat) + beat) % beat;
    return offset != 0 && offset + layout->rhythm.ticks[i] > beat;
}

/* The frames of every segment, the notes between, and a range of no more than a twelfth. */
bool motive_random_draw_melody(const MotiveRandomPreferences *preferences, const SpelledKey *key, const Layout *layout, FillRng *rng, Feature feature, Melody *melody) {
    Place place;
    memset(&place, 0, sizeof(place));
    memset(melody, 0, sizeof(*melody));
    long onset[MAX_NOTES];
    long at = 0;
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        onset[i] = at;
        at += layout->rhythm.ticks[i];
    }
    for (size_t s = 0; s < layout->segment_count; s++) {
        for (size_t i = layout->first[s]; i <= layout->last[s]; i++) {
            place.segment[i] = (int)s;
            /* An off-beat note held across the next beat (a syncopation) is a chord tone. */
            place.frame[i] = layout->rhythm.on_beat[i] || i == layout->first[s] || layout->chord_first[i] ||
                             (preferences->syncopation_one_in > 0 && crosses_beat(layout, onset, i));
        }
    }
    /* A lead-in's last note is a frame note (a chord tone off the beat); when the last segment repeats a, so is a's. */
    if (layout->grid.ending == ENDING_LEAD_IN) {
        size_t last_segment = layout->segment_count - 1;
        place.frame[layout->last[last_segment]] = true;
        if (layout->repeats[last_segment]) {
            place.frame[layout->last[0]] = true;
        }
    }
    /* A pickup that leads into the accent is no longer than the note it leads into. */
    if (preferences->pickup_from_accent && layout->rhythm.pickup > 0 && layout->rhythm.ticks[layout->cell_start] < layout->rhythm.pickup) {
        return false;
    }
    if (!draw_frame_a(preferences, key, layout, rng, feature, &place, melody)) {
        return false;
    }
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (!(layout->repeats[s] ? draw_frame_repeat(preferences, key, layout, rng, s, &place, melody)
                                 : draw_frame_new(preferences, key, layout, rng, s, &place, melody))) {
            return false;
        }
    }
    /* ... and sits one chord tone below it. */
    if (preferences->pickup_from_accent && layout->rhythm.pickup > 0 &&
        !set_frame(key, layout, &place, melody, 0, place.rung[layout->cell_start] - 1)) {
        return false;
    }
    bool accented[SEGMENT_MAX] = {false, false, false};
    if (preferences->long_nct_one_in > 0 && !place_long_accents(preferences, key, layout, rng, &place, melody, accented)) {
        return false;
    }
    if (preferences->accented_one_in > 0 && !place_accents(preferences, key, layout, rng, &place, melody, accented)) {
        return false;
    }
    if (!draw_decorations(preferences, key, layout, rng, &place, melody)) {
        return false;
    }
    if (!ending_holds(layout, &place, melody)) {
        return false;
    }
    /* A note tied across the barline is a chord tone of the chord after it too. */
    for (size_t s = 1; s < layout->segment_count; s++) {
        if (layout->boundary[s] == SYNC_TIE_BAR &&
            !motive_random_is_chord_step(place.position[layout->last[s - 1]] - motive_random_chord_shift(layout->degree[layout->first[s]]))) {
            return false;
        }
    }
    /* A motive with no pickup does not open as one that has: a short note, then a longer one on the root, higher. */
    if (preferences->pickup_from_accent && layout->rhythm.pickup == 0 && layout->rhythm.ticks[0] <= layout->grid.beat &&
        layout->rhythm.ticks[1] > layout->rhythm.ticks[0] &&
        motive_random_step_of(place.position[1] - motive_random_chord_shift(layout->degree[1])) == 0 && melody->pitches[1] > melody->pitches[0]) {
        return false;
    }
    if (preferences->avoid_repeats) {
        for (size_t k = 0; k + 1 < layout->rhythm.count; k++) {
            bool suspension = melody->decorated[k + 1] && melody->types[k + 1] == NON_CHORD_TONE_SUSPENSION;
            if (melody->pitches[k] == melody->pitches[k + 1] && !suspension && !place.repeat_pair[k + 1]) {
                return false;
            }
        }
    }
    int low = 127;
    int high = 0;
    for (size_t i = 0; i < layout->rhythm.count; i++) {
        low = melody->pitches[i] < low ? melody->pitches[i] : low;
        high = melody->pitches[i] > high ? melody->pitches[i] : high;
    }
    /* Leaps (a fourth or more) and skips (a third): at most leaps_per_motif and skips_per_motif, and each leap followed by a step back when asked. */
    unsigned leaps = 0;
    unsigned skips = 0;
    for (size_t k = 0; k + 1 < layout->rhythm.count; k++) {
        int move = melody->pitches[k + 1] - melody->pitches[k];
        if (abs(move) < LEAP_SEMITONES) {
            skips += abs(move) >= SKIP_SEMITONES;
            continue;
        }
        leaps++;
        if (preferences->leap_recovery && k + 2 < layout->rhythm.count) {
            int back = melody->pitches[k + 2] - melody->pitches[k + 1];
            if (abs(back) < 1 || abs(back) > 2 || (back > 0) == (move > 0)) {
                return false;
            }
        }
    }
    if (leaps > preferences->leaps_per_motif || skips > preferences->skips_per_motif) {
        return false;
    }
    return high - low <= MAX_RANGE_SEMITONES;
}

