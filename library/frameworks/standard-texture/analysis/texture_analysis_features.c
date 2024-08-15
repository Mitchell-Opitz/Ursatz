#include "texture_analysis_features.h"

#include <stdlib.h>
#include <string.h>

typedef unsigned long long Mask;

static int popcount(Mask mask) {
    int count = 0;
    while (mask != 0) {
        count += (int)(mask & 1u);
        mask >>= 1;
    }
    return count;
}

static Mask voice_bit(size_t voice) {
    return (Mask)1u << (voice % STANDARD_TEXTURE_MAX_VOICES);
}

bool standard_texture_event_in_window(const StandardTextureEvent *event, const StandardTextureWindow *window) {
    return event != NULL && window != NULL && standard_texture_q_compare(event->onset, window->end) < 0 &&
           standard_texture_q_compare(event->release, window->start) > 0;
}

double standard_texture_window_beats(const StandardTextureWindow *window) {
    if (window == NULL) {
        return 0.0;
    }
    return standard_texture_q_double(standard_texture_q_sub(window->end, window->start)) /
           standard_texture_q_double(window->beat);
}

static bool attacks_in_window(const StandardTextureEvent *event, const StandardTextureWindow *window) {
    return standard_texture_q_compare(event->onset, window->start) >= 0 && standard_texture_q_compare(event->onset, window->end) < 0;
}

static bool sounds_at(const StandardTextureEvent *event, StandardTextureQ time) {
    return standard_texture_q_compare(event->onset, time) <= 0 && standard_texture_q_compare(time, event->release) < 0;
}

/* The events that sound in the window, as a new array (NULL with none or on failure; *out_count says which). */
static StandardTextureEvent *filtered(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                      size_t *out_count) {
    StandardTextureEvent *kept = (StandardTextureEvent *)malloc((count == 0 ? 1 : count) * sizeof(StandardTextureEvent));
    size_t n = 0;
    for (size_t i = 0; kept != NULL && events != NULL && i < count; i++) {
        if (standard_texture_event_in_window(&events[i], window)) {
            kept[n++] = events[i];
        }
    }
    *out_count = kept == NULL ? 0 : n;
    return kept;
}

static int compare_q(const void *a, const void *b) {
    return standard_texture_q_compare(*(const StandardTextureQ *)a, *(const StandardTextureQ *)b);
}

/* The distinct attack times of the (filtered) events, sorted; the count in *out. */
static StandardTextureQ *attack_times(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                      size_t *out) {
    StandardTextureQ *times = (StandardTextureQ *)malloc((count == 0 ? 1 : count) * sizeof(StandardTextureQ));
    size_t n = 0;
    for (size_t i = 0; times != NULL && i < count; i++) {
        if (attacks_in_window(&events[i], window)) {
            times[n++] = events[i].onset;
        }
    }
    qsort(times, n, sizeof(StandardTextureQ), compare_q);
    size_t distinct = 0;
    for (size_t i = 0; i < n; i++) {
        if (i == 0 || standard_texture_q_compare(times[i], times[i - 1]) != 0) {
            times[distinct++] = times[i];
        }
    }
    *out = distinct;
    return times;
}

void standard_texture_feature_voice_counts(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                           int *out_max, double *out_mean) {
    if (out_max != NULL) {
        *out_max = 0;
    }
    if (out_mean != NULL) {
        *out_mean = 0.0;
    }
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    StandardTextureQ *points = (StandardTextureQ *)malloc((2 * n + 2) * sizeof(StandardTextureQ));
    if (in == NULL || points == NULL || window == NULL) {
        free(in);
        free(points);
        return;
    }
    size_t p = 0;
    points[p++] = window->start;
    points[p++] = window->end;
    for (size_t i = 0; i < n; i++) {
        if (standard_texture_q_compare(in[i].onset, window->start) > 0 && standard_texture_q_compare(in[i].onset, window->end) < 0) {
            points[p++] = in[i].onset;
        }
        if (standard_texture_q_compare(in[i].release, window->start) > 0 && standard_texture_q_compare(in[i].release, window->end) < 0) {
            points[p++] = in[i].release;
        }
    }
    qsort(points, p, sizeof(StandardTextureQ), compare_q);
    double weighted = 0.0;
    int most = 0;
    for (size_t k = 0; k + 1 < p; k++) {
        if (standard_texture_q_compare(points[k], points[k + 1]) == 0) {
            continue;
        }
        Mask mask = 0;
        for (size_t i = 0; i < n; i++) {
            if (sounds_at(&in[i], points[k])) {
                mask |= voice_bit(in[i].voice);
            }
        }
        int voices = popcount(mask);
        if (voices > most) {
            most = voices;
        }
        weighted += (double)voices * standard_texture_q_double(standard_texture_q_sub(points[k + 1], points[k]));
    }
    double length = standard_texture_q_double(standard_texture_q_sub(window->end, window->start));
    if (out_max != NULL) {
        *out_max = most;
    }
    if (out_mean != NULL) {
        *out_mean = length > 0.0 ? weighted / length : 0.0;
    }
    free(in);
    free(points);
}

double standard_texture_feature_synchrony(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                          size_t *out_attack_times) {
    if (out_attack_times != NULL) {
        *out_attack_times = 0;
    }
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    size_t times_count = 0;
    StandardTextureQ *times = in == NULL ? NULL : attack_times(in, n, window, &times_count);
    double share = 0.0;
    if (times != NULL && times_count > 0) {
        size_t shared = 0;
        for (size_t t = 0; t < times_count; t++) {
            Mask mask = 0;
            for (size_t i = 0; i < n; i++) {
                if (standard_texture_q_compare(in[i].onset, times[t]) == 0) {
                    mask |= voice_bit(in[i].voice);
                }
            }
            if (popcount(mask) >= 2) {
                shared++;
            }
        }
        share = (double)shared / (double)times_count;
    }
    if (out_attack_times != NULL) {
        *out_attack_times = times_count;
    }
    free(times);
    free(in);
    return share;
}

static bool voice_attacks_at(const StandardTextureEvent *events, size_t count, size_t voice, StandardTextureQ time) {
    for (size_t i = 0; i < count; i++) {
        if (events[i].voice == voice && standard_texture_q_compare(events[i].onset, time) == 0) {
            return true;
        }
    }
    return false;
}

/* The distinct voices that attack in the window, ascending (count in *out). */
static size_t attacking_voices(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window, size_t *voices,
                               size_t capacity) {
    size_t n = 0;
    for (size_t i = 0; i < count && n < capacity; i++) {
        if (!attacks_in_window(&events[i], window)) {
            continue;
        }
        bool seen = false;
        for (size_t k = 0; k < n; k++) {
            seen = seen || voices[k] == events[i].voice;
        }
        if (!seen) {
            voices[n++] = events[i].voice;
        }
    }
    for (size_t a = 0; a < n; a++) {
        for (size_t b = a + 1; b < n; b++) {
            if (voices[b] < voices[a]) {
                size_t t = voices[a];
                voices[a] = voices[b];
                voices[b] = t;
            }
        }
    }
    return n;
}

size_t standard_texture_feature_independence(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                             StandardTexturePair *pairs, size_t capacity, double *out_mean, double *out_min) {
    if (out_mean != NULL) {
        *out_mean = 0.0;
    }
    if (out_min != NULL) {
        *out_min = 0.0;
    }
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    size_t times_count = 0;
    StandardTextureQ *times = in == NULL ? NULL : attack_times(in, n, window, &times_count);
    size_t voices[STANDARD_TEXTURE_MAX_VOICES];
    size_t voice_count = in == NULL ? 0 : attacking_voices(in, n, window, voices, STANDARD_TEXTURE_MAX_VOICES);
    size_t pair_count = 0;
    double total = 0.0;
    double least = 1.0;
    for (size_t a = 0; times != NULL && a < voice_count; a++) {
        for (size_t b = a + 1; b < voice_count; b++) {
            size_t both = 0;
            size_t either = 0;
            for (size_t t = 0; t < times_count; t++) {
                bool in_a = voice_attacks_at(in, n, voices[a], times[t]);
                bool in_b = voice_attacks_at(in, n, voices[b], times[t]);
                both += (in_a && in_b) ? 1 : 0;
                either += (in_a || in_b) ? 1 : 0;
            }
            double independence = either == 0 ? 0.0 : 1.0 - (double)both / (double)either;
            if (pairs != NULL && pair_count < capacity) {
                pairs[pair_count].first = voices[a];
                pairs[pair_count].second = voices[b];
                pairs[pair_count].independence = independence;
            }
            total += independence;
            if (independence < least) {
                least = independence;
            }
            pair_count++;
        }
    }
    if (pair_count > 0) {
        if (out_mean != NULL) {
            *out_mean = total / (double)pair_count;
        }
        if (out_min != NULL) {
            *out_min = least;
        }
    }
    free(times);
    free(in);
    return pair_count;
}

/* The highest pitch of `voice` sounding at `time`, or false. */
static bool voice_pitch_at(const StandardTextureEvent *events, size_t count, size_t voice, StandardTextureQ time, long *pitch) {
    bool found = false;
    for (size_t i = 0; i < count; i++) {
        if (events[i].voice == voice && sounds_at(&events[i], time) && (!found || events[i].pitch > *pitch)) {
            *pitch = events[i].pitch;
            found = true;
        }
    }
    return found;
}

double standard_texture_feature_prominence(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                           double *out_ratio, double *out_gap, size_t *out_top_voice) {
    if (out_ratio != NULL) {
        *out_ratio = 0.0;
    }
    if (out_gap != NULL) {
        *out_gap = 0.0;
    }
    if (out_top_voice != NULL) {
        *out_top_voice = 0;
    }
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    if (in == NULL || n == 0) {
        free(in);
        return 0.0;
    }
    size_t voices[STANDARD_TEXTURE_MAX_VOICES];
    double pitch_sum[STANDARD_TEXTURE_MAX_VOICES];
    size_t pitch_count[STANDARD_TEXTURE_MAX_VOICES];
    size_t attacks[STANDARD_TEXTURE_MAX_VOICES];
    size_t voice_count = 0;
    for (size_t i = 0; i < n; i++) {
        size_t k = 0;
        while (k < voice_count && voices[k] != in[i].voice) {
            k++;
        }
        if (k == voice_count) {
            if (voice_count == STANDARD_TEXTURE_MAX_VOICES) {
                continue;
            }
            voices[voice_count] = in[i].voice;
            pitch_sum[voice_count] = 0.0;
            pitch_count[voice_count] = 0;
            attacks[voice_count] = 0;
            voice_count++;
        }
        pitch_sum[k] += (double)in[i].pitch;
        pitch_count[k]++;
        if (attacks_in_window(&in[i], window)) {
            attacks[k]++;
        }
    }
    if (voice_count < 2) {
        if (out_top_voice != NULL) {
            *out_top_voice = voice_count == 1 ? voices[0] : 0;
        }
        free(in);
        return 0.0;
    }
    size_t top = 0;
    for (size_t k = 1; k < voice_count; k++) {
        double mean_k = pitch_sum[k] / (double)pitch_count[k];
        double mean_top = pitch_sum[top] / (double)pitch_count[top];
        if (mean_k > mean_top || (mean_k == mean_top && voices[k] < voices[top])) {
            top = k;
        }
    }
    double beats = standard_texture_window_beats(window);
    double top_density = beats > 0.0 ? (double)attacks[top] / beats : 0.0;
    double others = 0.0;
    for (size_t k = 0; k < voice_count; k++) {
        if (k != top) {
            others += beats > 0.0 ? (double)attacks[k] / beats : 0.0;
        }
    }
    others /= (double)(voice_count - 1);
    double ratio;
    if (others == 0.0) {
        ratio = top_density == 0.0 ? 1.0 : STANDARD_TEXTURE_PROMINENCE_CAP;
    } else {
        ratio = top_density / others;
    }
    if (ratio < 1.0 / STANDARD_TEXTURE_PROMINENCE_CAP) {
        ratio = 1.0 / STANDARD_TEXTURE_PROMINENCE_CAP;
    }
    if (ratio > STANDARD_TEXTURE_PROMINENCE_CAP) {
        ratio = STANDARD_TEXTURE_PROMINENCE_CAP;
    }
    double prominence = ratio >= 1.0 ? ratio : 1.0 / ratio;

    /* the gap, over the top voice's attacks (the window start when it has none) */
    StandardTextureQ times[256];
    size_t time_count = 0;
    for (size_t i = 0; i < n && time_count < 256; i++) {
        if (in[i].voice == voices[top] && attacks_in_window(&in[i], window)) {
            times[time_count++] = in[i].onset;
        }
    }
    if (time_count == 0) {
        times[time_count++] = window->start;
    }
    double gap_sum = 0.0;
    size_t gap_count = 0;
    for (size_t t = 0; t < time_count; t++) {
        long top_pitch = 0;
        if (!voice_pitch_at(in, n, voices[top], times[t], &top_pitch)) {
            continue;
        }
        bool any = false;
        long highest_other = 0;
        for (size_t k = 0; k < voice_count; k++) {
            long pitch = 0;
            if (k != top && voice_pitch_at(in, n, voices[k], times[t], &pitch) && (!any || pitch > highest_other)) {
                highest_other = pitch;
                any = true;
            }
        }
        if (any) {
            gap_sum += (double)(top_pitch - highest_other);
            gap_count++;
        }
    }
    if (out_ratio != NULL) {
        *out_ratio = ratio;
    }
    if (out_gap != NULL) {
        *out_gap = gap_count > 0 ? gap_sum / (double)gap_count : 0.0;
    }
    if (out_top_voice != NULL) {
        *out_top_voice = voices[top];
    }
    free(in);
    return prominence;
}

static int compare_long_desc(const void *a, const void *b) {
    long left = *(const long *)a;
    long right = *(const long *)b;
    return (left < right) - (left > right);
}

double standard_texture_feature_register(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                         long *out_span) {
    if (out_span != NULL) {
        *out_span = 0;
    }
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    size_t times_count = 0;
    StandardTextureQ *times = in == NULL ? NULL : attack_times(in, n, window, &times_count);
    double spacing = 0.0;
    if (in != NULL && n > 0 && times != NULL) {
        long low = in[0].pitch;
        long high = in[0].pitch;
        for (size_t i = 1; i < n; i++) {
            low = in[i].pitch < low ? in[i].pitch : low;
            high = in[i].pitch > high ? in[i].pitch : high;
        }
        if (out_span != NULL) {
            *out_span = high - low;
        }
        double gap_total = 0.0;
        size_t gap_count = 0;
        for (size_t t = 0; t <= times_count; t++) {
            if (t == 0 && times_count > 0 && standard_texture_q_compare(times[0], window->start) == 0) {
                continue; /* the window start is the first attack time: counted once */
            }
            StandardTextureQ at = t == 0 ? window->start : times[t - 1];
            long pitches[STANDARD_TEXTURE_MAX_VOICES];
            size_t voices_seen[STANDARD_TEXTURE_MAX_VOICES];
            size_t found = 0;
            for (size_t i = 0; i < n; i++) {
                if (!sounds_at(&in[i], at)) {
                    continue;
                }
                size_t k = 0;
                while (k < found && voices_seen[k] != in[i].voice) {
                    k++;
                }
                if (k == found) {
                    if (found == STANDARD_TEXTURE_MAX_VOICES) {
                        continue;
                    }
                    voices_seen[found] = in[i].voice;
                    pitches[found++] = in[i].pitch;
                } else if (in[i].pitch > pitches[k]) {
                    pitches[k] = in[i].pitch;
                }
            }
            qsort(pitches, found, sizeof(long), compare_long_desc);
            for (size_t k = 0; k + 1 < found; k++) {
                gap_total += (double)(pitches[k] - pitches[k + 1]);
                gap_count++;
            }
        }
        spacing = gap_count > 0 ? gap_total / (double)gap_count : 0.0;
    }
    free(times);
    free(in);
    return spacing;
}

double standard_texture_feature_density(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window) {
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    size_t times_count = 0;
    StandardTextureQ *times = in == NULL ? NULL : attack_times(in, n, window, &times_count);
    double beats = standard_texture_window_beats(window);
    free(times);
    free(in);
    return beats > 0.0 ? (double)times_count / beats : 0.0;
}

typedef struct {
    StandardTextureQ onset;
    long pitch;
} Attack;

static int compare_attacks(const void *a, const void *b) {
    const Attack *left = (const Attack *)a;
    const Attack *right = (const Attack *)b;
    int by_time = standard_texture_q_compare(left->onset, right->onset);
    return by_time != 0 ? by_time : (left->pitch < right->pitch) - (left->pitch > right->pitch);
}

/* The score of one voice's attacks (sorted, one per onset): the share of its (interval, inter-onset) pairs a repeated cell covers. */
static double voice_pattern_score(const Attack *attacks, size_t n, StandardTextureQ beat) {
    if (n < 3) {
        return 0.0;
    }
    size_t m = n - 1;
    long d[256];
    StandardTextureQ o[256];
    if (m > 256) {
        m = 256;
    }
    for (size_t i = 0; i < m; i++) {
        d[i] = attacks[i + 1].pitch - attacks[i].pitch;
        o[i] = standard_texture_q_sub(attacks[i + 1].onset, attacks[i].onset);
    }
    size_t best = 0;
    for (size_t length = 1; length <= 4 && length <= m / 2; length++) {
        for (size_t s = 0; s + length <= m; s++) {
            StandardTextureQ duration = standard_texture_q(0, 1);
            for (size_t k = 0; k < length; k++) {
                duration = standard_texture_q_add(duration, o[s + k]);
            }
            if (standard_texture_q_compare(duration, beat) > 0) {
                continue;
            }
            size_t occurrences = 1;
            size_t j = s + length;
            while (j + length <= m) {
                bool same = true;
                for (size_t k = 0; same && k < length; k++) {
                    same = d[s + k] == d[j + k] && standard_texture_q_compare(o[s + k], o[j + k]) == 0;
                }
                if (same) {
                    occurrences++;
                    j += length;
                } else {
                    j++;
                }
            }
            if (occurrences >= 2 && occurrences * length > best) {
                best = occurrences * length;
            }
        }
    }
    return (double)best / (double)m;
}

/* The distinct attacks of `voice` in the window, in order (one per onset, the highest pitch), in `attacks` (room for n). */
static size_t voice_attacks(const StandardTextureEvent *in, size_t n, const StandardTextureWindow *window, size_t voice,
                            Attack *attacks) {
    size_t m = 0;
    for (size_t i = 0; i < n; i++) {
        if (in[i].voice == voice && attacks_in_window(&in[i], window)) {
            attacks[m].onset = in[i].onset;
            attacks[m].pitch = in[i].pitch;
            m++;
        }
    }
    qsort(attacks, m, sizeof(Attack), compare_attacks);
    size_t distinct = 0;
    for (size_t i = 0; i < m; i++) {
        if (i == 0 || standard_texture_q_compare(attacks[i].onset, attacks[distinct - 1].onset) != 0) {
            attacks[distinct++] = attacks[i];
        }
    }
    return distinct;
}

double standard_texture_feature_pattern(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                        double *out_max, size_t *out_voices) {
    if (out_max != NULL) {
        *out_max = 0.0;
    }
    if (out_voices != NULL) {
        *out_voices = 0;
    }
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    Attack *attacks = (Attack *)malloc((n == 0 ? 1 : n) * sizeof(Attack));
    size_t voices[STANDARD_TEXTURE_MAX_VOICES];
    size_t voice_count = in == NULL ? 0 : attacking_voices(in, n, window, voices, STANDARD_TEXTURE_MAX_VOICES);
    double total = 0.0;
    double most = 0.0;
    size_t scored = 0;
    for (size_t v = 0; attacks != NULL && v < voice_count; v++) {
        size_t distinct = voice_attacks(in, n, window, voices[v], attacks);
        if (distinct < 3) {
            continue;
        }
        double score = voice_pattern_score(attacks, distinct, window->beat);
        total += score;
        most = score > most ? score : most;
        scored++;
    }
    if (out_max != NULL) {
        *out_max = most;
    }
    if (out_voices != NULL) {
        *out_voices = scored;
    }
    free(attacks);
    free(in);
    return scored > 0 ? total / (double)scored : 0.0;
}

double standard_texture_feature_voice_pattern(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                              size_t voice) {
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    Attack *attacks = (Attack *)malloc((n == 0 ? 1 : n) * sizeof(Attack));
    double score = 0.0;
    if (in != NULL && attacks != NULL) {
        size_t distinct = voice_attacks(in, n, window, voice, attacks);
        score = distinct >= 3 ? voice_pattern_score(attacks, distinct, window->beat) : 0.0;
    }
    free(attacks);
    free(in);
    return score;
}

static int compare_by_mean_pitch(const void *a, const void *b) {
    const double *left = (const double *)a;
    const double *right = (const double *)b;
    /* rows of {mean pitch, voice id}: higher pitch first, then the lower id */
    if (left[0] != right[0]) {
        return left[0] > right[0] ? -1 : 1;
    }
    return (left[1] > right[1]) - (left[1] < right[1]);
}

size_t standard_texture_consolidate_voices(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                           StandardTextureEvent *out) {
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    if (in == NULL || out == NULL) {
        free(in);
        return 0;
    }
    memcpy(out, in, n * sizeof(StandardTextureEvent));
    free(in);

    /* the voices: id, track, mean pitch, whether it attacks in the window */
    size_t ids[STANDARD_TEXTURE_MAX_VOICES];
    size_t tracks[STANDARD_TEXTURE_MAX_VOICES];
    double pitch_sum[STANDARD_TEXTURE_MAX_VOICES];
    size_t pitch_count[STANDARD_TEXTURE_MAX_VOICES];
    bool attacks[STANDARD_TEXTURE_MAX_VOICES];
    size_t voices = 0;
    for (size_t i = 0; i < n; i++) {
        size_t k = 0;
        while (k < voices && ids[k] != out[i].voice) {
            k++;
        }
        if (k == voices) {
            if (voices == STANDARD_TEXTURE_MAX_VOICES) {
                continue;
            }
            ids[voices] = out[i].voice;
            tracks[voices] = out[i].track;
            pitch_sum[voices] = 0.0;
            pitch_count[voices] = 0;
            attacks[voices] = false;
            voices++;
        }
        pitch_sum[k] += (double)out[i].pitch;
        pitch_count[k]++;
        attacks[k] = attacks[k] || attacks_in_window(&out[i], window);
    }
    double order[STANDARD_TEXTURE_MAX_VOICES][2];
    for (size_t k = 0; k < voices; k++) {
        order[k][0] = pitch_sum[k] / (double)pitch_count[k];
        order[k][1] = (double)k;
    }
    qsort(order, voices, sizeof(order[0]), compare_by_mean_pitch);

    /* the lines: each takes the voices of its track it never attacks together with */
    size_t line_of[STANDARD_TEXTURE_MAX_VOICES];
    size_t line_track[STANDARD_TEXTURE_MAX_VOICES];
    Mask line_members[STANDARD_TEXTURE_MAX_VOICES];
    bool line_open[STANDARD_TEXTURE_MAX_VOICES]; /* made of voices that attack: only these take another voice */
    size_t lines = 0;
    for (size_t r = 0; r < voices; r++) {
        size_t v = (size_t)order[r][1];
        size_t chosen = lines;
        for (size_t g = 0; attacks[v] && g < lines; g++) {
            if (!line_open[g] || line_track[g] != tracks[v]) {
                continue;
            }
            bool together = false;
            for (size_t i = 0; !together && i < n; i++) {
                if (out[i].voice != ids[v] || !attacks_in_window(&out[i], window)) {
                    continue;
                }
                for (size_t j = 0; !together && j < n; j++) {
                    together = (line_members[g] & voice_bit(out[j].voice)) != 0 && attacks_in_window(&out[j], window) &&
                               standard_texture_q_compare(out[j].onset, out[i].onset) == 0;
                }
            }
            if (!together) {
                chosen = g;
                break;
            }
        }
        if (chosen == lines) {
            line_track[lines] = tracks[v];
            line_members[lines] = 0;
            line_open[lines] = attacks[v];
            lines++;
        }
        line_of[v] = chosen;
        line_members[chosen] |= voice_bit(ids[v]);
    }
    for (size_t i = 0; i < n; i++) {
        for (size_t v = 0; v < voices; v++) {
            if (ids[v] == out[i].voice) {
                out[i].voice = line_of[v];
                break;
            }
        }
    }
    return n;
}

double standard_texture_feature_unison(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                       double synchrony, bool *out_unison) {
    if (out_unison != NULL) {
        *out_unison = false;
    }
    size_t n = 0;
    StandardTextureEvent *in = filtered(events, count, window, &n);
    size_t times_count = 0;
    StandardTextureQ *times = in == NULL ? NULL : attack_times(in, n, window, &times_count);
    size_t considered = 0;
    size_t same = 0;
    for (size_t t = 0; times != NULL && t <= times_count; t++) {
        if (t == 0 && times_count > 0 && standard_texture_q_compare(times[0], window->start) == 0) {
            continue;
        }
        StandardTextureQ at = t == 0 ? window->start : times[t - 1];
        size_t voices_seen[STANDARD_TEXTURE_MAX_VOICES];
        long pitches[STANDARD_TEXTURE_MAX_VOICES];
        size_t found = 0;
        for (size_t i = 0; i < n; i++) {
            if (!sounds_at(&in[i], at)) {
                continue;
            }
            size_t k = 0;
            while (k < found && voices_seen[k] != in[i].voice) {
                k++;
            }
            if (k == found) {
                if (found == STANDARD_TEXTURE_MAX_VOICES) {
                    continue;
                }
                voices_seen[found] = in[i].voice;
                pitches[found++] = in[i].pitch;
            } else if (in[i].pitch > pitches[k]) {
                pitches[k] = in[i].pitch;
            }
        }
        if (found < 2) {
            continue;
        }
        considered++;
        bool all_same = true;
        long first_class = ((pitches[0] % 12) + 12) % 12;
        for (size_t k = 1; k < found; k++) {
            all_same = all_same && ((pitches[k] % 12) + 12) % 12 == first_class;
        }
        same += all_same ? 1 : 0;
    }
    double share = considered > 0 ? (double)same / (double)considered : 0.0;
    if (out_unison != NULL) {
        *out_unison = considered > 0 && same == considered && synchrony >= 1.0 - 1e-12;
    }
    free(times);
    free(in);
    return share;
}

double standard_texture_feature_confidence(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window) {
    double total = 0.0;
    size_t n = 0;
    for (size_t i = 0; events != NULL && window != NULL && i < count; i++) {
        if (attacks_in_window(&events[i], window)) {
            total += events[i].confidence;
            n++;
        }
    }
    return n > 0 ? total / (double)n : 1.0;
}

void standard_texture_measure_features(const StandardTextureEvent *events, size_t count, const StandardTextureWindow *window,
                                       int measure, StandardTextureWindowFeatures *out) {
    if (out == NULL || window == NULL) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->measure = measure;
    out->start = window->start;
    out->end = window->end;
    out->voice_confidence = 1.0;
    StandardTextureEvent *lines = (StandardTextureEvent *)malloc((count == 0 ? 1 : count) * sizeof(StandardTextureEvent));
    if (lines == NULL) {
        return;
    }
    size_t n = events == NULL ? 0 : standard_texture_consolidate_voices(events, count, window, lines);
    out->events = n;
    for (size_t i = 0; i < n; i++) {
        if (attacks_in_window(&lines[i], window)) {
            out->attacks++;
        }
    }
    standard_texture_feature_voice_counts(lines, n, window, &out->voices_max, &out->voices_mean);
    out->synchrony = standard_texture_feature_synchrony(lines, n, window, &out->attack_times);
    out->pair_count = standard_texture_feature_independence(lines, n, window, NULL, 0, &out->independence_mean, &out->independence_min);
    out->prominence = standard_texture_feature_prominence(lines, n, window, &out->prominence_ratio, &out->top_gap, &out->top_voice);
    out->spacing = standard_texture_feature_register(lines, n, window, &out->register_span);
    out->density = standard_texture_feature_density(lines, n, window);
    out->pattern_mean = standard_texture_feature_pattern(lines, n, window, &out->pattern_max, &out->pattern_voices);
    out->unison_share = standard_texture_feature_unison(lines, n, window, out->synchrony, &out->unison);
    out->voice_confidence = standard_texture_feature_confidence(lines, n, window);

    /* the top voice's pattern, and the greatest among the voices that accompany it */
    size_t top = n > 0 ? lines[0].voice : 0;
    if (out->voices_max >= 2) {
        top = out->top_voice;
    }
    out->pattern_top = standard_texture_feature_voice_pattern(lines, n, window, top);
    for (size_t i = 0; i < n; i++) {
        if (lines[i].voice != top && attacks_in_window(&lines[i], window)) {
            double score = standard_texture_feature_voice_pattern(lines, n, window, lines[i].voice);
            out->pattern_support = score > out->pattern_support ? score : out->pattern_support;
        }
    }
    free(lines);
}

typedef struct {
    size_t track;
    size_t voice;
} VoiceKey;

static int compare_keys(const void *a, const void *b) {
    const VoiceKey *left = (const VoiceKey *)a;
    const VoiceKey *right = (const VoiceKey *)b;
    if (left->track != right->track) {
        return left->track < right->track ? -1 : 1;
    }
    return (left->voice > right->voice) - (left->voice < right->voice);
}

StandardTextureAnalysisStatus standard_texture_measure_windows(const StandardTextureInput *input, const StandardTextureConfig *config,
                                                               StandardTextureWindows *out) {
    if (input == NULL || out == NULL || input->map == NULL) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_NULL_ARGUMENT;
    }
    StandardTextureConfig fallback = standard_texture_config_default();
    if (config == NULL) {
        config = &fallback;
    }
    if (!standard_texture_config_valid(config)) {
        return STANDARD_TEXTURE_ANALYSIS_ERR_INVALID;
    }
    StandardTextureWindows found = {NULL, 0};
    if (input->note_count == 0) {
        *out = found;
        return STANDARD_TEXTURE_ANALYSIS_OK;
    }
    VoiceKey *keys = (VoiceKey *)malloc(input->note_count * sizeof(VoiceKey));
    StandardTextureEvent *events = (StandardTextureEvent *)malloc(input->note_count * sizeof(StandardTextureEvent));
    if (keys == NULL || events == NULL) {
        free(keys);
        free(events);
        return STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
    }
    for (size_t i = 0; i < input->note_count; i++) {
        keys[i].track = input->notes[i].track;
        keys[i].voice = input->notes[i].voice;
    }
    qsort(keys, input->note_count, sizeof(VoiceKey), compare_keys);
    size_t distinct = 0;
    for (size_t i = 0; i < input->note_count; i++) {
        if (i == 0 || compare_keys(&keys[i], &keys[i - 1]) != 0) {
            keys[distinct++] = keys[i];
        }
    }
    if (distinct > STANDARD_TEXTURE_MAX_VOICES) {
        free(keys);
        free(events);
        return STANDARD_TEXTURE_ANALYSIS_ERR_INVALID;
    }
    size_t usable = 0;
    StandardTextureQ first = standard_texture_q(0, 1);
    StandardTextureQ last = standard_texture_q(0, 1);
    for (size_t i = 0; i < input->note_count; i++) {
        StandardTextureQ onset;
        StandardTextureQ release;
        if (!standard_texture_span_q(input->notes[i].span, &onset, &release)) {
            continue;
        }
        onset = standard_texture_snap(config, onset, false);
        release = standard_texture_snap(config, release, true);
        if (standard_texture_q_compare(release, onset) <= 0) {
            continue;
        }
        VoiceKey key = {input->notes[i].track, input->notes[i].voice};
        const VoiceKey *at = (const VoiceKey *)bsearch(&key, keys, distinct, sizeof(VoiceKey), compare_keys);
        events[usable].onset = onset;
        events[usable].release = release;
        events[usable].pitch = input->notes[i].pitch;
        events[usable].voice = (size_t)(at - keys);
        events[usable].confidence = input->notes[i].voice_confidence;
        events[usable].track = input->notes[i].track;
        if (usable == 0 || standard_texture_q_compare(onset, first) < 0) {
            first = onset;
        }
        if (usable == 0 || standard_texture_q_compare(release, last) > 0) {
            last = release;
        }
        usable++;
    }
    int first_measure = 0;
    int last_measure = 0;
    StandardTextureAnalysisStatus status = STANDARD_TEXTURE_ANALYSIS_OK;
    if (usable > 0 && standard_texture_measure_at(input->map, first, &first_measure) &&
        standard_texture_last_measure_touched(input->map, last, &last_measure) && last_measure >= first_measure) {
        found.items = (StandardTextureWindowFeatures *)calloc((size_t)(last_measure - first_measure + 1), sizeof(*found.items));
        if (found.items == NULL) {
            status = STANDARD_TEXTURE_ANALYSIS_ERR_ALLOC;
        }
        for (int measure = first_measure; status == STANDARD_TEXTURE_ANALYSIS_OK && measure <= last_measure; measure++) {
            StandardTextureWindow window;
            if (!standard_texture_measure_bounds(input->map, measure, &window.start, &window.end) ||
                !standard_texture_beat_length(input->map, measure, &window.beat)) {
                continue;
            }
            standard_texture_measure_features(events, usable, &window, measure, &found.items[found.count]);
            found.count++;
        }
    }
    free(keys);
    free(events);
    if (status != STANDARD_TEXTURE_ANALYSIS_OK) {
        standard_texture_windows_destroy(&found);
        return status;
    }
    *out = found;
    return STANDARD_TEXTURE_ANALYSIS_OK;
}

void standard_texture_windows_destroy(StandardTextureWindows *windows) {
    if (windows == NULL) {
        return;
    }
    free(windows->items);
    windows->items = NULL;
    windows->count = 0;
}
