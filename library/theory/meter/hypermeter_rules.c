#include "theory/meter/hypermeter_rules.h"

#include <stddef.h>

bool standard_meter_hypermeasure_score(unsigned phrase_measures, int arrival_index, bool elided, unsigned length,
                                       double *out_score) {
    if (out_score == NULL || (length != 2 && length != 4) || phrase_measures == 0 || arrival_index < -1 ||
        (!elided && arrival_index >= (int)phrase_measures)) {
        return false;
    }
    double fill = (double)((phrase_measures / length) * length) / (double)phrase_measures;
    if (arrival_index < 0 && !elided) {
        *out_score = fill;
        return true;
    }
    double cadence;
    if (elided) {
        cadence = phrase_measures % length == 0 ? 1.0 : 0.5;
    } else {
        unsigned last_start = ((phrase_measures - 1) / length) * length;
        cadence = (unsigned)arrival_index >= last_start ? 1.0 : 0.0;
    }
    *out_score = (fill + cadence) / 2.0;
    return true;
}

bool standard_meter_hypermeasure_choose(unsigned phrase_measures, int arrival_index, bool elided, unsigned *out_length,
                                        double *out_score) {
    double four = 0.0;
    double two = 0.0;
    if (out_length == NULL || out_score == NULL ||
        !standard_meter_hypermeasure_score(phrase_measures, arrival_index, elided, 4, &four) ||
        !standard_meter_hypermeasure_score(phrase_measures, arrival_index, elided, 2, &two)) {
        return false;
    }
    if (four >= two) {
        *out_length = 4;
        *out_score = four;
    } else {
        *out_length = 2;
        *out_score = two;
    }
    return true;
}

bool standard_meter_hypermeasure_extended_length(unsigned base_length, int added_measures, unsigned *out_length) {
    long result = (long)base_length + (long)added_measures;
    if (out_length == NULL || base_length == 0 || result < 1) {
        return false;
    }
    *out_length = (unsigned)result;
    return true;
}

bool standard_meter_hypermetric_elision(unsigned ending_length, unsigned *out_count_in_ending, unsigned *out_count_in_next) {
    if (ending_length == 0 || out_count_in_ending == NULL || out_count_in_next == NULL) {
        return false;
    }
    *out_count_in_ending = ending_length;
    *out_count_in_next = 1;
    return true;
}
