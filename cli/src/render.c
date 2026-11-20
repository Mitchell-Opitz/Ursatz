#include "render.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include <common/files.h>

#include "cli_options.h"
#include "cli_tools.h"

/* Writes the file atomically (common_file_write_atomic), so a failure leaves no half-written file at path. */
static bool write_file_atomically(const char *path, const void *data, size_t size, char *message, size_t message_size) {
    if (common_file_write_atomic(path, data, size, NULL) != URSATZ_OK) {
        snprintf(message, message_size, "could not write '%s'", path);
        return false;
    }
    return true;
}

/* The skipped lane spans of the API's export response, one line each. */
static void report_skipped(const char *json) {
    cJSON *response = cJSON_Parse(json);
    const cJSON *skip = NULL;
    cJSON_ArrayForEach(skip, cJSON_GetObjectItemCaseSensitive(response, "skipped")) {
        fprintf(stderr, "Skipped lane span %.0f (%s): %s\n", cJSON_GetObjectItemCaseSensitive(skip, "span")->valuedouble,
                cJSON_GetObjectItemCaseSensitive(skip, "lane")->valuestring,
                cJSON_GetObjectItemCaseSensitive(skip, "reason")->valuestring);
    }
    cJSON_Delete(response);
}

/* Prints the API's refusal as the export command always has and returns the exit status. */
static int report_export_error(const UrsatzError *error, const char *plan_path, const char *tempo_text) {
    static const char READ[] = "cannot read plan";
    static const char RENDER[] = "cannot render plan";
    if (strcmp(error->path, "/tempo") == 0) {
        fprintf(stderr,
                "Error: --tempo must be from 20 to 400 quarter notes per minute, a whole number or a fraction like "
                "195/2, not '%s'\n",
                tempo_text);
    } else if (strncmp(error->message, READ, sizeof(READ) - 1) == 0) {
        fprintf(stderr, "Error: cannot read plan '%s'%s\n", plan_path, error->message + sizeof(READ) - 1);
    } else if (strncmp(error->message, RENDER, sizeof(RENDER) - 1) == 0) {
        fprintf(stderr, "Error: cannot render plan '%s'\n", plan_path);
    } else {
        fprintf(stderr, "Error: %s\n", error->message);
    }
    return 1;
}

int run_render(const CliArgs *args) {
    const char *plan_path = args->input_count > 0 ? args->inputs[0] : NULL;
    const char *midi_path = cli_value(args, "--midi");
    const char *musicxml_path = cli_value(args, "--musicxml");
    const char *tempo_text = cli_value(args, "--tempo");
    bool plain_midi = cli_switch(args, "--plain-midi");
    if (args->help) {
        fprintf(stdout, "%s\n", RENDER_USAGE_LINE);
        return 0;
    }
    if (!plan_path || (!midi_path && !musicxml_path)) {
        fprintf(stderr, "Error: render needs a plan file and at least one of --midi or --musicxml\n");
        fprintf(stderr, "%s\n", RENDER_USAGE_LINE);
        return 1;
    }

    char *plan_text = NULL;
    size_t plan_size = 0;
    if (!cli_read_file(plan_path, &plan_text, &plan_size)) {
        fprintf(stderr, "Error: cannot read plan '%s': could not open '%s'\n", plan_path, plan_path);
        return 1;
    }
    cJSON *plan = cJSON_Parse(plan_text);
    free(plan_text);
    if (!plan) {
        fprintf(stderr, "Error: cannot read plan '%s': not valid JSON\n", plan_path);
        return 1;
    }
    cJSON *request = cJSON_CreateObject();
    if (!request || !cJSON_AddItemToObject(request, "plan", plan)) {
        fprintf(stderr, "Error: out of memory\n");
        cJSON_Delete(plan);
        cJSON_Delete(request);
        return 1;
    }
    cJSON_AddBoolToObject(request, "midi", midi_path != NULL);
    cJSON_AddBoolToObject(request, "musicxml", musicxml_path != NULL);
    cJSON_AddBoolToObject(request, "plain_midi", plain_midi);
    if (tempo_text) {
        cJSON_AddStringToObject(request, "tempo", tempo_text);
    }
    if (!request) {
        fprintf(stderr, "Error: out of memory\n");
        return 1;
    }

    CliResponse response;
    UrsatzStatus api_status = cli_composer("export", request, NULL, &response);
    cJSON_Delete(request);
    if (api_status != URSATZ_OK) {
        return report_export_error(&response.error, plan_path, tempo_text);
    }
    report_skipped(response.json);

    int status = 0;
    char message[160];
    if (midi_path && !write_file_atomically(midi_path, response.midi, response.midi_size, message, sizeof message)) {
        fprintf(stderr, "Error: MIDI export failed: %s\n", message);
        status = 1;
    } else if (midi_path) {
        printf("MIDI written to %s\n", midi_path);
    }
    if (musicxml_path &&
        !write_file_atomically(musicxml_path, response.musicxml, strlen(response.musicxml), message, sizeof message)) {
        fprintf(stderr, "Error: MusicXML export failed: %s\n", message);
        status = 1;
    } else if (musicxml_path) {
        printf("MusicXML written to %s\n", musicxml_path);
    }
    cli_response_destroy(&response);
    return status;
}

