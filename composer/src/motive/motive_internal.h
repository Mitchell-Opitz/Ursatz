#ifndef URSATZ_COMPOSER_MOTIVE_INTERNAL_H
#define URSATZ_COMPOSER_MOTIVE_INTERNAL_H

#include <stdbool.h>

#include <cJSON.h>
#include <ursatz_composer/motive.h>

/* What an input says about the motive besides its notes. */
typedef struct {
    bool has_meter;
    PlanMeter meter;
    /* The input declares the meter (a file's time signature, a JSON "meter"), as opposed to Ursatz's 4/4 default. */
    bool meter_declared;
    bool has_key;
    PlanKey key;
    /* A JSON "form" and "pickup"; the other inputs have neither, and the caller's options can set the pickup. */
    MotiveForm form;
    bool has_pickup;
    PlanTime pickup;
} MotiveDocument;

/* Records the first problem only. error may be NULL. */
void motive_set_error(MotiveError *error, const char *location, const char *format, ...);

PlanTime motive_time(long num, unsigned long den);
PlanTime motive_time_plus(PlanTime a, PlanTime b);
PlanTime motive_time_minus(PlanTime a, PlanTime b);

/* The pickup of a motive, or zero when it is unset (a zero-initialized Motive has a denominator of 0). */
PlanTime motive_pickup(const Motive *motive);

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
/* A degree list: the key for the pitches is the document's (the options' key), else C major. */
MotiveResult motive_parse_degrees_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error);
MotiveResult motive_parse_json_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error);

/* JSON building shared with the analysis document. The motive as an object (see motive_to_json); NULL on failure. */
cJSON *motive_json_object(const Motive *motive);
/* {"num", "den"}. */
cJSON *motive_time_json(PlanTime time);
/* Prints document as the project's JSON text and deletes it. NULL on failure; caller frees the text. */
char *motive_json_text(cJSON *document);

/* The key of a MIDI key signature count of sharps (negative for flats) and mode. */
PlanKey motive_key_from_signature(int sharps_flats, PlanKeyMode mode);

/*
 * The step of the key's scale a MIDI pitch is on (7 to an octave, 0 the tonic's pitch class), and whether it is a
 * raised degree (1) or on the degree (0); and the pitch at a step. A pitch outside the scale is on the degree below
 * it, raised. The pitch is not range-checked.
 */
void motive_scale_position(const PlanKey *key, int pitch, long *position, int *alteration);
int motive_position_pitch(const PlanKey *key, long position, int alteration);

/* The files, filling a motive's notes and a document. */
MotiveResult motive_read_midi(const unsigned char *data, size_t size, Motive *motive, MotiveDocument *document,
                              MotiveError *error);
MotiveResult motive_read_musicxml(const char *data, size_t size, Motive *motive, MotiveDocument *document,
                                  MotiveError *error);

#endif
