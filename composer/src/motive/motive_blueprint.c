#include <ursatz_composer/motive_blueprint.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <analysis/motive/motive_sketch.h>
#include <analysis/similarity/similarity_scoring.h>
#include <theory/tonality/tonal_key.h>

#include <plan/plan_state.h>

#include "harmony/harmony_internal.h"
#include "motive/motive_internal.h"
#include "util/util.h"

/* The major scale on C, bit pc: the scale of a key with no material (the zero key reads as C major). */
#define MAJOR_ON_C 0xAB5u

int composer_event_pitch(const ComposerChords *chords, const SpelledKey *key, PlanEventType type, int previous, int next) {
    (void)chords;
    if (previous < 0 && next < 0) {
        return -1;
    }
    previous = previous < 0 ? next : previous;
    next = next < 0 ? previous : next;
    /* The key's natural scale, with the pitch classes of the placed neighbours. */
    int tonic = key ? spelled_key_tonic_pitch_class(key) : 0;
    unsigned scale = key && key->material ? util_scale_mask(tonic, key->material) : 0;
    if (scale == 0) {
        scale = ((MAJOR_ON_C << tonic) | (MAJOR_ON_C >> (12 - tonic))) & 0xFFFu;
    }
    scale |= (1u << util_pitch_class(previous)) | (1u << util_pitch_class(next));
    int distance = next - previous;
    switch (type) {
    case PLAN_EVENT_APPOGGIATURA:
        return standard_tonality_mask_step(scale, next, distance >= 1 && distance <= 2 ? -1 : 1);
    case PLAN_EVENT_SUSPENSION:
    case PLAN_EVENT_RETARDATION:
        return previous;
    case PLAN_EVENT_PASSING: {
        int low = previous < next ? previous : next;
        int high = previous < next ? next : previous;
        if (high - low == 3 || high - low == 4) {
            return standard_tonality_mask_step(scale, low, 1);
        }
        /* The scale tone nearest the middle, the lower on a tie. */
        int best = -1;
        for (int pitch = low; pitch <= high; pitch++) {
            if (((scale >> util_pitch_class(pitch)) & 1u) &&
                (best < 0 || util_absolute(2 * pitch - (low + high)) < util_absolute(2 * best - (low + high)))) {
                best = pitch;
            }
        }
        return best;
    }
    case PLAN_EVENT_NEIGHBOR:
        return standard_tonality_mask_step(scale, previous, next > previous ? -1 : 1);
    case PLAN_EVENT_ESCAPE:
        return standard_tonality_mask_step(scale, previous, next > previous ? -1 : 1);
    case PLAN_EVENT_ANTICIPATION:
        return next;
    default:
        return -1;
    }
}

/* ------------------------------------------------------------ the blueprint motive */

/* The non-chord tone of an event type. */
static NonChordTone event_tone(PlanEventType type) {
    switch (type) {
    case PLAN_EVENT_APPOGGIATURA:
        return NON_CHORD_TONE_APPOGGIATURA;
    case PLAN_EVENT_SUSPENSION:
        return NON_CHORD_TONE_SUSPENSION;
    case PLAN_EVENT_PASSING:
        return NON_CHORD_TONE_PASSING;
    case PLAN_EVENT_NEIGHBOR:
        return NON_CHORD_TONE_NEIGHBOR;
    case PLAN_EVENT_ESCAPE:
        return NON_CHORD_TONE_ESCAPE_TONE;
    case PLAN_EVENT_ANTICIPATION:
        return NON_CHORD_TONE_ANTICIPATION;
    default:
        return NON_CHORD_TONE_RETARDATION;
    }
}

/* What the draw keeps: each note's pitch, whether it has one, and which event it is (-1 for none). */
typedef struct {
    const BlueprintMotiveRequest *request;
    Motive *motive;
    int pitch[MOTIVE_MAX_NOTES];
    bool placed[MOTIVE_MAX_NOTES];
    bool anchored[MOTIVE_MAX_NOTES];
    int event[MOTIVE_MAX_NOTES];
    unsigned tones[MOTIVE_MAX_NOTES]; /* the pitch classes of the chord at each note's onset */
    size_t chord[MOTIVE_MAX_NOTES];
} Draw;

/* The note that starts at an offset, or -1. */
static int note_at(const BlueprintMotiveRequest *request, Rational offset) {
    for (size_t i = 0; i < request->note_count; i++) {
        if (rational_cmp(request->onsets[i], offset) == 0) {
            return (int)i;
        }
    }
    return -1;
}

static MotiveResult refuse(MotiveError *error, const char *location, const char *message) {
    motive_set_error(error, location, "%s", message);
    return MOTIVE_ERR_VALUE;
}

/* The last note before i that has a pitch, or -1. */
static int placed_before(const Draw *draw, size_t i) {
    for (size_t k = i; k > 0; k--) {
        if (draw->placed[k - 1]) {
            return (int)(k - 1);
        }
    }
    return -1;
}

/* The pitch the note after an anchored event resolves to, -1 for none. */
static int resolution(const Draw *draw, size_t i) {
    if (i == 0 || draw->event[i - 1] < 0 || !draw->anchored[i - 1]) {
        return -1;
    }
    const BlueprintMotiveRequest *request = draw->request;
    int from = draw->pitch[i - 1];
    unsigned scale = util_scale_mask(spelled_key_tonic_pitch_class(&request->key), request->key.material) |
                     (1u << util_pitch_class(from));
    int before = placed_before(draw, i - 1);
    switch ((PlanEventType)draw->event[i - 1]) {
    case PLAN_EVENT_APPOGGIATURA:
    case PLAN_EVENT_SUSPENSION:
        return standard_tonality_mask_step(scale, from, -1);
    case PLAN_EVENT_RETARDATION:
        return standard_tonality_mask_step(scale, from, 1);
    case PLAN_EVENT_ANTICIPATION:
        return from;
    case PLAN_EVENT_NEIGHBOR:
        return before >= 0 ? draw->pitch[before] : -1;
    case PLAN_EVENT_PASSING:
        return before >= 0 && draw->pitch[before] != from ? standard_tonality_mask_step(scale, from, from > draw->pitch[before] ? 1 : -1)
                                                          : -1;
    default:
        return -1;
    }
}

/*
 * Of the chord tones of note i in range other than `avoid`, at most `widest` from target (-1: any) and on its `direction` side
 * (0: either), the nearest to target; ties drawn from rng. -1 for none.
 */
static int nearest(const Draw *draw, size_t i, int target, int avoid, int direction, int widest, FillRng *rng) {
    const BlueprintMotiveRequest *request = draw->request;
    int best[2 * 128];
    size_t count = 0;
    int distance = -1;
    for (int pitch = request->range_low; pitch <= request->range_high; pitch++) {
        int away = util_absolute(pitch - target);
        if (pitch == avoid || !((draw->tones[i] >> util_pitch_class(pitch)) & 1u) || (widest >= 0 && away > widest) ||
            (direction != 0 && (pitch - target) * direction <= 0)) {
            continue;
        }
        if (distance < 0 || away < distance) {
            distance = away;
            count = 0;
        }
        if (away == distance) {
            best[count++] = pitch;
        }
    }
    if (count == 0) {
        return -1;
    }
    return best[count > 1 ? fill_rng_below(rng, count) : 0];
}

/* The pitch of a note that is not an anchor or an event: see composer_blueprint_motive. leap is the direction of a leap to recover from, 0 for none. */
static int choose(const Draw *draw, size_t i, int leap, bool leapt, FillRng *rng) {
    const BlueprintMotiveRequest *request = draw->request;
    int resolved = resolution(draw, i);
    if (resolved >= request->range_low && resolved <= request->range_high && ((draw->tones[i] >> util_pitch_class(resolved)) & 1u)) {
        return resolved;
    }
    int before = placed_before(draw, i);
    if (before < 0) {
        return nearest(draw, i, (request->range_low + request->range_high) / 2, -1, 0, -1, rng);
    }
    int from = draw->pitch[before];
    int pitch = -1;
    if (leap != 0) {
        /* A step the other way, else the nearest the other way. */
        pitch = nearest(draw, i, from, from, -leap, 2, rng);
        pitch = pitch < 0 ? nearest(draw, i, from, from, -leap, -1, rng) : pitch;
    }
    if (pitch < 0) {
        pitch = nearest(draw, i, from, from, 0, 2, rng);
    }
    if (pitch < 0) {
        pitch = nearest(draw, i, from, from, 0, -1, rng);
        /* A second leap only when no nearer chord tone is left. */
        if (pitch >= 0 && leapt && util_absolute(pitch - from) >= 5) {
            int closer = nearest(draw, i, from, from, 0, 4, rng);
            pitch = closer >= 0 ? closer : pitch;
        }
    }
    return pitch;
}

/* The time as text, "3/8". */
static void time_text(Rational time, char *out, size_t size) {
    if (time.den == 1) {
        snprintf(out, size, "%lld", (long long)time.num);
    } else {
        snprintf(out, size, "%lld/%lld", (long long)time.num, (long long)time.den);
    }
}

MotiveResult composer_blueprint_motive(const ComposerChords *chords, const BlueprintMotiveRequest *request, FillRng *rng,
                                       Motive *out, MotiveError *error) {
    char location[32];
    char message[MOTIVE_ERROR_MESSAGE_MAX];
    if (!request || !rng || !out) {
        return MOTIVE_ERR_NULL;
    }
    if (request->note_count == 0 || request->note_count > MOTIVE_MAX_NOTES) {
        return refuse(error, "", "a blueprint motive needs 1 to 64 notes");
    }
    if (request->chord_count == 0 || request->chord_count > MOTIVE_MAX_CHORDS) {
        return refuse(error, "", "a blueprint motive drawn from its rhythm needs its chords (1 to 8)");
    }
    if (request->anchor_count > BLUEPRINT_MOTIVE_MAX_ANCHORS || request->event_count > BLUEPRINT_MOTIVE_MAX_EVENTS ||
        request->range_low < 0 || request->range_high > 127 || request->range_low > request->range_high) {
        return refuse(error, "", "a blueprint motive takes up to 32 anchors, 16 events and a range inside 0 to 127");
    }
    Motive motive;
    motive_init(&motive, request->meter);
    motive_set_key(&motive, &request->key, MOTIVE_SKETCH_KEY_GIVEN);
    Draw *draw = calloc(1, sizeof(*draw));
    if (!draw) {
        return MOTIVE_ERR_ALLOC;
    }
    draw->request = request;
    draw->motive = &motive;
    MotiveResult result = MOTIVE_OK;
    for (size_t i = 0; i < request->note_count; i++) {
        Rational end;
        draw->event[i] = -1;
        motive.notes[i].onset = request->onsets[i];
        motive.notes[i].duration = request->durations[i];
        if (request->durations[i].num <= 0 || request->durations[i].den == 0 || request->onsets[i].num < 0 ||
            (i > 0 && (rational_add(request->onsets[i - 1], request->durations[i - 1], &end) != RATIONAL_OK ||
                       rational_cmp(request->onsets[i], end) < 0))) {
            snprintf(location, sizeof(location), "note %zu", i);
            result = refuse(error, location, "the notes of a blueprint motive follow one another without overlapping");
            break;
        }
        size_t at = 0;
        for (size_t c = 1; c < request->chord_count; c++) {
            if (rational_cmp(request->chords[c].onset, request->onsets[i]) <= 0) {
                at = c;
            }
        }
        int pcs[MOTIVE_SKETCH_CHORD_TONES_MAX];
        size_t members = motive_sketch_chord_tones(&motive, &request->chords[at], pcs);
        draw->chord[i] = at;
        for (size_t m = 0; m < members; m++) {
            draw->tones[i] |= 1u << util_pitch_class(pcs[m]);
        }
    }
    for (size_t a = 0; result == MOTIVE_OK && a < request->anchor_count; a++) {
        int i = note_at(request, request->anchors[a].offset);
        if (i < 0 || request->anchors[a].midi < 0 || request->anchors[a].midi > 127) {
            char at[32];
            time_text(request->anchors[a].offset, at, sizeof(at));
            snprintf(location, sizeof(location), "anchor %zu", a);
            snprintf(message, sizeof(message), "anchor at %s: no note starts there", at);
            result = refuse(error, location, message);
            break;
        }
        draw->pitch[i] = request->anchors[a].midi;
        draw->placed[i] = true;
        draw->anchored[i] = true;
    }
    for (size_t e = 0; result == MOTIVE_OK && e < request->event_count; e++) {
        int i = note_at(request, request->events[e].offset);
        if (i < 0) {
            char at[32];
            time_text(request->events[e].offset, at, sizeof(at));
            snprintf(location, sizeof(location), "event %zu", e);
            snprintf(message, sizeof(message), "event at %s: no note starts there", at);
            result = refuse(error, location, message);
            break;
        }
        draw->event[i] = (int)request->events[e].type;
    }
    /* The other notes in order, the events left for last. */
    int leap = 0;
    bool leapt = false;
    for (size_t i = 0; result == MOTIVE_OK && i < request->note_count; i++) {
        int before = placed_before(draw, i);
        if (!draw->placed[i] && draw->event[i] < 0) {
            int pitch = choose(draw, i, leap, leapt, rng);
            if (pitch < 0) {
                char numeral[MOTIVE_CHORD_NUMERAL_MAX];
                char at[32];
                if (!motive_chord_numeral(&request->chords[draw->chord[i]], numeral, sizeof(numeral))) {
                    snprintf(numeral, sizeof(numeral), "its chord");
                }
                time_text(request->onsets[i], at, sizeof(at));
                snprintf(location, sizeof(location), "note %zu", i);
                snprintf(message, sizeof(message), "note %zu at %s: no chord tone of %s in %d to %d", i, at, numeral,
                         request->range_low, request->range_high);
                result = refuse(error, location, message);
                break;
            }
            draw->pitch[i] = pitch;
            draw->placed[i] = true;
        }
        if (draw->placed[i] && before >= 0) {
            int move = draw->pitch[i] - draw->pitch[before];
            leap = util_absolute(move) >= 5 ? (move > 0 ? 1 : -1) : 0;
            leapt = leapt || leap != 0;
        }
    }
    /* The events, each from the notes around it. */
    for (size_t i = 0; result == MOTIVE_OK && i < request->note_count; i++) {
        if (draw->placed[i]) {
            continue;
        }
        int previous = i > 0 ? draw->pitch[i - 1] : -1;
        int next = -1;
        for (size_t k = i + 1; k < request->note_count && next < 0; k++) {
            next = draw->placed[k] ? draw->pitch[k] : -1;
        }
        int pitch = composer_event_pitch(chords, &request->key, (PlanEventType)draw->event[i], previous, next);
        if (pitch < request->range_low || pitch > request->range_high) {
            char at[32];
            time_text(request->onsets[i], at, sizeof(at));
            snprintf(location, sizeof(location), "note %zu", i);
            snprintf(message, sizeof(message), "note %zu at %s: the event falls outside %d to %d", i, at, request->range_low,
                     request->range_high);
            result = refuse(error, location, message);
            break;
        }
        draw->pitch[i] = pitch;
        draw->placed[i] = true;
    }
    if (result == MOTIVE_OK) {
        motive.note_count = request->note_count;
        for (size_t i = 0; i < request->note_count; i++) {
            motive.notes[i].pitch = draw->pitch[i];
            if (draw->event[i] >= 0) {
                motive.tone_overrides[motive.tone_override_count++] =
                    (MotiveToneOverride){i, MOTIVE_TONE_NON_CHORD, event_tone((PlanEventType)draw->event[i])};
            } else if ((draw->tones[i] >> util_pitch_class(draw->pitch[i])) & 1u) {
                motive.tone_overrides[motive.tone_override_count++] = (MotiveToneOverride){i, MOTIVE_TONE_CHORD, NON_CHORD_TONE_PASSING};
            }
        }
        size_t last = request->note_count - 1;
        if (rational_add(request->onsets[last], request->durations[last], &motive.length) != RATIONAL_OK) {
            result = refuse(error, "", "the notes of a blueprint motive are too long");
        }
        /* The chords that start inside the motive are its own harmony. */
        for (size_t c = 0; result == MOTIVE_OK && c < request->chord_count && rational_cmp(request->chords[c].onset, motive.length) < 0; c++) {
            motive.chords[motive.chord_count++] = request->chords[c];
        }
        if (result == MOTIVE_OK && !motive_sketch_validate(&motive)) {
            result = refuse(error, "", "the drawn motive is not a valid motive (check the chords' onsets and the meter)");
        }
    }
    free(draw);
    if (result == MOTIVE_OK) {
        *out = motive;
    }
    return result;
}

/* ------------------------------------------------------------ rhythms of a bar */

#define CELLS_MAX 128

size_t composer_rhythm_cells(const KnowledgeSnapshot *knowledge, const PlanMeter *meter, unsigned job, const PatternEntry **out, size_t capacity) {
    char want[16];
    size_t count = 0;
    snprintf(want, sizeof(want), "%d/%d", (int)meter->beats_per_bar, (int)meter->beat_unit);
    for (size_t i = 0; knowledge && i < knowledge_snapshot_pattern_count(knowledge) && count < capacity; i++) {
        const PatternEntry *entry = knowledge_snapshot_pattern_at(knowledge, i);
        if (entry && entry->is_rhythm && entry->rhythm.meter && strcmp(entry->rhythm.meter, want) == 0 &&
            (job == 0 || (entry->rhythm.jobs & job) != 0)) {
            out[count++] = entry;
        }
    }
    return count;
}

bool composer_rhythm_cell_notes(const PatternEntry *entry, const KnowledgeSnapshot *knowledge, const PlanMeter *meter, PlanNote *out,
                                size_t *out_count) {
    Rational bar_length;
    char closed[256];
    if (!entry || !entry->rhythm.rhythm || plan_meter_bar_length(meter, &bar_length) != PLAN_OK ||
        snprintf(closed, sizeof(closed), "%s x:w", entry->rhythm.rhythm) >= (int)sizeof(closed)) {
        return false;
    }
    MotiveOptions options;
    memset(&options, 0, sizeof(options));
    options.has_meter = true;
    options.meter = *meter;
    options.knowledge = knowledge;
    Motive motive;
    MotiveError detail;
    if (motive_from_rhythm_text(closed, &options, &motive, &detail) != MOTIVE_OK || motive.note_count < 2 ||
        rational_cmp(motive.notes[motive.note_count - 1].onset, bar_length) != 0) {
        return false;
    }
    *out_count = motive.note_count - 1;
    for (size_t i = 0; i < *out_count && i < COMPOSER_RHYTHM_NOTES_MAX; i++) {
        out[i] = plan_note(motive_pitch(&motive, i), motive.notes[i].onset, motive.notes[i].duration);
    }
    return *out_count > 0;
}

bool composer_rhythm_variant(const PlanNote *base, size_t count, FillRng *rng, PlanNote *out, size_t *out_count) {
    size_t splits = count < COMPOSER_RHYTHM_NOTES_MAX ? count : 0;
    size_t joins = 0;
    for (size_t i = 0; i + 1 < count; i++) {
        Rational end;
        joins += rational_add(base[i].offset, base[i].duration, &end) == RATIONAL_OK && rational_cmp(end, base[i + 1].offset) == 0;
    }
    if (splits + joins == 0) {
        return false;
    }
    size_t pick = (size_t)fill_rng_below(rng, splits + joins);
    *out_count = 0;
    if (pick < splits) {
        for (size_t i = 0; i < count; i++) {
            Rational half;
            if (i != pick) {
                out[(*out_count)++] = base[i];
                continue;
            }
            if (rational_div_int(base[i].duration, 2, &half) != RATIONAL_OK) {
                return false;
            }
            PlanNote first = base[i];
            PlanNote second = base[i];
            first.duration = half;
            second.duration = half;
            if (rational_add(first.offset, half, &second.offset) != RATIONAL_OK) {
                return false;
            }
            out[(*out_count)++] = first;
            out[(*out_count)++] = second;
        }
        return true;
    }
    size_t join = pick - splits;
    for (size_t i = 0; i < count; i++) {
        Rational end;
        bool joinable = i + 1 < count && rational_add(base[i].offset, base[i].duration, &end) == RATIONAL_OK &&
                        rational_cmp(end, base[i + 1].offset) == 0;
        if (joinable && join == 0) {
            PlanNote joined = base[i];
            if (rational_add(joined.duration, base[i + 1].duration, &joined.duration) != RATIONAL_OK) {
                return false;
            }
            out[(*out_count)++] = joined;
            i++;
            join = (size_t)-1;
            continue;
        }
        join -= joinable && join != (size_t)-1 ? 1 : 0;
        out[(*out_count)++] = base[i];
    }
    return true;
}

/* How similar two rhythms are (similarity_rhythm_score on their durations); 1 when they cannot be compared. */
static Rational rhythm_similarity(const PlanNote *a, size_t a_count, const PlanNote *b, size_t b_count) {
    SimilarityDuration left[COMPOSER_RHYTHM_NOTES_MAX];
    SimilarityDuration right[COMPOSER_RHYTHM_NOTES_MAX];
    SimilarityScore score = {1, 1};
    for (size_t i = 0; i < a_count; i++) {
        left[i] = a[i].duration;
    }
    for (size_t i = 0; i < b_count; i++) {
        right[i] = b[i].duration;
    }
    if (a_count == 0 || b_count == 0 || similarity_rhythm_score(left, a_count, right, b_count, &score) != SIMILARITY_SCORE_OK) {
        score = (Rational){1, 1};
    }
    return score;
}

bool composer_rhythm_contrast(const KnowledgeSnapshot *knowledge, const PlanMeter *meter, const PlanNote *base, size_t count, FillRng *rng,
                              PlanNote *out, size_t *out_count) {
    const PatternEntry *cells[CELLS_MAX];
    size_t far[CELLS_MAX];
    size_t lengths[CELLS_MAX];
    size_t cell_count = composer_rhythm_cells(knowledge, meter, 0, cells, CELLS_MAX);
    PlanNote *lines = malloc(CELLS_MAX * COMPOSER_RHYTHM_NOTES_MAX * sizeof(PlanNote));
    if (!lines) {
        return false;
    }
    size_t far_count = 0;
    size_t least = CELLS_MAX;
    Rational least_score = {2, 1};
    Rational half = {1, 2};
    for (size_t c = 0; c < cell_count; c++) {
        PlanNote *line = lines + c * COMPOSER_RHYTHM_NOTES_MAX;
        if (!composer_rhythm_cell_notes(cells[c], knowledge, meter, line, &lengths[c])) {
            continue;
        }
        Rational score = rhythm_similarity(base, count, line, lengths[c]);
        if (rational_cmp(score, half) <= 0) {
            far[far_count++] = c;
        }
        if (rational_cmp(score, least_score) < 0) {
            least_score = score;
            least = c;
        }
    }
    bool found = least != CELLS_MAX;
    if (found) {
        size_t pick = far_count > 0 ? far[fill_rng_below(rng, far_count)] : least;
        *out_count = lengths[pick];
        memcpy(out, lines + pick * COMPOSER_RHYTHM_NOTES_MAX, lengths[pick] * sizeof(PlanNote));
    }
    free(lines);
    return found;
}

size_t composer_draw_weighted(FillRng *rng, const unsigned *weights, size_t count) {
    unsigned total = 0;
    for (size_t i = 0; i < count; i++) {
        total += weights[i];
    }
    if (total == 0) {
        return count;
    }
    unsigned at = (unsigned)fill_rng_below(rng, total);
    for (size_t i = 0; i < count; i++) {
        if (at < weights[i]) {
            return i;
        }
        at -= weights[i];
    }
    return count - 1;
}

/* The length of a beat in whole notes: the beat unit, or three of them in a compound meter. */
static Rational beat_length(const PlanMeter *meter) {
    long long num = meter->beat_unit == 8 && meter->beats_per_bar % 3 == 0 ? 3 : 1;
    return (Rational){num, meter->beat_unit};
}

const PatternEntry *composer_rhythm_draw_cell(const KnowledgeSnapshot *knowledge, const PlanMeter *meter, unsigned pattern_job,
                                              bool previous_ends_half, unsigned long_weak, bool commit, FillRng *rng, PlanNote *out,
                                              size_t *out_count) {
    const PatternEntry *entries[CELLS_MAX];
    unsigned weights[CELLS_MAX];
    size_t lengths[CELLS_MAX];
    size_t count = composer_rhythm_cells(knowledge, meter, pattern_job, entries, CELLS_MAX);
    PlanNote *lines = malloc(CELLS_MAX * COMPOSER_RHYTHM_NOTES_MAX * sizeof(PlanNote));
    if (!lines) {
        return NULL;
    }
    size_t usable = 0;
    Rational beat = beat_length(meter);
    for (size_t c = 0; c < count; c++) {
        PlanNote *line = lines + c * COMPOSER_RHYTHM_NOTES_MAX;
        weights[c] = 0;
        if (!composer_rhythm_cell_notes(entries[c], knowledge, meter, line, &lengths[c])) {
            continue;
        }
        if (line[0].duration.num == 1 && line[0].duration.den == 2 && previous_ends_half) {
            continue;
        }
        weights[c] = 1;
        for (size_t n = 1; n < lengths[c]; n++) {
            if (rational_cmp(line[n].duration, beat) >= 0 && line[n].offset.num > 0) {
                weights[c] = 1 + long_weak;
            }
        }
        usable++;
    }
    const PatternEntry *found = NULL;
    if (usable > 0) {
        size_t pick = commit ? composer_draw_weighted(rng, weights, count) : 0;
        if (!commit) {
            for (size_t c = 0; c < count && !found; c++) {
                pick = weights[c] > 0 ? c : pick;
                found = weights[c] > 0 ? entries[c] : NULL;
            }
        } else if (pick < count) {
            found = entries[pick];
        }
        if (found && commit) {
            *out_count = lengths[pick];
            memcpy(out, lines + pick * COMPOSER_RHYTHM_NOTES_MAX, lengths[pick] * sizeof(PlanNote));
        }
    }
    free(lines);
    return found;
}

/* ------------------------------------------------------------ rhythm-only motives */

PlanResult composer_rhythm_motive_attach(Plan *plan, NodeId parent, Rational start, const Motive *motive, const char *name,
                                         NodeId *out_node, LaneSpanId *out_span) {
    if (!plan || !motive) {
        return PLAN_ERR_NULL;
    }
    if (motive->note_count == 0 || motive->note_count > PLAN_TONES_MAX_NOTES) {
        return PLAN_ERR_BAD_PAYLOAD;
    }
    PlanConstraint constraint;
    memset(&constraint, 0, sizeof(constraint));
    constraint.kind = PLAN_CONSTRAINT_MOTIVE_TONES;
    PlanMotiveTones *tones = &constraint.params.motive_tones;
    for (size_t c = 0; c < motive->chord_count && c < PLAN_TONES_MAX_CHORDS; c++) {
        Rational end = c + 1 < motive->chord_count ? motive->chords[c + 1].onset : motive->length;
        PlanToneChord *chord = &tones->chords[tones->chord_count];
        if (motive_chord_numeral(&motive->chords[c], chord->numeral, sizeof(chord->numeral)) &&
            rational_sub(end, motive->chords[c].onset, &chord->length) == RATIONAL_OK && chord->length.num > 0) {
            chord->onset = motive->chords[c].onset;
            tones->chord_count++;
        }
    }
    if (tones->chord_count == 0) {
        snprintf(tones->chords[0].numeral, sizeof(tones->chords[0].numeral), "I");
        tones->chords[0].onset = (Rational){0, 1};
        tones->chords[0].length = motive->length;
        tones->chord_count = 1;
    }
    for (size_t i = 0; i < motive->note_count; i++) {
        snprintf(tones->labels[i], PLAN_TONE_LABEL_MAX, "%s", HARMONY_TONE_RHYTHM);
    }
    tones->label_count = motive->note_count;
    NodeId node;
    LaneSpanId span;
    PlanResult result = motive_attach_as(plan, parent, start, motive, name, PLAN_STATE_REALIZED, &node, &span);
    result = result == PLAN_OK ? plan_node_add_constraint(plan, node, &constraint) : result;
    result = result == PLAN_OK ? plan_node_set_state(plan, node, PLAN_STATE_GIVEN) : result;
    result = result == PLAN_OK ? plan_lane_span_set_state(plan, span, PLAN_STATE_GIVEN) : result;
    if (result == PLAN_OK && out_node) {
        *out_node = node;
    }
    if (result == PLAN_OK && out_span) {
        *out_span = span;
    }
    return result;
}
