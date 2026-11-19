#include "support/test_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <ursatz_composer/composer.h>

#include <common/json.h>

#include "ursatz_test.h"

void test_write_text(const char *path, const char *text) {
    if (!test_write_file(path, text, strlen(text))) {
        fprintf(stderr, "cannot write %s\n", path);
        abort();
    }
}

/* Removes the identity members of every note under node. */
static void strip_note_identity(cJSON *node) {
    cJSON *child;
    if (cJSON_IsObject(node)) {
        cJSON *notes = cJSON_GetObjectItemCaseSensitive(node, "notes");
        cJSON *note;
        cJSON_ArrayForEach(note, notes) {
            cJSON_DeleteItemFromObjectCaseSensitive(note, "id");
            cJSON_DeleteItemFromObjectCaseSensitive(note, "source");
            cJSON_DeleteItemFromObjectCaseSensitive(note, "operation");
        }
    }
    cJSON_ArrayForEach(child, node) {
        strip_note_identity(child);
    }
}

/* Names the constraints of every list under node as formats before 6 did. */
static void rename_region_constraints(cJSON *node) {
    cJSON *child;
    cJSON_ArrayForEach(child, node) {
        if (cJSON_IsArray(child) && child->string && strcmp(child->string, "constraints") == 0) {
            cJSON *constraint;
            cJSON_ArrayForEach(constraint, child) {
                cJSON *kind = cJSON_GetObjectItemCaseSensitive(constraint, "kind");
                if (cJSON_IsString(kind) && strcmp(kind->valuestring, "region") == 0) {
                    cJSON_SetValuestring(kind, "key");
                }
            }
        }
        rename_region_constraints(child);
    }
}

void test_plan_document_as_format_five(cJSON *document) {
    cJSON *name = cJSON_GetObjectItemCaseSensitive(document, "format");
    if (!cJSON_IsString(name) || strcmp(name->valuestring, "ursatz-plan") != 0) {
        return;
    }
    cJSON_ReplaceItemInObjectCaseSensitive(document, "format", cJSON_CreateString("ursatz-composer-plan"));
    cJSON_ReplaceItemInObjectCaseSensitive(document, "format_version", cJSON_CreateNumber(5));
    cJSON_DeleteItemFromObjectCaseSensitive(document, "derived_from");
    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(document, "metadata");
    cJSON *version = cJSON_DetachItemFromObjectCaseSensitive(metadata, "producer_version");
    cJSON *key = cJSON_DetachItemFromObjectCaseSensitive(metadata, "producer_key");
    cJSON_DeleteItemFromObjectCaseSensitive(metadata, "producer");
    cJSON_DeleteItemFromObjectCaseSensitive(metadata, "composition_key");
    cJSON_DeleteItemFromObjectCaseSensitive(metadata, "knowledge_hash");
    cJSON *analyzer = cJSON_DetachItemFromObjectCaseSensitive(metadata, "analyzer_version");
    cJSON_AddItemToObject(metadata, "composer_version", version);
    cJSON_AddItemToObject(metadata, "ursatz_commit", key);
    if (analyzer) {
        cJSON_AddItemToObject(metadata, "analyzer_version", analyzer);
    }
    /* The lane spans carried claim members; the keys are "keys" and a region has no kind. */
    cJSON *regions = cJSON_GetObjectItemCaseSensitive(document, "regions");
    if (regions) {
        cJSON *span;
        cJSON_ArrayForEach(span, regions) {
            cJSON *payload = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(span, "composer"), "payload");
            cJSON_DeleteItemFromObjectCaseSensitive(payload, "kind");
        }
        cJSON_free(regions->string);
        regions->string = strdup("keys");
    }
    rename_region_constraints(document);
}

char *test_plan_text_as_format_five(const char *text) {
    cJSON *document = cJSON_Parse(text);
    if (!document) {
        return NULL;
    }
    test_plan_document_as_format_five(document);
    char *printed = cJSON_Print(document);
    cJSON_Delete(document);
    char *out = printed ? common_json_indent(printed) : NULL;
    cJSON_free(printed);
    return out;
}

char *test_plan_text_as_format_three(const char *text) {
    cJSON *document = cJSON_Parse(text);
    if (!document) {
        return NULL;
    }
    test_plan_document_as_format_five(document);
    cJSON_DeleteItemFromObjectCaseSensitive(document, "next_note_id");
    cJSON_ReplaceItemInObjectCaseSensitive(document, "format_version", cJSON_CreateNumber(3));
    strip_note_identity(document);
    char *printed = cJSON_Print(document);
    cJSON_Delete(document);
    char *out = printed ? common_json_indent(printed) : NULL; /* the project's layout: 2 spaces and "key": value */
    cJSON_free(printed);
    return out;
}

MidiImportResult test_midi_import_notes(const unsigned char *data, size_t size, NoteEvent ***out_events,
                                        PitchNumericValue ***out_pitch_values, size_t *out_count) {
    MidiImport import;
    MidiImportResult result = midi_importer_import_complete(data, size, &import);
    if (result != MIDI_IMPORT_OK) {
        return result;
    }
    *out_events = import.events;
    *out_pitch_values = import.pitch_values;
    *out_count = import.count;
    import.events = NULL;
    import.pitch_values = NULL;
    import.count = 0;
    midi_import_destroy(&import);
    return MIDI_IMPORT_OK;
}

const PlanProducer *test_plan_producer(void) {
    static PlanProducer producer;
    static bool made;
    if (!made) {
        producer = composer_plan_producer(NULL);
        made = true;
    }
    return &producer;
}
