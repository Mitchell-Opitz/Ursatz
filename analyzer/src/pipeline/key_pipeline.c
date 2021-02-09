#include "pipeline/key_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/key_estimation.h"
#include "core/entity.h"
#include "core/provenance_reference.h"
#include "core/value.h"
#include "framework.h"
#include "identity/entity_id.h"
#include "identity/schema_version.h"
#include "pipeline/interpretation_util.h"
#include "pipeline/pitch_class_util.h"
#include "structure/structure.h"

static Structure *build_whole_piece_region(NoteEvent *const *events, size_t count) {
    EntityID *region_id;
    if (entity_id_create("ursatz-analyzer-region-whole-piece", &region_id) != ENTITY_ID_OK) {
        return NULL;
    }
    SchemaVersion *schema_version;
    if (schema_version_create(1, &schema_version) != SCHEMA_VERSION_OK) {
        entity_id_destroy(region_id);
        return NULL;
    }
    ProvenanceReference *provenance;
    if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
        schema_version_destroy(schema_version);
        entity_id_destroy(region_id);
        return NULL;
    }
    Entity *entity;
    if (entity_create(region_id, "Structure", schema_version, NULL, provenance, &entity) != ENTITY_OK) {
        provenance_reference_destroy(provenance);
        schema_version_destroy(schema_version);
        entity_id_destroy(region_id);
        return NULL;
    }

    EntityID **event_ids = malloc(sizeof(EntityID *) * count);
    if (!event_ids) {
        entity_destroy(entity);
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        if (entity_id_create(entity_id_value(note_event_id(events[i])), &event_ids[i]) != ENTITY_ID_OK) {
            for (size_t k = 0; k < i; k++) {
                entity_id_destroy(event_ids[k]);
            }
            free(event_ids);
            entity_destroy(entity);
            return NULL;
        }
    }

    Structure *region;
    if (structure_create(entity, event_ids, count, &region) != STRUCTURE_OK) {
        for (size_t i = 0; i < count; i++) {
            entity_id_destroy(event_ids[i]);
        }
        free(event_ids);
        entity_destroy(entity);
        return NULL;
    }
    free(event_ids);
    return region;
}

/* Major-key tonic pitch class for each sharps/flats count 0-7, per the
   circle of fifths (index 0 = C, no accidentals). The minor tonic
   pitch class for the same count is always 9 semitones above the
   major tonic (the relative minor), regardless of sharps/flats sign. */
static const int SHARPS_MAJOR_TONIC[8] = {0, 7, 2, 9, 4, 11, 6, 1};  /* C G D A E B F# C# */
static const int FLATS_MAJOR_TONIC[8] = {0, 5, 10, 3, 8, 1, 6, 11};  /* C F Bb Eb Ab Db Gb Cb */

/* Reverse lookups of SHARPS_MAJOR_TONIC/FLATS_MAJOR_TONIC: for a given
   tonic pitch class, how many sharps (or flats) its major key needs,
   or NO_ACCIDENTAL_COUNT if that pitch class has no representation
   within the practical 0-7 range on that side (i.e. it would require a
   double sharp/flat). Built once from the two source tables above. */
#define NO_ACCIDENTAL_COUNT 99

static int sharps_needed_for_tonic(int tonic_pitch_class) {
    for (int count = 0; count < 8; count++) {
        if (SHARPS_MAJOR_TONIC[count] == tonic_pitch_class) {
            return count;
        }
    }
    return NO_ACCIDENTAL_COUNT;
}

static int flats_needed_for_tonic(int tonic_pitch_class) {
    for (int count = 0; count < 8; count++) {
        if (FLATS_MAJOR_TONIC[count] == tonic_pitch_class) {
            return count;
        }
    }
    return NO_ACCIDENTAL_COUNT;
}

/* Whether tonic_pitch_class reads more naturally in flats than sharps,
   absent any other signal (e.g. no key signature to defer to): true
   when its flat-side circle-of-fifths count is strictly lower than its
   sharp-side count -- the spelling that avoids double sharps/flats.
   Natural-letter tonics (C, D, E, F, G, A, B) have identical sharp and
   flat spellings, so the tie there is harmless either way. The one
   real tie, F#/Gb (6 of each), defaults to sharp (F#) per common
   notation convention. */
static bool prefers_flat_spelling(int tonic_pitch_class) {
    return flats_needed_for_tonic(tonic_pitch_class) < sharps_needed_for_tonic(tonic_pitch_class);
}

/* Returns true when the key signature decides the key (including when
   its label could not be allocated, reported via *out_of_memory). */
static bool key_estimate_from_key_signature(MidiKeySignature key_signature, KeyEstimate *out,
                                            bool *out_of_memory) {
    if (!key_signature.found || key_signature.sharps_flats < -7 || key_signature.sharps_flats > 7) {
        return false;
    }

    int abs_count = key_signature.sharps_flats < 0 ? -key_signature.sharps_flats : key_signature.sharps_flats;
    int major_tonic = key_signature.sharps_flats >= 0 ? SHARPS_MAJOR_TONIC[abs_count] : FLATS_MAJOR_TONIC[abs_count];
    int tonic = key_signature.is_minor ? (major_tonic + 9) % 12 : major_tonic;

    const char *tonic_name = pitch_class_name(tonic, key_signature.sharps_flats < 0);
    const char *mode_name = key_signature.is_minor ? "minor" : "major";
    size_t label_size = (size_t)snprintf(NULL, 0, "%s %s", tonic_name, mode_name) + 1;
    char *label = malloc(label_size);
    if (!label) {
        *out_of_memory = true;
        return true;
    }
    snprintf(label, label_size, "%s %s", tonic_name, mode_name);

    out->label = label;
    out->tonic_pitch_class = tonic;
    out->is_major = !key_signature.is_minor;
    out->prefer_flats = key_signature.sharps_flats < 0;
    out->method = ANALYSIS_KEY_METHOD_KEY_SIGNATURE;
    out->found = true;
    return true;
}

/* Tries one candidate tonic against KeyEstimation (via the same Structure
   region/relative-pitch-class buffer every candidate reuses) and, on a
   match, fills *estimate and returns true. relative must have room for
   count entries; it is overwritten on every call regardless of outcome.
   A match whose label cannot be allocated sets *out_of_memory instead. */
static bool try_tonic(Structure *region, const Framework *framework, NoteEvent *const *events,
                       PitchNumericValue *const *pitch_values, size_t count, int *relative, int tonic,
                       KeyEstimate *estimate, bool *out_of_memory) {
    (void)events;
    for (size_t i = 0; i < count; i++) {
        long note_number = pitch_numeric_value_position_numerator(pitch_values[i]);
        int pitch_class = pitch_class_of(note_number);
        relative[i] = ((pitch_class - tonic) % 12 + 12) % 12;
    }

    KeyEstimationInput input = {region, framework, pitch_class_name(tonic, prefers_flat_spelling(tonic)), relative,
                                 count};
    AnalysisResult *result = NULL;
    if (key_estimation_analyze(&input, NULL, NULL, &result) != ANALYZER_OK) {
        return false;
    }

    const Interpretation *interpretation = analysis_result_interpretation_at(result, 0);
    size_t claim_size;
    const char *claim = (const char *)value_content(interpretation_claim(interpretation), &claim_size);
    char *label = strdup(claim);
    if (!label) {
        *out_of_memory = true;
        analysis_result_destroy(result);
        return false;
    }
    estimate->label = label;
    estimate->tonic_pitch_class = tonic;
    estimate->is_major = (strstr(claim, "major") != NULL);
    estimate->prefer_flats = prefers_flat_spelling(tonic);
    estimate->confidence = interpretation_confidence(interpretation);
    estimate->found = true;
    analysis_result_destroy(result);
    return true;
}

bool estimate_global_key(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                         MidiKeySignature key_signature, const int *priority_tonics, size_t priority_tonic_count,
                         KeyEstimate *out) {
    *out = (KeyEstimate){false, NULL, 0, false, false, ANALYSIS_KEY_METHOD_NONE, {false, 0, 0}};
    bool out_of_memory = false;
    if (count == 0) {
        return true;
    }

    if (key_estimate_from_key_signature(key_signature, out, &out_of_memory)) {
        return !out_of_memory;
    }

    Structure *region = build_whole_piece_region(events, count);
    if (!region) {
        return true;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (!framework) {
        structure_destroy(region);
        return true;
    }

    int *relative = malloc(sizeof(int) * count);
    if (!relative) {
        structure_destroy(region);
        return true;
    }

    for (size_t p = 0; p < priority_tonic_count && !out->found && !out_of_memory; p++) {
        int tonic = priority_tonics[p];
        if (tonic < 0 || tonic > 11) {
            continue;
        }
        if (try_tonic(region, framework, events, pitch_values, count, relative, tonic, out, &out_of_memory)) {
            out->method = ANALYSIS_KEY_METHOD_BASS_FALLBACK;
        }
    }

    for (int tonic = 0; tonic < 12 && !out->found && !out_of_memory; tonic++) {
        if (try_tonic(region, framework, events, pitch_values, count, relative, tonic, out, &out_of_memory)) {
            out->method = ANALYSIS_KEY_METHOD_PITCH_CLASS_SCAN;
        }
    }

    free(relative);
    structure_destroy(region);
    return !out_of_memory;
}

void key_estimate_destroy(KeyEstimate *estimate) {
    if (!estimate) {
        return;
    }
    free(estimate->label);
    estimate->label = NULL;
    estimate->found = false;
    estimate->method = ANALYSIS_KEY_METHOD_NONE;
}
