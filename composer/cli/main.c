#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cJSON.h>

#include "counterpoint.h"
#include "develop.h"
#include "fill.h"
#include "grow.h"
#include "listen.h"
#include "motive.h"
#include "plan.h"
#include "progression.h"
#include "random_motive.h"
#include "recognize.h"
#include "regenerate.h"
#include "piece.h"
#include "theme.h"
#include "variations.h"
#include "variants_cmd.h"
#include "cli_options.h"

#include <ursatz_composer/composer_api.h>

static void print_usage(FILE *out) {
    fprintf(out, "Usage: ursatz-composer [--version] [--help]\n");
    fprintf(out, "       ursatz-composer export <plan.json> [--midi <out.mid>] [--musicxml <out.xml>] [--tempo <bpm>] [--plain-midi]\n");
    fprintf(out, "%s\n", PROGRESSION_USAGE_LINE);
    fprintf(out, "%s\n", MOTIVE_USAGE_LINE);
    fprintf(out, "%s\n", DEVELOP_USAGE_LINE);
    fprintf(out, "%s\n", GROW_USAGE_LINE);
    fprintf(out, "%s\n", THEME_USAGE_LINE);
    fprintf(out, "%s\n", PIECE_USAGE_LINE);
    fprintf(out, "%s\n", COUNTERPOINT_USAGE_LINE);
    fprintf(out, "%s\n", PLAN_USAGE_LINE);
    fprintf(out, "%s\n", FILL_USAGE_LINE);
    fprintf(out, "%s\n", LOCK_USAGE_LINE);
    fprintf(out, "%s\n", REGENERATE_USAGE_LINE);
    fprintf(out, "%s\n", VARIATIONS_USAGE_LINE);
    fprintf(out, "%s\n", LISTEN_USAGE_LINE);
    fprintf(out, "%s\n", VARIANTS_USAGE_LINE);
    fprintf(out, "%s\n", RECOGNIZE_USAGE_LINE);
    fprintf(out, "%s\n", RANDOM_MOTIVE_USAGE_LINE);
    fprintf(out, "%s\n", HARMONIZE_USAGE_LINE);
}

/* The whole file as text, NUL-terminated; false when it cannot be read. */
static bool read_text_file(const char *path, char **text, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    size_t capacity = 4096;
    size_t used = 0;
    char *buffer = malloc(capacity);
    bool ok = buffer != NULL;
    while (ok && !feof(file)) {
        if (used + 1 >= capacity) {
            char *grown = realloc(buffer, capacity * 2);
            ok = grown != NULL;
            if (ok) {
                buffer = grown;
                capacity *= 2;
            }
        }
        if (ok) {
            used += fread(buffer + used, 1, capacity - used - 1, file);
            ok = !ferror(file);
        }
    }
    fclose(file);
    if (!ok) {
        free(buffer);
        return false;
    }
    buffer[used] = '\0';
    *text = buffer;
    *size = used;
    return true;
}

/* Writes to "<path>.tmp", then renames, so a failure leaves no half-written file at path. */
static bool write_file_atomically(const char *path, const void *data, size_t size, char *message, size_t message_size) {
    size_t length = strlen(path);
    char *temporary = malloc(length + sizeof(".tmp"));
    if (!temporary) {
        snprintf(message, message_size, "out of memory");
        return false;
    }
    memcpy(temporary, path, length);
    memcpy(temporary + length, ".tmp", sizeof(".tmp"));
    FILE *file = fopen(temporary, "wb");
    bool wrote = file && fwrite(data, 1, size, file) == size;
    bool closed = file && fclose(file) == 0;
    if (!wrote || !closed || rename(temporary, path) != 0) {
        remove(temporary);
        snprintf(message, message_size, "could not write '%s'", path);
        free(temporary);
        return false;
    }
    free(temporary);
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
static int report_export_error(const ComposerError *error, const char *plan_path, const char *tempo_text) {
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

static int run_export(int argc, char **argv) {
    const char *plan_path = NULL;
    const char *midi_path = NULL;
    const char *musicxml_path = NULL;
    const char *tempo_text = NULL;
    bool plain_midi = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--tempo") == 0 && i + 1 < argc && !tempo_text) {
            tempo_text = argv[++i];
        } else if (strcmp(argv[i], "--plain-midi") == 0) {
            plain_midi = true;
        } else if (strcmp(argv[i], "--midi") == 0 && i + 1 < argc && !midi_path) {
            midi_path = argv[++i];
        } else if (strcmp(argv[i], "--musicxml") == 0 && i + 1 < argc && !musicxml_path) {
            musicxml_path = argv[++i];
        } else if (argv[i][0] != '-' && !plan_path) {
            plan_path = argv[i];
        } else {
            fprintf(stderr, "Error: unexpected argument '%s'\n", argv[i]);
            print_usage(stderr);
            return 1;
        }
    }
    if (!plan_path || (!midi_path && !musicxml_path)) {
        fprintf(stderr, "Error: export needs a plan file and at least one of --midi or --musicxml\n");
        print_usage(stderr);
        return 1;
    }

    char *plan_text = NULL;
    size_t plan_size = 0;
    if (!read_text_file(plan_path, &plan_text, &plan_size)) {
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
    char *request_text = cJSON_PrintUnformatted(request);
    cJSON_Delete(request);
    if (!request_text) {
        fprintf(stderr, "Error: out of memory\n");
        return 1;
    }

    ComposerResponse response;
    ComposerStatus api_status = composer_api_export(request_text, NULL, &response);
    free(request_text);
    if (api_status != COMPOSER_OK) {
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
    composer_response_destroy(&response);
    return status;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "export") == 0) {
        return run_export(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "progression") == 0) {
        return run_progression(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "motive") == 0) {
        return run_motive(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "develop") == 0) {
        return run_develop(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "grow") == 0) {
        return run_grow(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "theme") == 0) {
        return run_theme(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "piece") == 0) {
        return run_piece(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "counterpoint") == 0) {
        return run_counterpoint(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "plan") == 0) {
        return run_plan(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "fill") == 0) {
        return run_fill(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "lock") == 0) {
        return run_lock(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "regenerate") == 0) {
        return run_regenerate(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "variations") == 0) {
        return run_variations(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "listen") == 0) {
        return run_listen(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "variants") == 0) {
        return run_variants(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "recognize") == 0) {
        return run_recognize(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "random-motive") == 0) {
        return run_random_motive(argc, argv);
    }
    if (argc >= 2 && strcmp(argv[1], "harmonize") == 0) {
        return run_harmonize(argc, argv);
    }
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        ComposerResponse response;
        memset(&response, 0, sizeof response);
        cJSON *versions = composer_api_versions(&response) == COMPOSER_OK ? cJSON_Parse(response.json) : NULL;
        const cJSON *version = cJSON_GetObjectItemCaseSensitive(versions, "composer_version");
        printf("ursatz-composer %s\n", cJSON_IsString(version) ? version->valuestring : "");
        cJSON_Delete(versions);
        composer_response_destroy(&response);
        return 0;
    }
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_usage(stdout);
        return 0;
    }
    print_usage(stderr);
    return argc == 1 ? 0 : 1;
}
