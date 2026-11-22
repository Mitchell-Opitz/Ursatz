#ifndef URSATZ_THEORY_KNOWLEDGE_VOCABULARY_ENTRY_H
#define URSATZ_THEORY_KNOWLEDGE_VOCABULARY_ENTRY_H

#include <stdbool.h>

/*
 * K4 vocabulary (target architecture 5.2, R-707): the names the enums of the theory library stand for, as entries a person can read
 * and cite. The `vocabulary` kind has five subtypes: cadence-kind, embellishment (a non-chord tone), texture, progression-type and
 * function (a harmonic function). Every entry has a `code`, the text the C table uses for the same thing ("perfect-authentic",
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
    VOCABULARY_FUNCTION
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

/* The member for the entry's subtype holds its fields; the others are zero. */
typedef struct {
    const char *id;
    const char *name;
    VocabularySubtype subtype;
    const char *code;
    VocabularyCadence cadence;
    VocabularyEmbellishment embellishment;
    VocabularyTexture texture;
} VocabularyEntry;

#endif
