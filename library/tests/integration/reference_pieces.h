#ifndef URSATZ_TESTS_INTEGRATION_REFERENCE_PIECES_H
#define URSATZ_TESTS_INTEGRATION_REFERENCE_PIECES_H

#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "identity/entity_id.h"
#include "roman_numeral_adapter.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/note_value.h"
#include "time/time_signature.h"

/*
 * What the tests that read the reference pieces (Bach 114, Mozart 545, Clementi 36, Field 1, the chorales) share:
 *   three_four_map  Bach 114's file declares 4/4 and the piece is 3/4, so a 3/4 MeterMap replaces the file's
 *   claim_intervals the semitone intervals above the root of a Roman numeral claim's chord (triad or seventh, by its quality)
 *   roman_degree    the degree 1..7 of a Roman numeral's letters in either case ("ii", "V"), or -1
 */

static inline MeterMap *three_four_map(void) {
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *three_four = NULL;
    assert(time_signature_create(3, quarter, &three_four) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {three_four, zero};
    EntityID *id = NULL;
    assert(entity_id_create("bach-114-score-meter", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(three_four);
    musical_time_destroy(zero);
    return map;
}

static inline void claim_intervals(const StandardTonalityRomanClaim *claim, int out[4], size_t *count) {
    static const int TRIADS[4][3] = {{0, 4, 7}, {0, 3, 7}, {0, 3, 6}, {0, 4, 8}};
    static const int SEVENTHS[5][4] = {{0, 4, 7, 11}, {0, 4, 7, 10}, {0, 3, 7, 10}, {0, 3, 6, 10}, {0, 3, 6, 9}};
    memset(out, 0, 4 * sizeof(int));
    if (claim->seventh) {
        memcpy(out, SEVENTHS[claim->quality], 4 * sizeof(int));
        *count = 4;
    } else {
        memcpy(out, TRIADS[claim->quality], 3 * sizeof(int));
        *count = 3;
    }
}

static inline int roman_degree(const char *letters, size_t length) {
    static const char *const NAMES[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    char lower[8] = {0};
    assert(length < sizeof(lower));
    for (size_t i = 0; i < length; i++) {
        lower[i] = (char)tolower((unsigned char)letters[i]);
    }
    for (int d = 0; d < 7; d++) {
        if (strcmp(lower, NAMES[d]) == 0) {
            return d + 1;
        }
    }
    return -1;
}

#endif
