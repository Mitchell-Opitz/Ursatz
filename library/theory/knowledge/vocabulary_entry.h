#ifndef URSATZ_THEORY_KNOWLEDGE_VOCABULARY_ENTRY_H
#define URSATZ_THEORY_KNOWLEDGE_VOCABULARY_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

#include "model/pitch/pitch_material.h"

/*
 * K4 vocabulary (target architecture 5.2, R-707): the names the enums of the theory library stand for, as entries a person can read
 * and cite. The `vocabulary` kind has seven subtypes: cadence-kind, embellishment (a non-chord tone), texture, progression-type,
 * function (a harmonic function), and Hindemith's interval-class and chord-group (R-764: the tables `chord_value` and the tension measure use). Every entry has a `code`, the text the C table uses for the same thing ("perfect-authentic",
 * "escape-tone", "melody and accompaniment", "tonic"); the enums stay the identity at run time and a test pins each table to its
 * entries. A cadence kind holds the row of the cadence table, an embellishment how the tone is approached and left, a texture the
 * code of its category's entry; the other two subtypes hold the code only (a progression type's example chords are for readers and
 * for the knowledge check). A snapshot owns the entries; the strings live as long as it does.
 */

typedef enum {
    VOCABULARY_CADENCE_KIND,
    VOCABULARY_EMBELLISHMENT,
    VOCABULARY_TEXTURE,
    VOCABULARY_PROGRESSION_TYPE,
    VOCABULARY_FUNCTION,
    VOCABULARY_INTERVAL_CLASS,
    VOCABULARY_CHORD_GROUP
} VocabularySubtype;

/* How an embellishing tone is approached or left. */
typedef enum {
    EMBELLISHMENT_MOTION_STEP,
    EMBELLISHMENT_MOTION_LEAP,
    EMBELLISHMENT_MOTION_STATIC, /* the tone is held over from the chord before (approach) or after it (leave) */
    EMBELLISHMENT_MOTION_NONE
} EmbellishmentMotion;

/* The direction of the tone's departure against its approach. */
typedef enum {
    EMBELLISHMENT_DIRECTION_SAME,
    EMBELLISHMENT_DIRECTION_OPPOSITE,
    EMBELLISHMENT_DIRECTION_UP,
    EMBELLISHMENT_DIRECTION_DOWN,
    EMBELLISHMENT_DIRECTION_NONE
} EmbellishmentDirection;

/* Where in the bar the tone sounds. */
typedef enum { EMBELLISHMENT_METRIC_STRONG, EMBELLISHMENT_METRIC_WEAK, EMBELLISHMENT_METRIC_ANY } EmbellishmentMetric;

typedef struct {
    const char *label;
    const char *abbreviation;
    const char *core; /* the name of the core cadence type the kind reduces to ("unknown" for none) */
    int strength;
    bool conclusive;
    /* A modal cadence kind names its mode and the degrees it moves through: the approach (1 to 3 degrees), then the arrival. NULL and 0 for the others. */
    const PitchMaterial *material;
    size_t approach_count;
    int approach_degrees[3];
    int arrival_degree;
} VocabularyCadence;

typedef struct {
    EmbellishmentMotion approach;
    EmbellishmentMotion leave;
    EmbellishmentDirection direction;
    EmbellishmentMetric metric;
} VocabularyEmbellishment;

typedef struct {
    const char *category; /* the code of the texture entry this texture belongs to (its own code for a category) */
} VocabularyTexture;

/* A row of Hindemith's Series 2 (the intervals from the most to the least consonant): its size in semitones, its rank 1 to 12, and the
   root of the interval (1 the lower tone, 2 the upper, 0 none). */
typedef struct {
    int semitones;
    int rank;
    int root;
} VocabularyInterval;

/* A chord group of Hindemith's table of chords, with its rank 1 (Ia) to 12 (VI) and the words of its description, as the
   schema's words in order: tritones none, one, several, any (0 to 3); sharp none, mild, sharp, any (0 to 3); root bass, above, none, any (0 to 3: any is a group that has a root in either place). */
typedef struct {
    int group; /* 1 to 6 */
    int rank;
    int tritones;
    int sharp;
    int root;
} VocabularyChordGroup;

/* The member for the entry's subtype holds its fields; the others are zero. */
typedef struct {
    const char *id;
    const char *name;
    VocabularySubtype subtype;
    const char *code;
    VocabularyCadence cadence;
    VocabularyEmbellishment embellishment;
    VocabularyTexture texture;
    VocabularyInterval interval;
    VocabularyChordGroup chord_group;
} VocabularyEntry;

#endif
