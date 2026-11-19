#ifndef URSATZ_READING_READING_H
#define URSATZ_READING_READING_H

#include <stdbool.h>
#include <stddef.h>

#include "analysis/harmony/chord_reading.h"
#include "model/core/rational.h"
#include "model/pitch/pitch_material.h"
#include "model/pitch/region.h"
#include "realization/line.h"
#include "realization/realization.h"
#include "theory/vocabulary/cadence_type.h"

/*
 * A reading (R-629, spec 4.7): what one reader says about one realization, as claims over it. The reader is the Analyzer, the Composer
 * (its own hearing) or a person; the realization is named by its work, its movement and its digest (a hash of the text of that
 * version of it, realization_digest). A reading holds claims of eight kinds: regions (a key), chords, cadences, groups (sections,
 * themes, phrases, subphrases and ideas of the form), motive occurrences, meters, lines and voice faults. Every claim has an id
 * unique in the reading, a span, a confidence (absent when the reader gives none) and the ids of the notes it rests on (its
 * evidence, performer note ids of the realization: none for a person's reading that names none). A claim made from a chain run
 * keeps the index of the element it came from (`detail`) so the analysis document can be printed from the reading.
 *
 * A reading keeps copies of every claim it is given and of each claim's evidence. A pointer read back stays valid until the reading is
 * destroyed. Material and chord types are borrowed from the knowledge snapshot, which has to outlive the reading.
 */

#define READING_NO_DETAIL ((size_t)-1)
#define READING_NO_PARENT ((size_t)-1)

typedef enum {
    READING_SOURCE_ANALYZER,
    READING_SOURCE_COMPOSER,
    READING_SOURCE_PERSON
} ReadingSourceKind;

/* Who read: a name, the producer key of the program ("" for a person), the stages of the chain that ran (0 for a person) and the hash of
   the knowledge snapshot it read with ("" for a person). */
typedef struct {
    ReadingSourceKind kind;
    char name[64];
    char producer_key[17];
    unsigned long stages;
    char knowledge_hash[17];
} ReadingSource;

/* The realization the reading is about; `digest` is 16 lowercase hexadecimal digits, "" while it is not known. */
typedef struct {
    char work[REALIZATION_ID_MAX];
    char movement[REALIZATION_ID_MAX];
    char digest[17];
} ReadingTarget;

typedef struct {
    bool present;
    Rational value; /* from 0 to 1 */
} ReadingConfidence;

typedef struct {
    char (*ids)[REALIZATION_ID_MAX]; /* performer note ids */
    size_t count;
} ReadingEvidence;

/* The members every claim begins with. */
#define READING_CLAIM_FIELDS         \
    char id[REALIZATION_ID_MAX];     \
    Rational start;                  \
    Rational end;                    \
    ReadingConfidence confidence;    \
    ReadingEvidence evidence;        \
    size_t detail /* the index of the chain result's element, READING_NO_DETAIL for none */

typedef struct {
    READING_CLAIM_FIELDS;
} ReadingClaim;

typedef struct {
    READING_CLAIM_FIELDS;
    int center; /* 0 to 11 */
    const PitchMaterial *material;
    RegionStrength strength;
    char label[32];
} ReadingRegion;

typedef struct {
    READING_CLAIM_FIELDS;
    bool has_reading;
    ChordReading reading; /* the chord, when has_reading; its sonority span is the claim's */
    char numeral[16];     /* "" when the chord is not in a region */
    char function[16];
    char label[32];
} ReadingChord;

typedef struct {
    READING_CLAIM_FIELDS;
    CadenceType type;
    Rational arrival;
    CadenceType alternatives[4];
    size_t alternative_count;
} ReadingCadence;

typedef enum {
    READING_GROUP_SECTION,
    READING_GROUP_THEME,
    READING_GROUP_PHRASE,
    READING_GROUP_SUBPHRASE,
    READING_GROUP_IDEA
} ReadingGroupKind;

typedef struct {
    READING_CLAIM_FIELDS;
    ReadingGroupKind kind;
    size_t parent; /* the index of an earlier group, READING_NO_PARENT for a top level one */
    char label[32];
    char form[24];
    char beginning[32];
    char ending[32];
    char commonality[24]; /* "" when none */
    int first_measure;
    int last_measure;
    size_t phrase_count;
    CadenceType part_cadences[2];
    size_t part_cadence_count;
    bool archetypal;
    bool fragmentation;
} ReadingGroup;

typedef struct {
    READING_CLAIM_FIELDS;
    size_t motive; /* the motive's index in the chain result */
    char kind[24];
} ReadingMotiveOccurrence;

typedef struct {
    READING_CLAIM_FIELDS;
    char time_signature[16]; /* "3/4" */
} ReadingMeter;

typedef struct {
    READING_CLAIM_FIELDS; /* the evidence holds the line's notes in order */
    LineRole role;
} ReadingLine;

typedef struct {
    READING_CLAIM_FIELDS;
    char rule[48];
    bool hard;
    int measure;
} ReadingVoiceFault;

typedef struct Reading Reading;
typedef struct AnalysisChain AnalysisChain;

/*
 * Makes an empty reading of the realization `target` names. BAD_VALUE for a source kind out of range, a name of 64 characters or more,
 * a producer key or knowledge hash that is not "" or 16 lowercase hexadecimal digits, or a digest that is not; BAD_ID for a work or
 * movement that is not a realization id. *out is untouched on failure.
 */
RealizationResult reading_create(const ReadingSource *source, const ReadingTarget *target, Reading **out);
void reading_destroy(Reading *reading);

/*
 * Each copies the claim and its evidence in. BAD_ID: the claim's id or one of its evidence ids is not a realization id; DUPLICATE_ID: the
 * reading holds a claim with that id (of any kind); BAD_VALUE: a start after its end, a time with no denominator, a confidence outside 0
 * to 1, a group whose parent is not an earlier group, a cadence type or role that is out of range. NULL arguments are ERR_NULL.
 */
RealizationResult reading_add_region(Reading *reading, const ReadingRegion *claim);
RealizationResult reading_add_chord(Reading *reading, const ReadingChord *claim);
RealizationResult reading_add_cadence(Reading *reading, const ReadingCadence *claim);
RealizationResult reading_add_group(Reading *reading, const ReadingGroup *claim);
RealizationResult reading_add_motive_occurrence(Reading *reading, const ReadingMotiveOccurrence *claim);
RealizationResult reading_add_meter(Reading *reading, const ReadingMeter *claim);
RealizationResult reading_add_line(Reading *reading, const ReadingLine *claim);
RealizationResult reading_add_voice_fault(Reading *reading, const ReadingVoiceFault *claim);

/* Names the version of the realization the reading is about once it is known (16 lowercase hexadecimal digits, BAD_VALUE otherwise). */
RealizationResult reading_set_target_digest(Reading *reading, const char *digest);

const ReadingSource *reading_source(const Reading *reading);
const ReadingTarget *reading_target(const Reading *reading);

size_t reading_region_count(const Reading *reading);
const ReadingRegion *reading_region_at(const Reading *reading, size_t index);
size_t reading_chord_count(const Reading *reading);
const ReadingChord *reading_chord_at(const Reading *reading, size_t index);
size_t reading_cadence_count(const Reading *reading);
const ReadingCadence *reading_cadence_at(const Reading *reading, size_t index);
size_t reading_group_count(const Reading *reading);
const ReadingGroup *reading_group_at(const Reading *reading, size_t index);
size_t reading_motive_occurrence_count(const Reading *reading);
const ReadingMotiveOccurrence *reading_motive_occurrence_at(const Reading *reading, size_t index);
size_t reading_meter_count(const Reading *reading);
const ReadingMeter *reading_meter_at(const Reading *reading, size_t index);
size_t reading_line_count(const Reading *reading);
const ReadingLine *reading_line_at(const Reading *reading, size_t index);
size_t reading_voice_fault_count(const Reading *reading);
const ReadingVoiceFault *reading_voice_fault_at(const Reading *reading, size_t index);

/* The chain run a reading was built from, borrowed (the caller keeps it alive as long as it asks for the detail); NULL when none. A
   reading read from a file has none. The detail of a claim is the index of an element of that chain's result. */
RealizationResult reading_set_detail(Reading *reading, const AnalysisChain *chain);
const AnalysisChain *reading_detail(const Reading *reading);

#endif
