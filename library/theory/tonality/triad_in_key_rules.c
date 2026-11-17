#include "theory/tonality/triad_in_key_rules.h"

#include <stddef.h>

#define MA STANDARD_TONALITY_TRIAD_MAJOR
#define MI STANDARD_TONALITY_TRIAD_MINOR
#define DIM STANDARD_TONALITY_TRIAD_DIMINISHED
#define AUG STANDARD_TONALITY_TRIAD_AUGMENTED

static const StandardTonalityTriadQuality QUALITIES[STANDARD_TONALITY_TRIAD_KEY_COUNT][7] = {
    {MA, MI, MI, MA, MA, MI, DIM}, /* major */
    {MI, DIM, MA, MI, MI, MA, MA}, /* natural minor */
    {MI, DIM, AUG, MI, MA, MA, DIM}, /* harmonic minor: raised leading tone */
};

static bool valid_key(StandardTonalityTriadKey key) {
    return key >= 0 && key < STANDARD_TONALITY_TRIAD_KEY_COUNT;
}

bool standard_tonality_triad_quality_in_key(StandardTonalityTriadKey key, int degree,
                                            StandardTonalityTriadQuality *quality) {
    if (!valid_key(key) || degree < 1 || degree > 7 || quality == NULL) {
        return false;
    }
    *quality = QUALITIES[key][degree - 1];
    return true;
}

