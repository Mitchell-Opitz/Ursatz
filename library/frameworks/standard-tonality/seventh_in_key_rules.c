#include "seventh_in_key_rules.h"

#include <stddef.h>

#define MAMA STANDARD_TONALITY_SEVENTH_MAJOR_MAJOR
#define MAMI STANDARD_TONALITY_SEVENTH_MAJOR_MINOR
#define MIMI STANDARD_TONALITY_SEVENTH_MINOR_MINOR
#define HDIM STANDARD_TONALITY_SEVENTH_HALF_DIMINISHED
#define FDIM STANDARD_TONALITY_SEVENTH_FULLY_DIMINISHED

static const StandardTonalitySeventhQuality QUALITIES[STANDARD_TONALITY_SEVENTH_KEY_COUNT][7] = {
    {MAMA, MIMI, MIMI, MAMA, MAMI, MIMI, HDIM}, /* major */
    {MIMI, HDIM, MAMA, MIMI, MIMI, MAMA, MAMI}, /* natural minor */
    {MIMI, HDIM, MAMA, MIMI, MAMI, MAMA, FDIM}, /* minor, raised leading tone */
};

static bool valid_key(StandardTonalitySeventhKey key) {
    return key >= 0 && key < STANDARD_TONALITY_SEVENTH_KEY_COUNT;
}

bool standard_tonality_seventh_quality_in_key(StandardTonalitySeventhKey key, int degree,
                                              StandardTonalitySeventhQuality *quality) {
    if (!valid_key(key) || degree < 1 || degree > 7 || quality == NULL) {
        return false;
    }
    *quality = QUALITIES[key][degree - 1];
    return true;
}

