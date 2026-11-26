#ifndef URSATZ_THEORY_KNOWLEDGE_FORM_ENTRY_H
#define URSATZ_THEORY_KNOWLEDGE_FORM_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

#include "model/core/rational.h"
#include "theory/knowledge/vocabulary_entry.h"

/*
 * K7 formal function and K8 theme type (target architecture 5.2, R-708). A FormalFunction says what an idea, a subphrase or a phrase
 * does in a theme (its core, after Caplin's initiating, medial and concluding functions; its level) and what its harmony and surface
 * follow from that: the progression types its harmony may take, whether its harmonic rhythm and its rhythmic activity run faster or
 * slower than the idea before, whether its harmony keeps the motive's chords or is free of them, the cadence it ends on, and for a
 * contrasting idea what it contrasts against and by how much, in the dimensions of R-709 (rhythm, contour, register, density). A
 * ThemeType is a beginning and an ending (formal functions of subphrase or phrase level) and the ideas between them, each with the
 * function of an idea, the slot its motive is drawn for, its half, its share of the theme in eighths (the units of the theme shape
 * table) and the cadence it carries. A snapshot owns both; the links point into it and live as long as it does.
 */

#define FORMAL_HARMONY_MAX 4
#define FORMAL_ASPECT_MAX 4
#define THEME_TYPE_IDEA_MAX 6
#define THEME_EXPECT_UNIT_MAX 8
#define THEME_EXPECT_HARMONY_MAX 6
#define THEME_EXPECT_CHORD_MAX 8

typedef enum {
    FORMAL_CORE_INITIATING,
    FORMAL_CORE_MEDIAL,
    FORMAL_CORE_CONCLUDING,
    FORMAL_CORE_BEFORE_THE_BEGINNING,
    FORMAL_CORE_AFTER_THE_END,
    FORMAL_CORE_CONNECTIVE
} FormalCore;

typedef enum { FORMAL_LEVEL_IDEA, FORMAL_LEVEL_SUBPHRASE, FORMAL_LEVEL_PHRASE } FormalLevel;

/* How an idea's harmonic rhythm or rhythmic activity runs against the idea before it. */
typedef enum { FORMAL_PACE_ANY, FORMAL_PACE_FASTER, FORMAL_PACE_SLOWER } FormalPace;

typedef enum { FORMAL_CADENCE_NONE, FORMAL_CADENCE_WEAK, FORMAL_CADENCE_STRONG, FORMAL_CADENCE_ANY } FormalCadence;

typedef struct {
    const char *aspect; /* "rhythm", "contour", "register" or "density" */
    Rational amount;    /* the least distance the contrast must reach, from 0 */
} FormalAspect;

typedef struct FormalFunction {
    const char *id;
    const char *name;
    const char *code;
    FormalCore core;
    FormalLevel level;
    size_t harmony_count;
    const VocabularyEntry *harmony[FORMAL_HARMONY_MAX]; /* progression types, in order of preference */
    FormalPace harmonic_rhythm;                         /* any or faster */
    FormalPace rhythmic_activity;
    bool keeps_motive_harmony; /* the harmony holds the motive's chords (false: it is free of them) */
    FormalCadence cadence;     /* any when the entry says nothing */
    const struct FormalFunction *contrast_against; /* NULL for none */
    size_t aspect_count;
    FormalAspect aspects[FORMAL_ASPECT_MAX];
} FormalFunction;

typedef enum { THEME_IDEA_CADENCE_NONE, THEME_IDEA_CADENCE_FIRST, THEME_IDEA_CADENCE_FINAL } ThemeIdeaCadence;

/* The jobs a unit of an idea may take (K8 slot expectations, R-749): bits of ThemeTypeExpect.jobs. */
#define THEME_JOB_MOTIF 1u
#define THEME_JOB_VARIANT 2u
#define THEME_JOB_CONTRAST 4u
#define THEME_JOB_RUNNING 8u
#define THEME_JOB_LONG 16u
#define THEME_JOB_SYNCOPATED 32u
#define THEME_JOB_LIQUIDATION 64u
#define THEME_JOB_CADENCE 128u

/* The sounds an idea's unit may carry: bits of ThemeTypeExpect.sounds. */
#define THEME_SOUND_NINTH_DOWNBEAT 1u
#define THEME_SOUND_SUS4_DOMINANT 2u
#define THEME_SOUND_APPOGGIATURA 4u
#define THEME_SOUND_MAJOR_SEVENTH 8u
#define THEME_SOUND_LEADING_TONE 16u

/* One alternative harmony of an idea: the chord texts of its bars (the own-harmony syntax, "IV V@2") or a restatement of the basic idea's. */
typedef struct {
    bool restate;
    size_t chord_count; /* one text per unit; 0 when restate */
    const char *chords[THEME_EXPECT_CHORD_MAX];
    const char *type_id; /* the progression type, one of the idea function's harmony entries */
    const VocabularyEntry *type;
} ThemeHarmonyTemplate;

/* What an idea expects of the blueprint written for it, per unit (an eighth of the theme); present is false when the entry says nothing. */
typedef struct {
    bool present;
    size_t job_count;                          /* units */
    unsigned jobs[THEME_EXPECT_UNIT_MAX];      /* bits THEME_JOB_*: the jobs a unit may take */
    size_t harmony_count;                      /* alternatives, 0 for none */
    ThemeHarmonyTemplate harmony[THEME_EXPECT_HARMONY_MAX];
    unsigned climax;                           /* bit k: unit k may hold the climax */
    unsigned sounds[THEME_EXPECT_UNIT_MAX];    /* bits THEME_SOUND_*: the sounds unit k may take */
} ThemeTypeExpect;

typedef struct {
    const char *name;
    const FormalFunction *function; /* of level idea */
    const char *slot;               /* "basic", "contrasting", "continuation" or "cadential" */
    bool in_beginning;
    int units; /* eighths of the theme, 1 to 8 */
    ThemeIdeaCadence cadence;
    ThemeTypeExpect expect;
} ThemeTypeIdea;

typedef struct {
    const char *id;
    const char *name;
    const char *code;
    const FormalFunction *beginning; /* of level subphrase or phrase */
    const FormalFunction *ending;
    size_t idea_count;
    ThemeTypeIdea ideas[THEME_TYPE_IDEA_MAX];
} ThemeType;

#endif
