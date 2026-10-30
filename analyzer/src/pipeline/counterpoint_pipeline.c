#include "pipeline/counterpoint_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/voice_separation.h"
#include "core/rational.h"
#include "pipeline/pitch_class_util.h"
#include "pipeline/spelling_util.h"
#include "standard-counterpoint/counterpoint_slots.h"
#include "standard-counterpoint/fifth_species_rules.h"
#include "standard-counterpoint/first_species_rules.h"
#include "standard-counterpoint/fourth_species_rules.h"
#include "standard-counterpoint/framework.h"
#include "standard-counterpoint/second_species_rules.h"
#include "standard-counterpoint/third_species_rules.h"
#include "standard-meter/segmentation_beat_grid.h"
#include "theory/framework.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_rational.h"
#include "time/time_span.h"
#include "util/misc_util.h"

/* The notes of one voice, as ImportedNotes indices in onset order. */
typedef struct {
    size_t *notes;
    size_t count;
} VoiceNotes;

typedef struct {
    VoiceNotes upper;
    VoiceNotes lower;
    double uncertainty;
} TwoVoices;

static void two_voices_destroy(TwoVoices *voices) {
    free(voices->upper.notes);
    free(voices->lower.notes);
    memset(voices, 0, sizeof(*voices));
}

static const MusicalTime *onset_of(const ImportedNotes *notes, size_t note) {
    return temporal_anchor_position(note_event_temporal_anchor(notes->events[note]));
}

static const ImportedNotes *sort_notes;

static int compare_onset(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    const MusicalTime *time_a = onset_of(sort_notes, a);
    const MusicalTime *time_b = onset_of(sort_notes, b);
    int order = rational_cmp(
        rational_ratio((long long)musical_time_numerator(time_a), (long long)musical_time_denominator(time_a)),
        rational_ratio((long long)musical_time_numerator(time_b), (long long)musical_time_denominator(time_b)));
    if (order != 0) {
        return order;
    }
    long pitch_a = pitch_numeric_value_position_numerator(sort_notes->pitch_values[a]);
    long pitch_b = pitch_numeric_value_position_numerator(sort_notes->pitch_values[b]);
    return pitch_a < pitch_b ? -1 : (pitch_a > pitch_b ? 1 : 0);
}

static void sort_voice(const ImportedNotes *notes, VoiceNotes *voice) {
    sort_notes = notes;
    qsort(voice->notes, voice->count, sizeof(size_t), compare_onset);
}

/* The sum of the voice's pitches over its note count, compared across voices without dividing. */
static long pitch_sum(const ImportedNotes *notes, const VoiceNotes *voice) {
    long sum = 0;
    for (size_t i = 0; i < voice->count; i++) {
        sum += pitch_numeric_value_position_numerator(notes->pitch_values[voice->notes[i]]);
    }
    return sum;
}

/* The input's two streams as the voices, the higher-sounding one on top. */
static CounterpointCheckResult voices_from_streams(const ImportedNotes *notes, TwoVoices *out) {
    size_t first = notes->streams[0];
    VoiceNotes voices[2] = {{malloc(notes->count * sizeof(size_t)), 0}, {malloc(notes->count * sizeof(size_t)), 0}};
    if (!voices[0].notes || !voices[1].notes) {
        free(voices[0].notes);
        free(voices[1].notes);
        return COUNTERPOINT_CHECK_ERR_ALLOC;
    }
    for (size_t i = 0; i < notes->count; i++) {
        VoiceNotes *voice = &voices[notes->streams[i] == first ? 0 : 1];
        voice->notes[voice->count++] = i;
    }
    sort_voice(notes, &voices[0]);
    sort_voice(notes, &voices[1]);
    /* the higher mean pitch is the upper voice: sum_a / count_a against sum_b / count_b */
    bool first_is_upper = pitch_sum(notes, &voices[0]) * (long)voices[1].count >=
                          pitch_sum(notes, &voices[1]) * (long)voices[0].count;
    out->upper = voices[first_is_upper ? 0 : 1];
    out->lower = voices[first_is_upper ? 1 : 0];
    out->uncertainty = 0;
    return COUNTERPOINT_CHECK_OK;
}

/* The kernel's separation of a single stream, which must find two voices and no chord notes. */
static CounterpointCheckResult voices_from_separation(const ImportedNotes *notes, TwoVoices *out) {
    VoiceAssignment assignment = {0};
    VoiceSeparationConfig config = voice_separation_config_default();
    VoiceSeparationResult separated =
        voice_separate(notes->events, notes->pitch_values, notes->count, NULL, notes->meter.meter_map,
                       standard_meter_segmentation_beat_grid, NULL, &config, &assignment);
    if (separated != VOICE_SEPARATION_OK) {
        return separated == VOICE_SEPARATION_ERR_ALLOC ? COUNTERPOINT_CHECK_ERR_ALLOC
                                                       : COUNTERPOINT_CHECK_NOT_TWO_VOICES;
    }
    size_t voice_count = 0;
    double least_confidence = 1;
    bool chord_notes = false;
    for (size_t i = 0; i < assignment.count; i++) {
        const VoiceAssignmentEntry *entry = &assignment.entries[i];
        voice_count = entry->voice + 1 > voice_count ? entry->voice + 1 : voice_count;
        chord_notes = chord_notes || entry->chord_member;
        least_confidence = entry->confidence < least_confidence ? entry->confidence : least_confidence;
    }
    CounterpointCheckResult result = COUNTERPOINT_CHECK_NOT_TWO_VOICES;
    VoiceNotes upper = {malloc(notes->count * sizeof(size_t)), 0};
    VoiceNotes lower = {malloc(notes->count * sizeof(size_t)), 0};
    if (!upper.notes || !lower.notes) {
        result = COUNTERPOINT_CHECK_ERR_ALLOC;
    } else if (voice_count == 2 && !chord_notes) {
        for (size_t i = 0; i < assignment.count; i++) {
            VoiceNotes *voice = assignment.entries[i].voice == 0 ? &upper : &lower;
            voice->notes[voice->count++] = i;
        }
        sort_voice(notes, &upper);
        sort_voice(notes, &lower);
        out->upper = upper;
        out->lower = lower;
        out->uncertainty = 1 - (least_confidence < 0 ? 0 : (least_confidence > 1 ? 1 : least_confidence));
        upper.notes = lower.notes = NULL;
        result = COUNTERPOINT_CHECK_OK;
    }
    free(upper.notes);
    free(lower.notes);
    voice_assignment_destroy(&assignment);
    return result;
}

/* ---- Exact times ---- */

/* Set when a time does not fit a Rational; the species check then ends in failure instead of reading a wrapped time. */
static bool q_overflowed;

static Rational q_checked(RationalResult result, Rational value) {
    if (result != RATIONAL_OK) {
        q_overflowed = true;
        return (Rational){0, 1};
    }
    return value;
}

static Rational q_add(Rational a, Rational b) {
    Rational sum = {0, 1};
    return q_checked(rational_add(a, b, &sum), sum);
}
static Rational q_mul(Rational a, long long k) {
    Rational product = {0, 1};
    return q_checked(rational_mul_int(a, k, &product), product);
}
static Rational q_div(Rational a, long long k) {
    Rational quotient = {0, 1};
    return q_checked(rational_div_int(a, k, &quotient), quotient);
}
static int q_cmp(Rational a, Rational b) { return rational_cmp(a, b); }
static Rational q_of_time(const MusicalTime *time) {
    Rational value = {0, 1};
    return q_checked(rational_of_musical_time(time, &value), value);
}

static Rational onset_q(const ImportedNotes *notes, size_t note) { return q_of_time(onset_of(notes, note)); }

static Rational release_q(const ImportedNotes *notes, size_t note) {
    Rational length = {0, 1};
    return q_add(onset_q(notes, note),
                 q_checked(rational_of_duration(note_event_duration(notes->events[note]), &length), length));
}

/* ---- The lines of a species ---- */

typedef struct {
    StandardCounterpointSlot *slots; /* species 2 to 5: the counterpoint's slots, slots_per_bar for each cantus note */
    size_t slot_count;
    const PitchSpelling **cantus; /* one spelling per cantus firmus note */
    size_t cantus_count;
    const PitchSpelling **line; /* species 1: one spelling per counterpoint note */
    size_t line_count;
} SpeciesLines;

static void species_lines_destroy(SpeciesLines *lines) {
    free(lines->slots);
    free(lines->cantus);
    free(lines->line);
    memset(lines, 0, sizeof(*lines));
}

static int slots_per_bar(int species) {
    static const int SLOTS[6] = {0, 1, 2, 4, 2, 8};
    return SLOTS[species];
}

/* Whether each voice is a line of single notes, one after the other. */
static bool is_monophonic(const ImportedNotes *notes, const VoiceNotes *voice) {
    for (size_t i = 0; i + 1 < voice->count; i++) {
        if (q_cmp(release_q(notes, voice->notes[i]), onset_q(notes, voice->notes[i + 1])) > 0) {
            return false;
        }
    }
    return true;
}

/* Species 1: the lines note against note, position by position. */
static const char *build_first_species(const ImportedNotes *notes, const VoiceNotes *cantus, const VoiceNotes *line,
                                       PitchSpelling *const *spelled, SpeciesLines *out) {
    if (cantus->count != line->count) {
        return "the voices do not have the same number of notes";
    }
    out->cantus = malloc(cantus->count * sizeof(const PitchSpelling *));
    out->line = malloc(line->count * sizeof(const PitchSpelling *));
    if (!out->cantus || !out->line) {
        return NULL;
    }
    for (size_t i = 0; i < cantus->count; i++) {
        if (q_cmp(onset_q(notes, cantus->notes[i]), onset_q(notes, line->notes[i])) != 0) {
            return "the voices do not move together note against note";
        }
        out->cantus[i] = spelled[cantus->notes[i]];
        out->line[i] = spelled[line->notes[i]];
    }
    out->cantus_count = cantus->count;
    out->line_count = line->count;
    return NULL;
}

/* Species 2 to 5: one cantus firmus note to a measure, the counterpoint sampled on the slots of each measure. A slot
   holds an attack when a note starts on it, a tie when a note sounds on it from before, a rest when none sounds. */
static const char *build_slotted(const ImportedNotes *notes, int species, const VoiceNotes *cantus,
                                 const VoiceNotes *line, PitchSpelling *const *spelled, SpeciesLines *out) {
    const MeterMap *map = notes->meter.meter_map;
    int per_bar = slots_per_bar(species);
    out->cantus = malloc(cantus->count * sizeof(const PitchSpelling *));
    out->slots = malloc(cantus->count * (size_t)per_bar * sizeof(StandardCounterpointSlot));
    if (!out->cantus || !out->slots) {
        return NULL;
    }
    size_t attacks = 0;
    size_t next = 0; /* the first note of the counterpoint that has not ended */
    int previous_measure = 0;
    for (size_t bar = 0; bar < cantus->count; bar++) {
        Rational onset = onset_q(notes, cantus->notes[bar]);
        MusicalTime *at = NULL;
        TimeSpan *span = NULL;
        int measure = 0;
        if (musical_time_create((long)onset.num, (unsigned long)onset.den, &at) != MUSICAL_TIME_OK) {
            return NULL;
        }
        MeterMapResult found = meter_map_measure_index_at(map, at, &measure);
        musical_time_destroy(at);
        if (found != METER_MAP_OK || meter_map_measure_span(map, measure, &span) != METER_MAP_OK) {
            return "the measures cannot be read";
        }
        Rational start = q_of_time(time_span_start(span));
        Rational end = q_of_time(time_span_end(span));
        time_span_destroy(span);
        if (q_cmp(onset, start) != 0 || (bar > 0 && measure != previous_measure + 1)) {
            return "the cantus firmus is not one note to a measure";
        }
        previous_measure = measure;
        out->cantus[bar] = spelled[cantus->notes[bar]];
        for (int slot = 0; slot < per_bar; slot++) {
            Rational at_slot = q_add(start, q_div(q_mul(q_add(end, q_mul(start, -1)), slot), per_bar));
            while (next < line->count && q_cmp(release_q(notes, line->notes[next]), at_slot) <= 0) {
                next++;
            }
            StandardCounterpointSlot *cell = &out->slots[out->slot_count++];
            if (next < line->count && q_cmp(onset_q(notes, line->notes[next]), at_slot) <= 0) {
                bool attack = q_cmp(onset_q(notes, line->notes[next]), at_slot) == 0;
                attacks += attack ? 1 : 0;
                *cell = (StandardCounterpointSlot){attack ? STANDARD_COUNTERPOINT_SLOT_ATTACK
                                                          : STANDARD_COUNTERPOINT_SLOT_TIE,
                                                   spelled[line->notes[next]]};
            } else {
                *cell = (StandardCounterpointSlot){STANDARD_COUNTERPOINT_SLOT_REST, NULL};
            }
        }
    }
    out->cantus_count = cantus->count;
    if (attacks != line->count) {
        return "the counterpoint has notes off the slots of its measures";
    }
    return NULL;
}

/* ---- The library's species check ---- */

typedef struct {
    unsigned hard;
    unsigned soft;
} GroupMasks;

static const char *first_group(unsigned group) {
    return standard_counterpoint_first_species_group_name((StandardCounterpointFirstSpeciesGroup)group);
}
static const char *second_group(unsigned group) {
    return standard_counterpoint_second_species_group_name((StandardCounterpointSecondSpeciesGroup)group);
}
static const char *third_group(unsigned group) {
    return standard_counterpoint_third_species_group_name((StandardCounterpointThirdSpeciesGroup)group);
}
static const char *fourth_group(unsigned group) {
    return standard_counterpoint_fourth_species_group_name((StandardCounterpointFourthSpeciesGroup)group);
}
static const char *fifth_group(unsigned group) {
    return standard_counterpoint_fifth_species_group_name((StandardCounterpointFifthSpeciesGroup)group);
}

/* Runs the species' check over the lines. Returns false when the library declines the input (the shape is not the
   species'). */
static bool run_species_check(int species, bool cantus_upper, const SpeciesLines *lines, const PitchSpelling *tonic,
                              StandardCounterpointResult *result) {
    StandardCounterpointCantusPart part =
        cantus_upper ? STANDARD_COUNTERPOINT_CANTUS_IN_UPPER : STANDARD_COUNTERPOINT_CANTUS_IN_LOWER;
    if (species == 1) {
        StandardCounterpointLine cantus = {lines->cantus, lines->cantus_count};
        StandardCounterpointLine counterpoint = {lines->line, lines->line_count};
        StandardCounterpointPair pair = {cantus_upper ? &cantus : &counterpoint, cantus_upper ? &counterpoint : &cantus};
        StandardCounterpointInput input = {&pair, part, tonic, NULL};
        return standard_counterpoint_first_species_check(&input, result);
    }
    StandardCounterpointLine cantus = {lines->cantus, lines->cantus_count};
    StandardCounterpointSlotLine counterpoint = {lines->slots, lines->slot_count};
    switch (species) {
    case 2: {
        StandardCounterpointSecondSpeciesExercise exercise = {&cantus, &counterpoint, part};
        StandardCounterpointSecondSpeciesInput input = {&exercise, tonic};
        return standard_counterpoint_second_species_exercise_valid(&exercise) &&
               standard_counterpoint_second_species_check(&input, result);
    }
    case 3: {
        StandardCounterpointThirdSpeciesExercise exercise = {&cantus, &counterpoint, part};
        StandardCounterpointThirdSpeciesInput input = {&exercise, tonic};
        return standard_counterpoint_third_species_exercise_valid(&exercise) &&
               standard_counterpoint_third_species_check(&input, result);
    }
    case 4: {
        StandardCounterpointFourthSpeciesExercise exercise = {&cantus, &counterpoint, part};
        StandardCounterpointFourthSpeciesInput input = {&exercise, tonic};
        return standard_counterpoint_fourth_species_exercise_valid(&exercise) &&
               standard_counterpoint_fourth_species_check(&input, result);
    }
    default: {
        StandardCounterpointFifthSpeciesExercise exercise = {&cantus, &counterpoint, part};
        StandardCounterpointFifthSpeciesInput input = {&exercise, tonic};
        return standard_counterpoint_fifth_species_exercise_valid(&exercise) &&
               standard_counterpoint_fifth_species_check(&input, result);
    }
    }
}

/* The violations the result names, by group, the hard one before the soft one. False on allocation failure. */
static bool collect_violations(int species, const StandardCounterpointResult *result, AnalysisCounterpointCheck *out) {
    static const unsigned GROUPS[6] = {0, STANDARD_COUNTERPOINT_FIRST_SPECIES_GROUP_COUNT,
                                       STANDARD_COUNTERPOINT_SECOND_SPECIES_GROUP_COUNT,
                                       STANDARD_COUNTERPOINT_THIRD_SPECIES_GROUP_COUNT,
                                       STANDARD_COUNTERPOINT_FOURTH_SPECIES_GROUP_COUNT,
                                       STANDARD_COUNTERPOINT_FIFTH_SPECIES_GROUP_COUNT};
    const char *(*name_of)(unsigned) = species == 1 ? first_group
                                       : species == 2 ? second_group
                                       : species == 3 ? third_group
                                       : species == 4 ? fourth_group
                                                      : fifth_group;
    out->violations = calloc(2 * GROUPS[species] + 1, sizeof(AnalysisCounterpointViolation));
    if (!out->violations) {
        return false;
    }
    for (unsigned group = 0; group < GROUPS[species]; group++) {
        if (result->hard_violations & (1u << group)) {
            out->violations[out->violation_count++] = (AnalysisCounterpointViolation){name_of(group), true};
        }
        if (result->soft_violations & (1u << group)) {
            out->violations[out->violation_count++] = (AnalysisCounterpointViolation){name_of(group), false};
        }
    }
    return true;
}

/* 1 or 2 for that many streams, 3 for more. */
static int stream_count(const ImportedNotes *notes) {
    if (!notes->streams) {
        return 1;
    }
    size_t first = notes->streams[0];
    size_t second = first;
    int count = 1;
    for (size_t i = 0; i < notes->count; i++) {
        size_t stream = notes->streams[i];
        if (stream == first || (count == 2 && stream == second)) {
            continue;
        }
        if (count == 2) {
            return 3;
        }
        second = stream;
        count = 2;
    }
    return count;
}

/* The span and the notes of both voices. False on allocation failure. */
static bool record_span(const ImportedNotes *notes, const TwoVoices *voices, AnalysisCounterpointCheck *out) {
    out->note_ids = malloc((voices->upper.count + voices->lower.count) * sizeof(size_t));
    if (!out->note_ids) {
        return false;
    }
    Rational start = {0, 1};
    Rational end = {0, 1};
    bool first = true;
    for (int v = 0; v < 2; v++) {
        const VoiceNotes *voice = v == 0 ? &voices->upper : &voices->lower;
        for (size_t i = 0; i < voice->count; i++) {
            size_t note = voice->notes[i];
            out->note_ids[out->note_id_count++] = note;
            Rational on = onset_q(notes, note);
            Rational off = release_q(notes, note);
            start = first || q_cmp(on, start) < 0 ? on : start;
            end = first || q_cmp(off, end) > 0 ? off : end;
            first = false;
        }
    }
    qsort(out->note_ids, out->note_id_count, sizeof(size_t), size_compare);
    out->start_numerator = (long)start.num;
    out->start_denominator = (unsigned long)start.den;
    out->end_numerator = (long)end.num;
    out->end_denominator = (unsigned long)end.den;
    return true;
}

/* Runs the species check over the two voices and fills out. */
static CounterpointCheckResult check_voices(const ImportedNotes *notes, const TwoVoices *voices, bool cantus_upper,
                                            AnalysisReport *report, AnalysisCounterpointCheck *out) {
    CounterpointCheckResult result = COUNTERPOINT_CHECK_ERR_ALLOC;
    q_overflowed = false;
    bool prefer_flats = report->key_region_count > 0 ? report->key_regions[report->home_region].prefer_flats : false;
    PitchSpelling **spelled = calloc(notes->count, sizeof(PitchSpelling *));
    PitchSpelling *tonic = NULL;
    SpeciesLines lines = {0};
    if (!spelled || !record_span(notes, voices, out)) {
        goto done;
    }
    for (size_t i = 0; i < out->note_id_count; i++) {
        if (note_pitch_spelling(notes, out->note_ids[i], prefer_flats, &spelled[out->note_ids[i]]) < 0) {
            goto done;
        }
    }
    out->framework = framework_name(standard_counterpoint_framework());
    const VoiceNotes *cantus = cantus_upper ? &voices->upper : &voices->lower;
    const VoiceNotes *line = cantus_upper ? &voices->lower : &voices->upper;
    for (size_t i = 0; i < out->note_id_count && !out->shape_error; i++) {
        out->shape_error = spelled[out->note_ids[i]] ? NULL : "a note has no pitch name";
    }
    if (!out->shape_error && !(is_monophonic(notes, cantus) && is_monophonic(notes, line))) {
        out->shape_error = "a voice has notes that sound together";
    }
    if (!out->shape_error) {
        out->shape_error = out->species == 1 ? build_first_species(notes, cantus, line, spelled, &lines)
                                              : build_slotted(notes, out->species, cantus, line, spelled, &lines);
        if (!out->shape_error && (!lines.cantus || (out->species == 1 ? !lines.line : !lines.slots))) {
            goto done; /* an allocation failed in the builder */
        }
    }
    if (!out->shape_error && report->key_region_count > 0) {
        const AnalysisKeyRegion *home = &report->key_regions[report->home_region];
        char name[16];
        snprintf(name, sizeof(name), "%s4", pitch_class_name(home->tonic_pitch_class, home->prefer_flats));
        if (spelling_from_name(name, &tonic) < 0) {
            goto done;
        }
    }
    if (!out->shape_error) {
        StandardCounterpointResult checked;
        if (run_species_check(out->species, cantus_upper, &lines, tonic, &checked)) {
            out->valid = checked.hard_violations == 0;
            if (!collect_violations(out->species, &checked, out)) {
                goto done;
            }
        } else {
            out->shape_error = "the voices are not an exercise of the species";
        }
    }
    if (q_overflowed) {
        goto done; /* a time does not fit an exact rational */
    }
    result = COUNTERPOINT_CHECK_OK;
done:
    for (size_t i = 0; spelled && i < notes->count; i++) {
        pitch_spelling_destroy(spelled[i]);
    }
    free(spelled);
    pitch_spelling_destroy(tonic);
    species_lines_destroy(&lines);
    return result;
}

CounterpointCheckResult counterpoint_check_collect(const ImportedNotes *notes, int species, bool cantus_upper,
                                                   AnalysisReport *report) {
    if (notes->count < 2) {
        return COUNTERPOINT_CHECK_NOT_TWO_VOICES;
    }
    int streams = stream_count(notes);
    if (streams > 2) {
        return COUNTERPOINT_CHECK_NOT_TWO_VOICES;
    }
    TwoVoices voices = {0};
    CounterpointCheckResult result =
        streams == 2 ? voices_from_streams(notes, &voices) : voices_from_separation(notes, &voices);
    if (result == COUNTERPOINT_CHECK_OK) {
        AnalysisCounterpointCheck check = {
            .present = true, .species = species, .cantus_upper = cantus_upper, .uncertainty = voices.uncertainty};
        result = check_voices(notes, &voices, cantus_upper, report, &check);
        if (result == COUNTERPOINT_CHECK_OK) {
            report->counterpoint_check = check;
        } else {
            free(check.violations);
            free(check.note_ids);
        }
    }
    two_voices_destroy(&voices);
    return result;
}
