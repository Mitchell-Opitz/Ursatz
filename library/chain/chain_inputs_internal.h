#ifndef URSATZ_CHAIN_CHAIN_INPUTS_INTERNAL_H
#define URSATZ_CHAIN_CHAIN_INPUTS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "chain/chain_inputs.h"
#include "model/core/rational.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/time_span.h"

/* Internal to the chain's input builders (one file per stage, chain_inputs_*.c): what they share. Not staged. */

/* An id and the index of the note that has it; sorted by id with chain_compare_keys, for bsearch. */
typedef struct {
    const char *id;
    size_t index;
} NoteKey;

int chain_compare_keys(const void *a, const void *b);

/* The exact time as a Rational. False when it does not fit. */
bool chain_rational_of(const MusicalTime *time, Rational *out);

/* The MeterMap measure holding the time, or -1. */
int chain_measure_of(const MeterMap *map, const MusicalTime *time);

/* A scale degree label of the outer scale degrees ("3", "b6", "#4"): *degree 1..7 and *alteration -1, 0 or 1. False for NULL or a label
   that is not one. */
bool chain_parse_degree(const char *label, int *degree, int *alteration);

/* Keeps a span [start, end) in a build's array of spans (the array grows from 256), or NULL on failure; chain_release_spans destroys
   the spans a build kept and the array. */
const TimeSpan *chain_keep_span(TimeSpan ***spans, size_t *count, size_t *capacity, const MusicalTime *start, const MusicalTime *end);
void chain_release_spans(TimeSpan **spans, size_t count);

#endif
