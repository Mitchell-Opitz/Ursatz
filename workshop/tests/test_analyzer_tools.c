#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <common/files.h>

#include <ursatz_workshop/workshop_api.h>

/*
 * The Analyzer's three tools (R-656) on a tools workshop: analyze (the document, the text export, the layers and the model plan of Bach 114),
 * corpus (over a one-piece corpus: a copy of Bach 114, because Mozart 545 of the sonatina fixture takes 26 s in a debug build) and style-profile (over its output). The document and the text are compared with the
 * Analyzer's goldens as test_document_bytes and test_text_export_snapshots compare them: the file is analyzed under the name the goldens
 * record, so the document's source is the same.
 */

static char *read_text(const char *path) {
    char *data = NULL;
    size_t size = 0;
    UrsatzError error;
    assert(common_file_read(path, &data, &size, &error) == URSATZ_OK);
    char *text = realloc(data, size + 1);
    assert(text);
    text[size] = '\0';
    return text;
}

static void copy_file(const char *from, const char *to) {
    UrsatzError error;
    assert(common_file_copy(from, to, &error) == URSATZ_OK);
}

static const char *text_member(const cJSON *object, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

/* Replaces the value of the first string member `"<name>": "<value>"` by "*" (the metadata's, which change with the build). */
static char *mask_member(char *text, const char *name) {
    char key[64];
    snprintf(key, sizeof(key), "\"%s\": \"", name);
    char *found = strstr(text, key);
    assert(found);
    char *value = found + strlen(key);
    char *end = strchr(value, '"');
    assert(end);
    size_t head = (size_t)(value - text);
    char *masked = malloc(head + 1 + strlen(end) + 1);
    assert(masked);
    memcpy(masked, text, head);
    masked[head] = '*';
    strcpy(masked + head + 1, end);
    free(text);
    return masked;
}

/* The text export without its "analyzer: ..." line, which changes with the build. */
/* The lines between the view frame: after the first line, before the trailing "# leaves out" and "# expand" lines. A new string. */
static char *without_frame(const char *framed) {
    assert(strncmp(framed, "ursatz-text 1 view reading Bach 114.mid\n", 40) == 0);
    const char *body = strchr(framed, '\n') + 1;
    const char *tail = strstr(body, "# leaves out: ");
    assert(tail && strstr(tail, "\n# expand: ursatz analyze Bach 114.mid --view reading --measures a-b\n") != NULL);
    char *copy = malloc((size_t)(tail - body) + 1);
    assert(copy);
    memcpy(copy, body, (size_t)(tail - body));
    copy[tail - body] = '\0';
    return copy;
}

static char *without_analyzer_line(const char *text) {
    const char *line = strstr(text, "\nanalyzer: ");
    char *copy = malloc(strlen(text) + 1);
    assert(copy);
    if (!line) {
        strcpy(copy, text);
        return copy;
    }
    size_t head = (size_t)(line - text) + 1;
    const char *rest = strchr(line + 1, '\n');
    memcpy(copy, text, head);
    strcpy(copy + head, rest ? rest + 1 : "");
    return copy;
}

static UrsatzStatus call(Workshop *workshop, const char *name, const char *params_text, WorkshopReply *reply, UrsatzError *error) {
    cJSON *params = cJSON_Parse(params_text);
    assert(params);
    memset(error, 0, sizeof(*error));
    UrsatzStatus status = workshop_tool(workshop, name, params, NULL, reply, error);
    cJSON_Delete(params);
    return status;
}

static void test_analyze(Workshop *workshop) {
    UrsatzError error;
    WorkshopReply reply;
    copy_file(URSATZ_FIXTURES "/framework-v1-corpus/Bach 114.mid", "Bach 114.mid");

    assert(call(workshop, "analyze", "{\"path\":\"Bach 114.mid\",\"layers\":true,\"model_plan\":true}", &reply, &error) == URSATZ_OK);
    assert(cJSON_IsArray(cJSON_GetObjectItemCaseSensitive(reply.json, "warnings")));
    assert(strcmp(text_member(reply.json, "document_path"), "Bach 114.analysis.json") == 0);
    char *document = mask_member(mask_member(strdup(text_member(reply.json, "document")), "analyzer_version"), "producer_key");
    char *golden = read_text(URSATZ_SNAPSHOTS "/bach-114.analysis.bytes.json");
    assert(strcmp(document, golden) == 0);
    free(document);
    free(golden);
    cJSON *realization = cJSON_Parse(text_member(reply.json, "realization"));
    cJSON *reading = cJSON_Parse(text_member(reply.json, "reading"));
    assert(cJSON_IsObject(realization) && cJSON_IsObject(reading));
    cJSON_Delete(realization);
    cJSON_Delete(reading);
    cJSON *plan = cJSON_Parse(text_member(reply.json, "plan"));
    assert(plan);
    const cJSON *producer = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(plan, "metadata"), "producer");
    assert(cJSON_IsString(producer) && strcmp(producer->valuestring, "ursatz-analyzer") == 0);
    cJSON_Delete(plan);
    workshop_reply_destroy(&reply);

    /* the text export, and no document */
    assert(call(workshop, "analyze", "{\"path\":\"Bach 114.mid\",\"text\":{}}", &reply, &error) == URSATZ_OK);
    assert(!cJSON_GetObjectItemCaseSensitive(reply.json, "document"));
    char *body = without_frame(text_member(reply.json, "text"));
    char *text = without_analyzer_line(body);
    free(body);
    golden = read_text(URSATZ_SNAPSHOTS "/bach-114.export.txt");
    assert(strcmp(text, golden) == 0);
    free(text);
    free(golden);
    workshop_reply_destroy(&reply);

    /* an empty range, a missing file, a pickup longer than the measure */
    assert(call(workshop, "analyze", "{\"path\":\"Bach 114.mid\",\"text\":{\"first_measure\":9000,\"last_measure\":9001}}", &reply, &error) ==
           URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/params/text") == 0);
    workshop_reply_destroy(&reply);
    assert(call(workshop, "analyze", "{\"path\":\"no-such-file.mid\"}", &reply, &error) == URSATZ_ERR_FORMAT);
    assert(strcmp(error.path, "/params/path") == 0 && error.message[0] != '\0');
    workshop_reply_destroy(&reply);
    assert(call(workshop, "analyze", "{\"path\":\"Bach 114.mid\",\"meter\":\"3/4\",\"pickup\":\"3/2\"}", &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/params/pickup") == 0);
    assert(strcmp(error.message, "the pickup must be shorter than one measure of the meter") == 0);
    workshop_reply_destroy(&reply);
    assert(call(workshop, "analyze", "{}", &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/params/path") == 0);
    workshop_reply_destroy(&reply);
}

static void test_corpus_and_style_profile(Workshop *workshop) {
    UrsatzError error;
    WorkshopReply reply;
    assert(common_dir_make("corpus-in", &error) == URSATZ_OK);
    copy_file("Bach 114.mid", "corpus-in/Bach 114.mid");

    assert(call(workshop, "corpus", "{\"dir\":\"corpus-in\",\"out\":\"corpus-out\"}", &reply, &error) == URSATZ_OK);
    assert(cJSON_GetObjectItemCaseSensitive(reply.json, "analyzed")->valuedouble == 1);
    assert(cJSON_GetObjectItemCaseSensitive(reply.json, "failed")->valuedouble == 0);
    assert(strcmp(text_member(reply.json, "out"), "corpus-out") == 0);
    const cJSON *pieces = cJSON_GetObjectItemCaseSensitive(reply.json, "pieces");
    assert(cJSON_GetArraySize(pieces) == 1);
    assert(strcmp(text_member(cJSON_GetArrayItem(pieces, 0), "state"), "analyzed") == 0);
    assert(strcmp(text_member(cJSON_GetArrayItem(pieces, 0), "path"), "Bach 114.mid") == 0);
    workshop_reply_destroy(&reply);
    assert(common_path_exists("corpus-out/corpus-index.json"));

    assert(call(workshop, "corpus", "{\"dir\":\"corpus-in\",\"out\":\"corpus-out\",\"resume\":true}", &reply, &error) == URSATZ_OK);
    assert(cJSON_GetObjectItemCaseSensitive(reply.json, "skipped")->valuedouble == 1);
    assert(cJSON_GetObjectItemCaseSensitive(reply.json, "analyzed")->valuedouble == 0);
    workshop_reply_destroy(&reply);

    assert(call(workshop, "corpus", "{\"dir\":\"no-such-folder\"}", &reply, &error) != URSATZ_OK);
    assert(error.message[0] != '\0');
    workshop_reply_destroy(&reply);

    assert(call(workshop, "style-profile", "{\"dir\":\"corpus-out\",\"name\":\"Sonatinas\"}", &reply, &error) == URSATZ_OK);
    assert(cJSON_GetObjectItemCaseSensitive(reply.json, "pieces")->valuedouble == 1);
    assert(cJSON_GetObjectItemCaseSensitive(reply.json, "stale")->valuedouble == 0);
    assert(strcmp(text_member(reply.json, "path"), "corpus-out/style-model.json") == 0);
    assert(common_path_exists("corpus-out/style-model.json"));
    workshop_reply_destroy(&reply);
    assert(call(workshop, "style-profile", "{\"dir\":\"corpus-out\",\"name\":\"\"}", &reply, &error) == URSATZ_ERR_REQUEST);
    assert(strcmp(error.path, "/params/name") == 0);
    workshop_reply_destroy(&reply);
}

int main(void) {
    UrsatzError error;
    /* what an earlier run that stopped half way left in this folder */
    (void)common_dir_remove("corpus-in", NULL);
    (void)common_dir_remove("corpus-out", NULL);
    Workshop *workshop = NULL;
    assert(workshop_tools_open(&workshop, &error) == URSATZ_OK);
    test_analyze(workshop);
    test_corpus_and_style_profile(workshop);
    workshop_close(workshop);
    assert(common_dir_remove("corpus-in", NULL) == URSATZ_OK);
    assert(common_dir_remove("corpus-out", NULL) == URSATZ_OK);
    remove("Bach 114.mid");
    puts("ok");
    return 0;
}
