#ifndef URSATZ_ANALYZER_NOTATION_READING_H
#define URSATZ_ANALYZER_NOTATION_READING_H

#include <stdbool.h>

/*
 * A notation reading: how the notes of an input are to be read as notation, when the file's own meter or note
 * values are not the ones a score would write. It is applied to the imported notes before analysis (see
 * notation_reading_apply); nothing applies one by itself.
 *
 * has_meter: beats/beat_type is the meter to read the piece in (replacing the file's). has_pickup: the first
 * measure is a pickup of pickup_numerator/pickup_denominator whole notes, counted after scaling and shorter than
 * one measure of the meter. scale: every start and duration is multiplied by scale_numerator/scale_denominator;
 * 1/1 leaves the notes unchanged. The allowed scales are 1/4, 1/2, 1, 2 and 4.
 */
typedef struct {
    bool has_meter;
    int beats;
    int beat_type;
    bool has_pickup;
    long pickup_numerator;
    unsigned long pickup_denominator;
    long scale_numerator;
    unsigned long scale_denominator;
} NotationReading;

/* The reading that changes nothing: no meter, no pickup, scale 1/1. */
NotationReading notation_reading_none(void);

/*
 * Each parser reads the whole text ("3/4", "2", "1/2", "1/4") and on success sets only its own fields of *r,
 * returning true. On false *r is untouched. The meter's beat type must be 1, 2, 4, 8 or 16 and its beats 1 to 64;
 * the scale one of the allowed scales; the pickup a positive fraction.
 */
bool notation_reading_parse_meter(const char *text, NotationReading *r);
bool notation_reading_parse_scale(const char *text, NotationReading *r);
bool notation_reading_parse_pickup(const char *text, NotationReading *r);

#endif
