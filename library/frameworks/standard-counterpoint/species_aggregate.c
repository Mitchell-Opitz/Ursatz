#include "species_aggregate.h"

#include <stddef.h>

#include "harmonic_rules.h"

void standard_counterpoint_species_record(StandardCounterpointResult *result, unsigned group, unsigned hard,
                                          unsigned soft) {
    if (hard != 0) {
        result->hard_violations |= 1u << group;
    }
    if (soft != 0) {
        result->soft_violations |= 1u << group;
    }
}

bool standard_counterpoint_species_spacing_check(const StandardCounterpointLine *cantus_line,
                                                 const StandardCounterpointSlotLine *counterpoint_slots,
                                                 size_t slots_per_bar, StandardCounterpointCantusPart cantus_part,
                                                 unsigned *hard, unsigned *soft) {
    if (!standard_counterpoint_line_valid(cantus_line) || counterpoint_slots == NULL ||
        counterpoint_slots->slots == NULL || hard == NULL || soft == NULL || slots_per_bar == 0 ||
        counterpoint_slots->count != slots_per_bar * cantus_line->count ||
        counterpoint_slots->count > STANDARD_COUNTERPOINT_SPECIES_MAX_SLOTS) {
        return false;
    }
    const PitchSpelling *cantus_notes[STANDARD_COUNTERPOINT_SPECIES_MAX_SLOTS];
    const PitchSpelling *counterpoint_notes[STANDARD_COUNTERPOINT_SPECIES_MAX_SLOTS];
    size_t count = 0;
    for (size_t slot = 0; slot < counterpoint_slots->count; slot++) {
        const StandardCounterpointSlot *entry = &counterpoint_slots->slots[slot];
        if (entry->kind == STANDARD_COUNTERPOINT_SLOT_REST) {
            continue;
        }
        cantus_notes[count] = cantus_line->notes[slot / slots_per_bar];
        counterpoint_notes[count] = entry->pitch;
        count++;
    }

    StandardCounterpointLine cantus = {cantus_notes, count};
    StandardCounterpointLine counterpoint = {counterpoint_notes, count};
    StandardCounterpointPair pair;
    if (cantus_part == STANDARD_COUNTERPOINT_CANTUS_IN_LOWER) {
        pair.lower = &cantus;
        pair.upper = &counterpoint;
    } else {
        pair.lower = &counterpoint;
        pair.upper = &cantus;
    }

    static const StandardCounterpointHarmonicRule rules[] = {
        STANDARD_COUNTERPOINT_HARMONIC_RULE_MAX_GAP, STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE,
        STANDARD_COUNTERPOINT_HARMONIC_RULE_NO_CROSSING, STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION};
    unsigned hard_mask = 0;
    unsigned soft_mask = 0;
    for (size_t i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        if (standard_counterpoint_harmonic_rule_holds(rules[i], &pair)) {
            continue;
        }
        bool is_soft = rules[i] == STANDARD_COUNTERPOINT_HARMONIC_RULE_MOSTLY_WITHIN_OCTAVE ||
                       rules[i] == STANDARD_COUNTERPOINT_HARMONIC_RULE_CLIMAX_COORDINATION;
        if (is_soft) {
            soft_mask |= 1u << i;
        } else {
            hard_mask |= 1u << i;
        }
    }
    *hard = hard_mask;
    *soft = soft_mask;
    return true;
}
