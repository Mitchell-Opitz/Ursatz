#ifndef URSATZ_COMPOSER_MOTIVE_INTERNAL_H
#define URSATZ_COMPOSER_MOTIVE_INTERNAL_H

#include <stdbool.h>

#include <ursatz_composer/motive.h>

/* What an input says about the motive besides its notes. */
typedef struct {
    bool has_meter;
    PlanMeter meter;
    /* The input declares the meter (a file's time signature, a JSON "meter"), as opposed to Ursatz's 4/4 default. */
    bool meter_declared;
    bool has_key;
    PlanKey key;
} MotiveDocument;

/* Records the first problem only. error may be NULL. */
void motive_set_error(MotiveError *error, const char *location, const char *format, ...);

PlanTime motive_time(long num, unsigned long den);
PlanTime motive_time_plus(PlanTime a, PlanTime b);
PlanTime motive_time_minus(PlanTime a, PlanTime b);

/* A note's end. */
PlanTime motive_note_end(const PlanNote *note);

/*
 * Checks the notes of motive: at least one, pitch 0 to 127, a positive duration, in order and not overlapping.
 * Writes where the last one ends to *length.
 */
MotiveResult motive_check_notes(const Motive *motive, PlanTime *length, MotiveError *error);

/*
 * Seals the notes (motive_check_notes, then sets the length), then settles the meter and the key from what the
 * input said and what the caller asked for (see motive.h).
 */
MotiveResult motive_complete(Motive *motive, const MotiveDocument *document, const MotiveOptions *options,
                             MotiveError *error);

/* The text inputs, each filling a motive's notes and a document. */
MotiveResult motive_parse_notes_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error);
MotiveResult motive_parse_json_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error);

/* The key of a MIDI key signature count of sharps (negative for flats) and mode. */
PlanKey motive_key_from_signature(int sharps_flats, PlanKeyMode mode);

/* The files, filling a motive's notes and a document. */
MotiveResult motive_read_midi(const unsigned char *data, size_t size, Motive *motive, MotiveDocument *document,
                              MotiveError *error);
MotiveResult motive_read_musicxml(const char *data, size_t size, Motive *motive, MotiveDocument *document,
                                  MotiveError *error);

#endif
