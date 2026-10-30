#include "pipeline/melody_pipeline.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis_melody.h"
#include "analysis_melody_climax.h"
#include "analysis_melody_leaps.h"
#include "analysis_melody_nct.h"
#include "cantus_firmus_rules.h"
#include "key_vocabulary.h"
#include "outer_scale_degrees.h"
#include "pipeline/pitch_class_util.h"
#include "pipeline/spelling_util.h"
#include "semantics/beat_strength.h"
#include "pitch/pitch_spelling.h"
#include "standard-counterpoint/framework.h"
#include "tendency_rules.h"
#include "theory/framework.h"
#include "util/misc_util.h"

_Static_assert(NON_CHORD_TONE_COUNT == ANALYSIS_MELODY_NCT_TYPES, "the report counts every NonChordTone");

/* A phrase's line: the library's line and, per entry, the ImportedNotes index of its note (SIZE_MAX for a rest). */
typedef struct {
    StandardCounterpointMelodyLine line;
    size_t *note;
    char (*name)[16]; /* each entry's note name, e.g. "E5"; empty for a rest */
    const AnalysisKeyRegion **region; /* the key region each note starts in; NULL for a rest or outside every region */
    size_t note_count; /* entries that are notes */
} PhraseLine;

static void phrase_line_destroy(PhraseLine *built) {
    standard_counterpoint_melody_line_destroy(&built->line);
    free(built->note);
    free(built->name);
    free(built->region);
    memset(built, 0, sizeof(*built));
}

/* The position of the voice segment's member that is the entry's note, or member_count when it is none. */
static size_t member_of(const Segment *segment, const OuterVoiceEntry *entry) {
    const char *id = entity_id_value(entry->note_id);
    for (size_t m = 0; m < segment->member_count; m++) {
        if (strcmp(segment->members[m].note_id, id) == 0) {
            return m;
        }
    }
    return segment->member_count;
}

/* The key region the time lies in, or the last one for a time past the end; NULL with none. */
static const AnalysisKeyRegion *region_at(const AnalysisReport *report, long long n, long long d) {
    for (size_t r = 0; r < report->key_region_count; r++) {
        const AnalysisKeyRegion *region = &report->key_regions[r];
        if (fraction_compare(n, d, region->end_numerator, (long long)region->end_denominator) < 0) {
            return region;
        }
    }
    return report->key_region_count > 0 ? &report->key_regions[report->key_region_count - 1] : NULL;
}

/* A degree label as the library names it: "1" to "7", with a "b" or "#" before it for an altered degree. */
static bool parse_degree(const char *label, int *degree, int *alteration) {
    *alteration = 0;
    if (!label) {
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

/* The note's name as the outer voice lines name it: as the input spells it when it does, else by the key region's
   flats or sharps. */
static void name_note(const ImportedNotes *notes, size_t note, long pitch, const AnalysisKeyRegion *region,
                      char out[16]) {
    if (notes->source.spelling_notated && spelled_note_name(notes->events[note], pitch, true, out)) {
        return;
    }
    snprintf(out, 16, "%s%d", pitch_class_name(pitch_class_of(pitch), region ? region->prefer_flats : false),
             (int)(pitch / 12) - 1);
}

/* The library's non-chord-tone result for a note, from the chord identification of the chord segment its voice segment
   starts in: no chord gives UNCLASSIFIED; a note listed among the chord's non-chord tones gives NON_CHORD_TONE with the
   type (UNCLASSIFIED when the library left the type open); a note not listed gives CHORD_TONE. Ported from
   melody_build_from_line. chord is a cursor over the entries, which only moves forward. */
static void classify_note(const AnalysisReport *report, const Segment *segment, size_t note, size_t *chord,
                          size_t *tone, StandardCounterpointMelodyNote *out) {
    out->nct = STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED;
    while (*chord + 1 < report->entry_count &&
           fraction_compare(segment->start_numerator, (long long)segment->start_denominator,
                            report->entries[*chord].end_numerator,
                            (long long)report->entries[*chord].end_denominator) >= 0) {
        (*chord)++;
    }
    if (*chord >= report->entry_count || report->entries[*chord].chord.label == NULL) {
        return;
    }
    out->nct = STANDARD_COUNTERPOINT_NCT_CHORD_TONE;
    while (*tone < report->non_chord_tone_count && report->non_chord_tones[*tone].chord_index < *chord) {
        (*tone)++;
    }
    for (size_t t = *tone; t < report->non_chord_tone_count && report->non_chord_tones[t].chord_index == *chord; t++) {
        const AnalysisNonChordTone *found = &report->non_chord_tones[t];
        if (found->note_id != note) {
            continue;
        }
        out->nct = STANDARD_COUNTERPOINT_NCT_UNCLASSIFIED;
        for (int type = 0; found->type && type < NON_CHORD_TONE_COUNT; type++) {
            if (strcmp(non_chord_tone_name((NonChordTone)type), found->type) == 0) {
                out->nct = STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE;
                out->nct_type = (NonChordTone)type;
            }
        }
    }
}

/* Adds the soprano note of voice segment v to the line. False on allocation or library failure. */
static bool add_note(PhraseLine *built, const ImportedNotes *notes, const AnalysisReport *report,
                     const Segmentation *voices, size_t v, const OuterVoiceEntry *entry, size_t member,
                     size_t *chord, size_t *tone) {
    const Segment *segment = &voices->segments[v];
    const CommonPracticeMember *attack = &segment->members[member];
    StandardCounterpointMelodyNote note;
    memset(&note, 0, sizeof(note));
    note.present = true;
    note.note_id = entity_id_value(entry->note_id);
    note.pitch = entry->pitch;
    note.confidence = 1.0;
    note.has_strength = !attack->held;
    note.strength = attack->strength;
    const AnalysisKeyRegion *region = region_at(report, segment->start_numerator, (long long)segment->start_denominator);
    long pitch = pitch_numeric_value_position_numerator(entry->pitch);
    if (region) {
        parse_degree(common_practice_scale_degree_label(
                         (int)(((pitch % 12) + 12) % 12), region->tonic_pitch_class,
                         region->is_major ? COMMON_PRACTICE_KEY_MAJOR : COMMON_PRACTICE_KEY_MINOR),
                     &note.degree, &note.alteration);
    }
    classify_note(report, segment, segment->note_indices[member], chord, tone, &note);
    char ref[96];
    snprintf(ref, sizeof(ref), "%s/%s", voices->segment_ids[v], note.note_id);
    note.nct_ref = ref;
    built->note[built->line.count] = segment->note_indices[member];
    name_note(notes, segment->note_indices[member], pitch, region, built->name[built->line.count]);
    built->region[built->line.count] = region;
    if (standard_counterpoint_melody_line_add_note(&built->line, &note) != STANDARD_COUNTERPOINT_MELODY_OK) {
        return false;
    }
    built->note_count++;
    return true;
}

/* Builds the line of the soprano notes whose voice segment starts in [start, end). */
static bool build_phrase_line(const ImportedNotes *notes, const Segmentation *voices, const OuterVoiceLines *lines, const AnalysisReport *report,
                              const AnalysisPhrase *phrase, PhraseLine *built) {
    memset(built, 0, sizeof(*built));
    standard_counterpoint_melody_line_init(&built->line);
    built->note = malloc(sizeof(size_t) * (voices->segment_count + 1));
    built->name = calloc(voices->segment_count + 1, sizeof(char[16]));
    built->region = calloc(voices->segment_count + 1, sizeof(const AnalysisKeyRegion *));
    if (!built->note || !built->name || !built->region) {
        return false;
    }
    const char *previous = NULL;
    bool started = false;
    size_t chord = 0;
    size_t tone = 0;
    for (size_t v = 0; v < voices->segment_count; v++) {
        const Segment *segment = &voices->segments[v];
        if (fraction_compare(segment->start_numerator, (long long)segment->start_denominator, phrase->start_numerator,
                             (long long)phrase->start_denominator) < 0) {
            continue;
        }
        if (fraction_compare(segment->start_numerator, (long long)segment->start_denominator, phrase->end_numerator,
                             (long long)phrase->end_denominator) >= 0) {
            break;
        }
        const OuterVoiceEntry *entry = &lines->soprano[v];
        size_t member = entry->present ? member_of(segment, entry) : segment->member_count;
        if (member >= segment->member_count) {
            if (started && previous != NULL) { /* one rest for a gap, none before the first note */
                built->note[built->line.count] = SIZE_MAX;
                if (standard_counterpoint_melody_line_add_rest(&built->line) != STANDARD_COUNTERPOINT_MELODY_OK) {
                    return false;
                }
            }
            previous = NULL;
            continue;
        }
        const char *id = entity_id_value(entry->note_id);
        if (previous != NULL && strcmp(previous, id) == 0) {
            continue; /* the same note, held into this segment */
        }
        previous = id;
        if (!started && segment->members[member].held) {
            continue; /* held in from the phrase before: not an attack of this phrase */
        }
        started = true;
        if (!add_note(built, notes, report, voices, v, entry, member, &chord, &tone)) {
            return false;
        }
    }
    return true;
}

static const char *direction_name(int direction) {
    return direction == CONTOUR_DIRECTION_UP ? "up" : (direction == CONTOUR_DIRECTION_DOWN ? "down" : "same");
}

static long pitch_of(const PhraseLine *built, int entry) {
    return pitch_numeric_value_position_numerator(built->line.notes[entry].pitch);
}

static void free_melody(AnalysisMelody *melody) {
    free(melody->note_ids);
    free(melody->directions);
    free(melody->leaps);
    free(melody->consecutive_leaps);
    free(melody->ncts);
    free(melody->seventh_breaks);
}

typedef enum { ANALYZE_OK, ANALYZE_DECLINED, ANALYZE_ALLOC } AnalyzeResult;

/* The seventh degree and the library's tendencies of a phrase's line, judged on spellings the way the outer voice lines
   spell the notes. Leaves the tendency fields empty when a note's name does not spell. */
static AnalyzeResult read_tendencies(const PhraseLine *built, AnalysisMelody *out) {
    size_t count = built->line.count;
    PitchSpelling **spelled = calloc(count + 1, sizeof(PitchSpelling *));
    const PitchSpelling **notes_only = calloc(count + 1, sizeof(const PitchSpelling *));
    out->seventh_breaks = calloc(count + 1, sizeof(AnalysisMelodyBreak));
    AnalyzeResult result = ANALYZE_ALLOC;
    if (!spelled || !notes_only || !out->seventh_breaks) {
        goto done;
    }
    out->seventh_rule = standard_counterpoint_cantus_rule_name(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE);
    size_t note_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (!built->line.notes[i].present) {
            continue;
        }
        int spell = spelling_from_name(built->name[i], &spelled[i]);
        if (spell < 0) {
            goto done;
        }
        if (spell == 0) {
            result = ANALYZE_DECLINED;
            goto done;
        }
        notes_only[note_count++] = spelled[i];
    }
    StandardCounterpointLine line = {notes_only, note_count};
    for (int t = 0; t < ANALYSIS_MELODY_TENDENCIES; t++) {
        out->tendency_names[t] = standard_counterpoint_tendency_name((StandardCounterpointTendency)t);
        out->tendency_holds[t] = standard_counterpoint_tendency_holds((StandardCounterpointTendency)t, &line);
    }
    for (size_t i = 0; i + 1 < count; i++) {
        const StandardCounterpointMelodyNote *note = &built->line.notes[i];
        if (!note->present || note->degree != 7 || !built->line.notes[i + 1].present || !built->region[i]) {
            continue;
        }
        const AnalysisKeyRegion *region = built->region[i];
        char tonic_name[16];
        snprintf(tonic_name, sizeof(tonic_name), "%s4", pitch_class_name(region->tonic_pitch_class, region->prefer_flats));
        PitchSpelling *tonic = NULL;
        int spell = spelling_from_name(tonic_name, &tonic);
        if (spell <= 0) {
            result = spell < 0 ? ANALYZE_ALLOC : ANALYZE_DECLINED;
            goto done;
        }
        const PitchSpelling *pair_notes[2] = {spelled[i], spelled[i + 1]};
        StandardCounterpointLine pair = {pair_notes, 2};
        bool followed = standard_counterpoint_cantus_rule_holds(STANDARD_COUNTERPOINT_CANTUS_RULE_LEADING_TONE, &pair,
                                                                 tonic);
        pitch_spelling_destroy(tonic);
        if (followed) {
            out->seventh_followed++;
        } else {
            AnalysisMelodyBreak *broken = &out->seventh_breaks[out->seventh_broken++];
            broken->note = built->note[i];
            broken->next_note = built->note[i + 1];
            broken->uncertainty = region->uncertainty;
        }
    }
    result = ANALYZE_OK;
done:
    for (size_t i = 0; spelled && i < count; i++) {
        pitch_spelling_destroy(spelled[i]);
    }
    free(spelled);
    free(notes_only);
    return result;
}


/* Reads the library's results for one phrase's line into out. ANALYZE_DECLINED when the library gives none for the
   line (out is then released by the caller). */
static AnalyzeResult analyze_phrase(const PhraseLine *built, size_t phrase_index, const AnalysisPhrase *phrase,
                           AnalysisMelody *out) {
    StandardCounterpointMelodicIntervals intervals = {0};
    StandardCounterpointContourSummaries summaries = {0};
    StandardCounterpointClimaxes climaxes = {0};
    StandardCounterpointLeapRecoveries recoveries = {0};
    StandardCounterpointConsecutiveLeapList consecutive = {0};
    StandardCounterpointNctLabels labels = {0};
    StandardCounterpointNctCountList counts = {0};
    AnalyzeResult result = ANALYZE_DECLINED;
    StandardCounterpointMelodyStatus status =
        standard_counterpoint_melody_intervals(&built->line, NULL, &intervals);
    if (status == STANDARD_COUNTERPOINT_MELODY_OK) {
        status = standard_counterpoint_melody_contour_summaries(&built->line, &intervals, NULL, 0, &summaries);
    }
    if (status == STANDARD_COUNTERPOINT_MELODY_OK) {
        status = standard_counterpoint_melody_climaxes(&built->line, &intervals, NULL, 0, &climaxes);
    }
    if (status == STANDARD_COUNTERPOINT_MELODY_OK) {
        status = standard_counterpoint_melody_leap_recoveries(&built->line, &intervals, &recoveries);
    }
    if (status == STANDARD_COUNTERPOINT_MELODY_OK) {
        status = standard_counterpoint_melody_consecutive_leaps(&built->line, &intervals, &consecutive);
    }
    if (status == STANDARD_COUNTERPOINT_MELODY_OK) {
        status = standard_counterpoint_melody_nct_labels(&built->line, &labels);
    }
    if (status == STANDARD_COUNTERPOINT_MELODY_OK) {
        status = standard_counterpoint_melody_nct_counts(&built->line, NULL, 0, &counts);
    }
    if (status == STANDARD_COUNTERPOINT_MELODY_ERR_ALLOC) {
        result = ANALYZE_ALLOC;
        goto done;
    }
    if (status != STANDARD_COUNTERPOINT_MELODY_OK || summaries.count != 1 || climaxes.count != 1 ||
        counts.count != 1) {
        goto done;
    }
    const StandardCounterpointContourSummaryClaim *contour = &summaries.items[0].claim;
    const StandardCounterpointClimaxClaim *climax = &climaxes.items[0].claim;

    out->phrase = phrase_index;
    out->framework = framework_name(standard_counterpoint_framework());
    out->start_numerator = phrase->start_numerator;
    out->start_denominator = phrase->start_denominator;
    out->end_numerator = phrase->end_numerator;
    out->end_denominator = phrase->end_denominator;
    out->first_measure = phrase->first_measure;
    out->last_measure = phrase->last_measure;
    out->note_ids = malloc(sizeof(size_t) * built->note_count);
    out->directions = malloc(sizeof(const char *) * (intervals.count > 0 ? intervals.count : 1));
    out->leaps = calloc(recoveries.count > 0 ? recoveries.count : 1, sizeof(AnalysisMelodyLeap));
    out->consecutive_leaps = calloc(consecutive.count > 0 ? consecutive.count : 1, sizeof(AnalysisMelodyConsecutiveLeaps));
    out->ncts = calloc(labels.count > 0 ? labels.count : 1, sizeof(AnalysisMelodyNct));
    if (!out->note_ids || !out->directions || !out->leaps || !out->consecutive_leaps || !out->ncts) {
        result = ANALYZE_ALLOC;
        goto done;
    }
    for (size_t i = 0; i < built->line.count; i++) {
        if (built->line.notes[i].present) {
            out->note_ids[out->note_id_count++] = built->note[i];
        }
    }
    out->lowest_pitch = pitch_of(built, contour->lowest_index);
    out->lowest_note = built->note[contour->lowest_index];
    out->highest_pitch = pitch_of(built, contour->highest_index);
    out->highest_note = built->note[contour->highest_index];
    memcpy(out->lowest_name, built->name[contour->lowest_index], sizeof(out->lowest_name));
    memcpy(out->highest_name, built->name[contour->highest_index], sizeof(out->highest_name));
    out->range_semitones = contour->range_semitones;
    out->climax_note = built->note[climax->first_index];
    out->climax_occurrences = climax->occurrence_count;
    out->climax_unique = climax->unique != 0;
    out->climax_relative_position = climax->relative_position;
    out->climax_has_strength = climax->has_strength != 0;
    out->climax_strength = climax->has_strength ? beat_strength_name((BeatStrength)climax->strength) : NULL;
    out->climax_by_leap = climax->by_leap != 0;
    out->climax_final = climax->final_note != 0;
    out->up = contour->up;
    out->down = contour->down;
    out->same = contour->same;
    out->step_count = contour->step_count;
    out->leap_count = contour->leap_count;
    out->net_direction = direction_name(contour->net_direction);
    out->net_semitones = contour->net_semitones;
    for (size_t k = 0; k < intervals.count; k++) {
        out->directions[out->direction_count++] = direction_name(intervals.items[k].claim.direction);
    }
    for (size_t k = 0; k < recoveries.count; k++) {
        const StandardCounterpointLeapRecoveryClaim *claim = &recoveries.items[k].claim;
        AnalysisMelodyLeap *leap = &out->leaps[out->leap_total++];
        leap->from_note = built->note[claim->from];
        leap->to_note = built->note[claim->to];
        leap->semitones = claim->semitones;
        leap->direction = direction_name(claim->direction);
        leap->generic_size = claim->generic_size;
        leap->approximate = claim->approximate != 0;
        leap->status = standard_counterpoint_leap_status_name((StandardCounterpointLeapStatus)claim->status);
        leap->uncertainty = recoveries.items[k].uncertainty;
    }
    for (size_t k = 0; k < consecutive.count; k++) {
        const StandardCounterpointConsecutiveLeapsClaim *claim = &consecutive.items[k].claim;
        AnalysisMelodyConsecutiveLeaps *pair = &out->consecutive_leaps[out->consecutive_leap_count++];
        pair->first_note = built->note[claim->first_from];
        pair->middle_note = built->note[claim->middle];
        pair->last_note = built->note[claim->last_to];
        pair->same_direction = claim->same_direction != 0;
        pair->three_in_a_row = claim->three_in_a_row != 0;
        pair->uncertainty = consecutive.items[k].uncertainty;
    }
    const StandardCounterpointNctCountsClaim *tally = &counts.items[0].claim;
    out->chord_tone_count = tally->chord_tones;
    out->unclassified_count = tally->unclassified;
    out->non_chord_tone_count = tally->non_chord_tones;
    for (int type = 0; type < NON_CHORD_TONE_COUNT; type++) {
        out->nct_type_names[type] = non_chord_tone_name((NonChordTone)type);
        out->nct_by_type[type] = tally->by_type[type];
    }
    for (size_t k = 0; k < labels.count; k++) {
        const StandardCounterpointNctClaim *claim = &labels.items[k].claim;
        if (claim->status == (int)STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE) {
            AnalysisMelodyNct *nct = &out->ncts[out->nct_total++];
            nct->note = built->note[claim->index];
            nct->type = non_chord_tone_name((NonChordTone)claim->type);
            nct->uncertainty = labels.items[k].uncertainty;
        }
    }
    AnalyzeResult tendencies = read_tendencies(built, out);
    if (tendencies == ANALYZE_ALLOC) {
        result = ANALYZE_ALLOC;
        goto done;
    }
    if (tendencies == ANALYZE_DECLINED) { /* a name that does not spell: no tendencies for this phrase */
        memset(out->tendency_names, 0, sizeof(out->tendency_names));
        memset(out->tendency_holds, 0, sizeof(out->tendency_holds));
        out->seventh_rule = NULL;
        out->seventh_followed = 0;
        out->seventh_broken = 0;
    }
    out->uncertainty = summaries.items[0].uncertainty > climaxes.items[0].uncertainty ? summaries.items[0].uncertainty
                                                                                    : climaxes.items[0].uncertainty;
    result = ANALYZE_OK;
done:
    standard_counterpoint_nct_count_list_destroy(&counts);
    standard_counterpoint_nct_labels_destroy(&labels);
    standard_counterpoint_consecutive_leap_list_destroy(&consecutive);
    standard_counterpoint_leap_recoveries_destroy(&recoveries);
    standard_counterpoint_climaxes_destroy(&climaxes);
    standard_counterpoint_contour_summaries_destroy(&summaries);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    return result;
}

bool melody_stage_collect(const ImportedNotes *notes, const Segmentation *voices, const OuterVoiceLines *lines,
                          AnalysisReport *report) {
    if (report->phrase_count == 0 || lines->count == 0 || lines->count != voices->segment_count) {
        return true;
    }
    AnalysisMelody *melodies = calloc(report->phrase_count, sizeof(AnalysisMelody));
    if (!melodies) {
        return false;
    }
    size_t count = 0;
    bool ok = true;
    for (size_t p = 0; ok && p < report->phrase_count; p++) {
        PhraseLine built;
        ok = build_phrase_line(notes, voices, lines, report, &report->phrases[p], &built);
        if (ok && built.note_count > 0) {
            AnalyzeResult result = analyze_phrase(&built, p, &report->phrases[p], &melodies[count]);
            ok = result != ANALYZE_ALLOC;
            if (result == ANALYZE_OK) {
                count++;
            } else {
                free_melody(&melodies[count]);
                memset(&melodies[count], 0, sizeof(melodies[count]));
            }
        }
        phrase_line_destroy(&built);
    }
    if (!ok) {
        for (size_t i = 0; i < count; i++) {
            free_melody(&melodies[i]);
        }
        free(melodies);
        return false;
    }
    report->melodies = melodies;
    report->melody_count = count;
    return true;
}
