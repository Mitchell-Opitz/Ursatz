#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_workshop/workshop_api.h>

#include "coordinator/command_schemas.h"
#include "ursatz_test.h"

/*
 * The command reference (R-664): workshop/docs/command-reference.md is the markdown the tool `commands` generates, byte for byte
 * (URSATZ_UPDATE_GOLDENS=1 writes the page); the sheet is at most 120 lines; every command has an example.
 */

#ifndef TEXT_COMMAND_REFERENCE
#error "TEXT_COMMAND_REFERENCE must name workshop/docs/command-reference.md"
#endif

static char *tool_text(Workshop *workshop, const char *params_text) {
    cJSON *params = cJSON_Parse(params_text);
    WorkshopReply reply;
    UrsatzError error = {{0}, {0}};
    UrsatzStatus status = workshop_tool(workshop, "commands", params, NULL, &reply, &error);
    cJSON_Delete(params);
    const cJSON *text = cJSON_GetObjectItemCaseSensitive(reply.json, "text");
    char *copy = status == URSATZ_OK && cJSON_IsString(text) ? strdup(text->valuestring) : NULL;
    CHECKF(copy != NULL, "%s: %s", params_text, error.message);
    workshop_reply_destroy(&reply);
    return copy;
}

static size_t lines_of(const char *text) {
    size_t lines = 0;
    for (const char *p = text; *p; p++) {
        lines += *p == '\n' ? 1 : 0;
    }
    return lines;
}

int main(void) {
    UrsatzError error;
    Workshop *workshop = NULL;
    if (workshop_tools_open(&workshop, &error) != URSATZ_OK) {
        fprintf(stderr, "cannot open the tools workshop: %s\n", error.message);
        return 1;
    }
    char *markdown = tool_text(workshop, "{\"format\":\"markdown\"}");
    if (markdown && test_update_goldens()) {
        CHECKF(test_write_file(TEXT_COMMAND_REFERENCE, markdown, strlen(markdown)), "cannot write %s", TEXT_COMMAND_REFERENCE);
        free(markdown);
        workshop_close(workshop);
        return test_goldens_updated("command_reference");
    }
    char *page = test_read_file(TEXT_COMMAND_REFERENCE, NULL);
    CHECKF(page != NULL, "no %s (URSATZ_UPDATE_GOLDENS=1 writes it)", TEXT_COMMAND_REFERENCE);
    CHECKF(markdown && page && strcmp(markdown, page) == 0,
           "workshop/docs/command-reference.md is not what the tool commands generates: run the test with URSATZ_UPDATE_GOLDENS=1 to write it");
    free(page);
    free(markdown);

    char *sheet = tool_text(workshop, "{\"format\":\"sheet\"}");
    CHECKF(sheet && lines_of(sheet) <= 120, "the sheet is %zu lines, at most 120", sheet ? lines_of(sheet) : 0);
    CHECK(sheet && strstr(sheet, "item.save item:<id>@v<n> body object [note string]") != NULL);
    CHECK(sheet && strstr(sheet, "project.create name string [description string]  ") != NULL);
    free(sheet);

    char *grammar = tool_text(workshop, "{\"format\":\"grammar\"}");
    CHECK(grammar && strstr(grammar, "item-save = \"item.save\" , \"item:\" , id , \"@v\" , integer , { ") != NULL);
    free(grammar);

    char *json = tool_text(workshop, "{\"format\":\"json\"}");
    cJSON *all = json ? cJSON_Parse(json) : NULL;
    CHECK(cJSON_IsArray(all) && cJSON_GetArraySize(all) == 43);
    const cJSON *command;
    int examples = 0;
    cJSON_ArrayForEach(command, all) {
        const cJSON *example = cJSON_GetObjectItemCaseSensitive(command, "example");
        CHECKF(cJSON_IsString(example) && example->valuestring[0] != '\0', "%s has an example", cJSON_GetObjectItemCaseSensitive(command, "name")->valuestring);
        /* the examples are in the text grammar: the parser reads each into a command of the same name */
        if (cJSON_IsString(example)) {
            cJSON *parse_params = cJSON_CreateObject();
            cJSON_AddStringToObject(parse_params, "text", example->valuestring);
            WorkshopReply parsed;
            UrsatzError parse_error = {{0}, {0}};
            UrsatzStatus parse_status = workshop_tool(workshop, "commands-parse", parse_params, NULL, &parsed, &parse_error);
            const cJSON *parsed_commands = cJSON_GetObjectItemCaseSensitive(parsed.json, "commands");
            CHECKF(parse_status == URSATZ_OK && cJSON_GetArraySize(parsed_commands) == 1 &&
                       strcmp(cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(parsed_commands, 0), "command")->valuestring,
                              cJSON_GetObjectItemCaseSensitive(command, "name")->valuestring) == 0,
                   "the example of %s parses: %s\n%s", cJSON_GetObjectItemCaseSensitive(command, "name")->valuestring, parse_error.message, example->valuestring);
            workshop_reply_destroy(&parsed);
            cJSON_Delete(parse_params);
            examples++;
        }
        const cJSON *summary = cJSON_GetObjectItemCaseSensitive(command, "summary");
        CHECKF(cJSON_IsString(summary) && strlen(summary->valuestring) <= 80 && strstr(summary->valuestring, "\xe2\x80\x94") == NULL,
               "%s has a summary of at most 80 characters", cJSON_GetObjectItemCaseSensitive(command, "name")->valuestring);
    }
    cJSON_Delete(all);
    free(json);
    CHECKF(examples == 43, "%d examples parsed", examples);

    char *help = tool_text(workshop, "{\"format\":\"help\",\"name\":\"item.save\"}");
    CHECK(help && strncmp(help, "item.save item:<id>@v<n> body object [note string]\n", 50) == 0 && strstr(help, "Example:") != NULL && strstr(help, "Base:") != NULL);
    free(help);
    cJSON *params = cJSON_Parse("{\"format\":\"help\",\"name\":\"item.nope\"}");
    WorkshopReply reply;
    CHECK(workshop_tool(workshop, "commands", params, NULL, &reply, &error) == URSATZ_ERR_NOT_FOUND && strcmp(error.path, "/params/name") == 0);
    workshop_reply_destroy(&reply);
    cJSON_Delete(params);
    params = cJSON_Parse("{\"format\":\"nothing\"}");
    CHECK(workshop_tool(workshop, "commands", params, NULL, &reply, &error) == URSATZ_ERR_REQUEST && strcmp(error.path, "/params/format") == 0);
    workshop_reply_destroy(&reply);
    cJSON_Delete(params);
    workshop_close(workshop);
    return test_finish("command_reference");
}
