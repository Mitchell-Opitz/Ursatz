#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/galant/galant_prototypes.h"
#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The K5 galant schema knowledge of P70 B2 (R-759): the 26 `omt.schema.*` entries are pinned to the galant prototype table in both
 * directions (every row has its entry with equal function, flags and stage fields, every entry's name is a prototype's), and a user
 * schema of an unknown subtype or with a stage degree of 0 is refused with the file, the id and the field.
 */

static KnowledgeSnapshot *build(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

static int alteration_value(GalantAlteration alteration) {
    switch (alteration) {
        case GALANT_ALTERATION_NATURAL:
            return 0;
        case GALANT_ALTERATION_LOWERED:
            return -1;
        case GALANT_ALTERATION_RAISED:
            return 1;
        default:
            return 2;
    }
}

static void same_degree(const SchemaDegree *entry, const GalantDegree *table) {
    assert(entry->degree == table->degree && entry->alteration == alteration_value(table->alteration));
}

static void test_pinned_to_the_table(void) {
    KnowledgeSnapshot *snapshot = build();
    assert(knowledge_snapshot_schema_count(snapshot) == GALANT_SCHEMA_COUNT);
    for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
        const GalantPrototype *prototype = galant_prototype((GalantSchemaId)id);
        char entry_id[96];
        snprintf(entry_id, sizeof(entry_id), "omt.schema.%s", galant_schema_name((GalantSchemaId)id));
        const SchemaEntry *entry = knowledge_snapshot_schema(snapshot, entry_id);
        assert(entry != NULL && strcmp(entry->id, entry_id) == 0 && entry->subtype == SCHEMA_GALANT);
        assert(strcmp(entry->function, schema_function_name(prototype->function)) == 0);
        assert(entry->bass_defining == prototype->bass_defining && entry->melody_in_tenths == prototype->melody_in_tenths);
        assert(entry->stage_count == prototype->stage_count);
        for (size_t i = 0; i < entry->stage_count; i++) {
            const SchemaGalantStage *stage = &entry->galant[i];
            const GalantStage *row = &prototype->stages[i];
            assert(stage->strong == (row->strength == BEAT_STRENGTH_STRONG));
            same_degree(&stage->melody, &row->melody);
            same_degree(&stage->bass, &row->bass);
            assert(strcmp(stage->figures, row->figures == NULL ? "" : row->figures) == 0);
            assert(strcmp(stage->numeral, row->numeral) == 0);
            assert((stage->numeral_dominant == NULL) == (row->numeral_dominant == NULL));
            assert(row->numeral_dominant == NULL || strcmp(stage->numeral_dominant, row->numeral_dominant) == 0);
            assert(stage->has_melody_next == (row->melody_next.degree != 0));
            if (stage->has_melody_next) {
                same_degree(&stage->melody_next, &row->melody_next);
            }
        }
    }
    /* the other way: every entry is a prototype's, found by name */
    for (size_t i = 0; i < knowledge_snapshot_schema_count(snapshot); i++) {
        const SchemaEntry *entry = knowledge_snapshot_schema_at(snapshot, i);
        assert(entry != NULL && strncmp(entry->id, "omt.schema.", 11) == 0);
        bool found = false;
        for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
            found = found || strcmp(entry->id + 11, galant_schema_name((GalantSchemaId)id)) == 0;
        }
        assert(found);
    }
    assert(knowledge_snapshot_schema_at(snapshot, GALANT_SCHEMA_COUNT) == NULL);
    assert(knowledge_snapshot_schema(snapshot, "omt.material.major") == NULL && knowledge_snapshot_schema(snapshot, "no.schema.here") == NULL);
    /* spot checks of the table's odd rows: the Monte's two-note stages, the dominant numerals, the empty figures */
    const SchemaEntry *monte = knowledge_snapshot_schema(snapshot, "omt.schema.monte");
    assert(monte != NULL && monte->galant[0].has_melody_next && monte->galant[0].melody_next.degree == 7 &&
           monte->galant[0].melody_next.alteration == -1 && !monte->galant[1].has_melody_next);
    const SchemaEntry *modulating = knowledge_snapshot_schema(snapshot, "omt.schema.modulating-prinner");
    assert(modulating != NULL && strcmp(modulating->galant[0].numeral_dominant, "IV") == 0 && strcmp(modulating->name, "Modulating Prinner") == 0);
    const SchemaEntry *composta = knowledge_snapshot_schema(snapshot, "omt.schema.cadenza-composta");
    assert(composta != NULL && strcmp(composta->galant[2].numeral, "Cad.") == 0);
    knowledge_snapshot_destroy(snapshot);
}

/* A user file is refused: the status is not OK and the message names the file, the id and the field. */
static void refused(const char *text, const char *id, const char *field) {
    KnowledgeUserFile file = {"mine.json", text};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&file, 1, &snapshot, message, sizeof(message)) != KNOWLEDGE_OK);
    assert(snapshot == NULL);
    assert(strstr(message, "mine.json") != NULL && strstr(message, id) != NULL && strstr(message, field) != NULL);
}

#define STAGE(melody_degree)                                                                                                    \
    "{\"strength\":\"strong\",\"melody\":{\"degree\":" melody_degree ",\"alteration\":\"natural\"},"                         \
    "\"bass\":{\"degree\":1,\"alteration\":\"natural\"},\"figures\":\"5\",\"numeral\":\"I\"}"

#define ENTRY(id, subtype, melody_degree)                                                                                       \
    "{\"entries\":[{\"id\":\"" id "\",\"kind\":\"schema\",\"subtype\":\"" subtype "\",\"name\":\"mine\",\"content\":"          \
    "{\"function\":\"continuation\",\"bass_defining\":true,\"melody_in_tenths\":false,\"stages\":[" STAGE(melody_degree) "]},"    \
    "\"status\":\"reference\",\"version\":1}]}"

static void test_user_files(void) {
    refused(ENTRY("user.schema.odd", "galant-extra", "1"), "user.schema.odd", "subtype");
    refused(ENTRY("user.schema.zero", "galant", "0"), "user.schema.zero", "content.stages.degree");
    KnowledgeUserFile good = {"mine.json", ENTRY("user.schema.fine", "galant", "3")};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&good, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    const SchemaEntry *fine = knowledge_snapshot_schema(snapshot, "user.schema.fine");
    assert(fine != NULL && fine->stage_count == 1 && fine->galant[0].melody.degree == 3 && fine->galant[0].strong);
    assert(knowledge_snapshot_schema_count(snapshot) == GALANT_SCHEMA_COUNT + 1);
    knowledge_snapshot_destroy(snapshot);
}

int main(void) {
    test_pinned_to_the_table();
    test_user_files();
    puts("test_schema_entries: ok");
    return 0;
}
