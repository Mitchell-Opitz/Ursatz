#include "pipeline/rhythm_pipeline.h"


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline/harmonic_rhythm_pipeline.h"
#include "events/note_event.h"
#include "pitch/pitch_numeric_value.h"
#include "identity/entity_id.h"
#include "meter_analysis_density.h"
#include "meter_analysis_hemiola.h"
#include "meter_analysis_hypermeter.h"
#include "meter_analysis_input.h"
#include "meter_analysis_syncopation.h"
#include "meter_analysis_time.h"
#include "borrowed_rules.h"
#include "theory/framework.h"
#include "standard-meter/framework.h"
#include "structure/structure.h"
#include "time/duration.h"
#include "time/meter_map.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"
#include "time/time_signature.h"
#include "time/time_span.h"
#include "time/tuplet.h"
#include "util/misc_util.h"

/* The line of a note in the library's input: 0 the outer-voice soprano, 1 the bass, 2 any other. */
#define LINE_SOPRANO 0
#define LINE_BASS 1
#define LINE_OTHER 2

/* An id of the input and the ImportedNotes index of its note. */
typedef struct {
    const char *id;
    size_t note;
} IdIndex;

/* The library's input and what keeps it alive: the spans and times it borrows, and the index of each id. */
typedef struct {
    StandardMeterInput input;
    TimeSpan **spans;
    size_t span_count;
    size_t span_capacity;
    MusicalTime **times;
    size_t time_count;
    size_t time_capacity;
    IdIndex *ids; /* sorted by id; one per input note */
    size_t id_count;
    int *lines;   /* per ImportedNotes index: its line, or -1 for a note left out of the input */
} RhythmBuild;

static void build_destroy(RhythmBuild *build) {
    standard_meter_input_destroy(&build->input);
    for (size_t i = 0; i < build->span_count; i++) {
        time_span_destroy(build->spans[i]);
    }
    for (size_t i = 0; i < build->time_count; i++) {
        musical_time_destroy(build->times[i]);
    }
    free(build->spans);
    free(build->times);
    free(build->ids);
    free(build->lines);
    memset(build, 0, sizeof(*build));
}

/* A span [start, end) kept by the build, or NULL on failure. */
static const TimeSpan *keep_span(RhythmBuild *build, const MusicalTime *start, const MusicalTime *end) {
    if (build->span_count == build->span_capacity) {
        size_t capacity = build->span_capacity == 0 ? 512 : build->span_capacity * 2;
        TimeSpan **grown = realloc(build->spans, capacity * sizeof(TimeSpan *));
        if (!grown) {
            return NULL;
        }
        build->spans = grown;
        build->span_capacity = capacity;
    }
    TimeSpan *span = NULL;
    if (time_span_create(start, end, TIME_SPAN_BOUNDARY_LEFT_CLOSED, &span) != TIME_SPAN_OK) {
        return NULL;
    }
    build->spans[build->span_count++] = span;
    return span;
}

static int compare_ids(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The ids of the notes the line takes, sorted. NULL with count 0 on failure; *out_count is the number of ids. */
static const char **line_ids(const OuterVoiceEntry *entries, size_t count, size_t *out_count) {
    const char **ids = malloc((count == 0 ? 1 : count) * sizeof(char *));
    if (!ids) {
        return NULL;
    }
    size_t n = 0;
    for (size_t s = 0; s < count; s++) {
        if (entries[s].present && entries[s].note_id != NULL) {
            ids[n++] = entity_id_value(entries[s].note_id);
        }
    }
    qsort(ids, n, sizeof(char *), compare_ids);
    *out_count = n;
    return ids;
}

static bool in_ids(const char **ids, size_t count, const char *id) {
    return bsearch(&id, ids, count, sizeof(char *), compare_ids) != NULL;
}

static int compare_id_index(const void *a, const void *b) {
    return strcmp(((const IdIndex *)a)->id, ((const IdIndex *)b)->id);
}

/* The input's notes: every imported note with a length, in import order. A note the library refuses as invalid is left
   out (its line stays -1). */
static bool build_notes(RhythmBuild *build, const ImportedNotes *notes, const OuterVoiceLines *lines) {
    size_t soprano_count = 0;
    size_t bass_count = 0;
    const char **soprano = line_ids(lines->soprano, lines->count, &soprano_count);
    const char **bass = line_ids(lines->bass, lines->count, &bass_count);
    bool ok = soprano && bass && (build->lines = malloc((notes->count + 1) * sizeof(int))) != NULL;
    for (size_t i = 0; ok && i < notes->count; i++) {
        build->lines[i] = -1;
        const NoteEvent *event = notes->events[i];
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(event));
        MusicalTime *release = NULL;
        if (musical_time_add(onset, note_event_duration(event), &release) != MUSICAL_TIME_OK) {
            ok = false;
            break;
        }
        const TimeSpan *span = keep_span(build, onset, release);
        musical_time_destroy(release);
        if (!span) {
            ok = false;
            break;
        }
        const char *id = entity_id_value(note_event_id(event));
        int line = in_ids(soprano, soprano_count, id) ? LINE_SOPRANO : (in_ids(bass, bass_count, id) ? LINE_BASS : LINE_OTHER);
        StandardMeterAnalysisStatus status = standard_meter_input_add_note(
            &build->input, id, span, pitch_numeric_value_position_numerator(notes->pitch_values[i]), line);
        if (status == STANDARD_METER_ANALYSIS_OK) {
            build->lines[i] = line;
        } else if (status != STANDARD_METER_ANALYSIS_ERR_INVALID) {
            ok = false;
        }
    }
    free(soprano);
    free(bass);
    if (!ok) {
        return false;
    }
    build->ids = malloc((build->input.note_count + 1) * sizeof(IdIndex));
    if (!build->ids) {
        return false;
    }
    size_t next = 0;
    for (size_t i = 0; i < notes->count; i++) {
        if (build->lines[i] >= 0) {
            build->ids[next].id = build->input.notes[next].id;
            build->ids[next].note = i;
            next++;
        }
    }
    build->id_count = next;
    qsort(build->ids, build->id_count, sizeof(IdIndex), compare_id_index);
    return true;
}

/* The ImportedNotes index of the input note with the id, or SIZE_MAX with none. */
static size_t note_of_id(const RhythmBuild *build, const char *id) {
    IdIndex key = {id, 0};
    const IdIndex *found = bsearch(&key, build->ids, build->id_count, sizeof(IdIndex), compare_id_index);
    return found ? found->note : SIZE_MAX;
}

/* A copy of the time kept by the build, or NULL on failure. */
static const MusicalTime *keep_time(RhythmBuild *build, long numerator, unsigned long denominator) {
    if (build->time_count == build->time_capacity) {
        size_t capacity = build->time_capacity == 0 ? 128 : build->time_capacity * 2;
        MusicalTime **grown = realloc(build->times, capacity * sizeof(MusicalTime *));
        if (!grown) {
            return NULL;
        }
        build->times = grown;
        build->time_capacity = capacity;
    }
    MusicalTime *time = NULL;
    if (musical_time_create(numerator, denominator, &time) != MUSICAL_TIME_OK) {
        return NULL;
    }
    build->times[build->time_count++] = time;
    return time;
}

/* A chord change at the start of every chord segment whose harmony differs from the segment before it (the first
   segment is not one: nothing changes into it). */
static bool build_chord_changes(RhythmBuild *build, const AnalysisReport *report) {
    for (size_t i = 1; i < report->entry_count; i++) {
        const AnalysisReportEntry *entry = &report->entries[i];
        if (!harmonic_rhythm_is_change(&report->entries[i - 1], entry)) {
            continue;
        }
        const MusicalTime *time = keep_time(build, entry->time_numerator, entry->time_denominator);
        char id[48];
        snprintf(id, sizeof(id), "chord-change-%zu", i);
        if (!time || standard_meter_input_add_chord_change(&build->input, id, time) != STANDARD_METER_ANALYSIS_OK) {
            return false;
        }
    }
    return true;
}

/* The form stage's phrases as the library's phrases: the span with its extension, the arrival chord of the cadence and
   the length change of the expansion that concerns the phrase. */
static bool build_phrases(RhythmBuild *build, const FormStage *form) {
    const StandardFormPhrases *phrases = &form->phrases;
    for (size_t i = 0; i < phrases->count; i++) {
        const StandardFormPhrase *phrase = &phrases->items[i];
        const TimeSpan *span = keep_span(build, time_span_start(phrase->span), time_span_end(phrase->span));
        const TimeSpan *arrival = NULL;
        if (phrase->claim.cadence_index >= 0) {
            const TimeSpan *chord = form->input.cadences[phrase->claim.cadence_index].arrival;
            arrival = keep_span(build, time_span_start(chord), time_span_end(chord));
            if (!arrival) {
                return false;
            }
        }
        StandardMeterExpansionKind expansion = STANDARD_METER_EXPANSION_NONE;
        int measures = 0;
        for (size_t e = 0; e < form->expansions.count; e++) {
            const StandardFormExpansionClaim *claim = &form->expansions.items[e].claim;
            if (claim->phrase != phrase->claim.index) {
                continue;
            }
            switch (claim->kind) {
                case STANDARD_FORM_EXPANSION_INTERNAL:
                    expansion = STANDARD_METER_EXPANSION_INTERNAL;
                    break;
                case STANDARD_FORM_EXPANSION_EXTERNAL:
                    expansion = STANDARD_METER_EXPANSION_EXTERNAL;
                    break;
                default:
                    expansion = STANDARD_METER_EXPANSION_CONTRACTION;
                    break;
            }
            measures = claim->measures_changed;
            break;
        }
        char id[48];
        snprintf(id, sizeof(id), "phrase-%zu", i);
        if (!span || standard_meter_input_add_phrase(&build->input, id, span, arrival, expansion, measures, false) !=
                         STANDARD_METER_ANALYSIS_OK) {
            return false;
        }
    }
    return true;
}

/* ---- Density ---- */

/* The notes of phrase p on the line (-1: all), in time order: the notes whose onset, snapped as the library snaps it,
   is in the phrase's span. */
static size_t *phrase_notes(const RhythmBuild *build, const ImportedNotes *notes, size_t phrase, int line, size_t *count) {
    StandardMeterConfig config = standard_meter_config_default();
    StandardMeterQ start;
    StandardMeterQ end;
    *count = 0;
    size_t *ids = malloc((build->input.note_count + 1) * sizeof(size_t));
    if (!ids || !standard_meter_span_q(build->input.phrases[phrase].span, &start, &end)) {
        free(ids);
        return NULL;
    }
    for (size_t i = 0; i < notes->count; i++) {
        if (build->lines[i] < 0 || (line >= 0 && build->lines[i] != line)) {
            continue;
        }
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        StandardMeterQ at = standard_meter_snap(&config, standard_meter_q_of_time(onset), false, NULL);
        if (standard_meter_q_compare(at, start) >= 0 && standard_meter_q_compare(at, end) < 0) {
            ids[(*count)++] = i;
        }
    }
    return ids;
}

static bool collect_densities(const RhythmBuild *build, const ImportedNotes *notes, AnalysisReport *report) {
    StandardMeterDensities densities = {NULL, 0};
    if (standard_meter_find_densities(&build->input, NULL, &densities) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = densities.count;
    AnalysisDensity *items = calloc(count > 0 ? count : 1, sizeof(AnalysisDensity));
    bool ok = items != NULL;
    const char *framework = framework_name(standard_meter_framework());
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterDensityClaim *claim = &densities.items[i].claim;
        AnalysisDensity *item = &items[i];
        item->phrase = (size_t)claim->phrase;
        item->line = claim->line;
        item->framework = framework;
        item->notes = claim->notes;
        item->attacks = claim->attacks;
        item->measures = claim->measures;
        item->attacks_per_beat_mean = claim->attacks_per_beat_mean;
        item->attacks_per_beat_max = claim->attacks_per_beat_max;
        item->mean_inter_onset = claim->mean_inter_onset;
        item->on_beat_share = claim->on_beat_share;
        item->off_beat_share = claim->off_beat_share;
        item->bin_count = claim->bin_count;
        item->bins_truncated = claim->bins_truncated != 0;
        for (unsigned b = 0; b < claim->bin_count; b++) {
            item->bins[b].numerator = claim->bins[b].numerator;
            item->bins[b].denominator = claim->bins[b].denominator;
            item->bins[b].count = claim->bins[b].count;
        }
        item->note_ids = phrase_notes(build, notes, item->phrase, item->line, &item->note_id_count);
        ok = item->note_ids != NULL;
    }
    standard_meter_densities_destroy(&densities);
    if (!ok) {
        for (size_t i = 0; i < count; i++) {
            free(items[i].note_ids);
        }
        free(items);
        return false;
    }
    report->densities = items;
    report->density_count = count;
    return true;
}

/* ---- Syncopation ---- */

/* The library's StandardMeterLevel names. */
static const char *level_name(int level) {
    static const char *const NAMES[] = {"downbeat", "secondary-beat", "beat", "offbeat", "finer"};
    return level >= 0 && level < 5 ? NAMES[level] : "finer";
}

static bool collect_syncopations(const RhythmBuild *build, AnalysisReport *report) {
    StandardMeterSyncopations found = {NULL, 0};
    if (standard_meter_find_syncopations(&build->input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.count;
    AnalysisSyncopation *items = calloc(count > 0 ? count : 1, sizeof(AnalysisSyncopation));
    const char *framework = framework_name(standard_meter_framework());
    bool ok = items != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterSyncopationClaim *claim = &found.items[i].claim;
        AnalysisSyncopation *item = &items[i];
        item->framework = framework;
        item->note = note_of_id(build, claim->note_id);
        ok = item->note != SIZE_MAX;
        item->line = claim->line;
        item->measure = claim->measure;
        item->beat = claim->beat;
        item->attack_level = level_name(claim->attack_level);
        item->through_level = level_name(claim->through_level);
        item->strength = claim->strength;
        item->through_measure = claim->through_measure;
        item->through_beat = claim->through_beat;
        item->uncertainty = found.items[i].uncertainty;
    }
    standard_meter_syncopations_destroy(&found);
    if (!ok) {
        free(items);
        return false;
    }
    report->syncopations = items;
    report->syncopation_count = count;
    return true;
}

/* ---- Hemiola ---- */

static void free_hemiolas(AnalysisHemiola *items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(items[i].note_ids);
    }
    free(items);
}

static bool collect_hemiolas(const RhythmBuild *build, AnalysisReport *report) {
    StandardMeterHemiolas found = {NULL, 0};
    if (standard_meter_find_hemiolas(&build->input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.count;
    AnalysisHemiola *items = calloc(count > 0 ? count : 1, sizeof(AnalysisHemiola));
    const char *framework = framework_name(standard_meter_framework());
    bool ok = items != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterHemiola *hemiola = &found.items[i];
        AnalysisHemiola *item = &items[i];
        item->framework = framework;
        item->first_measure = hemiola->claim.first_measure;
        item->last_measure = hemiola->claim.last_measure;
        item->duration_evidence = hemiola->claim.duration_evidence;
        item->chord_evidence = hemiola->claim.chord_evidence;
        item->accent_evidence = hemiola->claim.accent_evidence;
        item->agreeing = hemiola->claim.agreeing;
        item->available = hemiola->claim.available;
        item->uncertainty = hemiola->uncertainty;
        size_t ids = structure_event_id_count(hemiola->shell);
        item->note_ids = malloc((ids > 0 ? ids : 1) * sizeof(size_t));
        ok = item->note_ids != NULL;
        for (size_t n = 0; ok && n < ids; n++) {
            size_t note = note_of_id(build, entity_id_value(structure_event_id_at(hemiola->shell, n)));
            if (note != SIZE_MAX) {
                item->note_ids[item->note_id_count++] = note;
            }
        }
        if (ok && item->note_id_count > 1) {
            qsort(item->note_ids, item->note_id_count, sizeof(size_t), size_compare);
        }
    }
    standard_meter_hemiolas_destroy(&found);
    if (!ok) {
        free_hemiolas(items, count);
        return false;
    }
    report->hemiolas = items;
    report->hemiola_count = count;
    return true;
}

/* ---- Hypermeter ---- */

static const char *hypermeasure_kind_name(int kind) {
    switch (kind) {
        case STANDARD_METER_HYPERMEASURE_SHORT:
            return "short";
        case STANDARD_METER_HYPERMEASURE_EXPANDED:
            return "expanded";
        case STANDARD_METER_HYPERMEASURE_EXTENSION:
            return "extension";
        case STANDARD_METER_HYPERMEASURE_CONTRACTED:
            return "contracted";
        default:
            return "regular";
    }
}

static void free_hypermeters(AnalysisHypermeter *items, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(items[i].hypermeasures);
        free(items[i].note_ids);
    }
    free(items);
}

static bool collect_hypermeter(const RhythmBuild *build, const ImportedNotes *notes, AnalysisReport *report) {
    StandardMeterHypermeter found;
    memset(&found, 0, sizeof(found));
    if (standard_meter_find_hypermeter(&build->input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.phrase_count;
    AnalysisHypermeter *items = calloc(count > 0 ? count : 1, sizeof(AnalysisHypermeter));
    const char *framework = framework_name(standard_meter_framework());
    bool ok = items != NULL;
    for (size_t i = 0; ok && i < count; i++) {
        const StandardMeterPhraseHypermeter *phrase = &found.phrases[i];
        AnalysisHypermeter *item = &items[i];
        item->framework = framework;
        item->phrase = (size_t)phrase->phrase;
        item->first_measure = phrase->first_measure;
        item->measures = phrase->measures;
        item->length = phrase->length;
        item->score = phrase->score;
        item->has_arrival = phrase->arrival_measure >= 0;
        item->arrival_measure = phrase->arrival_measure;
        item->hypermeasures = calloc(phrase->hypermeasure_count > 0 ? phrase->hypermeasure_count : 1,
                                     sizeof(AnalysisHypermeasure));
        ok = item->hypermeasures != NULL;
        for (size_t h = 0; ok && h < phrase->hypermeasure_count; h++) {
            const StandardMeterHypermeasure *source = &found.hypermeasures[phrase->hypermeasure_first + h];
            AnalysisHypermeasure *target = &item->hypermeasures[h];
            target->index = source->claim.hypermeasure;
            target->first_measure = source->claim.first_measure;
            target->last_measure = source->claim.last_measure;
            target->measures = source->claim.measures;
            target->kind = hypermeasure_kind_name(source->claim.kind);
            target->extra_hyperbeats = source->claim.extra_hyperbeats;
            target->contains_cadence = source->claim.contains_cadence != 0;
            target->score = source->claim.score;
            target->uncertainty = source->uncertainty;
        }
        item->hypermeasure_count = phrase->hypermeasure_count;
        if (ok) {
            item->note_ids = phrase_notes(build, notes, item->phrase, -1, &item->note_id_count);
            ok = item->note_ids != NULL;
        }
    }
    standard_meter_hypermeter_destroy(&found);
    if (!ok) {
        free_hypermeters(items, count);
        return false;
    }
    report->hypermeters = items;
    report->hypermeter_count = count;
    return true;
}

/* ---- Tuplets and borrowed divisions ---- */

/* The finest grid a tuplet is read on: a twelfth of a whole note is a triplet eighth, a 24th a sixteenth triplet. The
   library's own grid is 1/32, so a time on the finer grid but off the coarser is a third of a step: a tuplet's. */
#define TUPLET_GRID 96
#define NATIVE_STEPS 3 /* TUPLET_GRID steps in one step of the library's 1/32 grid */

typedef struct {
    long long n;
    long long d;
} Frac;

static long long gcd_ll(long long a, long long b) {
    while (b != 0) {
        long long t = a % b;
        a = b;
        b = t;
    }
    return a < 0 ? -a : a;
}

/* a / b reduced, for a >= 0 and b > 0. */
static Frac frac_div(StandardMeterQ a, StandardMeterQ b) {
    long long n = a.n * b.d;
    long long d = a.d * b.n;
    long long g = gcd_ll(n, d);
    return (Frac){g > 1 ? n / g : n, g > 1 ? d / g : d};
}

/* The time as a whole number of TUPLET_GRID steps, when it lies within a quarter of a step of one: the way a MIDI
   file's rounded times are read. False for a time further from the grid (a performance's rubato), which is then not
   read at all. */
static bool on_tuplet_grid(StandardMeterQ time, long long *steps) {
    long long scaled = time.n * TUPLET_GRID;
    long long nearest = (scaled + time.d / 2) / time.d;
    long long off = scaled - nearest * time.d;
    if (off < 0) {
        off = -off;
    }
    *steps = nearest;
    return off * 4 <= time.d;
}

typedef struct {
    size_t note;
    long long steps;         /* the attack, in steps of the tuplet grid */
    StandardMeterQ onset;
    long long next_steps;    /* the next attack, or -1 for the last */
    StandardMeterQ duration; /* the note's length when its release is on the grid, otherwise 0 */
    StandardMeterQ beat;     /* the length of the beat the note is attacked in */
    int division;            /* 2 (simple) or 3 (compound) */
    bool on_beat;
    size_t attack;           /* the position of the attack among the piece's attacks */
    unsigned actual;         /* the note's ratio actual:normal; 0 when it is not a tuplet note */
    unsigned normal;
} TupletNote;

static int compare_tuplet_notes(const void *a, const void *b) {
    const TupletNote *x = a;
    const TupletNote *y = b;
    if (x->steps != y->steps) {
        return x->steps < y->steps ? -1 : 1;
    }
    return x->note < y->note ? -1 : (x->note > y->note ? 1 : 0);
}

/* Reads a note's attack on the tuplet grid, and its release when that is on the grid too (a performer's articulation
   leaves it off, and the library's own release snap would pull a triplet's onto its 1/32 grid, so the raw release is
   rounded here). False when the attack is off the grid or the meter has no beat to read against. */
static bool read_tuplet_note(const MeterMap *map, StandardMeterQ onset, StandardMeterQ release, TupletNote *note) {
    long long release_steps = 0;
    int measure = 0;
    StandardMeterMeasureGrid grid;
    if (!on_tuplet_grid(onset, &note->steps)) {
        return false;
    }
    note->onset = standard_meter_q(note->steps, TUPLET_GRID);
    if (on_tuplet_grid(release, &release_steps) && release_steps > note->steps) {
        note->duration = standard_meter_q(release_steps - note->steps, TUPLET_GRID);
    }
    if (!standard_meter_measure_at(map, note->onset, &measure) || !standard_meter_measure_grid(map, measure, &grid) ||
        (grid.division != 2 && grid.division != 3)) {
        return false;
    }
    note->beat = grid.beat;
    note->division = (int)grid.division;
    note->on_beat = frac_div(standard_meter_q_sub(note->onset, grid.base), grid.beat).d == 1;
    return true;
}

/* Numbers the attacks and finds each note's next attack, from the notes in time order. */
static void link_attacks(TupletNote *notes, size_t count) {
    size_t attack = 0;
    for (size_t k = 0; k < count; k++) {
        if (k > 0 && notes[k].steps != notes[k - 1].steps) {
            attack++;
        }
        notes[k].attack = attack;
        notes[k].next_steps = -1;
        for (size_t j = k + 1; j < count; j++) {
            if (notes[j].steps != notes[k].steps) {
                notes[k].next_steps = notes[j].steps;
                break;
            }
        }
    }
}

/* A note with an attack, or a distance to the next attack, that is not a whole step of the library's 1/32 grid
   divides a step in three: a triplet note (3:2, which includes the sextuplets). */
static void mark_triplets(TupletNote *notes, size_t count) {
    for (size_t k = 0; k < count; k++) {
        bool attack_off = notes[k].steps % NATIVE_STEPS != 0;
        bool distance_off = notes[k].next_steps >= 0 && (notes[k].next_steps - notes[k].steps) % NATIVE_STEPS != 0;
        if (attack_off || distance_off) {
            notes[k].actual = 3;
            notes[k].normal = 2;
        }
    }
}

/* In a compound meter a beat halved in two equal notes is a duplet: a note on the beat whose next attack is half a
   beat later, and a note there that lasts, or is followed by an attack after, half a beat. The notes sound as two dotted
   values, so nothing else tells them apart; a dotted note and a short one do not match. */
static void mark_duplets(TupletNote *notes, size_t count) {
    for (size_t k = 0; k < count; k++) {
        if (notes[k].division != 3 || !notes[k].on_beat) {
            continue;
        }
        StandardMeterQ half = standard_meter_q_div_int(notes[k].beat, 2);
        StandardMeterQ later = standard_meter_q_add(notes[k].onset, half);
        for (size_t j = k + 1; j < count && standard_meter_q_compare(notes[j].onset, later) <= 0; j++) {
            if (standard_meter_q_compare(notes[j].onset, later) != 0) {
                continue;
            }
            bool next_is_half =
                notes[j].next_steps >= 0 && standard_meter_q_compare(standard_meter_q(notes[j].next_steps, TUPLET_GRID),
                                                                       standard_meter_q_add(later, half)) == 0;
            bool lasts_half = standard_meter_q_compare(notes[j].duration, half) == 0;
            if (notes[k].next_steps >= 0 &&
                standard_meter_q_compare(standard_meter_q(notes[k].next_steps, TUPLET_GRID), later) == 0 &&
                (next_is_half || lasts_half)) {
                for (size_t m = 0; m < 2; m++) {
                    TupletNote *marked = m == 0 ? &notes[k] : &notes[j];
                    if (marked->actual == 0) {
                        marked->actual = 2;
                        marked->normal = 3;
                    }
                }
                break;
            }
        }
    }
}

/* One run being built: the consecutive attacks of one ratio. */
typedef struct {
    size_t *notes;
    size_t count;
    unsigned attacks;
    size_t last_attack;      /* the attack number of the latest attack of the run */
    StandardMeterQ first;    /* the first attack */
    StandardMeterQ end;      /* where the latest attack's note leads: the next attack, or its release */
    StandardMeterQ group_end;
    bool has_group_end;
} TupletRun;

static const char *tuplet_name(unsigned actual, unsigned normal) {
    return actual == 3 && normal == 2 ? "triplet" : (actual == 2 && normal == 3 ? "duplet" : "tuplet");
}

static const char *level_text(TupletLevel level) {
    return level == TUPLET_LEVEL_BEAT ? "beat" : (level == TUPLET_LEVEL_SUBDIVISION ? "subdivision" : "multi-beat");
}

/* Names a finished run with the library's rules and appends it. A single attack is no group and is left out. */
static bool emit_tuplet(AnalysisTuplet **items, size_t *count, size_t *capacity, const ImportedNotes *notes,
                        const MeterMap *map, unsigned actual, unsigned normal, const TupletRun *run) {
    if (run->attacks < 2) {
        return true;
    }
    if (*count == *capacity) {
        size_t grown_capacity = *capacity == 0 ? 16 : *capacity * 2;
        AnalysisTuplet *grown = realloc(*items, grown_capacity * sizeof(AnalysisTuplet));
        if (!grown) {
            return false;
        }
        *items = grown;
        *capacity = grown_capacity;
    }
    AnalysisTuplet *item = &(*items)[*count];
    memset(item, 0, sizeof(*item));
    item->note_ids = malloc(run->count * sizeof(size_t));
    if (!item->note_ids) {
        return false;
    }
    memcpy(item->note_ids, run->notes, run->count * sizeof(size_t));
    item->note_id_count = run->count;
    item->framework = framework_name(standard_meter_framework());
    item->actual = actual;
    item->normal = normal;
    item->name = tuplet_name(actual, normal);
    item->attacks = run->attacks;
    item->meter = "unclassified";
    int measure = 0;
    double beat = 1.0;
    if (standard_meter_beat_of(map, run->first, &measure, &beat)) {
        item->measure = measure;
        item->beat = beat;
    }
    const MeterMapRegion *region = NULL;
    const MusicalTime *at = temporal_anchor_position(note_event_temporal_anchor(notes->events[run->notes[0]]));
    Tuplet *tuplet = NULL;
    if (meter_map_region_at(map, at, &region) == METER_MAP_OK && tuplet_create(actual, normal, &tuplet) == TUPLET_OK) {
        const TimeSignature *signature = meter_map_region_time_signature(region);
        MeterType type = standard_meter_type(signature);
        item->meter = type == METER_TYPE_SIMPLE ? "simple" : (type == METER_TYPE_COMPOUND ? "compound" : "unclassified");
        item->borrowed = standard_meter_tuplet_borrowed(tuplet, type);
        StandardMeterQ end = run->has_group_end ? run->group_end : run->end;
        StandardMeterQ span = standard_meter_q_sub(end, run->first);
        Duration *duration = NULL;
        TupletLevel level;
        if (span.n > 0 && duration_create((long)span.n, (unsigned long)span.d, &duration) == DURATION_OK) {
            if (standard_meter_tuplet_level(signature, duration, &level)) {
                item->has_level = true;
                item->level = level_text(level);
                item->level_typical = standard_meter_tuplet_level_typical(tuplet, level);
            }
            duration_destroy(duration);
        }
    }
    tuplet_destroy(tuplet);
    (*count)++;
    return true;
}

/* The runs of one ratio, from the notes in time order. A run goes on while its ratio's notes attack one after another
   (the next attack of the piece is the next attack of the run). */
static bool collect_runs(AnalysisTuplet **items, size_t *count, size_t *capacity, const ImportedNotes *notes,
                         const MeterMap *map, const TupletNote *line_notes, size_t note_count,
                         unsigned actual, unsigned normal, size_t *scratch) {
    TupletRun run;
    memset(&run, 0, sizeof(run));
    run.notes = scratch;
    bool open = false;
    for (size_t k = 0; k < note_count; k++) {
        const TupletNote *note = &line_notes[k];
        if (note->actual != actual || note->normal != normal) {
            continue;
        }
        StandardMeterQ end = note->next_steps >= 0 ? standard_meter_q(note->next_steps, TUPLET_GRID)
                                                   : standard_meter_q_add(note->onset, note->duration);
        if (open && note->attack == run.last_attack) {
            /* another note of the same attack */
        } else if (open && note->attack == run.last_attack + 1) {
            if (run.attacks == actual && !run.has_group_end) {
                run.group_end = note->onset;
                run.has_group_end = true;
            }
            run.attacks++;
            run.last_attack = note->attack;
            run.end = end;
        } else {
            if (open && !emit_tuplet(items, count, capacity, notes, map, actual, normal, &run)) {
                return false;
            }
            memset(&run, 0, sizeof(run));
            run.notes = scratch;
            open = true;
            run.attacks = 1;
            run.last_attack = note->attack;
            run.first = note->onset;
            run.end = end;
        }
        run.notes[run.count++] = note->note;
    }
    return !open || emit_tuplet(items, count, capacity, notes, map, actual, normal, &run);
}

static int compare_tuplets(const void *a, const void *b) {
    const AnalysisTuplet *x = a;
    const AnalysisTuplet *y = b;
    if (x->actual != y->actual) {
        return x->actual < y->actual ? -1 : 1;
    }
    if (x->measure != y->measure) {
        return x->measure < y->measure ? -1 : 1;
    }
    return x->beat < y->beat ? -1 : (x->beat > y->beat ? 1 : 0);
}

static bool collect_tuplets(const RhythmBuild *build, const ImportedNotes *notes, AnalysisReport *report) {
    const MeterMap *map = build->input.map;
    StandardMeterConfig config = standard_meter_config_default();
    TupletNote *stream = malloc((notes->count + 1) * sizeof(TupletNote));
    size_t *scratch = malloc((notes->count + 1) * sizeof(size_t));
    AnalysisTuplet *items = NULL;
    size_t count = 0;
    size_t capacity = 0;
    size_t stream_count = 0;
    bool ok = stream && scratch;
    /* every note is read as one stream: the outer voice lines split a melody across lines wherever the extraction
       thins it, and a tuplet is a figure of the music, not of a line */
    for (size_t i = 0; ok && i < notes->count; i++) {
        if (build->lines[i] < 0) {
            continue;
        }
        const NoteEvent *event = notes->events[i];
        StandardMeterQ onset = standard_meter_q_of_time(temporal_anchor_position(note_event_temporal_anchor(event)));
        StandardMeterQ release = standard_meter_q_add(onset, standard_meter_q(duration_numerator(note_event_duration(event)),
                                                                              (long long)duration_denominator(note_event_duration(event))));
        TupletNote *note = &stream[stream_count];
        memset(note, 0, sizeof(*note));
        note->note = i;
        if (read_tuplet_note(map, standard_meter_snap(&config, onset, false, NULL), release, note)) {
            stream_count++;
        }
    }
    if (ok) {
        qsort(stream, stream_count, sizeof(TupletNote), compare_tuplet_notes);
        link_attacks(stream, stream_count);
        mark_triplets(stream, stream_count);
        mark_duplets(stream, stream_count);
        ok = collect_runs(&items, &count, &capacity, notes, map, stream, stream_count, 2, 3, scratch) &&
             collect_runs(&items, &count, &capacity, notes, map, stream, stream_count, 3, 2, scratch);
    }
    free(stream);
    free(scratch);
    if (!ok) {
        for (size_t i = 0; i < count; i++) {
            free(items[i].note_ids);
        }
        free(items);
        return false;
    }
    if (count > 1) {
        qsort(items, count, sizeof(AnalysisTuplet), compare_tuplets);
    }
    report->tuplets = items;
    report->tuplet_count = count;
    return true;
}

bool rhythm_stage_collect(const ImportedNotes *notes, const OuterVoiceLines *lines, const FormStage *form,
                          AnalysisReport *report) {
    const MeterMap *map = notes->meter.meter_map;
    if (!map || notes->count == 0 || !form->built || form->phrases.count == 0 ||
        form->phrases.count != report->phrase_count || lines->count == 0) {
        return true;
    }
    RhythmBuild build;
    memset(&build, 0, sizeof(build));
    if (standard_meter_input_init(&build.input, map) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    bool ok = build_notes(&build, notes, lines) && build_chord_changes(&build, report) && build_phrases(&build, form) &&
              collect_densities(&build, notes, report) && collect_syncopations(&build, report) &&
              collect_hemiolas(&build, report) && collect_hypermeter(&build, notes, report) &&
              collect_tuplets(&build, notes, report);
    build_destroy(&build);
    return ok;
}
