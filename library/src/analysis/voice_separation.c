#include "analysis/voice_separation.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation_internal.h"
#include "pitch/tuning_system.h"
#include "time/time_span.h"
#include "time/temporal_anchor.h"

#define VS_MAX_VOICES_LIMIT 8

VoiceSeparationConfig voice_separation_config_default(void) {
    VoiceSeparationConfig config;
    config.max_voices = 4;
    config.allow_crossing = false;
    config.rest_hold_numerator = 2;
    config.rest_hold_denominator = 1;
    return config;
}

void voice_assignment_destroy(VoiceAssignment *assignment) {
    if (assignment == NULL) {
        return;
    }
    for (size_t i = 0; i < assignment->count; i++) {
        entity_id_destroy(assignment->entries[i].note_id);
    }
    free(assignment->entries);
    free(assignment->streams);
    assignment->entries = NULL;
    assignment->streams = NULL;
    assignment->count = 0;
    assignment->stream_count = 0;
}

/* --- arithmetic --- */

/* |a - b|, 0 on success. */
static int abs_diff(Rational a, Rational b, Rational *out) {
    return rational_cmp(a, b) >= 0 ? rational_sub(a, b, out) : rational_sub(b, a, out);
}

/* --- step 4: assign the attacks to free voices --- */

typedef struct {
    size_t violations;
    Rational cost;
    unsigned long recency;
    size_t voice[VS_MAX_VOICES_LIMIT]; /* by position in the attack order */
    bool valid;
} Candidate;

typedef struct {
    const VsVoice *voices;
    size_t voice_count;
    const VsAttack *attacks;
    const size_t *order;     /* attack indices, pitch descending then id ascending */
    const size_t *by_id;     /* attack positions (in `order`), id ascending */
    size_t attack_count;
    bool allow_crossing;
    size_t chosen[VS_MAX_VOICES_LIMIT];
    bool used[VS_MAX_VOICES_LIMIT];
    Candidate best;
    Candidate second;
    int failure;
} Search;

/* True when a ranks before b. */
static bool candidate_better(const Candidate *a, const Candidate *b, const size_t *by_id, size_t count) {
    if (a->violations != b->violations) {
        return a->violations < b->violations;
    }
    int by_cost = rational_cmp(a->cost, b->cost);
    if (by_cost != 0) {
        return by_cost < 0;
    }
    if (a->recency != b->recency) {
        return a->recency > b->recency;
    }
    for (size_t k = 0; k < count; k++) {
        size_t position = by_id[k];
        if (a->voice[position] != b->voice[position]) {
            return a->voice[position] < b->voice[position];
        }
    }
    return false;
}

static void evaluate(Search *search) {
    Candidate candidate;
    memset(&candidate, 0, sizeof(candidate));
    candidate.cost = rational_ratio(0, 1);
    candidate.valid = true;
    for (size_t k = 0; k < search->attack_count; k++) {
        size_t voice = search->chosen[k];
        Rational pitch = search->attacks[search->order[k]].pitch;
        Rational distance = {0, 1};
        if (abs_diff(pitch, search->voices[voice].pitch, &distance) != 0 ||
            rational_add(candidate.cost, distance, &candidate.cost) != RATIONAL_OK) {
            search->failure = -1;
            return;
        }
        candidate.recency += search->voices[voice].recency;
        candidate.voice[k] = voice;
        if (!search->allow_crossing) {
            for (size_t h = 0; h < search->voice_count; h++) {
                if (!search->voices[h].held) {
                    continue;
                }
                int against = rational_cmp(search->voices[h].pitch, pitch);
                if ((h < voice && against < 0) || (h > voice && against > 0)) {
                    candidate.violations++;
                }
            }
        }
    }
    if (!search->best.valid || candidate_better(&candidate, &search->best, search->by_id, search->attack_count)) {
        search->second = search->best;
        search->best = candidate;
    } else if (!search->second.valid ||
               candidate_better(&candidate, &search->second, search->by_id, search->attack_count)) {
        search->second = candidate;
    }
}

static void enumerate(Search *search, size_t position, size_t first_voice) {
    if (search->failure != 0) {
        return;
    }
    if (position == search->attack_count) {
        evaluate(search);
        return;
    }
    for (size_t v = first_voice; v < search->voice_count; v++) {
        if (search->voices[v].held || search->used[v]) {
            continue;
        }
        search->used[v] = true;
        search->chosen[position] = v;
        enumerate(search, position + 1, search->allow_crossing ? 0 : v + 1);
        search->used[v] = false;
    }
}

/* The attacks ordered by pitch descending, then id ascending; and the same attacks by id ascending. */
static void order_attacks(const VsAttack *attacks, size_t count, size_t *order) {
    for (size_t i = 0; i < count; i++) {
        order[i] = i;
    }
    for (size_t i = 1; i < count; i++) {
        size_t value = order[i];
        size_t j = i;
        while (j > 0) {
            const VsAttack *before = &attacks[order[j - 1]];
            const VsAttack *now = &attacks[value];
            int by_pitch = rational_cmp(before->pitch, now->pitch);
            if (by_pitch > 0 || (by_pitch == 0 && strcmp(before->id, now->id) <= 0)) {
                break;
            }
            order[j] = order[j - 1];
            j--;
        }
        order[j] = value;
    }
}

int vs_assign_onset(const VsVoice *voices, size_t voice_count, const VsAttack *attacks, size_t attack_count,
                    bool allow_crossing, size_t *out_voice, double *out_margin) {
    size_t free_count = 0;
    for (size_t v = 0; v < voice_count; v++) {
        free_count += voices[v].held ? 0 : 1;
    }
    if (attack_count > free_count || attack_count > VS_MAX_VOICES_LIMIT || voice_count > VS_MAX_VOICES_LIMIT) {
        return -2;
    }
    if (attack_count == 0) {
        *out_margin = 1.0;
        return 0;
    }

    size_t order[VS_MAX_VOICES_LIMIT];
    size_t by_id[VS_MAX_VOICES_LIMIT];
    order_attacks(attacks, attack_count, order);
    for (size_t k = 0; k < attack_count; k++) {
        by_id[k] = k;
    }
    for (size_t i = 1; i < attack_count; i++) {
        size_t value = by_id[i];
        size_t j = i;
        while (j > 0 && strcmp(attacks[order[by_id[j - 1]]].id, attacks[order[value]].id) > 0) {
            by_id[j] = by_id[j - 1];
            j--;
        }
        by_id[j] = value;
    }

    Search search;
    memset(&search, 0, sizeof(search));
    search.voices = voices;
    search.voice_count = voice_count;
    search.attacks = attacks;
    search.order = order;
    search.by_id = by_id;
    search.attack_count = attack_count;
    search.allow_crossing = allow_crossing;
    enumerate(&search, 0, 0);
    if (search.failure != 0 || !search.best.valid) {
        return search.failure != 0 ? search.failure : -2;
    }
    for (size_t k = 0; k < attack_count; k++) {
        out_voice[order[k]] = search.best.voice[k];
    }

    double margin = 1.0;
    if (search.second.valid && search.second.violations == search.best.violations) {
        if (rational_cmp(search.second.cost, search.best.cost) == 0) {
            margin = 0.0;
        } else {
            Rational gap = {0, 1};
            if (rational_sub(search.second.cost, search.best.cost, &gap) != RATIONAL_OK) {
                return -1;
            }
            margin = ((double)gap.num * (double)search.second.cost.den) /
                     ((double)gap.den * (double)search.second.cost.num);
        }
    }
    *out_margin = margin;
    return 0;
}

/* --- step 6: more attacks than free voices --- */

int vs_split_overflow(const VsAttack *attacks, size_t attack_count, size_t keep, bool *is_member) {
    size_t order[64];
    if (attack_count > 64) {
        return -1;
    }
    /* ascending pitch, then ascending id: the reverse of the descending order */
    order_attacks(attacks, attack_count, order);
    for (size_t i = 0; i < attack_count / 2; i++) {
        size_t swap = order[i];
        order[i] = order[attack_count - 1 - i];
        order[attack_count - 1 - i] = swap;
    }
    /* equal pitches: the reversal puts the higher id first; put the lower id first */
    for (size_t i = 1; i < attack_count; i++) {
        if (rational_cmp(attacks[order[i - 1]].pitch, attacks[order[i]].pitch) == 0 &&
            strcmp(attacks[order[i - 1]].id, attacks[order[i]].id) > 0) {
            size_t swap = order[i - 1];
            order[i - 1] = order[i];
            order[i] = swap;
        }
    }
    for (size_t i = 0; i < attack_count; i++) {
        is_member[i] = false;
    }
    size_t present = attack_count;
    while (present > keep) {
        bool found = false;
        size_t drop = 0;
        Rational smallest = {0, 1};
        size_t previous = 0;
        bool have_previous = false;
        for (size_t i = 0; i < attack_count; i++) {
            if (is_member[order[i]]) {
                continue;
            }
            if (have_previous) {
                Rational gap = {0, 1};
                if (abs_diff(attacks[order[i]].pitch, attacks[order[previous]].pitch, &gap) != 0) {
                    return -1;
                }
                if (!found || rational_cmp(gap, smallest) < 0) {
                    found = true;
                    smallest = gap;
                    drop = order[previous]; /* the lower note of the closest pair */
                }
            }
            previous = i;
            have_previous = true;
        }
        if (!found) {
            /* one note left and still too many: keep is 0 */
            for (size_t i = 0; i < attack_count; i++) {
                if (!is_member[order[i]]) {
                    drop = order[i];
                    break;
                }
            }
        }
        is_member[drop] = true;
        present--;
    }
    return 0;
}

size_t vs_nearest_voice(const Rational *current, const bool *has_current, size_t voice_count,
                        Rational pitch) {
    size_t best = voice_count;
    Rational best_distance = {0, 1};
    for (size_t v = 0; v < voice_count; v++) {
        if (!has_current[v]) {
            continue;
        }
        Rational distance = {0, 1};
        if (abs_diff(pitch, current[v], &distance) != 0) {
            continue; /* overflow: farthest */
        }
        if (best == voice_count || rational_cmp(distance, best_distance) < 0) {
            best = v;
            best_distance = distance;
        }
    }
    return best;
}

/* --- step 5: continuity across rests --- */

int vs_reference_kept(Rational last_release, Rational at, Rational hold_beats,
                      VsBeatLength beat_length, void *beat_context, bool *out_kept) {
    if (rational_cmp(last_release, at) >= 0) {
        *out_kept = true;
        return 0;
    }
    Rational rest = {0, 1};
    Rational beat = {0, 1};
    Rational window = {0, 1};
    if (rational_sub(at, last_release, &rest) != RATIONAL_OK) {
        return -1;
    }
    if (!beat_length(beat_context, last_release, &beat)) {
        return -3;
    }
    if (rational_mul(beat, hold_beats, &window) != RATIONAL_OK) {
        return -1;
    }
    *out_kept = rational_cmp(rest, window) <= 0;
    return 0;
}

/* The beat length from the injected beat grid: the length of the beat that holds `at`. */
typedef struct {
    const MeterMap *map;
    BeatGridProvider provider;
    void *provider_context;
} BeatGridContext;

static bool grid_beat_length(void *context, Rational at, Rational *out) {
    const BeatGridContext *grid = (const BeatGridContext *)context;
    MusicalTime *time = musical_time_from_rational_or_null(at);
    if (time == NULL) {
        return false;
    }
    int measure = 0;
    MeterMapResult indexed = meter_map_measure_index_at(grid->map, time, &measure);
    musical_time_destroy(time);
    TimeSpan *span = NULL;
    if (indexed != METER_MAP_OK || meter_map_measure_span(grid->map, measure, &span) != METER_MAP_OK) {
        return false;
    }
    Rational measure_end;
    bool measure_ok = rational_of_musical_time(time_span_end(span), &measure_end) == RATIONAL_OK;
    time_span_destroy(span);
    if (!measure_ok) {
        return false;
    }

    MusicalTime **starts = NULL;
    size_t count = 0;
    if (!grid->provider(grid->map, measure, grid->provider_context, &starts, &count) || count == 0) {
        return false;
    }
    bool found = false;
    for (size_t k = 0; k < count && !found; k++) {
        Rational beat_start;
        Rational beat_end = measure_end;
        if (rational_of_musical_time(starts[k], &beat_start) != RATIONAL_OK ||
            (k + 1 < count && rational_of_musical_time(starts[k + 1], &beat_end) != RATIONAL_OK)) {
            break;
        }
        if (rational_cmp(beat_start, at) <= 0 && rational_cmp(at, beat_end) < 0) {
            found = rational_sub(beat_end, beat_start, out) == 0;
            break;
        }
    }
    for (size_t k = 0; k < count; k++) {
        musical_time_destroy(starts[k]);
    }
    free(starts);
    return found;
}

/* --- the notes of one stream --- */

typedef struct {
    const char *id;
    Rational onset;
    Rational release;
    Rational pitch;
    size_t stream;
} VsNote;

typedef struct {
    size_t stream;
    Rational onset;
    size_t index;
} SortKey;

static int compare_keys(const void *a, const void *b) {
    const SortKey *x = (const SortKey *)a;
    const SortKey *y = (const SortKey *)b;
    if (x->stream != y->stream) {
        return x->stream < y->stream ? -1 : 1;
    }
    int by_onset = rational_cmp(x->onset, y->onset);
    if (by_onset != 0) {
        return by_onset;
    }
    return x->index < y->index ? -1 : (x->index > y->index ? 1 : 0);
}

typedef struct {
    size_t voice;
    double confidence;
    bool member;
} NoteResult;

/* The most notes sounding at once among the stream's notes (keys[lo..hi), in onset order). */
static int max_simultaneous(const VsNote *notes, const SortKey *keys, size_t lo, size_t hi, size_t *out) {
    Rational *active = (Rational *)calloc(hi - lo > 0 ? hi - lo : 1, sizeof(Rational));
    if (active == NULL) {
        return -2;
    }
    size_t active_count = 0;
    size_t most = 0;
    size_t i = lo;
    while (i < hi) {
        Rational onset = notes[keys[i].index].onset;
        size_t kept = 0;
        for (size_t a = 0; a < active_count; a++) {
            if (rational_cmp(active[a], onset) > 0) {
                active[kept++] = active[a];
            }
        }
        active_count = kept;
        while (i < hi && rational_cmp(notes[keys[i].index].onset, onset) == 0) {
            active[active_count++] = notes[keys[i].index].release;
            i++;
        }
        if (active_count > most) {
            most = active_count;
        }
    }
    free(active);
    *out = most;
    return 0;
}

typedef struct {
    bool has_note;
    Rational pitch;    /* the voice's last primary pitch */
    Rational release;  /* the last primary's release */
    unsigned long recency;
} VoiceState;

static int nominal_pitch(Rational lowest, Rational highest, size_t voice, size_t voice_count,
                         Rational *out) {
    Rational range = {0, 1};
    Rational offset = {0, 1};
    if (rational_sub(highest, lowest, &range) != RATIONAL_OK ||
        rational_mul(range, rational_ratio(2 * voice + 1, 2 * voice_count), &offset) != RATIONAL_OK) {
        return -1;
    }
    return rational_sub(highest, offset, out);
}

/* The pitch a voice is measured from at `onset` is its last pitch while it keeps a reference (step 5), else
   its nominal pitch (the middle of its band of the stream's range). */
static int separate_stream(const VsNote *notes, const SortKey *keys, size_t lo, size_t hi,
                           const VoiceSeparationConfig *config, Rational hold, const BeatGridContext *grid,
                           NoteResult *results, size_t *out_voice_count) {
    size_t simultaneous = 0;
    int status = max_simultaneous(notes, keys, lo, hi, &simultaneous);
    if (status != 0) {
        return status;
    }
    size_t voice_count = simultaneous < config->max_voices ? simultaneous : config->max_voices;
    if (voice_count == 0) {
        voice_count = 1;
    }
    *out_voice_count = voice_count;

    Rational lowest = notes[keys[lo].index].pitch;
    Rational highest = lowest;
    for (size_t i = lo; i < hi; i++) {
        Rational pitch = notes[keys[i].index].pitch;
        if (rational_cmp(pitch, lowest) < 0) {
            lowest = pitch;
        }
        if (rational_cmp(pitch, highest) > 0) {
            highest = pitch;
        }
    }

    VoiceState state[VS_MAX_VOICES_LIMIT];
    memset(state, 0, sizeof(state));
    unsigned long counter = 0;

    size_t i = lo;
    while (i < hi) {
        Rational onset = notes[keys[i].index].onset;
        size_t group_end = i;
        while (group_end < hi && rational_cmp(notes[keys[group_end].index].onset, onset) == 0) {
            group_end++;
        }
        size_t attacked = group_end - i;

        VsVoice voices[VS_MAX_VOICES_LIMIT];
        size_t free_count = 0;
        for (size_t v = 0; v < voice_count; v++) {
            voices[v].held = state[v].has_note && rational_cmp(state[v].release, onset) > 0;
            voices[v].recency = state[v].recency;
            bool kept = false;
            if (state[v].has_note) {
                int status = vs_reference_kept(state[v].release, onset, hold, grid_beat_length, (void *)grid, &kept);
                if (status != 0) {
                    return status;
                }
            }
            if (kept) {
                voices[v].pitch = state[v].pitch;
            } else if (nominal_pitch(lowest, highest, v, voice_count, &voices[v].pitch) != 0) {
                return -1;
            }
            free_count += voices[v].held ? 0 : 1;
        }

        VsAttack attacks[64];
        bool is_member[64];
        if (attacked > 64) {
            return -1;
        }
        for (size_t k = 0; k < attacked; k++) {
            attacks[k].id = notes[keys[i + k].index].id;
            attacks[k].pitch = notes[keys[i + k].index].pitch;
        }
        if (attacked > free_count) {
            if (vs_split_overflow(attacks, attacked, free_count, is_member) != 0) {
                return -1;
            }
        } else {
            for (size_t k = 0; k < attacked; k++) {
                is_member[k] = false;
            }
        }

        VsAttack primaries[VS_MAX_VOICES_LIMIT];
        size_t primary_of[VS_MAX_VOICES_LIMIT];
        size_t primary_count = 0;
        for (size_t k = 0; k < attacked; k++) {
            if (!is_member[k]) {
                primaries[primary_count] = attacks[k];
                primary_of[primary_count] = k;
                primary_count++;
            }
        }
        size_t assigned[VS_MAX_VOICES_LIMIT];
        double margin = 1.0;
        int assigned_status = vs_assign_onset(voices, voice_count, primaries, primary_count, config->allow_crossing,
                                              assigned, &margin);
        if (assigned_status != 0) {
            return assigned_status;
        }

        for (size_t p = 0; p < primary_count; p++) {
            const VsNote *note = &notes[keys[i + primary_of[p]].index];
            size_t voice = assigned[p];
            results[keys[i + primary_of[p]].index].voice = voice;
            results[keys[i + primary_of[p]].index].confidence = margin;
            results[keys[i + primary_of[p]].index].member = false;
            state[voice].has_note = true;
            state[voice].pitch = note->pitch;
            state[voice].release = note->release;
            state[voice].recency = ++counter;
        }

        Rational current[VS_MAX_VOICES_LIMIT];
        bool has_current[VS_MAX_VOICES_LIMIT];
        for (size_t v = 0; v < voice_count; v++) {
            has_current[v] = state[v].has_note && rational_cmp(state[v].release, onset) > 0;
            current[v] = state[v].pitch;
        }
        for (size_t k = 0; k < attacked; k++) {
            if (!is_member[k]) {
                continue;
            }
            size_t nearest = vs_nearest_voice(current, has_current, voice_count, attacks[k].pitch);
            results[keys[i + k].index].voice = nearest < voice_count ? nearest : 0;
            results[keys[i + k].index].confidence = margin;
            results[keys[i + k].index].member = true;
        }
        i = group_end;
    }
    return 0;
}

VoiceSeparationResult voice_separate(NoteEvent *const *notes, PitchNumericValue *const *pitch_values,
                                      size_t note_count, const size_t *stream_of_note,
                                      const MeterMap *map, BeatGridProvider provider, void *provider_context,
                                      const VoiceSeparationConfig *config, VoiceAssignment *out) {
    if (out == NULL || map == NULL || provider == NULL || (note_count > 0 && (notes == NULL || pitch_values == NULL))) {
        return VOICE_SEPARATION_ERR_NULL_ARGUMENT;
    }
    VoiceSeparationConfig effective = config != NULL ? *config : voice_separation_config_default();
    if (effective.max_voices < 1 || effective.max_voices > VS_MAX_VOICES_LIMIT ||
        effective.rest_hold_denominator == 0) {
        return VOICE_SEPARATION_ERR_BAD_CONFIG;
    }
    Rational hold = rational_ratio(effective.rest_hold_numerator, effective.rest_hold_denominator);
    BeatGridContext grid = {map, provider, provider_context};

    VsNote *facts = (VsNote *)calloc(note_count > 0 ? note_count : 1, sizeof(VsNote));
    SortKey *keys = (SortKey *)calloc(note_count > 0 ? note_count : 1, sizeof(SortKey));
    NoteResult *results = (NoteResult *)calloc(note_count > 0 ? note_count : 1, sizeof(NoteResult));
    if (facts == NULL || keys == NULL || results == NULL) {
        free(facts);
        free(keys);
        free(results);
        return VOICE_SEPARATION_ERR_ALLOC;
    }

    VoiceSeparationResult failure = VOICE_SEPARATION_OK;
    for (size_t i = 0; i < note_count && failure == VOICE_SEPARATION_OK; i++) {
        if (notes[i] == NULL || pitch_values[i] == NULL) {
            failure = VOICE_SEPARATION_ERR_NULL_NOTE;
            break;
        }
        if (i > 0 && !tuning_system_identity_equals(pitch_numeric_value_tuning_system(pitch_values[0]),
                                                    pitch_numeric_value_tuning_system(pitch_values[i]))) {
            failure = VOICE_SEPARATION_ERR_MIXED_TUNING;
            break;
        }
        long numerator = pitch_numeric_value_position_numerator(pitch_values[i]);
        const MusicalTime *position = temporal_anchor_position(note_event_temporal_anchor(notes[i]));
        if (numerator < 0 || musical_time_numerator(position) < 0) {
            failure = VOICE_SEPARATION_ERR_UNSUPPORTED_PITCH;
            break;
        }
        facts[i].id = entity_id_value(note_event_id(notes[i]));
        Rational length;
        if (rational_of_musical_time(position, &facts[i].onset) != RATIONAL_OK ||
            rational_of_duration(note_event_duration(notes[i]), &length) != RATIONAL_OK ||
            rational_add(facts[i].onset, length, &facts[i].release) != RATIONAL_OK) {
            failure = VOICE_SEPARATION_ERR_OVERFLOW;
            break;
        }
        facts[i].pitch =
            rational_ratio((unsigned long long)numerator, pitch_numeric_value_position_denominator(pitch_values[i]));
        facts[i].stream = stream_of_note != NULL ? stream_of_note[i] : 0;
        keys[i].stream = facts[i].stream;
        keys[i].onset = facts[i].onset;
        keys[i].index = i;
    }
    if (failure == VOICE_SEPARATION_OK && note_count > 0) {
        qsort(keys, note_count, sizeof(SortKey), compare_keys);
    }

    VoiceStreamCount *streams = (VoiceStreamCount *)calloc(note_count > 0 ? note_count : 1, sizeof(VoiceStreamCount));
    size_t stream_count = 0;
    if (streams == NULL && failure == VOICE_SEPARATION_OK) {
        failure = VOICE_SEPARATION_ERR_ALLOC;
    }
    size_t lo = 0;
    while (failure == VOICE_SEPARATION_OK && lo < note_count) {
        size_t hi = lo;
        while (hi < note_count && keys[hi].stream == keys[lo].stream) {
            hi++;
        }
        size_t voice_count = 0;
        int status = separate_stream(facts, keys, lo, hi, &effective, hold, &grid, results, &voice_count);
        if (status == -2) {
            failure = VOICE_SEPARATION_ERR_ALLOC;
        } else if (status == -3) {
            failure = VOICE_SEPARATION_ERR_BEAT_GRID;
        } else if (status != 0) {
            failure = VOICE_SEPARATION_ERR_OVERFLOW;
        } else {
            streams[stream_count].stream = keys[lo].stream;
            streams[stream_count].voice_count = voice_count;
            stream_count++;
        }
        lo = hi;
    }

    VoiceAssignmentEntry *entries = NULL;
    if (failure == VOICE_SEPARATION_OK) {
        entries = (VoiceAssignmentEntry *)calloc(note_count > 0 ? note_count : 1, sizeof(VoiceAssignmentEntry));
        if (entries == NULL) {
            failure = VOICE_SEPARATION_ERR_ALLOC;
        }
    }
    size_t built = 0;
    for (; failure == VOICE_SEPARATION_OK && built < note_count; built++) {
        if (entity_id_create(facts[built].id, &entries[built].note_id) != ENTITY_ID_OK) {
            failure = VOICE_SEPARATION_ERR_ALLOC;
            break;
        }
        entries[built].stream = facts[built].stream;
        entries[built].voice = results[built].voice;
        entries[built].confidence = results[built].confidence;
        entries[built].chord_member = results[built].member;
    }

    free(facts);
    free(keys);
    free(results);
    if (failure != VOICE_SEPARATION_OK) {
        for (size_t i = 0; i < built; i++) {
            entity_id_destroy(entries[i].note_id);
        }
        free(entries);
        free(streams);
        return failure;
    }
    out->entries = entries;
    out->count = note_count;
    out->streams = streams;
    out->stream_count = stream_count;
    return VOICE_SEPARATION_OK;
}
