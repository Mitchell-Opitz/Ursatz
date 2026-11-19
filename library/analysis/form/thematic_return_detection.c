#include "analysis/form/thematic_return_detection.h"

#include <stdlib.h>
#include <string.h>

#include "analysis/notes/contour.h"
#include "analysis/notes/rhythmic_pattern.h"

/* Root-cause fix (framework-v1-reference-set triage, Bach BWV Anh. 114): MotifWindowMatching's
   exact comparison rule has no minimum-length/distinctiveness floor, which is safe at Motif
   granularity (callers already hand it short, curated candidate windows) but is too permissive
   reused unchanged at Theme granularity, where short/generic ThemeOccurrence windows (e.g. a
   3-event stepwise run) exact-match across unrelated occurrences and produce near-total
   pairwise "resembles" links instead of genuine recurring-material matches. A window below this
   length is too short/generic to count as a genuine thematic return and is excluded from
   matching entirely. 4 is a stated default, not Registry-settled: Sentence Detection already
   treats 2-event material as a legitimate (if minimal) continuation, so 2 does not separate
   "genuine theme" from "incidental fragment" one level up; 4 gives real separation without a
   corpus-derived number to justify going higher. Expect this to move once more of the corpus is
   checked. The value is defined in the header. */

static size_t window_length_of(const MotifWindow *window) { return rhythmic_pattern_duration_count(window->rhythm); }

static ThematicReturnDetectionConfig effective_config(const ThematicReturnDetectionConfig *config) {
    ThematicReturnDetectionConfig effective = thematic_return_detection_default_config();
    if (config != NULL) {
        if (config->window_length >= THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH) {
            effective.window_length = config->window_length;
        }
        if (config->threshold > 0.0 && config->threshold <= 1.0) {
            effective.threshold = config->threshold;
        }
    }
    return effective;
}

/* Compares every stretch of one theme with every stretch of the other, using MotifWindowMatching's comparison on
   stretches of `window_length` events (the shorter theme's length when that is less). Contour and rhythm each
   score 0 to 1 (the share of equal directions and of equal durations); the similarity of a stretch pair is the
   lower of the two, so a return needs both shape and rhythm. The best stretch pair (first in (earlier offset,
   later offset) order on a tie) is the match, and it counts as a return when its similarity reaches the
   threshold. Contour direction is transposition-invariant, so a transposed statement scores the same as an
   untransposed one. A window below THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH is too short/generic and never
   matches. */
static bool find_return(const MotifWindow *earlier, const MotifWindow *later, const ThematicReturnDetectionConfig *config,
                        ThematicReturnPair *match) {
    size_t earlier_length = window_length_of(earlier);
    size_t later_length = window_length_of(later);
    size_t length = earlier_length < later_length ? earlier_length : later_length;
    if (length < THEMATIC_RETURN_DETECTION_MIN_WINDOW_LENGTH) {
        return false;
    }
    if (length > config->window_length) {
        length = config->window_length;
    }
    bool found = false;
    for (size_t i = 0; i + length <= earlier_length; i++) {
        for (size_t j = 0; j + length <= later_length; j++) {
            MotifWindowSimilarity score;
            if (motif_window_matching_compare(earlier, i, later, j, length, &score) != MOTIF_WINDOW_MATCHING_OK) {
                continue;
            }
            double similarity = score.contour_score < score.rhythm_score ? score.contour_score : score.rhythm_score;
            if (found && similarity <= match->similarity) {
                continue;
            }
            match->contour_score = score.contour_score;
            match->rhythm_score = score.rhythm_score;
            match->similarity = similarity;
            match->earlier_offset = i;
            match->later_offset = j;
            match->window_length = length;
            found = true;
        }
    }
    return found && match->similarity >= config->threshold;
}

static bool valid_entry_shape(const ThematicReturnDetectionEntry *entry) {
    if (entry->theme_occurrence_id == NULL || entry->window == NULL) {
        return false;
    }
    return entry->window->contour != NULL && entry->window->rhythm != NULL && entry->window->event_ids != NULL &&
           entry->window->event_id_count > 0;
}

ThematicReturnDetectionConfig thematic_return_detection_default_config(void) {
    ThematicReturnDetectionConfig config = {THEMATIC_RETURN_DETECTION_DEFAULT_WINDOW_LENGTH,
                                             THEMATIC_RETURN_DETECTION_DEFAULT_THRESHOLD};
    return config;
}

ThematicReturnDetectionResult thematic_return_detection_build_pairs_configured(
    const ThematicReturnDetectionEntry *entries, size_t entry_count, const ThematicReturnDetectionConfig *config_in,
    ThematicReturnPair **out_pairs, size_t *out_pair_count) {
    ThematicReturnDetectionConfig config = effective_config(config_in);
    *out_pairs = NULL;
    *out_pair_count = 0;
    if (entry_count > 0 && entries == NULL) {
        return THEMATIC_RETURN_DETECTION_ERR_NULL_ENTRIES;
    }
    for (size_t i = 0; i < entry_count; i++) {
        if (!valid_entry_shape(&entries[i])) {
            return THEMATIC_RETURN_DETECTION_ERR_NULL_ENTRY;
        }
    }

    size_t max_pair_count = entry_count < 2 ? 0 : (entry_count * (entry_count - 1)) / 2;
    if (max_pair_count == 0) {
        return THEMATIC_RETURN_DETECTION_OK;
    }
    ThematicReturnPair *pairs = (ThematicReturnPair *)calloc(max_pair_count, sizeof(ThematicReturnPair));
    if (pairs == NULL) {
        return THEMATIC_RETURN_DETECTION_ERR_ALLOC;
    }

    size_t out_index = 0;
    for (size_t i = 0; i < entry_count; i++) {
        for (size_t j = i + 1; j < entry_count; j++) {
            ThematicReturnPair pair;
            if (!find_return(entries[i].window, entries[j].window, &config, &pair)) {
                continue;
            }
            pair.earlier = i;
            pair.later = j;
            pairs[out_index++] = pair;
        }
    }
    if (out_index == 0) {
        free(pairs);
        return THEMATIC_RETURN_DETECTION_OK;
    }
    *out_pairs = pairs;
    *out_pair_count = out_index;
    return THEMATIC_RETURN_DETECTION_OK;
}
