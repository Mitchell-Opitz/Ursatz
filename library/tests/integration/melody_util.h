#ifndef URSATZ_TESTS_INTEGRATION_MELODY_UTIL_H
#define URSATZ_TESTS_INTEGRATION_MELODY_UTIL_H

#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/outer_voice.h"
#include "analysis_melody.h"
#include "outer_scale_degrees.h"
#include "weighted_pipeline.h"

/*
 * Glue for the melody analysis: builds the neutral melody line of standard-counterpoint (analysis_melody.h) from the soprano
 * (or the bass) of the outer lines (analysis/outer_voice.h), the scale degrees of L2.6 (common-practice-minimal's
 * common_practice_scale_degree_label) and the non-chord-tone results of L2.5 (the weighted chord identification's leftover
 * tones). standard-counterpoint includes none of these; this glue, which includes them all, lives in the tests.
 *
 * What it does, per line entry:
 *   - Consecutive segments with the same note id are ONE note (a note held across segments is merged); a note that is
 *     repeated at the same pitch is a second note. Segments where the line is absent become ONE rest.
 *   - pitch: the line's PitchNumericValue (borrowed from the OuterVoiceLines). No spelling (the MIDI file has none).
 *   - degree and alteration: the L2.6 label of the pitch class in the key ("1" to "7", a "b" or "#" prefix).
 *   - strength: the BeatStrength of the note's attack, from the segment members built by the pipeline.
 *   - nct: from the L2.5 result of the note's FIRST segment: a segment with no chord match gives UNCLASSIFIED; a note listed
 *     among the chord's leftover tones gives NON_CHORD_TONE with its NonChordTone (or UNCLASSIFIED when L2.5 left it
 *     unclassified); a note not listed gives CHORD_TONE. nct_ref is "<segment id>/<note id>".
 *   - the entry's segment and its start tick (384 per quarter; a whole note is 1536) are kept beside the line, to place an
 *     entry in a measure.
 * Only segments starting before `limit_ticks` are used (the first eight measures of a 3/4 piece are 8 * 1152 ticks).
 */

typedef struct {
    StandardCounterpointMelodyLine line;
    size_t *segment; /* each entry's first segment */
    long *tick;      /* each entry's start in ticks (a rest's: the first segment where the line is absent) */
} MelodyBuild;

static inline void melody_build_destroy(MelodyBuild *build) {
    standard_counterpoint_melody_line_destroy(&build->line);
    free(build->segment);
    free(build->tick);
    memset(build, 0, sizeof(*build));
}

/* The degree and alteration of an L2.6 label ("1".."7", optionally "b" or "#" first). */
static inline void melody_parse_degree(const char *label, int *degree, int *alteration) {
    assert(label != NULL);
    *alteration = 0;
    if (*label == 'b') {
        *alteration = -1;
        label++;
    } else if (*label == '#') {
        *alteration = 1;
        label++;
    }
    assert(*label >= '1' && *label <= '7');
    *degree = *label - '0';
}

static inline BeatStrength melody_attack_strength(const CommonPracticeSegment *segment, const char *note_id,
                                                  bool *found) {
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

static inline void melody_build_from_line(const WeightedPipeline *p, const OuterVoiceEntry *entries,
                                          const CommonPracticeWeightedResults *results, int tonic_pitch_class,
                                          CommonPracticeKeyMode mode, long limit_ticks, MelodyBuild *out) {
    memset(out, 0, sizeof(*out));
    standard_counterpoint_melody_line_init(&out->line);
    out->segment = (size_t *)calloc(p->segment_count + 1, sizeof(size_t));
    out->tick = (long *)calloc(p->segment_count + 1, sizeof(long));
    assert(out->segment != NULL && out->tick != NULL);
    const char *previous_id = NULL;
    for (size_t s = 0; s < p->segment_count; s++) {
        long tick = (long)(p->starts[s] * 1536.0 + 0.5);
        if (tick >= limit_ticks) {
            break;
        }
        const OuterVoiceEntry *entry = &entries[s];
        if (!entry->present) {
            if (previous_id != NULL || out->line.count == 0) {
                out->segment[out->line.count] = s;
                out->tick[out->line.count] = tick;
                assert(standard_counterpoint_melody_line_add_rest(&out->line) == STANDARD_COUNTERPOINT_MELODY_OK);
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
        melody_parse_degree(common_practice_scale_degree_label((int)(((midi % 12) + 12) % 12), tonic_pitch_class, mode),
                            &note.degree, &note.alteration);
        bool has_strength = false;
        BeatStrength strength = melody_attack_strength(&p->segments[s], id, &has_strength);
        note.has_strength = has_strength;
        note.strength = strength;

        note.nct = STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED;
        const CommonPracticeWeightedResult *result = &results->items[s];
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
        snprintf(ref, sizeof(ref), "%s/%s", p->segment_ids[s], id);
        note.nct_ref = ref;
        out->segment[out->line.count] = s;
        out->tick[out->line.count] = tick;
        assert(standard_counterpoint_melody_line_add_note(&out->line, &note) == STANDARD_COUNTERPOINT_MELODY_OK);
    }
}

#endif
