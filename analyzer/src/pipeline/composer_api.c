#include <ursatz_analyzer/composer_api.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chain/chain_style.h"
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

AnalyzerStatus analyzer_result_violations(const AnalyzerResult *result, AnalyzerViolation **out, size_t *count) {
    if (!result || !out || !count) {
        return ANALYZER_ERR_ARGUMENT;
    }
    *out = NULL;
    *count = 0;
    if (!analyzer_result_report(result) || !analyzer_result_notes(result)) {
        return ANALYZER_ERR_ARGUMENT;
    }
    /* The library finds the faults (chain_result_voice_faults); a ChainVoiceFault has the members of an AnalyzerViolation. */
    ChainVoiceFault *faults = NULL;
    size_t used = 0;
    if (!chain_result_voice_faults(analyzer_result_run_chain(result), &faults, &used)) {
        return ANALYZER_ERR_ALLOC;
    }
    AnalyzerViolation *list = used ? malloc(used * sizeof *list) : NULL;
    if (used && !list) {
        chain_voice_faults_free(faults, used);
        return ANALYZER_ERR_ALLOC;
    }
    for (size_t i = 0; i < used; i++) {
        list[i] = (AnalyzerViolation){faults[i].rule, faults[i].hard, faults[i].measure, faults[i].beat,
                                      faults[i].note_ids, faults[i].note_id_count};
    }
    free(faults);
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
    if (!analyzer_result_report_complete(result)) {
        return ANALYZER_ERR_ARGUMENT;
    }
    StyleReferenceResult measured = chain_style_distance(analyzer_result_run_chain(result), reference->reference, out);
    return measured == STYLE_REFERENCE_OK ? ANALYZER_OK
           : measured == STYLE_REFERENCE_ERR_COMPARE ? ANALYZER_ERR_LIBRARY
           : measured == STYLE_REFERENCE_ERR_ARGUMENT ? ANALYZER_ERR_ARGUMENT : ANALYZER_ERR_ALLOC;
}
