#include "repeat_navigation.h"

#include <stdlib.h>

NavigationOptions standard_markings_default_navigation_options(void) {
    NavigationOptions options;
    options.take_repeats_after_jump = false;
    return options;
}

static bool has_marker_at(const NavigationMarker *markers, size_t count, size_t measure, StructuralKind kind) {
    for (size_t i = 0; i < count; i++) {
        if (markers[i].measure == measure && markers[i].kind == kind) {
            return true;
        }
    }
    return false;
}

/* Measure of the first marker of kind, or false. */
static bool find_measure(const NavigationMarker *markers, size_t count, StructuralKind kind, size_t *out) {
    bool found = false;
    size_t best = 0;
    for (size_t i = 0; i < count; i++) {
        if (markers[i].kind == kind && (!found || markers[i].measure < best)) {
            best = markers[i].measure;
            found = true;
        }
    }
    if (found) {
        *out = best;
    }
    return found;
}

/* Start of the repeated section that a repeat-end at `end_measure` closes. */
static size_t section_start(const NavigationMarker *markers, size_t count, size_t end_measure) {
    bool found = false;
    size_t start = 0;
    for (size_t i = 0; i < count; i++) {
        if (markers[i].kind == STRUCTURAL_KIND_REPEAT_START && markers[i].measure <= end_measure &&
            (!found || markers[i].measure > start)) {
            start = markers[i].measure;
            found = true;
        }
    }
    return found ? start : 0;
}

/* Measure of the ending numbered `number` that starts after `after`, or false. */
static bool find_ending(const NavigationMarker *markers, size_t count, unsigned int number, size_t after,
                        size_t *out) {
    bool found = false;
    size_t best = 0;
    for (size_t i = 0; i < count; i++) {
        if (markers[i].kind == STRUCTURAL_KIND_ENDING && markers[i].parameter == number &&
            markers[i].measure > after && (!found || markers[i].measure < best)) {
            best = markers[i].measure;
            found = true;
        }
    }
    if (found) {
        *out = best;
    }
    return found;
}

/* Last measure belonging to the ending `number` that starts at `start`. */
static size_t ending_end(const NavigationMarker *markers, size_t count, size_t measure_count, unsigned int number,
                         size_t start) {
    size_t next;
    if (find_ending(markers, count, number + 1, start, &next)) {
        return next - 1;
    }
    /* The last ending: runs until a new repeated section begins, else to
       the end of the work. */
    size_t end = measure_count - 1;
    for (size_t i = 0; i < count; i++) {
        bool new_section = markers[i].kind == STRUCTURAL_KIND_REPEAT_START ||
                           (markers[i].kind == STRUCTURAL_KIND_ENDING && markers[i].parameter == 1);
        if (new_section && markers[i].measure > start && markers[i].measure - 1 < end) {
            end = markers[i].measure - 1;
        }
    }
    return end;
}

/* Highest ending number in the group containing the ending `number` at `start`. */
static unsigned int ending_group_max(const NavigationMarker *markers, size_t count, unsigned int number,
                                     size_t start) {
    unsigned int max = number;
    size_t at = start;
    size_t next;
    while (find_ending(markers, count, max + 1, at, &next)) {
        max++;
        at = next;
    }
    return max;
}

static bool ending_at(const NavigationMarker *markers, size_t count, size_t measure, unsigned int *out_number) {
    for (size_t i = 0; i < count; i++) {
        if (markers[i].measure == measure && markers[i].kind == STRUCTURAL_KIND_ENDING) {
            *out_number = markers[i].parameter;
            return true;
        }
    }
    return false;
}

NavigationResult standard_markings_playback_order(const NavigationMarker *markers, size_t marker_count,
                                                  size_t measure_count, const NavigationOptions *options,
                                                  size_t *out_order, size_t out_capacity, size_t *out_length) {
    if ((markers == NULL && marker_count > 0) || out_length == NULL || (out_order == NULL && out_capacity > 0)) {
        return NAVIGATION_ERR_NULL_ARGUMENT;
    }
    *out_length = 0;
    if (measure_count == 0) {
        return NAVIGATION_OK;
    }

    for (size_t i = 0; i < marker_count; i++) {
        if (markers[i].measure >= measure_count) {
            return NAVIGATION_ERR_MARKER_OUT_OF_RANGE;
        }
        if (!standard_markings_marker_well_formed(markers[i].kind, markers[i].parameter)) {
            return NAVIGATION_ERR_MALFORMED_MARKER;
        }
    }

    NavigationOptions effective = options != NULL ? *options : standard_markings_default_navigation_options();

    bool *taken = (bool *)calloc(marker_count > 0 ? marker_count : 1, sizeof(bool));
    if (taken == NULL) {
        return NAVIGATION_ERR_ALLOC;
    }

    size_t position = 0;
    unsigned int pass = 1;
    bool jumped = false;
    bool came_from_repeat = false;
    StructuralStop stop = STRUCTURAL_STOP_NONE;
    size_t steps = 0;
    size_t step_limit = 16 * (measure_count + 1) * (marker_count + 1);
    NavigationResult result = NAVIGATION_OK;

    while (position < measure_count) {
        if (++steps > step_limit) {
            result = NAVIGATION_ERR_RUNAWAY;
            break;
        }

        if (has_marker_at(markers, marker_count, position, STRUCTURAL_KIND_REPEAT_START) && !came_from_repeat) {
            pass = 1;
        }
        came_from_repeat = false;

        unsigned int ending_number;
        if (ending_at(markers, marker_count, position, &ending_number)) {
            unsigned int group_max = ending_group_max(markers, marker_count, ending_number, position);
            bool play;
            if (jumped && !effective.take_repeats_after_jump) {
                play = ending_number == group_max;
            } else {
                unsigned int wanted = pass < group_max ? pass : group_max;
                play = ending_number == wanted;
            }
            if (!play) {
                position = ending_end(markers, marker_count, measure_count, ending_number, position) + 1;
                continue;
            }
        }

        if (*out_length >= out_capacity) {
            result = NAVIGATION_ERR_BUFFER_TOO_SMALL;
            break;
        }
        out_order[(*out_length)++] = position;

        /* End-of-measure actions. */
        if (jumped && stop == STRUCTURAL_STOP_AT_FINE &&
            has_marker_at(markers, marker_count, position, STRUCTURAL_KIND_FINE)) {
            break;
        }
        if (jumped && stop == STRUCTURAL_STOP_AT_CODA &&
            has_marker_at(markers, marker_count, position, STRUCTURAL_KIND_TO_CODA)) {
            size_t coda;
            if (!find_measure(markers, marker_count, STRUCTURAL_KIND_CODA, &coda)) {
                result = NAVIGATION_ERR_MISSING_CODA;
                break;
            }
            position = coda;
            stop = STRUCTURAL_STOP_NONE; /* now past the to-coda sign for good */
            continue;
        }

        if (!jumped || effective.take_repeats_after_jump) {
            bool repeated = false;
            for (size_t i = 0; i < marker_count; i++) {
                if (markers[i].kind == STRUCTURAL_KIND_REPEAT_END && markers[i].measure == position && !taken[i]) {
                    taken[i] = true;
                    pass++;
                    position = section_start(markers, marker_count, position);
                    came_from_repeat = true;
                    repeated = true;
                    break;
                }
            }
            if (repeated) {
                continue;
            }
        }

        if (!jumped) {
            bool did_jump = false;
            for (size_t i = 0; i < marker_count; i++) {
                if (markers[i].measure != position) {
                    continue;
                }
                StructuralJump target = standard_markings_jump_target(markers[i].kind);
                if (target == STRUCTURAL_JUMP_NONE) {
                    continue;
                }
                size_t destination = 0;
                if (target == STRUCTURAL_JUMP_TO_SEGNO &&
                    !find_measure(markers, marker_count, STRUCTURAL_KIND_SEGNO, &destination)) {
                    result = NAVIGATION_ERR_MISSING_SEGNO;
                    break;
                }
                size_t coda_measure;
                if (standard_markings_jump_stop(markers[i].kind) == STRUCTURAL_STOP_AT_CODA &&
                    !find_measure(markers, marker_count, STRUCTURAL_KIND_CODA, &coda_measure)) {
                    result = NAVIGATION_ERR_MISSING_CODA;
                    break;
                }
                jumped = true;
                stop = standard_markings_jump_stop(markers[i].kind);
                position = destination;
                if (effective.take_repeats_after_jump) {
                    for (size_t j = 0; j < marker_count; j++) {
                        taken[j] = false;
                    }
                    pass = 1;
                    came_from_repeat = false;
                }
                did_jump = true;
                break;
            }
            if (result != NAVIGATION_OK) {
                break;
            }
            if (did_jump) {
                continue;
            }
        }

        position++;
    }

    free(taken);
    return result;
}

static bool source_of(const NavigationMarker *markers, size_t count, size_t measure, size_t depth,
                      size_t *out_source) {
    /* Each hop consumes one measure repeat sign, so a longer chain than there
       are markers cannot be well founded. */
    if (depth > count) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        unsigned int span = standard_markings_measure_repeat_span(markers[i].kind);
        if (span == 1 && markers[i].measure == measure) {
            if (measure < 1) {
                return false;
            }
            return source_of(markers, count, measure - 1, depth + 1, out_source);
        }
        if (span == 2 && (markers[i].measure == measure || markers[i].measure + 1 == measure)) {
            if (markers[i].measure < 2) {
                return false;
            }
            /* m plays m - 2; m + 1 plays m - 1 */
            size_t from = measure - 2;
            return source_of(markers, count, from, depth + 1, out_source);
        }
    }
    *out_source = measure;
    return true;
}

bool standard_markings_measure_content_source(const NavigationMarker *markers, size_t marker_count,
                                              size_t measure, size_t *out_source) {
    if ((markers == NULL && marker_count > 0) || out_source == NULL) {
        return false;
    }
    return source_of(markers, marker_count, measure, 0, out_source);
}
