#ifndef URSATZ_COMPOSER_MOTIVE_INTERNAL_H
#define URSATZ_COMPOSER_MOTIVE_INTERNAL_H

#include <stdbool.h>
#include <stdlib.h>

#include <cJSON.h>
#include <analysis/motive_relation.h>
#include <analysis/motive_sketch.h>
#include <core/rational.h>
#include <ursatz_composer/motive.h>
#include <ursatz_composer/motive_develop.h>

#define MOTIVE_HARMONY_TEXT_MAX 256

/* What an input says about the motive besides its notes. */
typedef struct {
    bool has_meter;
    PlanMeter meter;
    /* The input declares the meter (a file's time signature, a JSON "meter"), as opposed to Ursatz's 4/4 default. */
    bool meter_declared;
    bool has_key;
    AnalysisKey key;
    /* A JSON "form" and "pickup"; the other inputs have neither, and the caller's options can set the pickup. */
    MotiveSketchForm form;
    bool has_pickup;
    Rational pickup;
    /* A JSON "harmony" (text, read once the meter is settled), "features" and "role". */
    bool has_harmony_text;
    char harmony_text[MOTIVE_HARMONY_TEXT_MAX];
    bool has_features;
    MotiveSketchFeature features[MOTIVE_MAX_FEATURES];
    size_t feature_count;
    bool has_role;
    bool accompaniment;
} MotiveDocument;

/*
 * The motive of a motive node's Given or Realized melody lane span: its notes, and the meter and key of the lanes over
 * it, else the context's. False when the node has no such span. Shared with the motive-form operator.
 */
bool motive_develop_read_node(const PiecePlan *plan, NodeId node, const MotiveDevelopContext *context, Motive *out);

/* Records the first problem only. error may be NULL. */
void motive_set_error(MotiveError *error, const char *location, const char *format, ...);

/*
 * A MotiveScore from num/den in lowest terms (Rational). 0/0 stays 0/0, a score with nothing to compare. False when a
 * part does not fit a Rational or the denominator is 0 under a non-zero numerator; *out is then untouched.
 */
bool motive_score_make(unsigned long long numerator, unsigned long long denominator, MotiveScore *out);
bool motive_score_from_rational(Rational value, MotiveScore *out);
Rational motive_score_rational(MotiveScore score);

Rational motive_time(long num, unsigned long den);
Rational motive_time_plus(Rational a, Rational b);
Rational motive_time_minus(Rational a, Rational b);

/* The pickup of a motive, or zero when it is unset (a zero-initialized Motive has a denominator of 0). */
Rational motive_pickup(const Motive *motive);


/*
 * The harmonic role of every note of the motive against the given chords (the library's motive_tone_roles): out has
 * motive->note_count entries. overrides are 0-based note indices with their own role. Returns the library's status:
 * MOTIVE_TONE_NO_SCALE for a motive with no key or no pitches, MOTIVE_TONE_INVALID when the motive with those chords is
 * not a valid sketch.
 */
MotiveToneStatus motive_tone_labels(const Motive *motive, const MotiveSketchChord *chords, size_t chord_count,
                                    const MotiveToneOverride *overrides, size_t override_count, MotiveToneLabel *out);

/*
 * The best implied chord at 0 of a full motive with a key (the triad holding the most notes, then the most time, the
 * first of I V IV vi ii iii vii on a tie): what a motive without harmony of its own is read as. False for a motive that
 * is not valid, has no key, is not a full motive, or holds no chord tone.
 */
bool motive_implied_chord(const Motive *motive, MotiveSketchChord *out);

/*
 * The motive a library sketch result makes: the template with the notes, meter, pickup, harmony, features and role of
 * the sketch, and its scale when it moved (marked given), the length where the last note ends. The template's form, key
 * source and tone labels stay. False, with *out untouched, for a sketch the Composer cannot hold: not valid, a meter it
 * does not have, a time past a billion, a pickup off the thirty-second grid, notes that fail motive_check_notes. out may
 * be the template.
 */
bool motive_overlay_sketch(const MotiveSketch *developed, const Motive *template_motive, Motive *out);

/*
 * The library relation (analysis/motive_relation.h) applied to a motive: the variant is the motive with the notes, meter,
 * pickup, harmony, features, role and (for the dominant form) key the relation gives, its length where the last note
 * ends. The motive's form, key source and tone labels stay. The library's refusals keep their meaning as MOTIVE_DEV_*:
 * OK, UNCHANGED, RANGE, NOT_EXPRESSIBLE; NO_SCALE is NO_KEY, INVALID is INVALID, any other is NOT_APPLICABLE. A result
 * the Composer cannot hold (a time past a billion, a pickup off the thirty-second grid, a meter it does not have) is
 * NOT_EXPRESSIBLE. A full motive with a key and no harmony of its own is given its best implied chord at 0 for the
 * relation; the result leaves that chord out when it is the best implied chord of the result's notes, unless keep_implied
 * (the harmony rows, whose chords are the variant's own). *out is written only on OK. applied, when not NULL,
 * gets the library's own result (to tell NO_FEATURE).
 */
MotiveDevResult motive_relation_develop(const Motive *source, const MotiveRelationStep *step, bool keep_implied,
                                        Motive *out, MotiveRelationResult *applied);

/* The end of a motive note. */
Rational motive_event_end(const MotiveSketchNote *note);



/*
 * Checks the notes of motive: at least one, pitch 0 to 127, a positive duration, in order and not overlapping.
 * Writes where the last one ends to *length.
 */
MotiveResult motive_check_notes(const Motive *motive, Rational *length, MotiveError *error);

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
/* A rhythm ("x:q x:e r:e") and a contour ("0 2 1 3"); both set the document's form, and take the key as a degree list does. */
MotiveResult motive_parse_rhythm_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error);
MotiveResult motive_parse_contour_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error);
MotiveResult motive_parse_json_text(const char *text, Motive *motive, MotiveDocument *document, MotiveError *error);

/* JSON building shared with the analysis document. The motive as an object (see motive_to_json); NULL on failure. */
cJSON *motive_json_object(const Motive *motive);
/* Prints document as the project's JSON text and deletes it. NULL on failure; caller frees the text. */
char *motive_json_text(cJSON *document);

/* The key of a MIDI key signature count of sharps (negative for flats) and mode. */
AnalysisKey motive_key_from_signature(int sharps_flats, KeyStructureMode mode);

/*
 * The step of the key's scale a MIDI pitch is on (7 to an octave, 0 the tonic's pitch class), and whether it is a
 * raised degree (1) or on the degree (0); and the pitch at a step. A pitch outside the scale is on the degree below
 * it, raised. The pitch is not range-checked.
 */
void motive_scale_position(const AnalysisKey *key, int pitch, long *position, int *alteration);
int motive_position_pitch(const AnalysisKey *key, long position, int alteration);

/*
 * Checks the motive's own harmony and features: chords ascending inside the length, degree 1 to 7, alteration -1 to 1,
 * inversion 0 to 3 (3 only on a seventh); features with a label of 1 to 8 characters, distinct, inside the notes.
 */
MotiveResult motive_check_extras(const Motive *motive, MotiveError *error);

/* The files, filling a motive's notes and a document. */
MotiveResult motive_read_midi(const unsigned char *data, size_t size, Motive *motive, MotiveDocument *document,
                              MotiveError *error);
MotiveResult motive_read_musicxml(const char *data, size_t size, Motive *motive, MotiveDocument *document,
                                  MotiveError *error);

/*
 * standard-counterpoint's hard melodic rules on the motive, as one line (the clausula vera is for strict species and is
 * left out). False when the check itself could not run; otherwise true, with *fault the name of the first hard rule
 * broken, or NULL when none is.
 */
bool motive_melodic_fault(const Motive *motive, const char **fault);

/* Frees a candidate payload that is one allocation. */
static inline void motive_payload_destroy(void *payload) {
    free(payload);
}

#endif
