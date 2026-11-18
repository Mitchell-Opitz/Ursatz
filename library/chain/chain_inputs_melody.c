#include "chain/chain_inputs_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/harmony/outer_scale_degrees.h"
#include "model/pitch/midi_pitch.h"

/* --- the melody line --- */

void chain_melody_line_destroy(ChainMelodyLine *melody) {
    if (melody == NULL) {
        return;
    }
    standard_counterpoint_melody_line_destroy(&melody->line);
    free(melody->segment);
    memset(melody, 0, sizeof(*melody));
}

static BeatStrength member_attack_strength(const CommonPracticeSegment *segment, const char *note_id, bool *found) {
    for (size_t j = 0; j < segment->member_count; j++) {
        const CommonPracticeMember *member = &segment->members[j];
        if (member->note_id != NULL && strcmp(member->note_id, note_id) == 0 && !member->held) {
            *found = true;
            return member->strength;
        }
    }
    *found = false;
    return BEAT_STRENGTH_WEAK;
}

ChainInputStatus chain_build_melody_line(const ChainSegments *line_segments, size_t line_count, const OuterVoiceEntry *entries,
                                         const ChainSegments *segments, size_t count, const CommonPracticeWeightedResult *results,
                                         int tonic_pitch_class, CommonPracticeKeyMode mode, const CommonPracticeMaterials *materials,
                                         ChainMelodyLine *out) {
    if (line_segments == NULL || segments == NULL || entries == NULL || results == NULL || materials == NULL || out == NULL) {
        return CHAIN_INPUT_ERR_NULL_ARGUMENT;
    }
    if (line_segments->starts == NULL || segments->segments == NULL || segments->starts == NULL || line_count == 0 ||
        line_count > line_segments->count || count == 0 || count > segments->count) {
        return CHAIN_INPUT_ERR_BAD_NOTES;
    }
    ChainMelodyLine built;
    memset(&built, 0, sizeof(built));
    standard_counterpoint_melody_line_init(&built.line);
    built.segment = calloc(line_count + 1, sizeof(size_t));
    if (built.segment == NULL) {
        return CHAIN_INPUT_ERR_ALLOC;
    }
    const char *previous_id = NULL;
    size_t chord = 0; /* the chord segment holding the entry's line segment: the last that starts at or before it */
    for (size_t s = 0; s < line_count; s++) {
        const OuterVoiceEntry *entry = &entries[s];
        while (chord + 1 < count && rational_cmp(segments->starts[chord + 1], line_segments->starts[s]) <= 0) {
            chord++;
        }
        if (!entry->present) {
            if (previous_id != NULL || built.line.count == 0) {
                built.segment[built.line.count] = chord;
                if (standard_counterpoint_melody_line_add_rest(&built.line) != STANDARD_COUNTERPOINT_MELODY_OK) {
                    chain_melody_line_destroy(&built);
                    return CHAIN_INPUT_ERR_BAD_NOTES;
                }
            }
            previous_id = NULL;
            continue;
        }
        const char *id = entity_id_value(entry->note_id);
        if (previous_id != NULL && strcmp(previous_id, id) == 0) {
            continue; /* the same note, held into this segment */
        }
        previous_id = id;
        StandardCounterpointMelodyNote note;
        memset(&note, 0, sizeof(note));
        note.present = true;
        note.note_id = id;
        note.pitch = entry->pitch;
        note.confidence = 1.0;
        long midi = pitch_numeric_value_position_numerator(entry->pitch);
        if (!chain_parse_degree(common_practice_scale_degree_label(pitch_class_of_midi(midi), tonic_pitch_class, mode, materials), &note.degree,
                          &note.alteration)) {
            chain_melody_line_destroy(&built);
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
        bool has_strength = false;
        note.strength = member_attack_strength(&segments->segments[chord], id, &has_strength);
        note.has_strength = has_strength;

        note.nct = STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED;
        const CommonPracticeWeightedResult *result = &results[chord];
        if (result->matched) {
            note.nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
            for (size_t k = 0; k < result->leftover_count; k++) {
                if (strcmp(result->leftover[k].note_id, id) == 0) {
                    if (result->leftover[k].classified) {
                        note.nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
                        note.nct_type = result->leftover[k].tone;
                    } else {
                        note.nct = STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED;
                    }
                }
            }
        }
        char ref[160];
        snprintf(ref, sizeof(ref), "%s/%s", segments->ids[chord], id);
        note.nct_ref = ref;
        built.segment[built.line.count] = chord;
        if (standard_counterpoint_melody_line_add_note(&built.line, &note) != STANDARD_COUNTERPOINT_MELODY_OK) {
            chain_melody_line_destroy(&built);
            return CHAIN_INPUT_ERR_BAD_NOTES;
        }
    }
    *out = built;
    return CHAIN_INPUT_OK;
}
