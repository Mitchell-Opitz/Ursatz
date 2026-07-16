#ifndef MUSIC_EXPORT_SCORE_QUANTIZE_H
#define MUSIC_EXPORT_SCORE_QUANTIZE_H

#include <stdbool.h>
#include <stddef.h>

#include "events/note_event.h"
#include "time/meter_map.h"

/*
 * ScoreQuantize (music-export): turns the played times of notes into notated times on the beat grid of a MeterMap,
 * for the score export. Nothing imported or analyzed is changed: the notated times are a separate result, parallel to
 * the events, and only musicxml_export_write_score reads them (MusicXmlExportScore.quantize).
 *
 * All times are whole notes, exact, in lowest terms. For each beat of the meter (the beat of the region the onset is
 * in; a compound meter such as 6/8 uses the dotted beat) the module picks the grid with the lower total snap error of
 * the onsets in that beat, from the straight grid (1/grid_denominator of a whole note) and, when allowed, the triplet
 * grids of the beat (a third and a sixth of it); a tie goes to the straight grid. Onsets and ends snap to the nearest
 * point of that grid and a note is never shorter than one grid step. A small gap after a note, up to gap_absorb times
 * the note's own notated length and always less than a beat, is absorbed so the note runs on to the next onset in its
 * voice. Notes that end up with the same start and end in one voice are a chord, which the export already writes.
 */

typedef struct {
    unsigned long grid_denominator;      /* the straight grid is 1/grid_denominator of a whole note */
    bool allow_triplets;                 /* also consider the triplet grids of each beat */
    unsigned long gap_absorb_numerator;  /* a gap up to this fraction of the note's notated length is absorbed */
    unsigned long gap_absorb_denominator;
} ScoreQuantizeOptions;

/* A 32nd note grid, triplets allowed, gaps up to a quarter of the note's own length absorbed. */
ScoreQuantizeOptions score_quantize_options_default(void);

typedef enum {
    SCORE_QUANTIZE_OK = 0,
    SCORE_QUANTIZE_ERR_NULL_ARG,
    SCORE_QUANTIZE_ERR_ALLOC,
    SCORE_QUANTIZE_ERR_NOT_REPRESENTABLE /* a negative position, a zero duration, a bad option, an overflow */
} ScoreQuantizeResult;

/* The notated times, parallel to the events: positions in whole notes as numerator / denominator, reduced. */
typedef struct {
    size_t count;
    unsigned long long *start_num;
    unsigned long long *start_den;
    unsigned long long *end_num;
    unsigned long long *end_den;
} ScoreQuantized;

/* Frees the arrays and zeroes the struct. Safe on a zeroed struct and on NULL. */
void score_quantized_destroy(ScoreQuantized *quantized);

/*
 * Quantizes count events. part_index and staff are optional and parallel to events (NULL means part 0 and staff 1);
 * with the event's VoiceReference they give the voice group. meter_map may be NULL (a defaulted 4/4 is used) and
 * options may be NULL (the defaults). On OK, *out is filled and the caller calls score_quantized_destroy; on any other
 * result *out is zeroed.
 */
ScoreQuantizeResult score_quantize(NoteEvent *const *events, size_t count, const size_t *part_index, const int *staff,
                                   const MeterMap *meter_map, const ScoreQuantizeOptions *options,
                                   ScoreQuantized *out);

#endif
