#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <reading/reading_json.h>
#include <realization/realization_json.h>
#include <realization/work.h>

#include "test_knowledge.h"
#include "ursatz_test.h"

/*
 * The layers of one stored piece (R-630): the Analyzer's reading and Michelle's reading of the realization of Mozart 545, which is
 * stored once.
 *
 *   test_layers_two_readings          analyzes Mozart 545 with the CLI (--layers), puts Michelle's reading beside the Analyzer's,
 *                                     reads the folder back and checks that both readings name the one realization
 *   test_layers_two_readings smoke    reads only the stored fixture (Michelle's reading) and checks it against the stored digest
 *
 * The digest below is that of the realization of tests/fixtures/framework-v1-corpus/Mozart 545.mid (the import's realization, R-628).
 * An import or realization change that moves it re-records tests/fixtures/readings/mozart-545.michelle.json (its README) and this
 * constant.
 */

#define MOZART_545_DIGEST "e8c0e66e5eeed3e1"

static void check_michelle(const Reading *reading) {
    const ReadingSource *source = reading_source(reading);
    const ReadingTarget *target = reading_target(reading);
    CHECKF(source->kind == READING_SOURCE_PERSON && strcmp(source->name, "michelle") == 0, "Michelle's reading is a person's");
    CHECKF(source->stages == 0 && source->producer_key[0] == '\0' && source->knowledge_hash[0] == '\0', "a person has no producer");
    CHECKF(strcmp(target->work, "w1") == 0 && strcmp(target->movement, "m1") == 0, "the target is work w1, movement m1");
    CHECKF(strcmp(target->digest, MOZART_545_DIGEST) == 0, "the target digest is %s, not %s", MOZART_545_DIGEST, target->digest);
    CHECKF(reading_region_count(reading) == 10, "Michelle's regions: %zu", reading_region_count(reading));
    CHECKF(reading_cadence_count(reading) == 5, "Michelle's cadences: %zu", reading_cadence_count(reading));
}

static Reading *read_reading(const char *path) {
    char *text = test_must_read_file(path, NULL);
    Reading *reading = NULL;
    ReadingJsonError error;
    RealizationResult status = reading_from_json(test_knowledge(), text, &reading, &error);
    CHECKF(status == REALIZATION_OK && reading, "%s: %s: %s", path, error.path, error.message);
    free(text);
    return reading;
}

static bool is_note_of(const Work *work, const char *id) {
    for (size_t m = 0; m < work_movement_count(work); m++) {
        const Movement *movement = work_movement_at(work, m);
        for (size_t p = 0; p < movement_part_count(movement); p++) {
            const PerformerPart *part = movement_part_at(movement, p);
            for (size_t n = 0; n < performer_part_note_count(part); n++) {
                if (strcmp(performer_part_note_at(part, n)->id, id) == 0) {
                    return true;
                }
            }
        }
    }
    return false;
}

static size_t count_files(const char *dir) {
    size_t count = 0;
    DIR *handle = opendir(dir);
    if (!handle) {
        return 0;
    }
    for (struct dirent *entry; (entry = readdir(handle)) != NULL;) {
        if (entry->d_name[0] != '.') {
            count++;
        }
    }
    closedir(handle);
    return count;
}

static void remove_layers(const char *dir) {
    const char *files[] = {"realization.json", "readings/analyzer.json", "readings/michelle.json"};
    char path[1024];
    for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
        snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
        unlink(path);
    }
    snprintf(path, sizeof(path), "%s/readings", dir);
    rmdir(path);
    rmdir(dir);
}

static void check_layers(void) {
    char *dir = test_temp_path("layers");
    char command[2048];
    char *copy = test_temp_path("layers-input");
    /* The CLI writes the document beside its input: run it on a copy. */
    char input[1024];
    snprintf(input, sizeof(input), "%s.mid", copy);
    size_t midi_size = 0;
    {
        FILE *in = fopen(URSATZ_FIXTURES_DIR "/framework-v1-corpus/Mozart 545.mid", "rb");
        FILE *out = fopen(input, "wb");
        CHECKF(in && out, "cannot copy the fixture to %s", input);
        char block[4096];
        size_t got;
        while (in && out && (got = fread(block, 1, sizeof(block), in)) > 0) {
            fwrite(block, 1, got, out);
            midi_size += got;
        }
        if (in) {
            fclose(in);
        }
        if (out) {
            fclose(out);
        }
    }
    snprintf(command, sizeof(command), "\"%s\" analyze --layers \"%s\" \"%s\" > /dev/null", URSATZ_ANALYZER_CLI, dir, input);
    CHECKF(midi_size > 0 && system(command) == 0, "the CLI could not write the layers: %s", command);

    char path[1024];
    char *michelle = test_must_read_file(URSATZ_FIXTURES_DIR "/readings/mozart-545.michelle.json", NULL);
    snprintf(path, sizeof(path), "%s/readings/michelle.json", dir);
    CHECKF(test_write_file(path, michelle, strlen(michelle)), "cannot write %s", path);
    free(michelle);

    /* The folder: one realization file and two reading files. */
    char readings[1024];
    snprintf(readings, sizeof(readings), "%s/readings", dir);
    CHECKF(count_files(dir) == 2, "the folder holds %zu entries, not the realization and readings", count_files(dir));
    CHECKF(count_files(readings) == 2, "readings/ holds %zu files, not 2", count_files(readings));

    /* The realization is read once. */
    snprintf(path, sizeof(path), "%s/realization.json", dir);
    char *text = test_must_read_file(path, NULL);
    Work *work = NULL;
    RealizationJsonError work_error;
    CHECKF(realization_work_from_json(test_knowledge(), text, &work, &work_error) == REALIZATION_OK && work, "%s: %s: %s", path,
           work_error.path, work_error.message);
    free(text);
    char digest[17] = "";
    CHECKF(work && realization_digest(work, digest), "no digest of the realization");
    CHECKF(strcmp(digest, MOZART_545_DIGEST) == 0, "the realization's digest is %s, not %s", digest, MOZART_545_DIGEST);

    /* Both readings name it. */
    snprintf(path, sizeof(path), "%s/readings/analyzer.json", dir);
    Reading *analyzer = read_reading(path);
    snprintf(path, sizeof(path), "%s/readings/michelle.json", dir);
    Reading *person = read_reading(path);
    const Reading *readings_of[2] = {analyzer, person};
    for (size_t i = 0; i < 2; i++) {
        const ReadingTarget *target = readings_of[i] ? reading_target(readings_of[i]) : NULL;
        CHECKF(target && strcmp(target->work, "w1") == 0 && strcmp(target->movement, "m1") == 0 && strcmp(target->digest, digest) == 0,
               "reading %zu does not name the realization (work w1, movement m1, digest %s)", i, digest);
    }

    if (analyzer && work) {
        const ReadingSource *source = reading_source(analyzer);
        CHECKF(source->kind == READING_SOURCE_ANALYZER && strcmp(source->name, "ursatz-analyzer") == 0, "the Analyzer's source");
        size_t checked = 0;
        for (size_t i = 0; i < reading_region_count(analyzer); i++) {
            const ReadingRegion *claim = reading_region_at(analyzer, i);
            for (size_t e = 0; e < claim->evidence.count; e++, checked++) {
                CHECKF(is_note_of(work, claim->evidence.ids[e]), "region evidence %s is not a performer note", claim->evidence.ids[e]);
            }
        }
        for (size_t i = 0; i < reading_cadence_count(analyzer); i++) {
            const ReadingCadence *claim = reading_cadence_at(analyzer, i);
            for (size_t e = 0; e < claim->evidence.count; e++, checked++) {
                CHECKF(is_note_of(work, claim->evidence.ids[e]), "cadence evidence %s is not a performer note", claim->evidence.ids[e]);
            }
        }
        for (size_t i = 0; i < reading_chord_count(analyzer); i++) {
            const ReadingChord *claim = reading_chord_at(analyzer, i);
            for (size_t e = 0; e < claim->evidence.count; e++, checked++) {
                CHECKF(is_note_of(work, claim->evidence.ids[e]), "chord evidence %s is not a performer note", claim->evidence.ids[e]);
            }
        }
        CHECKF(checked > 0, "the Analyzer's reading has no evidence");
    }
    if (person) {
        check_michelle(person);
    }

    reading_destroy(analyzer);
    reading_destroy(person);
    work_destroy(work);
    remove_layers(dir);
    unlink(input);
    free(copy);
    free(dir);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    if (smoke) {
        Reading *reading = read_reading(URSATZ_FIXTURES_DIR "/readings/mozart-545.michelle.json");
        if (reading) {
            check_michelle(reading);
        }
        reading_destroy(reading);
    } else {
        check_layers();
    }
    return test_finish("test_layers_two_readings");
}
