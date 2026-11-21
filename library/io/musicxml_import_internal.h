#ifndef URSATZ_IO_MUSICXML_IMPORT_INTERNAL_H
#define URSATZ_IO_MUSICXML_IMPORT_INTERNAL_H

/* What musicxml_importer.c (the entry points and the document), musicxml_import_parts.c (the element handlers) and
   musicxml_import_maps.c (events and the meter, key, tempo and marking maps) share: private to io, not staged. */

#include "io/musicxml_importer.h"
#include <common/files.h>
#include <common/hash.h>
#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezxml.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/core/provenance_reference.h"
#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "io/midi_key_signature.h"
#include "model/pitch/pitch_identity.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/pitch_spelling.h"
#include "model/pitch/tuning_system.h"
#include "model/time/duration.h"
#include "model/core/rational.h"
#include "model/time/meter_map.h"
#include "model/time/note_value.h"
#include "model/time/tempo_map.h"
#include "model/time/time_span.h"
#include "model/time/tuplet.h"
#include "model/events/dynamics.h"
#include "model/events/tempo_marking.h"
#include "model/time/metronome_marking.h"
#include "model/time/time_signature.h"
#include "model/time/musical_time.h"
#include "model/time/temporal_anchor.h"

/*
 * MusicXMLImporter: see musicxml_importer.h and docs/modules/io.md. This is the only file that includes the
 * vendored XML parser (third_party/ezxml), and it does not let one of its types out through the header.
 *
 * One pass per part, in document order. Time is an Rational count of whole notes (a duration of d
 * divisions at D divisions per quarter note is d / (4 * D)), so no step rounds. A note is collected first and its
 * NoteEvent is built last, because a tie may still lengthen it.
 */

#define MUSICXML_IMPORTER_PROVENANCE_ID "musicxml-importer-v1"

typedef struct {
    Rational onset;
    Rational duration;
    PitchLetter letter;
    int accidental;
    int octave;
    long midi;       /* the sounding pitch: 12 * (octave + 1) + the step's semitone + alter */
    size_t part;
    size_t order;    /* document order within the part */
    char voice[32];  /* the notated <voice>, or "" */
    int staff;       /* the <staff>, 1 when none */
    unsigned tuplet_actual; /* <time-modification>: actual notes in the time of normal, 0 and 0 when none */
    unsigned tuplet_normal;
    char xml_id[64]; /* the note's xml:id (a tied note's first segment, without the export's "_a"), or "" for none or one too long */
} XNote;

typedef struct {
    size_t note;     /* index into notes */
    Rational end;
} OpenTie;

typedef struct {
    Rational position;
    unsigned beats;
    int exponent;       /* the beat unit is 2^-exponent of a whole note: 0 whole, 2 quarter, 3 eighth */
    size_t group_count; /* the beat groups of an additive "3+2", else 0 for the signature's default groups */
    unsigned groups[TIME_SIGNATURE_MAX_GROUPS];
    size_t part;
} TimeEvent;

typedef struct {
    Rational position;
    int fifths;
    MusicXmlModeHint hint;
    size_t part;
} KeyEvent;

typedef struct {
    Rational position;
    unsigned long microseconds_per_quarter;
    size_t part;
} TempoEvent;

typedef struct {
    Rational position;
    size_t part;
    size_t order;
    bool has_text;
    char text[160];
    bool has_metronome;
    Rational beat_unit;
    unsigned long bpm_numerator;
    unsigned long bpm_denominator;
} MarkingEvent;

typedef struct {
    Rational position;
    size_t part;
    size_t order;
    int staff;
    char name[40];
} DynamicEvent;

typedef struct {
    MusicXmlImport *out;
    TempoEvent *tempos;
    size_t tempo_count;
    size_t tempo_capacity;
    MarkingEvent *markings;
    size_t marking_count;
    size_t marking_capacity;
    DynamicEvent *dynamics;
    size_t dynamic_count;
    size_t dynamic_capacity;
    size_t unclassified_words;
    size_t direction_order;
    Rational *part_end;       /* per part: where its last measure ends */
    TimeEvent *times;
    size_t time_count;
    size_t time_capacity;
    KeyEvent *keys;
    size_t key_count;
    size_t key_capacity;
    bool has_pickup;               /* the first part's first measure is implicit and shorter than a measure */
    Rational pickup_length;
    XNote *notes;
    size_t note_count;
    size_t note_capacity;
    OpenTie *ties;
    size_t tie_count;
    size_t tie_capacity;
    MusicXmlImportStatus status;
    /* where the reader is, for messages */
    const char *part_id;
    const char *measure_number;
    size_t current_part;
    int *part_staves;              /* per part: the largest <staves> read */
} Ctx;

/* --- building the NoteEvents --- */

typedef struct {
    Rational onset;
    size_t part;
    size_t order;
    size_t index; /* into the collected notes */
} SortItem;

/* The ids given to the events so far (by the event index), to find a repeat: open addressing over the id texts. */
typedef struct {
    size_t *slots; /* an event index plus 1, 0 for an empty slot */
    size_t capacity;
    NoteEvent *const *events;
} IdSet;

void musicxml_import_warn(Ctx *ctx, const char *format, ...);
MusicXmlImportStatus musicxml_import_fail(Ctx *ctx, MusicXmlImportStatus status, const char *format, ...);
bool musicxml_import_read_text(ezxml_t element, char *buffer, size_t size);
bool musicxml_import_read_long(ezxml_t element, long *out);
bool musicxml_import_add_note(Ctx *ctx, const XNote *note, size_t *out_index);
bool musicxml_import_process_part(Ctx *ctx, ezxml_t part, size_t part_index);
int musicxml_import_compare_items(const void *a, const void *b);
void musicxml_import_id_set_add(IdSet *set, size_t event_index);
void musicxml_import_choose_event_id(Ctx *ctx, const IdSet *set, const XNote *note, const char *part_id, char *out, size_t size);
bool musicxml_import_build_event(Ctx *ctx, const XNote *note, const char *id_text, const char *part_id, const TuningSystem *tuning, NoteEvent **out_event, PitchNumericValue **out_value);
void musicxml_import_clear_import(MusicXmlImport *import);
void musicxml_import_destroy(MusicXmlImport *import);
bool musicxml_import_build_meter_map(Ctx *ctx);
bool musicxml_import_build_key_signatures(Ctx *ctx);
bool musicxml_import_build_tempo_map(Ctx *ctx);
bool musicxml_import_build_tempo_markings(Ctx *ctx);
bool musicxml_import_build_dynamics(Ctx *ctx);

#endif
