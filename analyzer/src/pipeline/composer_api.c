#include <ursatz_analyzer/composer_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "model/core/provenance_reference.h"
#include "model/core/voice_reference.h"
#include "model/identity/entity_id.h"
#include "io/midi_key_signature.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/pitch_identity.h"
#include "model/pitch/pitch_numeric_value.h"
#include "model/pitch/pitch_spelling.h"
#include "model/pitch/tuning_system.h"
#include "theory/tonality/common_practice_materials.h"
#include "theory/tonality/tonal_key.h"
#include "theory/style/style_model_build.h"
#include "theory/style/style_reference.h"
#include "model/time/duration.h"
#include "model/time/metrical_position.h"
#include "model/time/note_value.h"
#include "model/time/temporal_anchor.h"
#include "model/time/time_signature.h"

#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/style_profile.h>
#include <ursatz_analyzer/version.h>

#include "pipeline/analysis_result_internal.h"
#include "style/style_profile_internal.h"
#include "util/misc_util.h"

#define MIDI_NOTE_COUNT 128

/* The key signature (sharps or flats and the mode) of a tonic pitch class and a material; false when the material has none. */
static bool key_signature_of(int tonic, const PitchMaterial *material, int *fifths, bool *is_minor) {
    PitchLetter letter;
    int accidental;
    if (!material || !standard_tonality_key_tonic_of_pitch_class_material(tonic, material, &letter, &accidental) ||
        !standard_tonality_key_fifths_of(letter, accidental, material, fifths)) {
        return false;
    }
    CommonPracticeKeyMode mode;
    *is_minor = common_practice_key_mode_of(material, &mode) && mode == COMMON_PRACTICE_KEY_MINOR;
    return true;
}

/* What a hearing builds for the pass: an ImportedScore whose events are the caller's (imported_score_view). */
static void hearing_score_destroy(void *owner) {
    ImportedScore *score = owner;
    if (!score) {
        return;
    }
    imported_score_destroy(score);
    free(score);
}

static bool hearing_valid(const AnalyzerHearing *hearing) {
    if (!hearing || (hearing->count > 0 && (!hearing->events || !hearing->midi)) ||
        (hearing->has_key && (hearing->key_tonic_pitch_class < 0 || hearing->key_tonic_pitch_class > 11))) {
        return false;
    }
    for (size_t i = 0; i < hearing->count; i++) {
        if (!hearing->events[i] || hearing->midi[i] < 0 || hearing->midi[i] >= MIDI_NOTE_COUNT) {
            return false;
        }
    }
    return true;
}

/* The score of a hearing: the arrays are the score's, the events the caller's. NULL on allocation failure. */
static ImportedScore *hearing_score_build(const AnalyzerHearing *hearing) {
    ImportedScoreView view = {.events = hearing->events,
                              .count = hearing->count,
                              .midi = hearing->midi,
                              .meter = hearing->meter,
                              .streams = hearing->streams};
    if (hearing->has_key) {
        view.has_key = key_signature_of(hearing->key_tonic_pitch_class, hearing->key_material, &view.key_fifths,
                                        &view.key_minor);
        if (!view.has_key) {
            return NULL;
        }
    }
    ImportedScore *built = calloc(1, sizeof *built);
    if (!built) {
        return NULL;
    }
    if (imported_score_view(&view, built) != IMPORTED_SCORE_OK) {
        free(built);
        return NULL;
    }
    return built;
}

AnalyzerHearingOptions analyzer_hearing_options_default(void) {
    AnalyzerHearingOptions options = {analyzer_options_default()};
    return options;
}

AnalyzerStatus analyzer_hear(const AnalyzerHearing *hearing, const AnalyzerHearingOptions *options,
                             AnalyzerResult **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    AnalyzerHearingOptions used = options ? *options : analyzer_hearing_options_default();
    if (!hearing_valid(hearing)) {
        return ANALYZER_ERR_ARGUMENT;
    }
    ImportedScore *built = hearing_score_build(hearing);
    if (!built) {
        return ANALYZER_ERR_ALLOC;
    }
    AnalyzerResult *result = NULL;
    AnalyzerStatus status = analyzer_analyze(built, &used.analysis, &result);
    if (status != ANALYZER_OK) {
        hearing_score_destroy(built);
        return status;
    }
    analyzer_result_set_owner(result, built, hearing_score_destroy);
    *out = result;
    return ANALYZER_OK;
}

void analyzer_violations_free(AnalyzerViolation *violations, size_t count) {
    for (size_t i = 0; violations && i < count; i++) {
        free(violations[i].rule);
        free(violations[i].note_ids);
    }
    free(violations);
}

/* The measure and beat of a time in whole notes on the fragment's meter; 0 and 0.0 when it cannot be placed. */
static void place(const MeterMap *map, long numerator, unsigned long denominator, int *measure, double *beat) {
    *measure = 0;
    *beat = 0.0;
    MusicalTime time;
    MetricalPosition *position = NULL;
    const TimeSignature *signature = NULL;
    Duration unit;
    if (musical_time_make(numerator, (long long)denominator, &time) == MUSICAL_TIME_OK) {
        if (metrical_position_from_time(map, &time, &position) == METRICAL_POSITION_OK) {
            int m = metrical_position_measure(position);
            const Duration *offset = metrical_position_offset(position);
            if (metrical_position_reading_signature(map, m, &signature) == METRICAL_POSITION_OK &&
                time_signature_unit_duration(signature, &unit) == DURATION_OK && unit.value.num > 0) {
                *measure = m;
                *beat = 1.0 + ((double)offset->value.num / (double)offset->value.den) /
                                  ((double)unit.value.num / (double)unit.value.den);
            }
        }
    }
    metrical_position_destroy(position);
}

/* Appends one violation; the rule and the note ids are copied. false on allocation failure. */
static bool add_violation(const MeterMap *map, AnalyzerViolation **list, size_t *count, size_t *capacity, const char *rule, bool hard, long numerator, unsigned long denominator,
                          const size_t *note_ids, size_t note_id_count) {
    if (*count == *capacity) {
        size_t grown = *capacity ? *capacity * 2 : 8;
        AnalyzerViolation *bigger = realloc(*list, grown * sizeof **list);
        if (!bigger) {
            return false;
        }
        *list = bigger;
        *capacity = grown;
    }
    AnalyzerViolation *violation = &(*list)[*count];
    *violation = (AnalyzerViolation){0};
    violation->rule = strdup(rule);
    violation->note_ids = malloc((note_id_count > 0 ? note_id_count : 1) * sizeof *violation->note_ids);
    if (!violation->rule || !violation->note_ids) {
        free(violation->rule);
        free(violation->note_ids);
        return false;
    }
    memcpy(violation->note_ids, note_ids, note_id_count * sizeof *note_ids);
    violation->note_id_count = note_id_count;
    violation->hard = hard;
    place(map, numerator, denominator, &violation->measure, &violation->beat);
    (*count)++;
    return true;
}

static const char *voice_rule_id(const AnalysisVoiceViolation *fault) {
    if (strcmp(fault->rule, "parallel-fifths") == 0 || strcmp(fault->rule, "parallel-octaves") == 0) {
        return fault->rule;
    }
    return fault->arrival_interval == 5 ? "direct-fifths" : "direct-octaves";
}

AnalyzerStatus analyzer_result_violations(const AnalyzerResult *result, AnalyzerViolation **out, size_t *count) {
    if (!result || !out || !count) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    *count = 0;
    const ChainResult *report = analyzer_result_report(result);
    const ImportedScore *notes = analyzer_result_notes(result);
    if (!report || !notes) {
        return ANALYZER_ERR_ARGUMENT;
    }
    const MeterMap *map = notes->meter.meter_map;
    const AnalysisVoiceLeading *leading = &report->voice_leading;
    AnalyzerViolation *list = NULL;
    size_t used = 0, capacity = 0;
    bool ok = true;
    for (size_t i = 0; ok && leading->present && i < leading->violation_count; i++) {
        const AnalysisVoiceViolation *fault = &leading->violations[i];
        const ChainEntry *arrival = &report->entries[fault->entry_to];
        bool parallel = fault->rule[0] == 'p';
        ok = add_violation(map, &list, &used, &capacity, voice_rule_id(fault), parallel, arrival->time.num,
                           arrival->time.den, fault->note_ids, 4);
    }
    for (int pass = 0; ok && pass < 2; pass++) {
        const char *wanted = pass == 0 ? "seventh-resolves-down" : "leading-tone-resolves";
        for (size_t i = 0; ok && leading->present && i < leading->resolution_count; i++) {
            const AnalysisVoiceResolution *fault = &leading->resolutions[i];
            if (strcmp(fault->rule, wanted) != 0) {
                continue;
            }
            const ChainEntry *arrival = &report->entries[fault->entry_to];
            ok = add_violation(map, &list, &used, &capacity,
                               pass == 0 ? "unresolved-seventh" : "unresolved-leading-tone", false,
                               arrival->time.num, arrival->time.den, fault->note_ids, 2);
        }
    }
    if (!ok) {
        analyzer_violations_free(list, used);
        return ANALYZER_ERR_ALLOC;
    }
    *out = list;
    *count = used;
    return ANALYZER_OK;
}

/* ---- the style reference ---- */

struct AnalyzerStyleReference {
    StyleReference *reference; /* the library's (theory/style/style_reference.h), with the Analyzer's version */
};

void analyzer_style_reference_destroy(AnalyzerStyleReference *reference) {
    if (!reference) {
        return;
    }
    style_reference_destroy(reference->reference);
    free(reference);
}

AnalyzerStatus analyzer_style_reference_create(const StyleModel *model, const char *const *without,
                                               size_t without_count, AnalyzerStyleReference **out) {
    if (!out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    AnalyzerStyleReference *reference = calloc(1, sizeof *reference);
    if (!reference) {
        return ANALYZER_ERR_ALLOC;
    }
    StyleReferenceResult made =
        style_reference_create(model, without, without_count, URSATZ_ANALYZER_VERSION, &reference->reference);
    if (made != STYLE_REFERENCE_OK) {
        free(reference);
        return made == STYLE_REFERENCE_ERR_ARGUMENT ? ANALYZER_ERR_ARGUMENT : ANALYZER_ERR_ALLOC;
    }
    *out = reference;
    return ANALYZER_OK;
}

AnalyzerStatus analyzer_result_style_distance(const AnalyzerResult *result, const AnalyzerStyleReference *reference,
                                              StyleComparison *out) {
    if (!result || !reference || !out) {
        return ANALYZER_ERR_ARGUMENT;
    }
    memset(out, 0, sizeof *out);
    AnalyzerStatus status = ANALYZER_ERR_ALLOC;
    StyleProfile *profile = style_profile_create();
    StyleStatistic *fragment_stats = NULL;
    size_t fragment_count = 0;
    if (!profile) {
        goto done;
    }
    StyleProfileResult added = style_profile_add_result(profile, result);
    if (added != STYLE_PROFILE_OK) {
        status = added == STYLE_PROFILE_ERR_ALLOC ? ANALYZER_ERR_ALLOC : ANALYZER_ERR_ARGUMENT;
        goto done;
    }
    if (style_profile_statistics(profile, &fragment_stats, &fragment_count) != STYLE_PROFILE_OK) {
        goto done;
    }
    StyleReferenceResult measured = style_reference_distance(reference->reference, fragment_stats, fragment_count, out);
    status = measured == STYLE_REFERENCE_OK ? ANALYZER_OK
             : measured == STYLE_REFERENCE_ERR_COMPARE ? ANALYZER_ERR_LIBRARY
             : measured == STYLE_REFERENCE_ERR_ARGUMENT ? ANALYZER_ERR_ARGUMENT : ANALYZER_ERR_ALLOC;
done:
    free(fragment_stats);
    style_profile_destroy(profile);
    return status;
}
