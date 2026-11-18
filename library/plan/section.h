#ifndef URSATZ_PLAN_SECTION_H
#define URSATZ_PLAN_SECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "model/core/rational.h"
#include "theory/vocabulary/cadence_type.h"

/*
 * The section description (R-605): what a section is, written once for forms, plans and piece requests. A value, its
 * vocabulary and the rules about a section itself; the rules that look at other sections, a form's regions or the meter
 * belong to the code that holds those. The JSON codec is plan/section_json.h.
 */

#define SECTION_NAME_MAX 32
#define SECTION_ID_MAX 16
#define SECTION_LETTER_MAX 4
#define SECTION_CADENCE_NAME_MAX 32
#define SECTION_WORD_MAX 64
#define SECTION_TONIC_MAX 4
#define SECTION_MOTIVE_MAX 64
#define SECTION_NUMERALS_MAX 256
#define SECTION_SCHEMA_MAX 32
#define SECTION_CHORD_BARS_MAX 16
#define SECTION_CHORD_TEXT_MAX 32
#define SECTION_CONSTRUCTION_IDEAS 10
#define SECTION_CONSTRUCTION_EXTENSIONS 3

/* ------------------------------------------------------------- the vocabulary */

/* The role of a section: the first 19 are the roles a form or a request names (the order of the Composer's old FormRole), then
   the two roles only a plan holds. */
typedef enum {
    SECTION_ROLE_THEME,
    SECTION_ROLE_CONTRAST,
    SECTION_ROLE_RETURN,
    SECTION_ROLE_VARIATION,
    SECTION_ROLE_LAYER,
    SECTION_ROLE_PATTERN,
    SECTION_ROLE_SEQUENCE,
    SECTION_ROLE_EPISODE,
    SECTION_ROLE_CODETTA,
    SECTION_ROLE_LIQUIDATION,
    SECTION_ROLE_TRANSITION,
    SECTION_ROLE_RETRANSITION,
    SECTION_ROLE_SUBORDINATE,
    SECTION_ROLE_CODA,
    SECTION_ROLE_ELABORATION,
    SECTION_ROLE_INTRO,
    SECTION_ROLE_BODY,
    SECTION_ROLE_TURNAROUND,
    SECTION_ROLE_ENDING,
    SECTION_ROLE_MATERIAL,
    SECTION_ROLE_SEAM
} SectionRole;

#define SECTION_ROLE_FORM_COUNT 19
#define SECTION_ROLE_COUNT 21

const char *section_role_name(SectionRole role);
const char *section_role_label(SectionRole role);
bool section_role_find(const char *name, SectionRole *out);
/* The 15 theme-like roles: a section grown as a theme, a pattern, a sequence and the like. */
bool section_role_is_theme(SectionRole role);
/* Liquidation, retransition and coda: grown by an operator of their own, with no beginning or ending. */
bool section_role_is_operator(SectionRole role);

typedef enum {
    SECTION_BEGINNING_PRESENTATION,
    SECTION_BEGINNING_ANTECEDENT,
    SECTION_BEGINNING_COMPOUND_BASIC_IDEA
} SectionBeginning;

typedef enum {
    SECTION_ENDING_CONTINUATION,
    SECTION_ENDING_CONSEQUENT,
    SECTION_ENDING_CADENTIAL
} SectionEnding;

typedef struct {
    SectionBeginning beginning;
    SectionEnding ending;
} SectionPairing;

const char *section_beginning_name(SectionBeginning beginning);
bool section_beginning_find(const char *name, SectionBeginning *out);
const char *section_ending_name(SectionEnding ending);
bool section_ending_find(const char *name, SectionEnding *out);

/* The ideas of a theme, in the order of the Composer's GrowIdea. */
typedef enum {
    SECTION_IDEA_BASIC,
    SECTION_IDEA_REPETITION,
    SECTION_IDEA_CONTRASTING,
    SECTION_IDEA_CONSEQUENT_BASIC,
    SECTION_IDEA_CONSEQUENT_CONTRASTING,
    SECTION_IDEA_FRAGMENT_1,
    SECTION_IDEA_FRAGMENT_2,
    SECTION_IDEA_CADENTIAL,
    SECTION_IDEA_CADENTIAL_1,
    SECTION_IDEA_CADENTIAL_2
} SectionIdea;

const char *section_idea_name(SectionIdea idea);
bool section_idea_find(const char *name, SectionIdea *out);

typedef enum {
    SECTION_EXTENSION_REPEAT,
    SECTION_EXTENSION_STRETCH,
    SECTION_EXTENSION_ADDITION,
    SECTION_EXTENSION_EVADED
} SectionExtensionKind;

typedef struct {
    SectionExtensionKind kind;
    unsigned char idea; /* the SectionIdea the extension follows or stretches */
    unsigned char bars;
    bool varied;
} SectionExtension;

typedef enum {
    SECTION_CONSTRUCTION_GIVEN,
    SECTION_CONSTRUCTION_FROM_MOTIVE,
    SECTION_CONSTRUCTION_FROM_LENGTH
} SectionConstructionReason;

typedef struct {
    unsigned char idea_bars[SECTION_CONSTRUCTION_IDEAS];
    size_t extension_count;
    SectionExtension extensions[SECTION_CONSTRUCTION_EXTENSIONS];
    SectionConstructionReason reason;
} SectionConstruction;

const char *section_extension_kind_name(SectionExtensionKind kind);
bool section_extension_kind_find(const char *name, SectionExtensionKind *out);
const char *section_construction_reason_name(SectionConstructionReason reason);
bool section_construction_reason_find(const char *name, SectionConstructionReason *out);

typedef enum {
    SECTION_RETURN_EXACT,
    SECTION_RETURN_ORNAMENT,
    SECTION_RETURN_REHARMONIZE,
    SECTION_RETURN_REREGISTER,
    SECTION_RETURN_VARIED,
    SECTION_RETURN_FIGURATE,
    SECTION_RETURN_EMBELLISH,
    SECTION_RETURN_SETTING
} SectionReturnVariation;

typedef enum {
    SECTION_SETTING_NONE,
    SECTION_SETTING_BLOCK,
    SECTION_SETTING_ARPEGGIATED,
    SECTION_SETTING_ALBERTI,
    SECTION_SETTING_OOM_PAH
} SectionSetting;

typedef enum {
    SECTION_MODE_NONE,
    SECTION_MODE_PARALLEL,
    SECTION_MODE_TONIC_MAJOR,
    SECTION_MODE_HOME
} SectionModeShift;

typedef enum {
    SECTION_CONNECTIVE_NONE,
    SECTION_CONNECTIVE_SOMETIMES,
    SECTION_CONNECTIVE_ALWAYS
} SectionConnective;

/* A name is NULL for SECTION_SETTING_NONE, SECTION_MODE_NONE and SECTION_CONNECTIVE_NONE; a find never returns them. */
const char *section_return_variation_name(SectionReturnVariation variation);
bool section_return_variation_find(const char *name, SectionReturnVariation *out);
const char *section_setting_name(SectionSetting setting);
bool section_setting_find(const char *name, SectionSetting *out);
const char *section_mode_name(SectionModeShift mode);
bool section_mode_find(const char *name, SectionModeShift *out);
const char *section_connective_name(SectionConnective connective);
bool section_connective_find(const char *name, SectionConnective *out);

typedef enum { SECTION_FIGURE_NEIGHBOR, SECTION_FIGURE_SCALE, SECTION_FIGURE_BROKEN_CHORD } SectionFigureKind;
typedef enum { SECTION_FIGURE_EIGHTHS, SECTION_FIGURE_SIXTEENTHS } SectionFigureRhythm;

typedef struct {
    SectionFigureKind kind;
    SectionFigureRhythm rhythm;
    bool reharmonize;
    unsigned change_at;
    bool climax;
} SectionFigure;

const char *section_figure_kind_name(SectionFigureKind kind);
bool section_figure_kind_find(const char *name, SectionFigureKind *out);
const char *section_figure_rhythm_name(SectionFigureRhythm rhythm);
bool section_figure_rhythm_find(const char *name, SectionFigureRhythm *out);

typedef enum {
    SECTION_FUNCTION_TRANSITION,
    SECTION_FUNCTION_RETRANSITION,
    SECTION_FUNCTION_SUBORDINATE,
    SECTION_FUNCTION_CODA
} SectionFunctionKind;

typedef struct {
    SectionFunctionKind function;
    bool independent; /* a transition made from a motive of its own */
    bool lyric;       /* a subordinate theme whose motive is m0 augmented */
} SectionFunction;

const char *section_function_kind_name(SectionFunctionKind kind);
bool section_function_kind_find(const char *name, SectionFunctionKind *out);

/* The lanes of a plan, in the order of the Composer's PlanLaneKind. */
typedef enum {
    SECTION_LANE_METER,
    SECTION_LANE_KEY,
    SECTION_LANE_HARMONY,
    SECTION_LANE_BASS,
    SECTION_LANE_MELODY,
    SECTION_LANE_INNER_VOICE
} SectionLane;

#define SECTION_LANE_COUNT 6
#define SECTION_LAYER_BIT(lane) (1u << (unsigned)(lane))
#define SECTION_LAYERS_ALL ((1u << SECTION_LANE_COUNT) - 1u)

const char *section_lane_name(SectionLane lane);
bool section_lane_find(const char *name, SectionLane *out);

/* A region: where a section's key starts or ends. Words are kept as written; the Composer resolves them through the snapshot. */
typedef enum {
    SECTION_REGION_NONE,
    SECTION_REGION_HOME,
    SECTION_REGION_FORM,     /* a region of the form */
    SECTION_REGION_KEY,      /* a tonic and a material */
    SECTION_REGION_RELATION  /* a relation from an anchor */
} SectionRegionKind;

typedef enum { SECTION_ANCHOR_HOME, SECTION_ANCHOR_PREVIOUS, SECTION_ANCHOR_SECTION } SectionAnchor;

typedef struct {
    SectionRegionKind kind;
    char form_region[SECTION_NAME_MAX];
    char tonic[SECTION_TONIC_MAX];
    char material[SECTION_WORD_MAX];
    char relation[SECTION_WORD_MAX];
    SectionAnchor anchor;
    char anchor_section[SECTION_NAME_MAX];
} SectionRegion;

typedef enum { SECTION_HARMONY_NONE, SECTION_HARMONY_PROGRESSION, SECTION_HARMONY_SCHEMA, SECTION_HARMONY_CHORDS } SectionHarmonyKind;

typedef struct {
    unsigned bar;
    char text[SECTION_CHORD_TEXT_MAX];
} SectionChordBar;

typedef struct {
    SectionHarmonyKind kind;
    char numerals[SECTION_NUMERALS_MAX];
    unsigned cells;
    char schema[SECTION_SCHEMA_MAX];
    SectionChordBar chords[SECTION_CHORD_BARS_MAX];
    size_t chord_count;
} SectionHarmony;

/* ------------------------------------------------------------- the description */

typedef struct {
    char name[SECTION_NAME_MAX];
    char id[SECTION_ID_MAX];
    char letter[SECTION_LETTER_MAX];
    bool has_role;
    SectionRole role;
    size_t bars, beats; /* 0 for none */
    bool has_pickup;
    Rational pickup;
    bool has_pairing;
    SectionPairing pairing;
    bool has_cadence;
    CadenceType cadence;
    char cadence_name[SECTION_CADENCE_NAME_MAX];
    bool has_first_cadence;
    CadenceType first_cadence;
    bool closed;
    SectionRegion key_start, key_end;
    char motive[SECTION_MOTIVE_MAX]; /* a relation step as motive_relation_step_text writes it, "" for none */
    char returns[SECTION_NAME_MAX];
    SectionReturnVariation variation;
    SectionSetting setting;
    SectionModeShift mode;
    unsigned layers;
    int pedal; /* 0 or 5 */
    bool leads_in;
    SectionConnective connective;
    bool connective_written;
    bool has_construction;
    bool construction_auto;
    SectionConstruction construction;
    bool has_figure;
    SectionFigure figure;
    bool has_function;
    SectionFunction function;
    bool free_phrase;
    SectionHarmony harmony;
} SectionDescription;

/* What a description is used for; it decides which members it needs and which it may hold. */
typedef enum {
    SECTION_USE_FORM = 1u,
    SECTION_USE_PLAN = 2u,
    SECTION_USE_REQUEST = 4u,
    SECTION_USE_OVERLAY = 8u
} SectionUse;

/* path is relative to the section object, "/variation". */
typedef struct {
    char path[96];
    char message[192];
} SectionError;

/* Zeroed, the return variation exact, the regions none, the anchors home. */
void section_description_init(SectionDescription *description);

/* How many constraints the members set used to be: the order the fill picks its targets in follows it. */
size_t section_description_weight(const SectionDescription *description);

/* The rules about a section itself for one use; false with the first refusal in *error (may be NULL). */
bool section_description_check(const SectionDescription *description, SectionUse use, SectionError *error);

#endif
