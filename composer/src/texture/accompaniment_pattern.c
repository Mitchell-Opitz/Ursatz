#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <theory/knowledge/knowledge_snapshot.h>
#include <theory/vocabulary/texture_rules.h>

#include "texture/texture_internal.h"
#include "util/util.h"

/* The widest a voicing of three notes spans, in semitones, and the narrowest gap between two of its notes. */
#define VOICING_SPAN 16
#define VOICING_GAP 2
/* How many semitones worse than the best a voicing may be and still be drawn for the first beat. */
#define FIRST_SLACK 4
#define VOICING_MAX 4096

/* The K11 entries of today's four patterns (R-745, Schoenberg ch. IX), by pattern: the figure each writes is theirs. */
static const char *const FIGURE_IDS[] = {NULL,
                                         "schoenberg.pattern.block",
                                         "schoenberg.pattern.arpeggiated",
                                         "schoenberg.pattern.alberti",
                                         "schoenberg.pattern.oom-pah"};

/*
 * The figure a pattern writes, from the accompaniment entry: the pattern ENTRY's own, or the entry of one of the four named above in
 * the snapshot the stage holds. NULL for another pattern, for a snapshot without the entry and for an entry that is a rhythm cell.
 */
static const PatternAccompaniment *pattern_figure(const AccompContext *context, PlanAccompanimentPattern pattern) {
    const PatternEntry *entry = NULL;
    if (pattern == PLAN_ACCOMPANIMENT_ENTRY) {
        entry = context->entry;
    } else if (pattern >= PLAN_ACCOMPANIMENT_BLOCK && pattern <= PLAN_ACCOMPANIMENT_OOM_PAH && context->composer_chords &&
               context->composer_chords->knowledge) {
        entry = knowledge_snapshot_pattern(context->composer_chords->knowledge, FIGURE_IDS[pattern]);
    }
    return entry && !entry->is_rhythm ? &entry->accompaniment : NULL;
}

/* The steps the figure has for the context's meter: those of its `by_meter` row for "<beats>/<unit>", else its default steps. */
static const PatternStep *figure_steps(const AccompContext *context, const PatternAccompaniment *figure, size_t *count) {
    char meter[16];
    snprintf(meter, sizeof meter, "%d/%d", context->meter.beats_per_bar, context->meter.beat_unit);
    for (size_t m = 0; m < figure->meter_count; m++) {
        if (strcmp(figure->meters[m].meter, meter) == 0 && figure->meters[m].step_count > 0) {
            *count = figure->meters[m].step_count;
            return figure->meters[m].steps;
        }
    }
    *count = figure->step_count;
    return figure->steps;
}

const char *accomp_pattern_name(PlanAccompanimentPattern pattern) {
    static const char *const NAMES[] = {"any", "block", "arpeggiated", "alberti", "oom-pah", "motive", "entry"};
    return pattern >= PLAN_ACCOMPANIMENT_ANY && pattern <= PLAN_ACCOMPANIMENT_ENTRY ? NAMES[pattern] : "?";
}

/* Whether the pattern has a figure to write and may be written in the meter (a figure that is `compound` false is not written in 6/8). */
static bool figure_fits(const AccompContext *context, PlanAccompanimentPattern pattern) {
    const PatternAccompaniment *figure = pattern_figure(context, pattern);
    return figure && (figure->compound || !context->compound);
}

/* How many voices a figure writes: three where it has a `chord` step (every voice of the chord), else one. */
static size_t figure_voices(const PatternStep *steps, size_t count) {
    for (size_t s = 0; s < count; s++) {
        if (steps[s].kind == PATTERN_STEP_CHORD) {
            return 3;
        }
    }
    return 1;
}

bool accomp_pattern_allowed(const AccompContext *context, PlanAccompanimentPattern pattern) {
    if (pattern == PLAN_ACCOMPANIMENT_ENTRY) {
        return context->pattern == PLAN_ACCOMPANIMENT_ENTRY && figure_fits(context, pattern);
    }
    if (pattern >= PLAN_ACCOMPANIMENT_BLOCK && pattern <= PLAN_ACCOMPANIMENT_OOM_PAH && !figure_fits(context, pattern)) {
        return false;
    }
    if (pattern == PLAN_ACCOMPANIMENT_MOTIVE) {
        return context->pattern == PLAN_ACCOMPANIMENT_MOTIVE && context->motive_count > 0;
    }
    if (context->pattern != PLAN_ACCOMPANIMENT_ANY) {
        return pattern == context->pattern;
    }
    if (context->texture >= 0) {
        StandardTexture texture = (StandardTexture)context->texture;
        if (!standard_texture_is_homophonic(texture)) {
            return false;
        }
        if (texture == STANDARD_TEXTURE_HOMORHYTHM) {
            return pattern == PLAN_ACCOMPANIMENT_BLOCK;
        }
    }
    return pattern >= PLAN_ACCOMPANIMENT_BLOCK && pattern <= PLAN_ACCOMPANIMENT_OOM_PAH;
}

/* ------------------------------------------------------------- voicing */

typedef struct {
    int pitch[3]; /* low to high */
    size_t size;  /* 2 or 3 */
    long cost;
} Voicing;

/* What a voicing is penalised for in semitones, each: a chord tone of the three the chord is made of that is left out. */
#define MISSING_PENALTY 12

/* The distance in semitones between two voicings, voice by voice from the top. */
long accomp_voicing_distance(const int *a, size_t a_size, const int *b, size_t b_size) {
    long distance = 0;
    for (size_t k = 0; k < (a_size < b_size ? a_size : b_size); k++) {
        distance += labs((long)a[a_size - 1 - k] - b[b_size - 1 - k]);
    }
    return distance;
}

/* Adds the voicing p of `size` notes to out when every note is a chord tone and, of two or more, one is the third. */
static size_t consider(const int p[3], size_t size, const int pcs[3], const int *previous, size_t previous_size,
                       int center, Voicing *out, size_t count) {
    unsigned used = 0;
    for (size_t k = 0; k < size; k++) {
        bool tone = false;
        for (int c = 0; c < 3; c++) {
            if (p[k] % 12 == pcs[c]) {
                tone = true;
                used |= 1u << c;
            }
        }
        if (!tone) {
            return count;
        }
    }
    if ((size >= 2 && !(used & 2u)) || count >= VOICING_MAX) {
        return count;
    }
    Voicing *voicing = &out[count];
    memset(voicing->pitch, 0, sizeof(voicing->pitch));
    memcpy(voicing->pitch, p, size * sizeof(int));
    voicing->size = size;
    int missing = 3 - (int)((used & 1u) + ((used >> 1) & 1u) + ((used >> 2) & 1u));
    voicing->cost = (long)missing * MISSING_PENALTY;
    if (previous) {
        voicing->cost += accomp_voicing_distance(voicing->pitch, size, previous, previous_size);
    } else {
        long sum = 0;
        for (size_t k = 0; k < size; k++) {
            sum += p[k];
        }
        voicing->cost += labs(sum - (long)size * center) + (p[size - 1] - p[0]);
    }
    return count + 1;
}

/*
 * Every voicing of `size` chord tones in the window, low to high, with its cost. Of two or more, the third is always
 * in it (it gives the chord its quality); the root may be left out, since the bass sounds it, and a tone may be
 * doubled, but each tone left out of the root, third and fifth (the seventh in place of the fifth) costs
 * MISSING_PENALTY, so a voicing of all three is chosen unless the window leaves no room for one. Returns how many.
 */
static size_t voicings(const int pcs[3], size_t size, int low, int high, int gap, const int *previous,
                       size_t previous_size, int center, Voicing *out) {
    size_t count = 0;
    for (int p1 = low; p1 <= high; p1++) {
        if (size == 1) {
            int p[3] = {p1, 0, 0};
            count = consider(p, size, pcs, previous, previous_size, center, out, count);
            continue;
        }
        for (int p2 = p1 + gap; p2 <= high; p2++) {
            if (size == 2) {
                int p[3] = {p1, p2, 0};
                count = consider(p, size, pcs, previous, previous_size, center, out, count);
                continue;
            }
            for (int p3 = p2 + gap; p3 <= high && p3 - p1 <= VOICING_SPAN; p3++) {
                int p[3] = {p1, p2, p3};
                count = consider(p, size, pcs, previous, previous_size, center, out, count);
            }
        }
    }
    return count;
}

/*
 * The voicing of a chord in the window: three notes, or two or one when the window, which the bass and the melody
 * close in, has no room for more, or none (*out_size 0) when it has no chord tone at all: the accompaniment is silent
 * for the beat. The voicing is the one nearest the previous voicing (or, for the first, the middle of the window), a
 * tie settled by rng. slack lets a voicing that much worse than the best be drawn instead.
 */
static bool choose_voicing(const int pcs[3], int low, int high, const int *previous, size_t previous_size, long slack,
                           FillRng *rng, int out[3], size_t *out_size) {
    Voicing *all = malloc(VOICING_MAX * sizeof(*all));
    size_t count = 0;
    size_t size = 3;
    if (!all) {
        return false;
    }
    *out_size = 0;
    for (; count == 0 && size >= 1; size--) {
        for (int gap = VOICING_GAP; count == 0 && gap >= 1; gap--) {
            count = voicings(pcs, size, low, high, gap, previous, previous_size, (low + high) / 2, all);
            if (size == 1) {
                break;
            }
        }
    }
    if (count == 0) {
        free(all);
        return true;
    }
    long best = all[0].cost;
    for (size_t i = 1; i < count; i++) {
        best = all[i].cost < best ? all[i].cost : best;
    }
    size_t near = 0;
    for (size_t i = 0; i < count; i++) {
        near += all[i].cost <= best + slack;
    }
    size_t pick = (size_t)fill_rng_below(rng, near);
    for (size_t i = 0; i < count; i++) {
        if (all[i].cost <= best + slack && pick-- == 0) {
            memcpy(out, all[i].pitch, sizeof(all[i].pitch));
            *out_size = all[i].size;
            break;
        }
    }
    free(all);
    return true;
}

/*
 * The three pitch classes a chord is voiced in: its root, third and fifth, or, with a seventh, the seventh for the fifth. The
 * diminished tonic of a locrian key is voiced as its root, third and the root again (an octave up): its diminished fifth is not
 * sounded (R-725).
 */
static void voiced_pcs(const AccompContext *context, size_t chord, int out[3]) {
    if (context->chord_colored[chord]) {
        memcpy(out, context->chord_voice[chord], 3 * sizeof(int));
        return;
    }
    const int *pcs = context->chord_pcs[chord];
    out[0] = pcs[0];
    out[1] = pcs[1];
    out[2] = context->chord_pc_count[chord] > 3 ? pcs[3] : pcs[2];
    bool tonic = pcs[0] == spelled_key_tonic_pitch_class(&context->key);
    if (tonic && context->chord_pc_count[chord] == 3 && (pcs[2] - pcs[0] + 12) % 12 == 6) {
        out[2] = pcs[0];
    }
}

/*
 * The block of a K3 voicing (R-731) on a beat whose chord sounds it: the members on the chord's root, the lowest note the root's from C3
 * (MIDI 48) up, one note per member, low to high. False when the block would not lie above the bass and below the melody of the beat.
 */
static bool voice_block(const AccompContext *context, size_t chord, Rational start, Rational end, int out[ACCOMP_VOICES], size_t *size) {
    const VoicingEntry *voicing = context->voicing;
    /* The voicing has its own register: the constraint's low holds, its high does not (a shimmer spans 21 semitones, the default window 19),
       the melody still closes the block from above. */
    int low = context->low;
    int high = 127;
    for (size_t i = 0; i < context->bass_count; i++) {
        const AccompLine *line = &context->bass[i];
        if (rational_cmp(line->onset, end) < 0 && rational_cmp(line->end, start) > 0 && line->pitch + 1 > low) {
            low = line->pitch + 1;
        }
    }
    for (size_t i = 0; i < context->melody_count; i++) {
        const AccompLine *line = &context->melody[i];
        if (rational_cmp(line->onset, end) < 0 && rational_cmp(line->end, start) > 0 && line->pitch - 1 < high) {
            high = line->pitch - 1;
        }
    }
    int bottom = 48 + util_pitch_class(context->chord_pcs[chord][0]);
    if (voicing == NULL || voicing->member_count > ACCOMP_VOICES || bottom < low ||
        bottom + voicing->members[voicing->member_count - 1].semitones > high) {
        return false;
    }
    for (size_t m = 0; m < voicing->member_count; m++) {
        out[m] = bottom + voicing->members[m].semitones;
    }
    *size = voicing->member_count;
    return true;
}

/*
 * The planed block of a chord (R-763): the members of the first chord's voicing (`shape`, indexes into `pcs`, low to high) in the same order
 * from the bottom, in close position, its lowest note the pitch of the first member nearest `anchor_low` (a tie takes the lower). False
 * when the block leaves [low, high].
 */
static bool planed_block(const int pcs[3], const int *shape, size_t size, int anchor_low, int low, int high, int out[3]) {
    int bottom_pc = util_pitch_class(pcs[shape[0]]);
    int below = anchor_low - (((anchor_low - bottom_pc) % 12) + 12) % 12;
    out[0] = anchor_low - below <= below + 12 - anchor_low ? below : below + 12;
    for (size_t m = 1; m < size; m++) {
        int pc = util_pitch_class(pcs[shape[m]]);
        int pitch = out[m - 1] + 1;
        pitch += (((pc - pitch) % 12) + 12) % 12;
        out[m] = pitch;
    }
    return out[0] >= low && out[size - 1] <= high;
}

/* The members of a voicing as indexes into the voiced pitch classes of its chord, low to high. False when a tone is none of them. */
static bool voicing_members(const int pcs[3], const int *voicing, size_t size, int out[3]) {
    for (size_t i = 0; i < size; i++) {
        size_t m = 0;
        while (m < 3 && util_pitch_class(pcs[m]) != util_pitch_class(voicing[i])) {
            m++;
        }
        if (m == 3) {
            return false;
        }
        out[i] = (int)m;
    }
    return true;
}

/* The window the notes of the beat from start to end have to keep to: above the bass, below the melody. */
static void beat_window(const AccompContext *context, Rational start, Rational end, int *low, int *high) {
    *low = context->low;
    *high = context->high;
    for (size_t i = 0; i < context->bass_count; i++) {
        const AccompLine *line = &context->bass[i];
        if (rational_cmp(line->onset, end) < 0 && rational_cmp(line->end, start) > 0 && line->pitch + 1 > *low) {
            *low = line->pitch + 1;
        }
    }
    for (size_t i = 0; i < context->melody_count; i++) {
        const AccompLine *line = &context->melody[i];
        if (rational_cmp(line->onset, end) < 0 && rational_cmp(line->end, start) > 0 && line->pitch - 1 < *high) {
            *high = line->pitch - 1;
        }
    }
}

/*
 * The index of the unit of a figure that starts at `at`, which the figure's steps are read by (modulo their count). On a beat grid the
 * unit is a beat and counts from the bar. On an eighth grid it counts the eighths of the bar for a chord that starts on the meter's
 * cells, and from the chord's start for one that starts off them.
 */
static size_t unit_index(const AccompContext *context, const PatternAccompaniment *figure, Rational chord_start, Rational at) {
    Rational in_bar = position_remainder(position_add(at, context->phase), context->bar);
    if (figure->beat_grid) {
        return (size_t)position_floor_units(in_bar, context->beat);
    }
    bool on_cells = position_is_multiple(position_add(chord_start, context->phase), context->cell);
    return (size_t)(on_cells ? position_floor_units(in_bar, ACCOMP_EIGHTH) : position_floor_units(position_sub(at, chord_start), ACCOMP_EIGHTH));
}

/* The end of the unit that starts at `at` inside the beat that ends at `beat_end`: the beat's end on a beat grid, an eighth on the other. */
static Rational unit_end(const PatternAccompaniment *figure, Rational at, Rational beat_end) {
    Rational after = position_add(at, ACCOMP_EIGHTH);
    return figure->beat_grid || rational_cmp(after, beat_end) >= 0 ? beat_end : after;
}

/* Whether any unit of the beat [start, end) has a step that sounds a voicing (`chord` or `member`): a beat with none is not voiced. */
static bool beat_sounds_voicing(const AccompContext *context, const PatternAccompaniment *figure, const PatternStep *steps, size_t count,
                                Rational chord_start, Rational start, Rational end) {
    for (Rational at = start; rational_cmp(at, end) < 0; at = unit_end(figure, at, end)) {
        PatternStepKind kind = steps[unit_index(context, figure, chord_start, at) % count].kind;
        if (kind == PATTERN_STEP_CHORD || kind == PATTERN_STEP_MEMBER) {
            return true;
        }
    }
    return false;
}

/* The pitch of the bass note sounding at `at`, plus the octave a `bass_octave` step writes; false when no bass note sounds. */
static bool bass_octave_at(const AccompContext *context, Rational at, int *pitch) {
    for (size_t i = 0; i < context->bass_count; i++) {
        const AccompLine *line = &context->bass[i];
        if (rational_cmp(line->onset, at) <= 0 && rational_cmp(line->end, at) > 0) {
            *pitch = line->pitch + 12;
            return true;
        }
    }
    return false;
}

static bool add_note(AccompCandidate *candidate, size_t voice, Rational onset, Rational duration, int pitch) {
    if (candidate->note_count[voice] >= ACCOMP_MAX_NOTES) {
        return false;
    }
    AccompNote *note = &candidate->notes[voice][candidate->note_count[voice]++];
    note->onset = onset;
    note->duration = duration;
    note->pitch = pitch;
    return true;
}

/* ------------------------------------------------------------- motive */

/*
 * The chord tones a statement of the motive over [start, end) can use, low to high: the chord's tones inside the range,
 * above the bass and below the melody sounding in that time. Returns how many.
 */
static size_t statement_ladder(const AccompContext *context, size_t chord, Rational start, Rational end,
                               int ladder[128]) {
    int low = context->low;
    int high = context->high;
    size_t rungs = 0;
    for (size_t i = 0; i < context->bass_count; i++) {
        const AccompLine *line = &context->bass[i];
        if (rational_cmp(line->onset, end) < 0 && rational_cmp(line->end, start) > 0 && line->pitch + 1 > low) {
            low = line->pitch + 1;
        }
    }
    for (size_t i = 0; i < context->melody_count; i++) {
        const AccompLine *line = &context->melody[i];
        if (rational_cmp(line->onset, end) < 0 && rational_cmp(line->end, start) > 0 && line->pitch - 1 < high) {
            high = line->pitch - 1;
        }
    }
    for (int pitch = low; pitch <= high; pitch++) {
        if (context->chord_colored[chord]) {
            if ((context->chord_mask[chord] >> (pitch % 12)) & 1u) {
                ladder[rungs++] = pitch;
            }
            continue;
        }
        for (size_t k = 0; k < context->chord_pc_count[chord]; k++) {
            if (pitch % 12 == context->chord_pcs[chord][k]) {
                ladder[rungs++] = pitch;
                break;
            }
        }
    }
    return rungs;
}

/*
 * The notes of the motive pattern over [from, to) of a chord: the motive's rhythm from `from`, restated every motive
 * length and cut at `to`. Each statement starts on the chord tone nearest the motive's first pitch, among the tones in
 * the range above the bass and below the melody it sounds with (a tie takes the lower); each next note moves the way
 * the motive does, by the number of chord-tone steps whose distance is nearest the motive's interval (a tie takes the
 * fewer steps; an interval of 0 repeats the tone, and so does a move with no tone left in its direction). Under
 * `liquidate` only the first beat group of the motive sounds, and its last tone is held to `to`. A statement with no
 * chord tone between the bass and the melody is silent, as a beat of the other patterns is. False when the notes do
 * not fit.
 */
static bool motive_notes(const AccompContext *context, size_t chord, Rational from, Rational to, bool liquidate,
                         AccompCandidate *out) {
    int ladder[128];
    for (Rational start = from; rational_cmp(start, to) < 0; start = position_add(start, context->motive_length)) {
        Rational stop = position_add(start, context->motive_length);
        Rational end = rational_cmp(stop, to) < 0 ? stop : to;
        size_t rungs = statement_ladder(context, chord, start, end, ladder);
        size_t at = 0;
        bool sounded = false;
        if (rungs == 0) {
            if (liquidate) {
                break;
            }
            continue;
        }
        for (size_t n = 0; n < context->motive_count; n++) {
            const AccompMotiveNote *note = &context->motive[n];
            if (liquidate && rational_cmp(note->onset, context->beat) >= 0) {
                break;
            }
            Rational onset = position_add(start, note->onset);
            if (rational_cmp(onset, to) >= 0) {
                break;
            }
            if (n == 0) {
                for (size_t r = 1; r < rungs; r++) {
                    at = labs((long)ladder[r] - note->pitch) < labs((long)ladder[at] - note->pitch) ? r : at;
                }
            } else {
                int interval = note->pitch - context->motive[n - 1].pitch;
                size_t best = at;
                long best_gap = -1;
                for (size_t step = 1; interval != 0; step++) {
                    if (interval > 0 ? at + step >= rungs : step > at) {
                        break;
                    }
                    size_t r = interval > 0 ? at + step : at - step;
                    long gap = labs(labs((long)ladder[r] - ladder[at]) - labs(interval));
                    if (best_gap < 0 || gap < best_gap) {
                        best = r;
                        best_gap = gap;
                    }
                }
                at = best;
            }
            Rational note_stop = position_add(onset, note->duration);
            Rational note_end = rational_cmp(note_stop, to) < 0 ? note_stop : to;
            sounded = true;
            if (!add_note(out, 0, onset, position_sub(note_end, onset), ladder[at])) {
                return false;
            }
        }
        if (liquidate && sounded) {
            AccompNote *last = &out->notes[0][out->note_count[0] - 1];
            last->duration = position_sub(to, last->onset);
            break;
        }
    }
    return true;
}

/* The motive on every chord; the last bar of the section liquidated (only its first beat group, its last tone held). */
static bool build_motive(const AccompContext *context, AccompCandidate *out) {
    for (size_t chord = 0; chord < context->chord_count; chord++) {
        Rational start = context->chord_start[chord];
        Rational end = context->chord_start[chord + 1];
        Rational cut = rational_cmp(context->liquidate_from, start) < 0
                           ? start
                           : (rational_cmp(context->liquidate_from, end) > 0 ? end : context->liquidate_from);
        if ((rational_cmp(cut, start) > 0 && !motive_notes(context, chord, start, cut, false, out)) ||
            (rational_cmp(end, cut) > 0 && !motive_notes(context, chord, cut, end, true, out))) {
            return false;
        }
    }
    return out->note_count[0] > 0;
}

bool accomp_build(const AccompContext *context, PlanAccompanimentPattern pattern, FillRng *rng, AccompCandidate *out) {
    memset(out, 0, sizeof(*out));
    out->magic = ACCOMP_PAYLOAD_MAGIC;
    out->pattern = pattern;
    if (!accomp_pattern_allowed(context, pattern) || !position_is_positive(context->beat)) {
        return false;
    }
    /* The beats run on from each chord's start, so a chord of any length in beats is cut at its end. */
    for (size_t chord = 0; chord < context->chord_count; chord++) {
        for (Rational start = context->chord_start[chord]; rational_cmp(start, context->chord_start[chord + 1]) < 0;
             start = position_add(start, context->beat)) {
            Rational stop = position_add(start, context->beat);
            Rational end = rational_cmp(stop, context->chord_start[chord + 1]) < 0 ? stop : context->chord_start[chord + 1];
            if (out->beat_count >= ACCOMP_MAX_BEATS) {
                return false;
            }
            out->beat_start[out->beat_count] = start;
            out->beat_end[out->beat_count] = end;
            out->beat_count++;
        }
    }
    if (pattern == PLAN_ACCOMPANIMENT_MOTIVE) {
        out->voices = 1;
        return build_motive(context, out);
    }
    const PatternAccompaniment *figure = pattern_figure(context, pattern);
    size_t step_count = 0;
    const PatternStep *steps = figure ? figure_steps(context, figure, &step_count) : NULL;
    if (!figure || step_count == 0) {
        return false;
    }
    out->voices = figure_voices(steps, step_count);

    /* The chord on each beat that sounds one, in the position nearest the one before. A beat of rests draws nothing. */
    const int *previous = NULL;
    size_t previous_size = 0;
    /* A planed figure (R-763) keeps the shape of the first voiced chord: its chord, the member order of its voicing and its lowest note. */
    size_t anchor_chord = 0;
    size_t anchor_size = 0;
    int anchor_shape[3] = {0, 0, 0};
    int anchor_low = 0;
    for (size_t b = 0; b < out->beat_count; b++) {
        Rational start = out->beat_start[b];
        size_t chord = accomp_chord_at(context, start);
        int pcs[3];
        int low;
        int high;
        if (!beat_sounds_voicing(context, figure, steps, step_count, context->chord_start[chord], start, out->beat_end[b])) {
            continue;
        }
        voiced_pcs(context, chord, pcs);
        beat_window(context, start, out->beat_end[b], &low, &high);
        if (pattern == PLAN_ACCOMPANIMENT_BLOCK && context->chord_voiced[chord] &&
            voice_block(context, chord, start, out->beat_end[b], out->voicing[b], &out->voicing_size[b])) {
            out->voiced[b] = true;
            previous = out->voicing[b];
            previous_size = out->voicing_size[b];
            continue;
        }
        if (!choose_voicing(pcs, low, high, previous, previous_size, previous ? 0 : FIRST_SLACK, rng, out->voicing[b],
                            &out->voicing_size[b])) {
            return false;
        }
        if (figure->planing) {
            /* The draw above is made and discarded for a planed block, so the figure's other choices do not shift. */
            if (anchor_size == 0) {
                if (out->voicing_size[b] > 0 && voicing_members(pcs, out->voicing[b], out->voicing_size[b], anchor_shape)) {
                    anchor_chord = chord;
                    anchor_size = out->voicing_size[b];
                    anchor_low = out->voicing[b][0];
                }
            } else if (chord != anchor_chord) {
                int block[3];
                if (planed_block(pcs, anchor_shape, anchor_size, anchor_low, low, high, block)) {
                    memcpy(out->voicing[b], block, sizeof(block));
                    out->voicing_size[b] = anchor_size;
                }
            }
        }
        if (out->voicing_size[b] == 0) {
            continue;
        }
        out->voiced[b] = true;
        previous = out->voicing[b];
        previous_size = out->voicing_size[b];
    }

    /* The notes: each beat is cut into the figure's units, and a unit writes its step. */
    for (size_t b = 0; b < out->beat_count; b++) {
        Rational chord_start = context->chord_start[accomp_chord_at(context, out->beat_start[b])];
        size_t size = out->voicing_size[b];
        for (Rational at = out->beat_start[b]; rational_cmp(at, out->beat_end[b]) < 0; at = unit_end(figure, at, out->beat_end[b])) {
            size_t unit = unit_index(context, figure, chord_start, at);
            Rational length = position_sub(unit_end(figure, at, out->beat_end[b]), at);
            const PatternStep *step = &steps[unit % step_count];
            int bass = 0;
            switch (step->kind) {
            case PATTERN_STEP_REST:
                break;
            case PATTERN_STEP_CHORD:
                for (size_t voice = 0; out->voiced[b] && voice < size; voice++) {
                    /* Voice 1 is the top of the chord. */
                    if (!add_note(out, voice, at, length, out->voicing[b][size - 1 - voice])) {
                        return false;
                    }
                }
                break;
            case PATTERN_STEP_MEMBER: {
                /* A voicing of two notes is the low one and the high one over again, one of a single note. A member past the top is the top. */
                size_t index = size >= 3 ? ((size_t)step->member < size ? (size_t)step->member : size - 1) : (size == 2 ? unit % 2 : 0);
                if (out->voiced[b] && !add_note(out, 0, at, length, out->voicing[b][index])) {
                    return false;
                }
                break;
            }
            case PATTERN_STEP_BASS_OCTAVE:
                /* The bass note sounding then, an octave up: the judge refuses it only where it leaves the range or meets the melody. */
                if (bass_octave_at(context, at, &bass) && !add_note(out, 0, at, length, bass)) {
                    return false;
                }
                break;
            }
        }
    }
    return true;
}
