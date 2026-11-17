#include "analysis/harmony/key_regions_internal.h"

#include <stdlib.h>


/* --- keys on phrases (see key_regions.h, KEYS ON PHRASES) --- */

/* A phrase as measure slots (0 = the first measure of the piece), with what the path needs of it. */
typedef struct {
    size_t first;
    size_t last;
    unsigned cadence_keys;
    unsigned arrival_keys;
    bool closes;
    bool span_start;   /* starts a key signature span after the first */
    unsigned allowed;  /* the keys it may take (0 = all): its span's two keys when it starts a restricted span */
} PhraseSlot;

/* A path state is (key, satisfied, changed here); `satisfied`: the region holds a phrase that confirms its key. */
#define PHRASE_STATES (COMMON_PRACTICE_KEY_COUNT * 4)

#define STATE(key, satisfied, changed) ((size_t)(((key) * 2 + (satisfied)) * 2 + (changed)))

/* The phrases clipped to the measures, closed up and split at the key signature span starts. Returns the count in *out_count. */
static CommonPracticeKeyRegionsStatus build_phrase_slots(const CommonPracticeKeyRegionsConfig *config,
                                                         const MeasureWeights *measures, const SpanPlan *plan,
                                                         PhraseSlot **out, size_t *out_count) {
    long measure_count = (long)measures->measure_count;
    PhraseSlot *clipped = (PhraseSlot *)calloc(config->phrase_count + 1, sizeof(PhraseSlot));
    PhraseSlot *split = (PhraseSlot *)calloc(config->phrase_count + plan->count + 1, sizeof(PhraseSlot));
    if (clipped == NULL || split == NULL) {
        free(clipped);
        free(split);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    size_t count = 0;
    for (size_t i = 0; i < config->phrase_count; i++) {
        const CommonPracticeKeyPhrase *phrase = &config->phrases[i];
        long first = (long)phrase->first_measure - (long)measures->first_measure;
        long last = (long)phrase->last_measure - (long)measures->first_measure;
        if (last < 0 || first >= measure_count) {
            continue; /* outside the piece */
        }
        clipped[count].first = first < 0 ? 0 : (size_t)first;
        clipped[count].last = last >= measure_count ? (size_t)measure_count - 1 : (size_t)last;
        clipped[count].cadence_keys = phrase->cadence_keys;
        clipped[count].arrival_keys = phrase->arrival_keys;
        clipped[count].closes = phrase->closes;
        count++;
    }
    if (count == 0) { /* no phrase in the piece: the whole piece is one */
        clipped[0].last = (size_t)measure_count - 1;
        count = 1;
    }
    clipped[0].first = 0;
    for (size_t i = 1; i < count; i++) {
        clipped[i].first = clipped[i - 1].last + 1;
    }
    clipped[count - 1].last = (size_t)measure_count - 1;

    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        PhraseSlot part = clipped[i];
        for (size_t sp = 1; sp < plan->count; sp++) {
            size_t start = plan->start[sp];
            if (start > part.first && start <= part.last) {
                PhraseSlot before_part = part;
                before_part.last = start - 1;
                before_part.cadence_keys = 0;
                before_part.arrival_keys = 0;
                before_part.closes = false;
                split[total++] = before_part;
                part.first = start;
            }
        }
        split[total++] = part;
    }
    for (size_t i = 0; i < total; i++) {
        split[i].span_start = false;
        split[i].allowed = i == 0 ? plan->allowed[0] : 0;
        for (size_t sp = 1; sp < plan->count; sp++) {
            if (split[i].first == plan->start[sp]) {
                split[i].span_start = true;
                split[i].allowed = plan->allowed[sp];
            }
        }
    }
    free(clipped);
    *out = split;
    *out_count = total;
    return COMMON_PRACTICE_KEY_REGIONS_OK;
}

/* The score of every key for every phrase: the correlation of the phrase's pooled weights with the key's profile plus evidence_weight
   times the phrase's functional evidence over its chord count. */
static void phrase_scores(const PhraseSlot *phrases, size_t phrase_count, const MeasureWeights *measures,
                          const CommonPracticeKeyRegionsConfig *config, double (*score)[COMMON_PRACTICE_KEY_COUNT]) {
    double profile[COMMON_PRACTICE_KEY_COUNT][12];
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        key_regions_rotated_profile(&config->profiles, k, profile[k]);
    }
    for (size_t p = 0; p < phrase_count; p++) {
        double sum[12] = {0.0};
        double evidence[COMMON_PRACTICE_KEY_COUNT] = {0.0};
        double chords = 0.0;
        for (size_t m = phrases[p].first; m <= phrases[p].last; m++) {
            for (int pc = 0; pc < 12; pc++) {
                sum[pc] += measures->weights[m][pc];
            }
            chords += measures->chords[m];
            for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
                evidence[k] += measures->evidence[m][k];
            }
        }
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            score[p][k] = key_regions_correlate(sum, profile[k]) + (chords > 0.0 ? config->evidence_weight * evidence[k] / chords : 0.0);
        }
    }
}

static bool has_key(unsigned keys, int key) {
    return ((keys >> key) & 1u) != 0;
}

/* The least-cost key per phrase (see key_regions.h, KEYS ON PHRASES). */
static CommonPracticeKeyRegionsStatus choose_phrase_path(const PhraseSlot *phrases, size_t phrase_count,
                                                         const double (*score)[COMMON_PRACTICE_KEY_COUNT], double penalty,
                                                         int *path) {
    size_t cells = phrase_count * PHRASE_STATES;
    double *cost = (double *)malloc(cells * sizeof(double));
    int *from = (int *)malloc(cells * sizeof(int));
    if (cost == NULL || from == NULL) {
        free(cost);
        free(from);
        return COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    }
    for (size_t c = 0; c < cells; c++) {
        cost[c] = BIG;
        from[c] = -1;
    }
    for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
        if (phrases[0].allowed == 0 || has_key(phrases[0].allowed, k)) {
            cost[STATE(k, 1, 0)] = -score[0][k]; /* the first region of the piece is satisfied */
        }
    }
    for (size_t p = 1; p < phrase_count; p++) {
        const PhraseSlot *phrase = &phrases[p];
        const double *previous = &cost[(p - 1) * PHRASE_STATES];
        double *current = &cost[p * PHRASE_STATES];
        int *back = &from[p * PHRASE_STATES];
        bool free_change = phrase->span_start || phrases[p - 1].closes;
        for (int pass = 0; pass < 2; pass++) { /* staying first, so a tie keeps the earlier key */
            for (int j = 0; j < COMMON_PRACTICE_KEY_COUNT; j++) {
                for (int satisfied = 0; satisfied < 2; satisfied++) {
                    for (int changed = 0; changed < 2; changed++) {
                        double before_cost = previous[STATE(j, satisfied, changed)];
                        if (before_cost >= BIG) {
                            continue;
                        }
                        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
                            if ((pass == 0) != (j == k) || (phrase->allowed != 0 && !has_key(phrase->allowed, k))) {
                                continue;
                            }
                            size_t into;
                            double total;
                            if (j == k) {
                                /* a change into k at phrase p - 1 belongs at the arrival when that phrase's own cadence arrives in k
                                   and the phrase before it did not */
                                if (changed && !phrases[p - 1].span_start && has_key(phrases[p - 1].arrival_keys, k) &&
                                    !(p >= 2 && has_key(phrases[p - 2].arrival_keys, k))) {
                                    continue;
                                }
                                into = STATE(k, (satisfied || has_key(phrase->cadence_keys, k)) ? 1 : 0, 0);
                                total = before_cost - score[p][k];
                            } else {
                                if (!satisfied && !phrase->span_start) {
                                    continue; /* a change needs the region before satisfied */
                                }
                                into = STATE(k, (free_change || has_key(phrase->cadence_keys, k)) ? 1 : 0, 1);
                                total = before_cost + (free_change ? 0.0 : penalty) - score[p][k];
                            }
                            if (current[into] >= BIG || total < current[into] - TIE) {
                                current[into] = total;
                                back[into] = (int)STATE(j, satisfied, changed);
                            }
                        }
                    }
                }
            }
        }
    }
    const double *last = &cost[(phrase_count - 1) * PHRASE_STATES];
    int end = -1;
    for (int want_satisfied = 1; want_satisfied >= 0 && end < 0; want_satisfied--) {
        for (int k = 0; k < COMMON_PRACTICE_KEY_COUNT; k++) {
            for (int satisfied = 0; satisfied < 2; satisfied++) {
                for (int changed = 0; changed < 2; changed++) {
                    size_t state = STATE(k, satisfied, changed);
                    if (last[state] >= BIG || (want_satisfied && !satisfied)) {
                        continue;
                    }
                    if (end < 0 || last[state] < last[end] - TIE) {
                        end = (int)state;
                    }
                }
            }
        }
    }
    CommonPracticeKeyRegionsStatus status = COMMON_PRACTICE_KEY_REGIONS_OK;
    if (end < 0) {
        status = COMMON_PRACTICE_KEY_REGIONS_ERR_BAD_CONFIG;
    }
    for (size_t p = phrase_count; p-- > 0 && end >= 0;) {
        path[p] = end / 4;
        end = from[p * PHRASE_STATES + (size_t)end];
    }
    free(cost);
    free(from);
    return status;
}

/* Places the keys on the phrases: fills measure_key per measure slot, marks arrival slots and the first region of each restricted span
   (forced). */
CommonPracticeKeyRegionsStatus key_regions_place_keys_on_phrases(const CommonPracticeKeyRegionsConfig *config,
                                                            const MeasureWeights *measures, const SpanPlan *plan,
                                                            int *measure_key, bool *forced, bool *arrival_slot) {
    PhraseSlot *phrases = NULL;
    size_t phrase_count = 0;
    CommonPracticeKeyRegionsStatus status = build_phrase_slots(config, measures, plan, &phrases, &phrase_count);
    if (status != COMMON_PRACTICE_KEY_REGIONS_OK) {
        return status;
    }
    double(*score)[COMMON_PRACTICE_KEY_COUNT] =
        (double(*)[COMMON_PRACTICE_KEY_COUNT])calloc(phrase_count, sizeof(double[COMMON_PRACTICE_KEY_COUNT]));
    int *path = (int *)calloc(phrase_count, sizeof(int));
    bool *first_region = (bool *)calloc(measures->measure_count, sizeof(bool));
    if (score == NULL || path == NULL || first_region == NULL) {
        status = COMMON_PRACTICE_KEY_REGIONS_ERR_ALLOC;
    } else {
        phrase_scores(phrases, phrase_count, measures, config, score);
        status = choose_phrase_path(phrases, phrase_count, (const double(*)[COMMON_PRACTICE_KEY_COUNT])score,
                                    config->change_penalty, path);
    }
    if (status == COMMON_PRACTICE_KEY_REGIONS_OK) {
        for (size_t p = 0; p < phrase_count; p++) {
            for (size_t m = phrases[p].first; m <= phrases[p].last; m++) {
                measure_key[m] = path[p];
            }
        }
        /* the last measure of a phrase whose cadence arrives in the next phrase's key takes that key */
        for (size_t p = 0; p + 1 < phrase_count; p++) {
            if (path[p + 1] != path[p] && has_key(phrases[p].arrival_keys, path[p + 1]) && !phrases[p + 1].span_start) {
                measure_key[phrases[p].last] = path[p + 1];
                arrival_slot[phrases[p].last] = true;
            }
        }
        for (size_t sp = 0; sp < plan->count; sp++) {
            size_t end = sp + 1 < plan->count ? plan->start[sp + 1] : measures->measure_count;
            forced[plan->start[sp]] = plan->allowed[sp] != 0;
            for (size_t m = plan->start[sp]; plan->allowed[sp] != 0 && m < end && measure_key[m] == measure_key[plan->start[sp]]; m++) {
                first_region[m] = true; /* the first region of a restricted span */
            }
        }
        for (size_t r = 0; r < config->repeat_count; r++) {
            const CommonPracticeKeyRepeat *repeat = &config->repeats[r];
            for (int t = 0; t < repeat->measure_count; t++) {
                long earlier = (long)repeat->earlier_first + t - (long)measures->first_measure;
                long later = (long)repeat->later_first + t - (long)measures->first_measure;
                if (earlier < 0 || later < 0 || (size_t)earlier >= measures->measure_count ||
                    (size_t)later >= measures->measure_count || first_region[earlier]) {
                    continue;
                }
                measure_key[later] = measure_key[earlier];
                arrival_slot[later] = arrival_slot[earlier];
            }
        }
    }
    free(first_region);
    free(path);
    free(score);
    free(phrases);
    return status;
}
