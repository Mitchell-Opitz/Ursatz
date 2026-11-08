#include "harmonic_spans.h"

#include <math.h>
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
            if (c != a) {
                g->scratch[n].held = false;
            }
            n++;
        }
    }
    return n;
}

/* Reads cells a..b as one segment over the first cell's bass, in no key: the best chord over all 24 keys. */
static HarmonicSpanStatus read_part(const Grouper *g, size_t a, size_t b, Reading *out) {
    size_t n = merge_members(g, a, b);
    const int *bass_override = g->bass[a] >= 0 ? &g->bass[a] : NULL;

    CommonPracticeWeights weights;
    HarmonicSpanStatus status =
        map_status(common_practice_weight_segment(g->scratch, n, &g->config, &weights, bass_override));
    if (status != HARMONIC_SPAN_OK) {
        return status;
    }
    Reading reading = {false, 0.0, -1, false, false, false};
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
        reading.root_pitch_class = root;
        reading.bass_in_chord = bass_pc >= 0 && (mask & (1u << bass_pc)) != 0;
        reading.bass_is_root = bass_pc == root;
        bool has_root = weights.pitch_class[root] > 0.0;
        bool has_third = false;
        if (interval_count >= 2) {
            has_third = weights.pitch_class[(root + intervals[1]) % 12] > 0.0;
        }
        reading.complete = sounding >= 3 || (has_root && has_third && reading.bass_in_chord);
    }
    *out = reading;
    return HARMONIC_SPAN_OK;
}

/* Tests (a) to (d), and for a boundary inside a beat that each part's bass is its chord's root. */
static HarmonicSpanStatus try_boundary(const Grouper *g, size_t a, size_t b, size_t k, const Reading *whole,
                                       bool inside_beat, bool *passes) {
    *passes = false;
    Reading left;
    Reading right;
    HarmonicSpanStatus status = read_part(g, a, k - 1, &left);
    if (status != HARMONIC_SPAN_OK) {
        return status;
    }
    status = read_part(g, k, b, &right);
    if (status != HARMONIC_SPAN_OK) {
        return status;
    }
    if (!left.matched || !right.matched || left.root_pitch_class == right.root_pitch_class) {
        return HARMONIC_SPAN_OK;
    }
    if (!left.complete || !right.complete) {
        return HARMONIC_SPAN_OK;
    }
    if (!left.bass_in_chord || !right.bass_in_chord) {
        return HARMONIC_SPAN_OK;
    }
    if (inside_beat && (!left.bass_is_root || !right.bass_is_root)) {
        return HARMONIC_SPAN_OK;
    }
    double dur_left = g->cells[k - 1].end - g->cells[a].start;
    double dur_right = g->cells[b].end - g->cells[k].start;
    double dur_span = g->cells[b].end - g->cells[a].start;
    double weighted = (left.score * dur_left + right.score * dur_right) / dur_span;
    *passes = weighted >= whole->score + HARMONIC_SPAN_SPLIT_MARGIN;
    return HARMONIC_SPAN_OK;
}

/* True when cell k starts at the exact half of the beat it lies in: the beat runs from the last cell before k that
   starts on a beat to the next one after it (or the end of the span). */
static bool starts_at_half_beat(const Grouper *g, size_t a, size_t b, size_t k) {
    size_t before = k;
    while (before > a && g->cells[before - 1].level == HARMONIC_SPAN_LEVEL_INNER) {
        before--;
    }
    if (before == a && g->cells[a].level == HARMONIC_SPAN_LEVEL_INNER) {
        return false; /* no beat start in the span */
    }
    size_t beat_first = before == a ? a : before - 1;
    size_t beat_next = k + 1;
    while (beat_next <= b && g->cells[beat_next].level == HARMONIC_SPAN_LEVEL_INNER) {
        beat_next++;
    }
    double beat_start = g->cells[beat_first].start;
    double beat_end = beat_next <= b ? g->cells[beat_next].start : g->cells[b].end;
    return fabs(g->cells[k].start - 0.5 * (beat_start + beat_end)) <= TIME_TOLERANCE;
}

static HarmonicSpanStatus add_span(Grouper *g, size_t a, size_t b) {
    size_t i = g->spans.count++;
    g->spans.first[i] = a;
    g->spans.last[i] = b;
    g->spans.bass[i] = g->bass[a];
    return HARMONIC_SPAN_OK;
}

/* Splits cells a..b where the rules say, appending the resulting spans in time order. */
static HarmonicSpanStatus group_range(Grouper *g, size_t a, size_t b) {
    if (a == b) {
        return add_span(g, a, b);
    }
    Reading whole;
    HarmonicSpanStatus status = read_part(g, a, b, &whole);
    if (status != HARMONIC_SPAN_OK) {
        return status;
    }

    static const HarmonicSpanLevel LEVELS[] = {HARMONIC_SPAN_LEVEL_HALF_BAR, HARMONIC_SPAN_LEVEL_BEAT,
                                               HARMONIC_SPAN_LEVEL_INNER};
    for (size_t l = 0; l < sizeof(LEVELS) / sizeof(LEVELS[0]); l++) {
        bool inside_beat = LEVELS[l] == HARMONIC_SPAN_LEVEL_INNER;
        for (size_t k = a + 1; k <= b; k++) {
            if (g->cells[k].level != LEVELS[l] || (inside_beat && !starts_at_half_beat(g, a, b, k))) {
                continue;
            }
            bool passes = false;
            status = try_boundary(g, a, b, k, &whole, inside_beat, &passes);
            if (status != HARMONIC_SPAN_OK) {
                return status;
            }
            if (passes) {
                status = group_range(g, a, k - 1);
                return status != HARMONIC_SPAN_OK ? status : group_range(g, k, b);
            }
        }
    }
    return add_span(g, a, b);
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
        status = group_range(&g, a, b);
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
