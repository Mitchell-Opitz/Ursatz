#include "chain/chain_result_rhythm.h"


#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/rational.h"
#include "model/events/note_event.h"
#include "model/identity/entity_id.h"
#include "chain/chain_result_harmonic_rhythm.h"
#include "model/pitch/pitch_numeric_value.h"
#include "analysis/meter/meter_analysis_density.h"
#include "analysis/meter/meter_analysis_hemiola.h"
#include "analysis/meter/meter_analysis_hypermeter.h"
#include "analysis/meter/meter_analysis_check.h"
#include "analysis/meter/meter_analysis_input.h"
#include "analysis/meter/meter_analysis_tuplet.h"
#include "analysis/meter/meter_analysis_syncopation.h"
#include "theory/meter/borrowed_rules.h"
#include "analysis/notes/analysis_note.h"
#include "model/structure/structure.h"
#include "model/time/duration.h"
#include "model/time/meter_arithmetic.h"
#include "model/time/meter_map.h"
#include "model/time/musical_time.h"
#include "model/time/score_time.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_signature.h"
#include "model/time/time_span.h"
#include "model/time/tuplet.h"
#include "theory/vocabulary/framework_names.h"

/* qsort comparator for size_t values, ascending. */
static int size_less(const void *left, const void *right) {
    size_t a = *(const size_t *)left;
    size_t b = *(const size_t *)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}

/* The library's input (built by chain_build_meter_input, the one meter input builder) and what maps it back to the score. */
typedef struct {
    ChainMeterInput meter;
    const ChainScoreInput *score; /* the chain's neutral input: the ImportedScore index of a note id */
    int *lines; /* per ImportedScore index: its line (0 the outer soprano, 1 the bass, 2 any other), or -1 for a note not in the input */
} RhythmBuild;

static void build_destroy(RhythmBuild *build) {
    chain_meter_input_destroy(&build->meter);
    free(build->lines);
    memset(build, 0, sizeof(*build));
}

/* The ImportedScore index of the input note with the id, or SIZE_MAX with none. */
static size_t note_of_id(const RhythmBuild *build, const char *id) {
    size_t index = chain_score_input_import_index(build->score, id);
    return index == (size_t)-1 ? SIZE_MAX : index;
}

/* The input of every note, a chord change at the start of every chord segment whose harmony differs from the one before it (the
   harmonic rhythm's definition: another base numeral or key region) and the form stage's phrases with the arrival of their
   cadence and the length change of their expansion, all from the chain's one builder. Pickups count (first measure 0). */
static bool build_input(RhythmBuild *build, const ImportedScore *notes, const ChainScoreInput *input, const OuterVoiceLines *lines,
                        const ChainSegments *segments, const FormStage *form, const ChainResult *report) {
    bool *changes = NULL;
    if (!harmonic_rhythm_change_flags(report->entries, report->entry_count, &changes)) {
        return false;
    }
    ChainInputStatus built = chain_build_meter_input(&input->set, lines, segments, changes, 0, INT_MAX, &form->built.input,
                                                     &form->phrases, &form->expansions, &build->meter);
    free(changes);
    if (built != CHAIN_INPUT_OK) {
        return false;
    }
    build->score = input;
    build->lines = malloc((notes->count + 1) * sizeof(int));
    if (!build->lines) {
        return false;
    }
    for (size_t i = 0; i < notes->count; i++) {
        build->lines[i] = -1;
    }
    for (size_t k = 0; k < build->meter.note_count; k++) {
        size_t note = note_of_id(build, build->meter.notes[k].note_id);
        if (note == SIZE_MAX) {
            return false;
        }
        build->lines[note] = (int)build->meter.notes[k].voice;
    }
    return true;
}

/* ---- Density ---- */

/* The notes of phrase p on the line (-1: all), in time order: the notes whose onset, snapped as the library snaps it,
   is in the phrase's span. */
static size_t *phrase_notes(const RhythmBuild *build, const ImportedScore *notes, size_t phrase, int line, size_t *count) {
    StandardMeterConfig config = standard_meter_config_default();
    Rational start = build->meter.input.phrases[phrase].span.start.value;
    Rational end = build->meter.input.phrases[phrase].span.end.value;
    *count = 0;
    size_t *ids = malloc((build->meter.input.note_count + 1) * sizeof(size_t));
    if (!ids) {
        free(ids);
        return NULL;
    }
    for (size_t i = 0; i < notes->count; i++) {
        if (build->lines[i] < 0 || (line >= 0 && build->lines[i] != line)) {
            continue;
        }
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(notes->events[i]));
        Rational at = onset->value;
        if (!score_time_snap(at, config.onset_grid_denominator, config.onset_tolerance_denominator, &at, NULL)) {
            free(ids);
            *count = 0;
            return NULL;
        }
        if (rational_cmp(at, start) >= 0 && rational_cmp(at, end) < 0) {
            ids[(*count)++] = i;
        }
    }
    return ids;
}

static bool collect_densities(const RhythmBuild *build, const ImportedScore *notes, ChainResult *report) {
    StandardMeterDensities densities = {NULL, 0};
    if (standard_meter_find_densities(&build->meter.input, NULL, &densities) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = densities.count;
    AnalysisDensity *items = calloc(count > 0 ? count : 1, sizeof(AnalysisDensity));
    bool ok = items != NULL;
    const char *framework = FRAMEWORK_STANDARD_METER;
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
            item->bins[b].value = rational_ratio((long long)claim->bins[b].numerator, (long long)claim->bins[b].denominator);
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

/* Two syncopations of one place and kind, apart from the note. */
static bool same_syncopation_but_note(const AnalysisSyncopation *x, const AnalysisSyncopation *y) {
    return x->line == y->line && x->measure == y->measure && x->beat == y->beat && x->attack_level == y->attack_level &&
           x->through_level == y->through_level && x->strength == y->strength &&
           x->through_measure == y->through_measure && x->through_beat == y->through_beat;
}

static int syncopation_by_note(const void *a, const void *b) {
    size_t x = ((const AnalysisSyncopation *)a)->note;
    size_t y = ((const AnalysisSyncopation *)b)->note;
    return x < y ? -1 : (x > y ? 1 : 0);
}

/* The library reports the syncopations of notes that start together in the input's own order; the report keeps the order of
   the score: a run of syncopations of one place and kind is in the order of its notes. */
static void order_runs_by_note(AnalysisSyncopation *items, size_t count) {
    for (size_t first = 0; first < count;) {
        size_t last = first + 1;
        while (last < count && same_syncopation_but_note(&items[first], &items[last])) {
            last++;
        }
        qsort(items + first, last - first, sizeof(AnalysisSyncopation), syncopation_by_note);
        first = last;
    }
}

static bool collect_syncopations(const RhythmBuild *build, ChainResult *report) {
    StandardMeterSyncopations found = {NULL, 0};
    if (standard_meter_find_syncopations(&build->meter.input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.count;
    AnalysisSyncopation *items = calloc(count > 0 ? count : 1, sizeof(AnalysisSyncopation));
    const char *framework = FRAMEWORK_STANDARD_METER;
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
    order_runs_by_note(items, count);
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

static bool collect_hemiolas(const RhythmBuild *build, ChainResult *report) {
    StandardMeterHemiolas found = {NULL, 0};
    if (standard_meter_find_hemiolas(&build->meter.input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.count;
    AnalysisHemiola *items = calloc(count > 0 ? count : 1, sizeof(AnalysisHemiola));
    const char *framework = FRAMEWORK_STANDARD_METER;
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
            qsort(item->note_ids, item->note_id_count, sizeof(size_t), size_less);
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

static bool collect_hypermeter(const RhythmBuild *build, const ImportedScore *notes, ChainResult *report) {
    StandardMeterHypermeter found;
    memset(&found, 0, sizeof(found));
    if (standard_meter_find_hypermeter(&build->meter.input, NULL, &found) != STANDARD_METER_ANALYSIS_OK) {
        return false;
    }
    size_t count = found.phrase_count;
    AnalysisHypermeter *items = calloc(count > 0 ? count : 1, sizeof(AnalysisHypermeter));
    const char *framework = FRAMEWORK_STANDARD_METER;
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

static const char *meter_type_name(MeterType type) {
    return type == METER_TYPE_SIMPLE ? "simple" : (type == METER_TYPE_COMPOUND ? "compound" : "unclassified");
}

static const char *tuplet_level_name(TupletLevel level) {
    return level == TUPLET_LEVEL_BEAT ? "beat" : (level == TUPLET_LEVEL_SUBDIVISION ? "subdivision" : "multi-beat");
}

/* Every note of the input as the library's note set, and the tuplets the library finds in it. It reads all notes as one
   stream: a tuplet is a figure of the music, not of a line. */
static bool collect_tuplets(const RhythmBuild *build, const ImportedScore *notes, ChainResult *report) {
    (void)notes;
    size_t count = build->meter.input.note_count;
    const AnalysisNote *set_notes = build->meter.input.notes;
    StandardMeterTuplets found = {NULL, 0};
    AnalysisTuplet *items = NULL;
    AnalysisNoteSet set = {set_notes, count, build->meter.input.map};
    bool ok = standard_meter_find_tuplets(&set, NULL, &found) == STANDARD_METER_ANALYSIS_OK;
    if (ok && found.count > 0) {
        items = calloc(found.count, sizeof(AnalysisTuplet));
        ok = items != NULL;
    }
    for (size_t i = 0; ok && i < found.count; i++) {
        const StandardMeterTuplet *tuplet = &found.items[i];
        AnalysisTuplet *item = &items[i];
        item->note_ids = malloc((tuplet->note_count > 0 ? tuplet->note_count : 1) * sizeof(size_t));
        if (!item->note_ids) {
            ok = false;
            break;
        }
        for (size_t k = 0; k < tuplet->note_count; k++) {
            item->note_ids[k] = note_of_id(build, set_notes[tuplet->notes[k]].note_id);
        }
        item->note_id_count = tuplet->note_count;
        /* The notes of a tuplet in import order, as the report has always had them (the input is in time order). */
        qsort(item->note_ids, item->note_id_count, sizeof(size_t), size_less);
        item->framework = FRAMEWORK_STANDARD_METER;
        item->actual = tuplet->actual;
        item->normal = tuplet->normal;
        item->name = tuplet->name;
        item->meter = meter_type_name(tuplet->meter);
        item->borrowed = tuplet->borrowed;
        item->has_level = tuplet->has_level;
        if (tuplet->has_level) {
            item->level = tuplet_level_name(tuplet->level);
            item->level_typical = tuplet->level_typical;
        }
        item->measure = tuplet->measure;
        item->beat = tuplet->beat;
        item->attacks = tuplet->attacks;
    }
    size_t tuplet_count = found.count;
    standard_meter_tuplets_destroy(&found);
    if (!ok) {
        for (size_t i = 0; items && i < tuplet_count; i++) {
            free(items[i].note_ids);
        }
        free(items);
        return false;
    }
    report->tuplets = items;
    report->tuplet_count = tuplet_count;
    return true;
}

bool rhythm_stage_collect(const ImportedScore *notes, const ChainScoreInput *input, const OuterVoiceLines *lines,
                          const ChainSegments *segments, const FormStage *form, ChainResult *report) {
    const MeterMap *map = notes->meter.meter_map;
    if (!map || notes->count == 0 || !form->have_input || form->phrases.count == 0 ||
        form->phrases.count != report->phrase_count || lines->count == 0 || !segments) {
        return true;
    }
    RhythmBuild build;
    memset(&build, 0, sizeof(build));
    bool ok = build_input(&build, notes, input, lines, segments, form, report) &&
              collect_densities(&build, notes, report) && collect_syncopations(&build, report) &&
              collect_hemiolas(&build, report) && collect_hypermeter(&build, notes, report) &&
              collect_tuplets(&build, notes, report);
    build_destroy(&build);
    return ok;
}
