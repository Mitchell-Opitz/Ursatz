#ifndef URSATZ_THEORY_KNOWLEDGE_SCHEMA_ENTRY_H
#define URSATZ_THEORY_KNOWLEDGE_SCHEMA_ENTRY_H

#include <stdbool.h>
#include <stddef.h>

/*
 * K5 schema (target architecture 5.2, R-759): a galant schema, the stages of a melody and a bass with the harmony each stage
 * carries. The `galant` subtype holds the same data as a galant prototype (theory/galant/galant_prototypes.h), which stays the
 * identity every reader uses; the entry is pinned to it by test. The loader keeps its own degree type, because theory-knowledge does
 * not use theory-galant. A snapshot owns it; the strings live as long as the snapshot.
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
    const char *id;
    const char *name;
    SchemaSubtype subtype;
    const char *function; /* galant: opening-gambit, continuation, pre-cadential, cadence or post-cadential */
    bool bass_defining;
    bool melody_in_tenths;
    size_t stage_count;
    SchemaGalantStage galant[SCHEMA_GALANT_STAGES_MAX];
} SchemaEntry;

#endif
