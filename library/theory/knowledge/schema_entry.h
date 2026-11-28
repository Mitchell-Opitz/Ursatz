#ifndef URSATZ_THEORY_KNOWLEDGE_SCHEMA_ENTRY_H
#define URSATZ_THEORY_KNOWLEDGE_SCHEMA_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

/*
 * K5 schema (target architecture 5.2, R-759, R-760): a galant schema, the stages of a melody and a bass with the harmony each
 * stage carries, or a progression schema, the chords of a loop, an opening or a cadence. The `galant` subtype holds the same data as a
 * galant prototype (theory/galant/galant_prototypes.h), which stays the identity every reader uses; the entry is pinned to it by test.
 * The loader keeps its own degree type, because theory-knowledge does not use theory-galant. A progression stage keeps its chord as
 * text: theory-knowledge does not include the numeral grammar, so the reader of the entry parses it. `realizes` and `materials` are
 * ids that the snapshot checks (a progression-type vocabulary entry, K1 materials). A snapshot owns it; the strings live as long as
 * the snapshot.
 */

#define SCHEMA_STAGES_MAX 16
#define SCHEMA_GALANT_STAGES_MAX 8

typedef enum { SCHEMA_GALANT, SCHEMA_PROGRESSION } SchemaSubtype;

/* A scale degree a schema asks for: 1 to 7, with the alteration accepted. */
typedef struct {
    int degree;
    int alteration; /* 0 natural, -1 lowered, 1 raised, 2 either */
} SchemaDegree;

typedef struct {
    bool strong; /* the stage falls on a strong beat; else a weak one */
    SchemaDegree melody;
    SchemaDegree bass;
    const char *figures;          /* the figured bass above the bass note; empty when there is none */
    const char *numeral;          /* the Roman numeral in the tonic key */
    const char *numeral_dominant; /* the Roman numeral in the dominant key; NULL when absent */
    bool has_melody_next;         /* a stage whose melody has two notes (the Monte) */
    SchemaDegree melody_next;
} SchemaGalantStage;

typedef struct {
    const char *chord; /* the stage's chord against the center's major scale: "I", "bVII", "vi" */
    unsigned units;    /* 1 to 8, the stage's share of the loop (1 when not given) */
} SchemaProgressionStage;

#define SCHEMA_ROTATIONS_MAX 16
#define SCHEMA_MATERIALS_MAX 8

typedef struct {
    const char *id;
    const char *name;
    SchemaSubtype subtype;
    const char *function; /* galant: opening-gambit, continuation, pre-cadential, cadence or post-cadential */
    bool bass_defining;
    bool melody_in_tenths;
    size_t stage_count;
    SchemaGalantStage galant[SCHEMA_GALANT_STAGES_MAX];
    SchemaProgressionStage stages[SCHEMA_STAGES_MAX]; /* progression: stage_count of them (the galant stages keep their own array) */
    bool loop;
    size_t rotation_count;
    unsigned rotations[SCHEMA_ROTATIONS_MAX]; /* the stages a loop may start on */
    const char *role;                         /* loop, opening, continuation or cadence */
    size_t material_count;
    const char *materials[SCHEMA_MATERIALS_MAX]; /* K1 material ids the progression is heard in */
    const char *realizes;                        /* the id of the progression-type vocabulary entry it realizes */
} SchemaEntry;

#endif
