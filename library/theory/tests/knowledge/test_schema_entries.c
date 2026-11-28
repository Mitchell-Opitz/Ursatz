#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/galant/galant_prototypes.h"
#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The K5 schema knowledge of P70 B2 and B3 (R-759, R-760): the 26 galant `omt.schema.*` entries are pinned to the galant prototype
 * table in both directions (every row has its entry with equal function, flags and stage fields, every galant entry's name is a
 * prototype's), the fifteen progression entries read as the table of B3 says, and a user schema of an unknown subtype, with a stage
 * degree of 0, a rotation past its stages or a `realizes` that is not a progression type is refused with the file, the id and the
 * field.
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
    assert(knowledge_snapshot_schema_count(snapshot) == GALANT_SCHEMA_COUNT + 15);
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
    size_t galant = 0;
    for (size_t i = 0; i < knowledge_snapshot_schema_count(snapshot); i++) {
        const SchemaEntry *entry = knowledge_snapshot_schema_at(snapshot, i);
        assert(entry != NULL && strncmp(entry->id, "omt.schema.", 11) == 0);
        if (entry->subtype != SCHEMA_GALANT) {
            continue;
        }
        galant++;
        bool found = false;
        for (int id = 0; id < GALANT_SCHEMA_COUNT; id++) {
            found = found || strcmp(entry->id + 11, galant_schema_name((GalantSchemaId)id)) == 0;
        }
        assert(found);
    }
    assert(galant == GALANT_SCHEMA_COUNT);
    assert(knowledge_snapshot_schema_at(snapshot, GALANT_SCHEMA_COUNT + 15) == NULL);
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

static void test_progressions(void) {
    KnowledgeSnapshot *snapshot = build();
    size_t progressions = 0;
    for (size_t i = 0; i < knowledge_snapshot_schema_count(snapshot); i++) {
        const SchemaEntry *entry = knowledge_snapshot_schema_at(snapshot, i);
        progressions += entry->subtype == SCHEMA_PROGRESSION ? 1 : 0;
        if (entry->subtype == SCHEMA_PROGRESSION) {
            assert(entry->stage_count >= 1 && entry->realizes != NULL && entry->rotation_count >= 1);
            for (size_t s = 0; s < entry->stage_count; s++) {
                assert(entry->stages[s].chord != NULL && entry->stages[s].units >= 1 && entry->stages[s].units <= 8);
            }
        }
    }
    assert(progressions == 15);
    const SchemaEntry *lydian = knowledge_snapshot_schema(snapshot, "omt.schema.lydian-shuttle");
    assert(lydian != NULL && lydian->subtype == SCHEMA_PROGRESSION && lydian->stage_count == 2 && lydian->loop);
    assert(strcmp(lydian->stages[0].chord, "I") == 0 && lydian->stages[0].units == 1);
    assert(strcmp(lydian->stages[1].chord, "II") == 0 && lydian->stages[1].units == 1);
    assert(lydian->rotation_count == 2 && lydian->rotations[0] == 0 && lydian->rotations[1] == 1);
    assert(strcmp(lydian->role, "loop") == 0 && lydian->material_count == 1 && strcmp(lydian->materials[0], "omt.material.lydian") == 0);
    assert(strcmp(lydian->realizes, "omt.vocabulary.prolongational") == 0);
    const SchemaEntry *blues = knowledge_snapshot_schema(snapshot, "omt.schema.twelve-bar-blues");
    assert(blues != NULL && blues->stage_count == 6 && blues->stages[0].units == 4 && blues->stages[1].units == 2 && blues->stages[3].units == 1);
    unsigned bars = 0;
    for (size_t s = 0; s < blues->stage_count; s++) {
        bars += blues->stages[s].units;
    }
    assert(bars == 12 && blues->material_count == 2);
    const SchemaEntry *cadence = knowledge_snapshot_schema(snapshot, "omt.schema.aeolian-cadence");
    assert(cadence != NULL && !cadence->loop && strcmp(cadence->role, "cadence") == 0 && strcmp(cadence->stages[0].chord, "bVI") == 0);
    const SchemaEntry *doo_wop = knowledge_snapshot_schema(snapshot, "omt.schema.doo-wop");
    assert(doo_wop != NULL && doo_wop->rotation_count == 2 && doo_wop->rotations[1] == 2 && strcmp(doo_wop->realizes, "omt.vocabulary.phrase-model") == 0);
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

#define PROGRESSION(id, rotation, realizes)                                                                                    \
    "{\"entries\":[{\"id\":\"" id "\",\"kind\":\"schema\",\"subtype\":\"progression\",\"name\":\"mine\",\"content\":"       \
    "{\"stages\":[{\"chord\":\"I\"},{\"chord\":\"IV\",\"units\":2}],\"loop\":true,\"rotations\":[" rotation "],"           \
    "\"role\":\"loop\",\"realizes\":\"" realizes "\"},\"status\":\"reference\",\"version\":1}]}"

static void test_user_files(void) {
    refused(PROGRESSION("user.schema.far", "0,2", "omt.vocabulary.prolongational"), "user.schema.far", "content.rotations");
    refused(PROGRESSION("user.schema.lost", "0", "omt.vocabulary.no-such-type"), "user.schema.lost", "content.realizes");
    refused(PROGRESSION("user.schema.material", "0", "omt.material.major"), "user.schema.material", "content.realizes");
    refused(ENTRY("user.schema.odd", "galant-extra", "1"), "user.schema.odd", "subtype");
    refused(ENTRY("user.schema.zero", "galant", "0"), "user.schema.zero", "content.stages.degree");
    KnowledgeUserFile good = {"mine.json", ENTRY("user.schema.fine", "galant", "3")};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&good, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    const SchemaEntry *fine = knowledge_snapshot_schema(snapshot, "user.schema.fine");
    assert(fine != NULL && fine->stage_count == 1 && fine->galant[0].melody.degree == 3 && fine->galant[0].strong);
    assert(knowledge_snapshot_schema_count(snapshot) == GALANT_SCHEMA_COUNT + 16);
    knowledge_snapshot_destroy(snapshot);
}

int main(void) {
    test_pinned_to_the_table();
    test_progressions();
    test_user_files();
    puts("test_schema_entries: ok");
    return 0;
}
