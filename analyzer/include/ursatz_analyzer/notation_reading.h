#ifndef URSATZ_ANALYZER_NOTATION_READING_H
#define URSATZ_ANALYZER_NOTATION_READING_H

#include <stdbool.h>

typedef struct ImportedNotes ImportedNotes;

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

typedef enum {
    NOTATION_READING_OK = 0,
    NOTATION_READING_ERR_NULL_ARGUMENT,
    NOTATION_READING_ERR_INVALID, /* a scale that is not allowed, a meter out of range, a pickup not shorter than a measure */
    NOTATION_READING_ERR_OVERFLOW, /* a scaled time does not fit */
    NOTATION_READING_ERR_ALLOC
} NotationReadingResult;

/*
 * Applies reading to notes in place. Every NoteEvent is rebuilt with its start and duration times the scale, keeping
 * its EntityID, pitch, spelling, voice and provenance, so note ids (and every list of them) stay valid; pitch_values,
 * streams and the parts are untouched. The key signature events, meter.end_time, the tempo markings and the dynamics
 * are scaled with the notes.
 *
 * Meter: with has_meter, meter.meter_map becomes one region of the reading's meter, preceded by a pickup region when
 * has_pickup. Without it the existing map is kept with its change points scaled; with has_pickup alone the pickup is
 * laid before the map's first full measure's signature, and its later changes are dropped. The reading is recorded in
 * notes->source (reading_applied, reading).
 *
 * On any result other than NOTATION_READING_OK the notes are unchanged.
 */
NotationReadingResult notation_reading_apply(ImportedNotes *notes, const NotationReading *reading);

#endif
