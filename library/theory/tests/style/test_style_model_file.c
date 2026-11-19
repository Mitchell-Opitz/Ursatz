#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "theory/style/style_model_file.h"
#include "theory/style/style_profile.h"

#include "ursatz_test.h"

/* The style model file (R-617): a profile written with the version of its producer, read back as a model with the same
   statistics, the named categories dropped on request, and every refusal. */

static char directory[512];

static void path_of(const char *name, char *path, size_t size) {
    snprintf(path, size, "%s/%s", directory, name);
}

static StyleProfile *hand_profile(void) {
    StyleProfile *profile = style_profile_create();
    StylePiece piece = {0};
    style_piece_add_chord(&piece, "I", true, 0);
    style_piece_add_chord(&piece, "V", true, 0);
    style_piece_add_chord(&piece, "I", true, 0);
    style_piece_add_cadence_type(&piece, "perfect-authentic");
    style_piece_add_melody_range(&piece, 9);
    style_piece_set_home_key_label(&piece, "C major");
    CHECKF(profile && style_profile_add_piece(profile, &piece) == STYLE_PROFILE_OK &&
               style_profile_add_piece(profile, &piece) == STYLE_PROFILE_OK &&
               style_profile_add_training_corpus(profile, "hand-corpus") == STYLE_PROFILE_OK,
           "hand profile");
    style_piece_destroy(&piece);
    return profile;
}

static char *read_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = calloc(1, 1 << 16);
    size_t size = text ? fread(text, 1, (1 << 16) - 1, file) : 0;
    fclose(file);
    if (text) {
        text[size] = '\0';
    }
    return text;
}

static void test_round_trip(void) {
    StyleProfile *profile = hand_profile();
    char first[600], second[600];
    path_of("first.json", first, sizeof first);
    path_of("second.json", second, sizeof second);
    CHECKF(profile && style_model_file_write(profile, "hand", "1.2.3", first) == STYLE_MODEL_FILE_OK &&
               style_model_file_write(profile, "hand", "1.2.3", second) == STYLE_MODEL_FILE_OK,
           "write");
    char *a = read_file(first), *b = read_file(second);
    CHECKF(a && b && strcmp(a, b) == 0, "the same profile always gives the same bytes");
    CHECKF(a && strstr(a, "\"model_version\":\t\"1.2.3\"") && strstr(a, "\"training_corpus\":\t\"hand-corpus\"") &&
               strstr(a, "\"pieces\":\t2") && strstr(a, "\"style_name\":\t\"hand\""),
           "the file names its producer's version, its corpus, its pieces and its style");
    free(a);
    free(b);

    StyleModel *model = NULL;
    size_t pieces = 0;
    StyleStatistic *written = NULL, *read = NULL;
    size_t written_count = 0, read_count = 0;
    CHECKF(style_model_file_read(first, &model, &pieces) == STYLE_MODEL_FILE_OK && model && pieces == 2, "read");
    CHECKF(model && style_model_statistics(model, &read, &read_count) == STYLE_MODEL_FILE_OK &&
               style_profile_statistics(profile, &written, &written_count) == STYLE_PROFILE_OK && read_count == written_count,
           "statistics of the model and of the profile");
    for (size_t i = 0; i < read_count && i < written_count; i++) {
        CHECKF(strcmp(read[i].category, written[i].category) == 0 && strcmp(read[i].label, written[i].label) == 0 &&
                   read[i].count == written[i].count && read[i].total == written[i].total,
               "statistic %zu", i);
    }
    free(read);
    free(written);
    style_model_destroy(model);
    style_profile_destroy(profile);
}

static void test_read_without(void) {
    StyleProfile *profile = hand_profile();
    char path[600];
    path_of("without.json", path, sizeof path);
    CHECKF(style_model_file_write(profile, "hand", "test", path) == STYLE_MODEL_FILE_OK, "write");
    const char *const dropped[] = {"cadence-type", "mode"};
    StyleModel *model = NULL;
    StyleStatistic *statistics = NULL;
    size_t count = 0;
    CHECKF(style_model_file_read_without(path, dropped, 2, &model, NULL) == STYLE_MODEL_FILE_OK && model &&
               style_model_statistics(model, &statistics, &count) == STYLE_MODEL_FILE_OK,
           "read without two categories");
    for (size_t i = 0; i < count; i++) {
        CHECKF(strcmp(statistics[i].category, "cadence-type") != 0 && strcmp(statistics[i].category, "mode") != 0,
               "a dropped category is gone: %s", statistics[i].category);
    }
    CHECKF(count > 0, "the other categories stay");
    free(statistics);
    style_model_destroy(model);
    const char *const unknown[] = {"no-such-category"};
    model = (StyleModel *)1;
    CHECKF(style_model_file_read_without(path, unknown, 1, &model, NULL) == STYLE_MODEL_FILE_ERR_ARGUMENT,
           "an unknown category name is refused");
    style_profile_destroy(profile);
}

static void test_refusals(void) {
    StyleProfile *profile = hand_profile();
    char path[600], missing[600], garbage[600];
    path_of("refusals.json", path, sizeof path);
    path_of("missing.json", missing, sizeof missing);
    path_of("garbage.json", garbage, sizeof garbage);
    CHECKF(style_model_file_write(NULL, "hand", "v", path) == STYLE_MODEL_FILE_ERR_ARGUMENT &&
               style_model_file_write(profile, NULL, "v", path) == STYLE_MODEL_FILE_ERR_ARGUMENT &&
               style_model_file_write(profile, "", "v", path) == STYLE_MODEL_FILE_ERR_ARGUMENT &&
               style_model_file_write(profile, "hand", NULL, path) == STYLE_MODEL_FILE_ERR_ARGUMENT &&
               style_model_file_write(profile, "hand", "v", NULL) == STYLE_MODEL_FILE_ERR_ARGUMENT,
           "an unnamed style, no version or no path is refused");
    CHECKF(style_model_file_write(profile, "hand", "v", "/no/such/directory/model.json") == STYLE_MODEL_FILE_ERR_IO,
           "an unwritable path is an I/O error");
    StyleModel *model = NULL;
    CHECKF(style_model_file_read(missing, &model, NULL) == STYLE_MODEL_FILE_ERR_IO && !model, "a missing file");
    FILE *file = fopen(garbage, "wb");
    CHECKF(file != NULL, "cannot write the garbage file");
    if (file) {
        fputs("{\"schema\": \"something-else\"}\n", file);
        fclose(file);
        CHECKF(style_model_file_read(garbage, &model, NULL) == STYLE_MODEL_FILE_ERR_FORMAT && !model, "not a style model file");
    }
    CHECKF(style_model_file_read(NULL, &model, NULL) == STYLE_MODEL_FILE_ERR_ARGUMENT, "no path");
    style_profile_destroy(profile);
}

int main(void) {
    snprintf(directory, sizeof directory, "%s", URSATZ_STYLE_MODEL_FILE_WORK_DIR);
    char command[1100];
    snprintf(command, sizeof command, "rm -rf '%s' && mkdir -p '%s'", directory, directory);
    CHECKF(system(command) == 0, "cannot prepare %s", directory);
    test_round_trip();
    test_read_without();
    test_refusals();
    return test_finish("theory_style_model_file");
}
