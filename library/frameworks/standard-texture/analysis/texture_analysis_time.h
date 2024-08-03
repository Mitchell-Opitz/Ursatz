#ifndef URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_TIME_H
#define URSATZ_FRAMEWORKS_STANDARD_TEXTURE_ANALYSIS_TEXTURE_ANALYSIS_TIME_H

#include <stdbool.h>

#include "texture_analysis_input.h"

/* Internal to the texture adapter: exact times (a rational of whole notes) and the measure arithmetic over a MeterMap. */

typedef struct {
    long long n;
    long long d; /* positive */
} StandardTextureQ;

StandardTextureQ standard_texture_q(long long n, long long d);
int standard_texture_q_compare(StandardTextureQ a, StandardTextureQ b);
StandardTextureQ standard_texture_q_add(StandardTextureQ a, StandardTextureQ b);
StandardTextureQ standard_texture_q_sub(StandardTextureQ a, StandardTextureQ b);
double standard_texture_q_double(StandardTextureQ a);

bool standard_texture_span_q(const TimeSpan *span, StandardTextureQ *start, StandardTextureQ *end);

/* The time snapped per the configuration (an attack, or a release with the wider tolerance). */
StandardTextureQ standard_texture_snap(const StandardTextureConfig *config, StandardTextureQ time, bool release);

bool standard_texture_measure_at(const MeterMap *map, StandardTextureQ time, int *measure);
bool standard_texture_measure_bounds(const MeterMap *map, int index, StandardTextureQ *start, StandardTextureQ *end);
/* The last measure a span ending at `end` touches: the measure of `end`, or the one before when `end` is a barline. */
bool standard_texture_last_measure_touched(const MeterMap *map, StandardTextureQ end, int *measure);
/* The beat length at measure `index`: the unit of its time signature, or three units in a compound meter (a top number of 6, 9 or
   12 over an eighth or smaller unit). */
bool standard_texture_beat_length(const MeterMap *map, int index, StandardTextureQ *beat);
/* A new left-closed TimeSpan [start, end) the caller destroys, or NULL. */
TimeSpan *standard_texture_make_span(StandardTextureQ start, StandardTextureQ end);

#endif
