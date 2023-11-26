#include "analysis_six_four.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_claim.h"
#include "core/value.h"
#include "harmony_six_four.h"
#include "identity/entity_id.h"

static bool diatonic(const HarmonySegment *segment) {
    return segment->valid && !segment->applied;
}

static bool same_chord(const HarmonyChord *a, const HarmonyChord *b) {
    return a->degree == b->degree && a->inversion == b->inversion && a->seventh == b->seventh &&
           a->cadential == b->cadential && a->leading_tone == b->leading_tone &&
           a->half_diminished == b->half_diminished;
}

/* The run of consecutive segments making the same chord as segment i: [*first, *last]. */
static void run_of(const HarmonySegment *segments, size_t count, size_t i, size_t *first, size_t *last) {
    size_t a = i;
    size_t b = i;
    while (a > 0 && diatonic(&segments[a - 1]) && same_chord(&segments[a - 1].chord, &segments[i].chord)) {
        a--;
    }
    while (b + 1 < count && diatonic(&segments[b + 1]) && same_chord(&segments[b + 1].chord, &segments[i].chord)) {
        b++;
    }
    *first = a;
    *last = b;
}

static bool is_tonic_six_four(const HarmonyChord *chord) {
    return chord->degree == 1 && chord->inversion == 2 && !chord->seventh;
}

void harmony_analysis_mark_cadential(HarmonySegment *segments, size_t count) {
    size_t i = 0;
    while (i < count) {
        if (!diatonic(&segments[i])) {
            i++;
            continue;
        }
        size_t first, last;
        run_of(segments, count, i, &first, &last);
        const HarmonySegmentInput *start = segments[first].input;
        if (is_tonic_six_four(&segments[first].chord) && start->bass_degree == 5 && last + 1 < count &&
            diatonic(&segments[last + 1]) && segments[last + 1].chord.degree == 5 &&
            segments[last + 1].chord.inversion == 0 && segments[last + 1].input->bass_degree == 5 &&
            beat_strength_is_stronger(start->beat_strength, segments[last + 1].input->beat_strength)) {
            for (size_t s = first; s <= last; s++) {
                segments[s].chord.cadential = true;
            }
        }
        i = last + 1;
    }
}

/* The step of `degree` in the octave nearest `previous`. */
static int step_near(int previous, int degree) {
    int base = degree - 1;
    int best = base;
    int best_distance = 1000;
    for (int k = -4; k <= 4; k++) {
        int candidate = base + 7 * k;
        int distance = candidate > previous ? candidate - previous : previous - candidate;
        if (distance < best_distance) {
            best_distance = distance;
            best = candidate;
        }
    }
    return best;
}

static bool classify_run(const HarmonySegment *segments, size_t count, size_t first, size_t last,
                         SixFourType *out) {
    const HarmonySegment *middle = &segments[first];
    HarmonySixFour subject;
    memset(&subject, 0, sizeof(subject));
    subject.mode = middle->input->mode;
    bool cadential = middle->chord.cadential;

    const HarmonySegment *before = NULL;
    const HarmonySegment *after = NULL;
    if (first > 0 && diatonic(&segments[first - 1])) {
        size_t a, b;
        run_of(segments, count, first - 1, &a, &b);
        before = &segments[a];
    }
    if (last + 1 < count && diatonic(&segments[last + 1])) {
        after = &segments[last + 1];
    }
    if (after == NULL || (before == NULL && !cadential)) {
        return false;
    }
    if (before == NULL) {
        before = middle; /* the cadential 6/4 needs only the chord after */
    }
    const HarmonySegment *chain[3] = {before, middle, after};
    for (int j = 0; j < 3; j++) {
        if (chain[j]->input->mode != subject.mode || chain[j]->input->bass_degree < 1 ||
            chain[j]->input->bass_degree > 7) {
            return false;
        }
        subject.chords[j] = chain[j]->chord;
        subject.beat[j] = chain[j]->input->beat_strength;
    }
    bool soprano_known = true;
    for (int j = 0; j < 3; j++) {
        soprano_known = soprano_known && chain[j]->input->soprano_degree >= 1 && chain[j]->input->soprano_degree <= 7;
    }
    int bass_previous = chain[0]->input->bass_degree - 1;
    int soprano_previous = soprano_known ? chain[0]->input->soprano_degree - 1 : 0;
    for (int j = 0; j < 3; j++) {
        int bass = j == 0 ? bass_previous : step_near(bass_previous, chain[j]->input->bass_degree);
        int soprano = 0;
        if (soprano_known) {
            soprano = j == 0 ? soprano_previous : step_near(soprano_previous, chain[j]->input->soprano_degree);
            soprano_previous = soprano;
        }
        bass_previous = bass;
        subject.steps[j][VOICE_PART_BASS] = bass;
        subject.steps[j][VOICE_PART_SOPRANO] = soprano;
        subject.steps[j][VOICE_PART_ALTO] = soprano;
        subject.steps[j][VOICE_PART_TENOR] = soprano;
    }
    SixFourType type;
    if (!harmony_six_four_classify(&subject, &type)) {
        return false;
    }
    /* A passing 6/4 is on a weak beat: weaker than the chord before it, or weak. */
    if (type == SIX_FOUR_TYPE_PASSING && subject.beat[1] != BEAT_STRENGTH_WEAK &&
        !beat_strength_is_stronger(subject.beat[0], subject.beat[1])) {
        return false;
    }
    *out = type;
    return true;
}

HarmonySixFourStatus harmony_analysis_classify_six_fours(const HarmonySegment *segments, size_t count,
                                                         HarmonySixFourResult **out) {
    if (out == NULL || (segments == NULL && count > 0)) {
        return HARMONY_SIX_FOUR_ERR_NULL_ARGUMENT;
    }
    HarmonySixFourResult *results = (HarmonySixFourResult *)calloc(count > 0 ? count : 1, sizeof(*results));
    if (results == NULL) {
        return HARMONY_SIX_FOUR_ERR_ALLOC;
    }
    for (size_t i = 0; i < count; i++) {
        results[i].segment_id = segments[i].input->segment_id;
    }
    size_t i = 0;
    while (i < count) {
        if (!diatonic(&segments[i])) {
            i++;
            continue;
        }
        size_t first, last;
        run_of(segments, count, i, &first, &last);
        const HarmonyChord *chord = &segments[first].chord;
        if (chord->inversion == 2 && !chord->seventh) {
            SixFourType type;
            bool classified = classify_run(segments, count, first, last, &type);
            for (size_t s = first; s <= last; s++) {
                results[s].is_six_four = true;
                if (!classified) {
                    continue;
                }
                HarmonySixFourClaim claim;
                memset(&claim, 0, sizeof(claim));
                claim.type = (int)type;
                char id[256];
                snprintf(id, sizeof(id), "standard-harmony-six-four-%s", segments[s].input->segment_id);
                results[s].interpretation = harmony_analysis_make_interpretation(
                    id, segments[s].input->segment_id, HARMONY_SIX_FOUR_CLAIM_TYPE, &claim, sizeof(claim),
                    segments[s].input->uncertainty, "six-four-adapter");
                if (results[s].interpretation == NULL) {
                    harmony_six_four_results_destroy(results, count);
                    return HARMONY_SIX_FOUR_ERR_ALLOC;
                }
                results[s].classified = true;
                results[s].type = type;
            }
        }
        i = last + 1;
    }
    *out = results;
    return HARMONY_SIX_FOUR_OK;
}

void harmony_six_four_results_destroy(HarmonySixFourResult *results, size_t count) {
    if (results == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        interpretation_destroy(results[i].interpretation);
    }
    free(results);
}

bool harmony_six_four_claim_read(const Interpretation *interpretation, HarmonySixFourClaim *out) {
    if (interpretation == NULL || out == NULL) {
        return false;
    }
    const Value *claim = interpretation_claim(interpretation);
    size_t size = 0;
    if (claim == NULL || strcmp(value_type(claim), HARMONY_SIX_FOUR_CLAIM_TYPE) != 0) {
        return false;
    }
    const void *content = value_content(claim, &size);
    if (content == NULL || size != sizeof(*out)) {
        return false;
    }
    memcpy(out, content, sizeof(*out));
    return true;
}
