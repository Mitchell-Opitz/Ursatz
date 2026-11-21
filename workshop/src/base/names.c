#include "base/names.h"

#include <stdio.h>

#include <model/pitch/pitch_arithmetic.h>

#define PITCH_NAMES_MAX 4096

UrsatzStatus workshop_pitch_names(const cJSON *midi, cJSON **names, UrsatzError *error) {
    *names = NULL;
    if (!cJSON_IsArray(midi) || cJSON_GetArraySize(midi) > PITCH_NAMES_MAX) {
        return ursatz_error_set(error, URSATZ_ERR_REQUEST, "/params/midi", "midi is an array of up to %d whole numbers", PITCH_NAMES_MAX);
    }
    cJSON *array = cJSON_CreateArray();
    if (!array) {
        return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
    }
    int index = 0;
    const cJSON *item = NULL;
    cJSON_ArrayForEach(item, midi) {
        if (!cJSON_IsNumber(item) || item->valuedouble < 0 || item->valuedouble > 127 || item->valuedouble != (double)(int)item->valuedouble) {
            cJSON_Delete(array);
            char path[48];
            snprintf(path, sizeof(path), "/params/midi/%d", index);
            return ursatz_error_set(error, URSATZ_ERR_REQUEST, path, "a MIDI number is a whole number from 0 to 127");
        }
        int number = (int)item->valuedouble;
        char text[16];
        pitch_name_of_midi(number, false, text, sizeof(text));
        cJSON *name = cJSON_CreateString(text);
        if (!name) {
            cJSON_Delete(array);
            return ursatz_error_set(error, URSATZ_ERR_ALLOC, "", "out of memory");
        }
        cJSON_AddItemToArray(array, name);
        index++;
    }
    *names = array;
    return URSATZ_OK;
}
