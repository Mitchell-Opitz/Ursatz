#ifndef URSATZ_THEORY_KNOWLEDGE_INSTRUMENT_H
#define URSATZ_THEORY_KNOWLEDGE_INSTRUMENT_H

#include <stdbool.h>
#include <stddef.h>

/*
 * K12 instrument and ensemble template (target architecture 5.2, R-603): what a performer part needs to know about an instrument,
 * and a named set of instrument slots. The `instrument` kind has two subtypes, `instrument` and `ensemble`. An Instrument holds the
 * family, the transposition (sounding minus written, in semitones), the default clef of each staff (so `staff_count` is the
 * number of clefs), the sounding range in MIDI numbers, an optional comfortable range, the General MIDI program (numbered from 0)
 * and whether it has a sustain pedal. An EnsembleTemplate lists slots; each names its instrument by id, a part name and the number
 * of players. A snapshot owns both; the strings and the slots live as long as the snapshot.
 */
typedef enum {
    INSTRUMENT_CLEF_TREBLE,
    INSTRUMENT_CLEF_BASS,
    INSTRUMENT_CLEF_ALTO,
    INSTRUMENT_CLEF_TENOR
} InstrumentClef;

#define INSTRUMENT_STAVES_MAX 4

typedef struct {
    const char *id;
    const char *name;
    const char *family;
    int transposition;
    int staff_count;
    InstrumentClef clefs[INSTRUMENT_STAVES_MAX];
    int lowest;
    int highest;
    bool has_comfortable_range;
    int comfortable_lowest;
    int comfortable_highest;
    int general_midi_program;
    bool sustain_pedal;
} Instrument;

typedef struct {
    const char *instrument; /* the id of an instrument entry */
    const char *name;
    int players;
} EnsembleSlot;

typedef struct {
    const char *id;
    const char *name;
    const EnsembleSlot *slots;
    size_t slot_count;
} EnsembleTemplate;

/* True when `midi` lies from the instrument's lowest to its highest sounding note, both included. False for a NULL instrument. */
bool instrument_in_range(const Instrument *instrument, int midi);

#endif
