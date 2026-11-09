#include "harmonic_spans.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


#define BASS_PERSIST_SEMITONES 12
#define TIME_TOLERANCE 1e-9

/* The lowest pitch sounding at the cell's start, or -1 when nothing does. */
static int lowest_at_start(const HarmonicSpanCell *cell) {
    int lowest = -1;
    for (size_t i = 0; i < cell->segment->member_count; i++) {
        const CommonPracticeMember *member = &cell->segment->members[i];
        if (!member->held && member->onset > cell->start + TIME_TOLERANCE) {
            continue; /* attacked after the start */
        }
        if (lowest < 0 || member->pitch < lowest) {
            lowest = member->pitch;
        }
    }
    return lowest;
}

bool harmonic_span_basses(const HarmonicSpanCell *cells, size_t count, int *out_bass) {
    if (count == 0) {
        return true;
    }
    if (cells == NULL || out_bass == NULL) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (cells[i].segment == NULL || (cells[i].segment->members == NULL && cells[i].segment->member_count > 0)) {
            return false;
        }
    }

    int recent = -1;
    for (size_t i = 0; i < count; i++) {
        bool first_of_measure = i == 0 || cells[i].measure != cells[i - 1].measure;
        int lowest = lowest_at_start(&cells[i]);
        int bass;
        if (lowest < 0) {
            bass = recent;
        } else if (!first_of_measure && recent >= 0 && lowest - recent >= BASS_PERSIST_SEMITONES) {
            bass = recent;
        } else {
            bass = lowest;
        }
        out_bass[i] = bass;
        recent = bass;
    }
    return true;
}

/* --- grouping --- */

#define READING_ID "harmonic-span"

typedef struct {
    const HarmonicSpanCell *cells;
    size_t count;
    const int *bass;
    CommonPracticeWeightedConfig config; /* require_bass_chord_tone set */
    CommonPracticeMember *scratch;       /* room for every member of every cell */
    HarmonicSpans spans;                 /* capacity: count */
} Grouper;

/* What the tests read of a part. */
typedef struct {
    bool matched;
    double score;
    int root_pitch_class;
    bool complete;
    bool bass_in_chord;
    bool bass_is_root;
    bool valid;     /* matched, complete, and the bass a chord tone */
    int bass;       /* the bass the reading used (-1: none) */
    unsigned mask;  /* the best chord's pitch-class mask (0 when not matched) */
    bool bass_explained; /* every new bass of the part is a chord tone, a step between its neighbours or an altered chord tone
                            (true when not matched: there is no mask to read it against) */
} Reading;

static HarmonicSpanStatus map_status(CommonPracticeWeightedStatus status) {
    switch (status) {
    case COMMON_PRACTICE_WEIGHTED_OK:
        return HARMONIC_SPAN_OK;
    case COMMON_PRACTICE_WEIGHTED_ERR_NULL_ARGUMENT:
        return HARMONIC_SPAN_ERR_NULL_ARGUMENT;
    case COMMON_PRACTICE_WEIGHTED_ERR_ALLOC:
        return HARMONIC_SPAN_ERR_ALLOC;
    default:
        return HARMONIC_SPAN_ERR_BAD_INPUT;
    }
}

/* The chords grouping reads: major, minor and diminished triads, dominant sevenths and diminished sevenths. The
   other sevenths are left out: two chords a third apart (I and vi) would read as one seventh chord. */
static bool allowed_in_grouping(const CommonPracticeRankedChord *chord) {
    int intervals[4];
    size_t count = common_practice_ranked_chord_intervals(chord, intervals);
    if (count == 3) {
        return intervals[1] == 4 ? intervals[2] == 7 : (intervals[1] == 3 && (intervals[2] == 7 || intervals[2] == 6));
    }
    return count == 4 && chord->is_seventh &&
           (chord->seventh == COMMON_PRACTICE_SEVENTH_DOMINANT7 || chord->seventh == COMMON_PRACTICE_SEVENTH_DIMINISHED7);
}

/* The best chord of the weights over every key: for tonic 0 to 11 in major, then in minor, the first ranked candidate
   of an allowed quality; a later key replaces the best only when strictly higher. False when no key has one. */
static HarmonicSpanStatus best_chord_over_keys(const CommonPracticeWeights *weights,
                                               const CommonPracticeWeightedConfig *config,
                                               CommonPracticeRankedChord *best, int *best_tonic, bool *found) {
    *found = false;
    for (int m = 0; m < 2; m++) {
        CommonPracticeKeyMode mode = m == 0 ? COMMON_PRACTICE_KEY_MAJOR : COMMON_PRACTICE_KEY_MINOR;
        for (int tonic = 0; tonic < 12; tonic++) {
            CommonPracticeRankedChord ranked[COMMON_PRACTICE_CANDIDATE_COUNT];
            size_t ranked_count = 0;
            HarmonicSpanStatus status =
                map_status(common_practice_rank_candidates(weights, tonic, mode, config, ranked, &ranked_count));
            if (status != HARMONIC_SPAN_OK) {
                return status;
            }
            for (size_t i = 0; i < ranked_count; i++) {
                if (!allowed_in_grouping(&ranked[i])) {
                    continue;
                }
                if (!*found || ranked[i].score > best->score + TIME_TOLERANCE) {
                    *best = ranked[i];
                    *best_tonic = tonic;
                    *found = true;
                }
                break;
            }
        }
    }
    return HARMONIC_SPAN_OK;
}

static bool same_text(const char *a, const char *b) {
    if (a == NULL || b == NULL) {
        return a == b;
    }
    return strcmp(a, b) == 0;
}

static bool same_note(const CommonPracticeMember *a, const CommonPracticeMember *b) {
    return a->pitch == b->pitch && fabs(a->onset - b->onset) <= TIME_TOLERANCE && same_text(a->voice_id, b->voice_id) &&
           same_text(a->note_id, b->note_id);
}

/* --- resolved accents (R-382) --- */

static bool sounds_at_start(const CommonPracticeMember *member, double start) {
    return member->held || fabs(member->onset - start) <= TIME_TOLERANCE;
}

static int pitch_class_of(int pitch) {
    return ((pitch % 12) + 12) % 12;
}

/* The member that resolves member `m`: the first of m's voice attacked after m (by more than TIME_TOLERANCE), when it is attacked
   inside the window, one or two semitones from m, and a member on the bass pitch sounds at `start` and still sounds when it is
   attacked. SIZE_MAX when m has no such resolution. */
static size_t accent_resolution(const CommonPracticeMember *members, size_t count, size_t m, double start, double end,
                                int bass_pitch) {
    const CommonPracticeMember *accent = &members[m];
    if (accent->voice_id == NULL || !sounds_at_start(accent, start) || accent->pitch <= bass_pitch) {
        return SIZE_MAX;
    }
    size_t r = SIZE_MAX;
    for (size_t j = 0; j < count; j++) {
        if (j == m || !same_text(members[j].voice_id, accent->voice_id) || !(members[j].onset > accent->onset + TIME_TOLERANCE)) {
            continue;
        }
        if (r == SIZE_MAX || members[j].onset < members[r].onset - TIME_TOLERANCE) {
            r = j;
        }
    }
    if (r == SIZE_MAX || members[r].held || !(members[r].onset < end - TIME_TOLERANCE)) {
        return SIZE_MAX;
    }
    int step = members[r].pitch - accent->pitch;
    if (step < 0) {
        step = -step;
    }
    if (step != 1 && step != 2) {
        return SIZE_MAX;
    }
    for (size_t b = 0; b < count; b++) {
        if (b == m || members[b].pitch != bass_pitch || !sounds_at_start(&members[b], start)) {
            continue;
        }
        double release = (members[b].held ? start : members[b].onset) + members[b].duration;
        if (release >= members[r].onset - TIME_TOLERANCE) {
            return r;
        }
    }
    return SIZE_MAX;
}

HarmonicSpanStatus harmonic_span_mark_resolved_accents(CommonPracticeMember *members, size_t count, double start, double end,
                                                       int bass_pitch, const CommonPracticeWeightedConfig *config) {
    if (count > 0 && members == NULL) {
        return HARMONIC_SPAN_ERR_NULL_ARGUMENT;
    }
    if (!isfinite(start) || !isfinite(end) || !(end > start) || bass_pitch < -1) {
        return HARMONIC_SPAN_ERR_BAD_INPUT;
    }
    for (size_t i = 0; i < count; i++) {
        members[i].resolves_in_segment = false;
    }
    if (count == 0 || bass_pitch < 0) {
        return HARMONIC_SPAN_OK;
    }

    size_t *resolution = malloc(count * sizeof(*resolution));
    if (resolution == NULL) {
        return HARMONIC_SPAN_ERR_ALLOC;
    }
    size_t candidates = 0;
    for (size_t i = 0; i < count; i++) {
        resolution[i] = accent_resolution(members, count, i, start, end, bass_pitch);
        if (resolution[i] != SIZE_MAX) {
            members[i].resolves_in_segment = true;
            candidates++;
        }
    }
    HarmonicSpanStatus status = HARMONIC_SPAN_OK;
    if (candidates > 0) {
        /* The grouping's reading: every key, the allowed qualities, the chord holds the bass, with every candidate flagged. */
        CommonPracticeWeightedConfig reading_config = config != NULL ? *config : common_practice_weighted_config_default();
        reading_config.require_bass_chord_tone = true;
        reading_config.applied_chords = false;
        CommonPracticeWeights weights;
        status = map_status(common_practice_weight_segment(members, count, &reading_config, &weights, &bass_pitch));
        CommonPracticeRankedChord best;
        int best_tonic = 0;
        bool found = false;
        if (status == HARMONIC_SPAN_OK) {
            status = best_chord_over_keys(&weights, &reading_config, &best, &best_tonic, &found);
        }
        unsigned mask = found && status == HARMONIC_SPAN_OK ? common_practice_ranked_chord_mask(&best, best_tonic) : 0;
        for (size_t i = 0; i < count; i++) {
            if (resolution[i] == SIZE_MAX) {
                continue;
            }
            bool resolved_to_chord_tone = (mask & (1u << pitch_class_of(members[resolution[i]].pitch))) != 0;
            bool accent_is_chord_tone = (mask & (1u << pitch_class_of(members[i].pitch))) != 0;
            members[i].resolves_in_segment = status == HARMONIC_SPAN_OK && resolved_to_chord_tone && !accent_is_chord_tone;
        }
        if (status != HARMONIC_SPAN_OK) {
            for (size_t i = 0; i < count; i++) {
                members[i].resolves_in_segment = false;
            }
        }
    }
    free(resolution);
    return status;
}

/* The union of the members of cells a..b in g->scratch: a note sounding in several cells is one member whose duration
   adds up; a note first sounding after the part's first cell was attacked in the part. Returns the member count. */
static size_t merge_members(const Grouper *g, size_t a, size_t b) {
    size_t n = 0;
    for (size_t c = a; c <= b; c++) {
        const CommonPracticeSegment *segment = g->cells[c].segment;
        for (size_t i = 0; i < segment->member_count; i++) {
            const CommonPracticeMember *member = &segment->members[i];
            size_t j = 0;
            while (j < n && !same_note(&g->scratch[j], member)) {
                j++;
            }
            if (j < n) {
                g->scratch[j].duration += member->duration;
                continue;
            }
            g->scratch[n] = *member;
            g->scratch[n].resolves_in_segment = false;
            if (c != a) {
                g->scratch[n].held = false;
            }
            n++;
        }
    }
    return n;
}

/* The shorter way round between two pitch classes, 0 to 6. */
static int pitch_class_distance(int a, int b) {
    int d = abs(pitch_class_of(a) - pitch_class_of(b));
    return d > 6 ? 12 - d : d;
}

/* The bass after cell c: the bass of the first later cell (in any measure) whose bass differs from bass[c]; -1 at the piece's end. */
static int bass_after(const Grouper *g, size_t c) {
    size_t j = c + 1;
    while (j < g->count && g->bass[j] == g->bass[c]) {
        j++;
    }
    return j < g->count ? g->bass[j] : -1;
}

/* True when the bass of cell c is an altered chord tone of the mask: its pitch class is not in the mask, it lies one semitone from
   a tone of the mask, that tone sounds at the cell's start in a member above the bass note (held, or attacked there), and the bass
   after it is a semitone away (R-381): a chromatic bass against the chord tone it alters is no new chord. */
static bool altered_chord_tone(const Grouper *g, size_t c, unsigned mask) {
    int bass = g->bass[c];
    int after = bass_after(g, c);
    if (bass < 0 || after < 0 || (mask & (1u << pitch_class_of(bass))) != 0 || pitch_class_distance(bass, after) != 1) {
        return false;
    }
    const CommonPracticeSegment *segment = g->cells[c].segment;
    for (size_t i = 0; i < segment->member_count; i++) {
        const CommonPracticeMember *member = &segment->members[i];
        if (member->pitch != bass && (mask & (1u << pitch_class_of(member->pitch))) != 0 &&
            pitch_class_distance(member->pitch, bass) == 1 && sounds_at_start(member, g->cells[c].start)) {
            return true;
        }
    }
    return false;
}

/* True when cell c holds a new bass (R-383): it is at level BAR, HALF_BAR or BEAT, has a bass that differs from the cell before
   it, a member on that bass pitch is attacked in it (not held), and another member above it sounds at the cell's start. A note
   with nothing above it is a melody the bass rule (R-370) took for the bass while the left hand rests, not a new bass (R-387). */
static bool is_new_bass(const Grouper *g, size_t c) {
    if (g->cells[c].level == HARMONIC_SPAN_LEVEL_INNER || g->bass[c] < 0 || g->bass[c] == g->bass[c - 1]) {
        return false;
    }
    const CommonPracticeSegment *segment = g->cells[c].segment;
    bool attacked = false;
    bool above = false;
    for (size_t i = 0; i < segment->member_count; i++) {
        const CommonPracticeMember *member = &segment->members[i];
        if (member->pitch == g->bass[c] && !member->held && member->onset <= g->cells[c].start + TIME_TOLERANCE) {
            attacked = true;
        }
        if (member->pitch > g->bass[c] && sounds_at_start(member, g->cells[c].start)) {
            above = true;
        }
    }
    return attacked && above;
}

/* True when every new bass of cells a + 1..b is explained by the mask: its pitch class is in the mask, or it is a step from the
   bass before it and a step to the bass after it, or it is an altered chord tone of the mask (R-383). */
static bool bass_explained_by(const Grouper *g, size_t a, size_t b, unsigned mask) {
    for (size_t c = a + 1; c <= b; c++) {
        if (!is_new_bass(g, c) || (mask & (1u << pitch_class_of(g->bass[c]))) != 0) {
            continue;
        }
        int before = g->bass[c - 1];
        int after = bass_after(g, c);
        if (before >= 0 && after >= 0 && pitch_class_distance(before, g->bass[c]) >= 1 &&
            pitch_class_distance(before, g->bass[c]) <= 2 && pitch_class_distance(g->bass[c], after) >= 1 &&
            pitch_class_distance(g->bass[c], after) <= 2) {
            continue;
        }
        if (!altered_chord_tone(g, c, mask)) {
            return false;
        }
    }
    return true;
}

/* Reads cells a..b as one segment over the given bass (-1: the lowest sounding pitch), in no key: the best chord over
   all 24 keys. */
static HarmonicSpanStatus read_over(const Grouper *g, size_t a, size_t b, int bass_pitch, Reading *out) {
    size_t n = merge_members(g, a, b);
    const int *bass_override = bass_pitch >= 0 ? &bass_pitch : NULL;

    HarmonicSpanStatus status =
        harmonic_span_mark_resolved_accents(g->scratch, n, g->cells[a].start, g->cells[b].end, bass_pitch, &g->config);
    if (status != HARMONIC_SPAN_OK) {
        return status;
    }
    CommonPracticeWeights weights;
    status = map_status(common_practice_weight_segment(g->scratch, n, &g->config, &weights, bass_override));
    if (status != HARMONIC_SPAN_OK) {
        return status;
    }
    Reading reading = {false, 0.0, -1, false, false, false, false, bass_pitch, 0, true};
    CommonPracticeRankedChord best;
    int best_tonic = 0;
    bool found = false;
    if (n > 0) {
        status = best_chord_over_keys(&weights, &g->config, &best, &best_tonic, &found);
        if (status != HARMONIC_SPAN_OK) {
            return status;
        }
    }
    if (found) {
        reading.score = best.score;
        reading.matched = best.score >= g->config.min_coverage;
    }
    if (reading.matched) {
        unsigned mask = common_practice_ranked_chord_mask(&best, best_tonic);
        int intervals[4];
        size_t interval_count = common_practice_ranked_chord_intervals(&best, intervals);
        int root = best.root_pitch_class;
        int bass_pc = weights.bass_pitch_class;
        int sounding = 0;
        for (int pc = 0; pc < 12; pc++) {
            if ((mask & (1u << pc)) != 0 && weights.pitch_class[pc] > 0.0) {
                sounding++;
            }
        }
        reading.mask = mask;
        reading.bass_explained = bass_explained_by(g, a, b, mask);
        reading.root_pitch_class = root;
        reading.bass_in_chord = bass_pc >= 0 && (mask & (1u << bass_pc)) != 0;
        reading.bass_is_root = bass_pc == root;
        bool has_root = weights.pitch_class[root] > 0.0;
        bool has_third = false;
        if (interval_count >= 2) {
            has_third = weights.pitch_class[(root + intervals[1]) % 12] > 0.0;
        }
        bool has_fifth = false;
        if (interval_count >= 3) {
            has_fifth = weights.pitch_class[(root + intervals[2]) % 12] > 0.0;
        }
        reading.complete = sounding >= 3 || (has_root && has_third && reading.bass_in_chord) ||
                           (has_root && has_fifth && reading.bass_is_root);
    }
    reading.valid = reading.matched && reading.complete && reading.bass_in_chord && reading.bass_explained;
    *out = reading;
    return HARMONIC_SPAN_OK;
}

/* Reads cells a..b over their first cell's bass. When that reading is not valid and the part's first later cell with
   a different bass lies inside the part's first beat (only INNER cells up to it) one or two semitones away, the part is
   read over that bass instead and that reading is used when it is valid (R-373): an accented passing tone,
   appoggiatura or suspension in the bass resolves by step and does not make the part invalid. */
static HarmonicSpanStatus read_part(const Grouper *g, size_t a, size_t b, Reading *out) {
    HarmonicSpanStatus status = read_over(g, a, b, g->bass[a], out);
    if (status != HARMONIC_SPAN_OK || out->valid) {
        return status;
    }
    size_t j = a + 1;
    while (j <= b && g->bass[j] == g->bass[a]) {
        j++;
    }
    if (j > b || g->bass[a] < 0 || g->bass[j] < 0) {
        return HARMONIC_SPAN_OK;
    }
    int step = g->bass[j] - g->bass[a];
    if (step < 0) {
        step = -step;
    }
    if (step != 1 && step != 2) {
        return HARMONIC_SPAN_OK;
    }
    for (size_t c = a + 1; c <= j; c++) {
        if (g->cells[c].level != HARMONIC_SPAN_LEVEL_INNER) {
            return HARMONIC_SPAN_OK;
        }
    }
    Reading stepped;
    status = read_over(g, a, b, g->bass[j], &stepped);
    if (status == HARMONIC_SPAN_OK && stepped.valid) {
        *out = stepped;
    }
    return status;
}

static HarmonicSpanStatus add_span(Grouper *g, size_t a, size_t b, int bass) {
    size_t i = g->spans.count++;
    g->spans.first[i] = a;
    g->spans.last[i] = b;
    g->spans.bass[i] = bass;
    return HARMONIC_SPAN_OK;
}

/* Adds the span of cells a..b whose reading is `reading`, cutting it at its half when it lies inside one beat: its
   first cell is at level BEAT or stronger and every other cell is INNER, a cell starts at the exact half of its time
   range, both halves are matched, complete and have their bass as a chord tone and read different roots, and they beat
   the span's score by the margin (R-372 (2)). One cut at most. */
static HarmonicSpanStatus emit_span(Grouper *g, size_t a, size_t b, const Reading *reading) {
    size_t half = 0;
    if (g->cells[a].level != HARMONIC_SPAN_LEVEL_INNER) {
        bool inside_beat = b > a;
        for (size_t c = a + 1; c <= b && inside_beat; c++) {
            inside_beat = g->cells[c].level == HARMONIC_SPAN_LEVEL_INNER;
        }
        double middle = 0.5 * (g->cells[a].start + g->cells[b].end);
        for (size_t c = a + 1; c <= b && inside_beat && half == 0; c++) {
            if (fabs(g->cells[c].start - middle) <= TIME_TOLERANCE) {
                half = c;
            }
        }
    }
    if (half != 0) {
        Reading left;
        Reading right;
        HarmonicSpanStatus status = read_part(g, a, half - 1, &left);
        if (status != HARMONIC_SPAN_OK) {
            return status;
        }
        status = read_part(g, half, b, &right);
        if (status != HARMONIC_SPAN_OK) {
            return status;
        }
        if (left.valid && right.valid && left.root_pitch_class != right.root_pitch_class) {
            double dur_left = g->cells[half - 1].end - g->cells[a].start;
            double dur_right = g->cells[b].end - g->cells[half].start;
            double dur_span = g->cells[b].end - g->cells[a].start;
            double weighted = (left.score * dur_left + right.score * dur_right) / dur_span;
            if (weighted >= reading->score + HARMONIC_SPAN_SPLIT_MARGIN) {
                add_span(g, a, half - 1, left.bass);
                return add_span(g, half, b, right.bass);
            }
        }
    }
    return add_span(g, a, b, reading->bass);
}

/* A measure's candidate boundaries: the cells after its first at level HALF_BAR or BEAT (at most
   MAX_BOUNDARIES; no supported meter has more). points[0] is the first cell, points[1..n] the boundaries and
   points[n + 1] one past the last cell: part (i, j) is the cells points[i] to points[j] - 1, read once. */
#define MAX_BOUNDARIES 12

typedef struct {
    size_t points[MAX_BOUNDARIES + 2];
    size_t n;
    Reading *memo;
    unsigned char *done;
} Measure;

static HarmonicSpanStatus part_reading(const Grouper *g, Measure *m, size_t i, size_t j, const Reading **out) {
    size_t slot = i * (m->n + 2) + j;
    if (!m->done[slot]) {
        HarmonicSpanStatus status = read_part(g, m->points[i], m->points[j] - 1, &m->memo[slot]);
        if (status != HARMONIC_SPAN_OK) {
            return status;
        }
        m->done[slot] = 1;
    }
    *out = &m->memo[slot];
    return HARMONIC_SPAN_OK;
}

/* The partition of one measure that boundary set `mask` (bit i: boundary i + 1 is a cut) makes. */
typedef struct {
    bool valid;
    size_t parts;
    double value;     /* the parts' scores weighted by duration */
    double penalized; /* value less HARMONIC_SPAN_PART_COST per part after the first */
} Partition;

static HarmonicSpanStatus score_partition(const Grouper *g, Measure *m, unsigned mask, Partition *out) {
    double measure_duration = g->cells[m->points[m->n + 1] - 1].end - g->cells[m->points[0]].start;
    Partition result = {true, 0, 0.0, 0.0};
    size_t start = 0;
    int previous_root = -1;
    for (size_t j = 1; j <= m->n + 1; j++) {
        if (j <= m->n && (mask & (1u << (j - 1))) == 0) {
            continue;
        }
        const Reading *reading;
        HarmonicSpanStatus status = part_reading(g, m, start, j, &reading);
        if (status != HARMONIC_SPAN_OK) {
            return status;
        }
        if (!reading->valid || reading->root_pitch_class == previous_root) {
            result.valid = false;
            *out = result;
            return HARMONIC_SPAN_OK;
        }
        double duration = g->cells[m->points[j] - 1].end - g->cells[m->points[start]].start;
        result.value += reading->score * duration / measure_duration;
        result.parts++;
        previous_root = reading->root_pitch_class;
        start = j;
    }
    result.penalized = result.value - HARMONIC_SPAN_PART_COST * (double)(result.parts - 1);
    *out = result;
    return HARMONIC_SPAN_OK;
}

/* True when partition (a_mask, a) beats the current best (b_mask, b): higher penalized value, within TIME_TOLERANCE
   the fewer parts, then the one whose first differing boundary is earlier (the lowest differing bit set). */
static bool partition_better(const Partition *a, unsigned a_mask, const Partition *b, unsigned b_mask) {
    if (a->penalized > b->penalized + TIME_TOLERANCE) {
        return true;
    }
    if (a->penalized < b->penalized - TIME_TOLERANCE) {
        return false;
    }
    if (a->parts != b->parts) {
        return a->parts < b->parts;
    }
    unsigned differing = a_mask ^ b_mask;
    return (a_mask & (differing & (0u - differing))) != 0;
}

/* True when cell k keeps the bass of the cell before it and sounds that bass note held over from before: the bass has not moved,
   so the measure is not cut here (R-379 (4)). */
static bool holds_bass_note(const Grouper *g, size_t k) {
    if (g->bass[k] < 0 || g->bass[k] != g->bass[k - 1]) {
        return false;
    }
    const CommonPracticeSegment *segment = g->cells[k].segment;
    for (size_t i = 0; i < segment->member_count; i++) {
        if (segment->members[i].held && segment->members[i].pitch == g->bass[k]) {
            return true;
        }
    }
    return false;
}

/* Cuts cells a..b (one measure) at the best valid partition, or leaves it whole; appends the spans in time order. */
static HarmonicSpanStatus group_measure(Grouper *g, size_t a, size_t b) {
    Reading whole_reading;
    HarmonicSpanStatus status = read_part(g, a, b, &whole_reading);
    if (status != HARMONIC_SPAN_OK) {
        return status;
    }
    Measure m;
    m.n = 0;
    m.points[0] = a;
    for (size_t k = a + 1; k <= b && m.n < MAX_BOUNDARIES; k++) {
        if ((g->cells[k].level == HARMONIC_SPAN_LEVEL_HALF_BAR || g->cells[k].level == HARMONIC_SPAN_LEVEL_BEAT) &&
            !holds_bass_note(g, k) && !(whole_reading.matched && altered_chord_tone(g, k, whole_reading.mask))) {
            m.points[++m.n] = k;
        }
    }
    m.points[m.n + 1] = b + 1;
    size_t stride = m.n + 2;
    m.memo = malloc(stride * stride * sizeof(*m.memo));
    m.done = calloc(stride * stride, sizeof(*m.done));
    if (m.memo == NULL || m.done == NULL) {
        free(m.memo);
        free(m.done);
        return HARMONIC_SPAN_ERR_ALLOC;
    }
    m.memo[m.n + 1] = whole_reading; /* part (0, n + 1) is the whole measure, already read */
    m.done[m.n + 1] = 1;

    const Reading *whole = NULL;
    status = part_reading(g, &m, 0, m.n + 1, &whole);
    unsigned best_mask = 0;
    Partition best = {false, 0, 0.0, 0.0};
    for (unsigned mask = 1; status == HARMONIC_SPAN_OK && mask < (1u << m.n); mask++) {
        Partition candidate;
        status = score_partition(g, &m, mask, &candidate);
        bool margin = whole->bass_explained; /* R-383: a whole with an unexplained new bass needs no margin to be cut */
        if (status != HARMONIC_SPAN_OK || !candidate.valid ||
            (margin && candidate.value < whole->score + HARMONIC_SPAN_SPLIT_MARGIN)) {
            continue;
        }
        if (!best.valid || partition_better(&candidate, mask, &best, best_mask)) {
            best = candidate;
            best_mask = mask;
        }
    }

    size_t start = 0;
    for (size_t j = 1; status == HARMONIC_SPAN_OK && j <= m.n + 1; j++) {
        if (j <= m.n && (best_mask & (1u << (j - 1))) == 0) {
            continue;
        }
        const Reading *reading;
        status = part_reading(g, &m, start, j, &reading);
        if (status == HARMONIC_SPAN_OK) {
            status = emit_span(g, m.points[start], m.points[j] - 1, reading);
        }
        start = j;
    }
    free(m.memo);
    free(m.done);
    return status;
}

static HarmonicSpanStatus validate(const HarmonicSpanCell *cells, size_t count, const int *bass) {
    for (size_t i = 0; i < count; i++) {
        const HarmonicSpanCell *cell = &cells[i];
        if (cell->segment == NULL || (cell->segment->members == NULL && cell->segment->member_count > 0) ||
            !isfinite(cell->start) || !isfinite(cell->end) || !(cell->end > cell->start) ||
            (unsigned)cell->level > (unsigned)HARMONIC_SPAN_LEVEL_INNER || bass[i] < -1) {
            return HARMONIC_SPAN_ERR_BAD_INPUT;
        }
    }
    return HARMONIC_SPAN_OK;
}

HarmonicSpanStatus harmonic_spans_group(const HarmonicSpanCell *cells, size_t count, const int *bass,
                                        const CommonPracticeWeightedConfig *config, HarmonicSpans *out) {
    if (out == NULL || (count > 0 && (cells == NULL || bass == NULL))) {
        return HARMONIC_SPAN_ERR_NULL_ARGUMENT;
    }
    HarmonicSpanStatus status = validate(cells, count, bass);
    if (status != HARMONIC_SPAN_OK) {
        return status;
    }
    if (count == 0) {
        HarmonicSpans empty = {0, NULL, NULL, NULL};
        *out = empty;
        return HARMONIC_SPAN_OK;
    }

    Grouper g;
    g.cells = cells;
    g.count = count;
    g.bass = bass;
    g.config = config != NULL ? *config : common_practice_weighted_config_default();
    g.config.require_bass_chord_tone = true;
    g.config.applied_chords = false; /* grouping is key-free: it already reads these shapes as diatonic in another key */
    size_t members = 1;
    for (size_t i = 0; i < count; i++) {
        members += cells[i].segment->member_count;
    }
    g.scratch = malloc(members * sizeof(*g.scratch));
    g.spans.count = 0;
    g.spans.first = malloc(count * sizeof(*g.spans.first));
    g.spans.last = malloc(count * sizeof(*g.spans.last));
    g.spans.bass = malloc(count * sizeof(*g.spans.bass));
    if (g.scratch == NULL || g.spans.first == NULL || g.spans.last == NULL || g.spans.bass == NULL) {
        status = HARMONIC_SPAN_ERR_ALLOC;
    }

    size_t a = 0;
    while (status == HARMONIC_SPAN_OK && a < count) {
        size_t b = a;
        while (b + 1 < count && cells[b + 1].measure == cells[a].measure) {
            b++;
        }
        status = group_measure(&g, a, b);
        a = b + 1;
    }
    free(g.scratch);
    if (status != HARMONIC_SPAN_OK) {
        harmonic_spans_destroy(&g.spans);
        return status;
    }
    *out = g.spans;
    return HARMONIC_SPAN_OK;
}

void harmonic_spans_destroy(HarmonicSpans *spans) {
    if (spans == NULL) {
        return;
    }
    free(spans->first);
    free(spans->last);
    free(spans->bass);
    spans->first = NULL;
    spans->last = NULL;
    spans->bass = NULL;
    spans->count = 0;
}
