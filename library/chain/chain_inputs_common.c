#include "chain/chain_inputs_internal.h"

#include <string.h>

int chain_compare_keys(const void *a, const void *b) {
    return strcmp(((const NoteKey *)a)->id, ((const NoteKey *)b)->id);
}

int chain_measure_of(const MeterMap *map, const MusicalTime *time) {
    int measure = 0;
    return meter_map_measure_index_at(map, time, &measure) == METER_MAP_OK ? measure : -1;
}

bool chain_parse_degree(const char *label, int *degree, int *alteration) {
    *alteration = 0;
    if (label == NULL) {
        return false;
    }
    if (*label == 'b') {
        *alteration = -1;
        label++;
    } else if (*label == '#') {
        *alteration = 1;
        label++;
    }
    if (*label < '1' || *label > '7') {
        return false;
    }
    *degree = *label - '0';
    return true;
}
