#include "triad_identification.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define TRIAD_DEGREE_COUNT 3

/* Root, third, fifth scale degrees (1-7) for each diatonic triad,
   compared as an unordered set -- see header for the tie-break rule. */
static const int TRIAD_DEGREES[COMMON_PRACTICE_TRIAD_MAJOR_KEY_COUNT][TRIAD_DEGREE_COUNT] = {
    {1, 3, 5}, /* I   */
    {2, 4, 6}, /* ii  */
    {3, 5, 7}, /* iii */
    {4, 6, 1}, /* IV  */
    {5, 7, 2}, /* V   */
    {6, 1, 3}, /* vi  */
    {7, 2, 4}, /* vii dim */
};

static bool contains_degree(const int *degrees, size_t count, int degree) {
    for (size_t i = 0; i < count; i++) {
        if (degrees[i] == degree) {
            return true;
        }
    }
    return false;
}

static size_t count_distinct(const int *scale_degrees, size_t scale_degree_count, int *out_distinct,
                              size_t out_capacity) {
    size_t distinct_count = 0;
    for (size_t i = 0; i < scale_degree_count; i++) {
        int degree = scale_degrees[i];
        if (degree < 1 || degree > 7) {
            continue;
        }
        if (!contains_degree(out_distinct, distinct_count, degree) && distinct_count < out_capacity) {
            out_distinct[distinct_count++] = degree;
        }
    }
    return distinct_count;
}

bool common_practice_identify_triad(const int *scale_degrees, size_t scale_degree_count, CommonPracticeTriad *out) {
    if (scale_degrees == NULL || out == NULL) {
        return false;
    }

    int distinct[7];
    size_t distinct_count = count_distinct(scale_degrees, scale_degree_count, distinct, 7);
    if (distinct_count < TRIAD_DEGREE_COUNT) {
        return false;
    }

    bool found = false;
    CommonPracticeTriad picked = (CommonPracticeTriad)0;
    for (int triad = 0; triad < COMMON_PRACTICE_TRIAD_MAJOR_KEY_COUNT; triad++) {
        bool matches = true;
        for (int i = 0; i < TRIAD_DEGREE_COUNT; i++) {
            if (!contains_degree(distinct, distinct_count, TRIAD_DEGREES[triad][i])) {
                matches = false;
                break;
            }
        }
        if (!matches) {
            continue;
        }
        if (!found) {
            found = true;
            picked = (CommonPracticeTriad)triad;
        } else {
            fprintf(stderr,
                    "common_practice_identify_triad: ambiguous subset match -- picked %s, also fits %s "
                    "(scan-order tie-break, see triad_identification.h)\n",
                    common_practice_triad_label(picked), common_practice_triad_label((CommonPracticeTriad)triad));
        }
    }

    if (!found) {
        return false;
    }
    *out = picked;
    return true;
}

bool common_practice_identify_triad_chord(const int *scale_degrees, size_t scale_degree_count,
                                           EntityID *sonority_target_id, Chord **out_chord) {
    CommonPracticeTriad triad;
    if (!common_practice_identify_triad(scale_degrees, scale_degree_count, &triad)) {
        return false;
    }
    return common_practice_build_triad_chord(triad, sonority_target_id, out_chord) == CHORD_OK;
}