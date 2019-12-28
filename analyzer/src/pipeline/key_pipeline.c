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

static bool key_estimate_from_key_signature(MidiKeySignature key_signature, KeyEstimate *out) {
    if (!key_signature.found || key_signature.sharps_flats < -7 || key_signature.sharps_flats > 7) {
        return false;
    }

    int abs_count = key_signature.sharps_flats < 0 ? -key_signature.sharps_flats : key_signature.sharps_flats;
    int major_tonic = key_signature.sharps_flats >= 0 ? SHARPS_MAJOR_TONIC[abs_count] : FLATS_MAJOR_TONIC[abs_count];
    int tonic = key_signature.is_minor ? (major_tonic + 9) % 12 : major_tonic;

    snprintf(out->label, sizeof(out->label), "%s %s", pitch_class_name(tonic, key_signature.sharps_flats < 0),
             key_signature.is_minor ? "minor" : "major");
    out->tonic_pitch_class = tonic;
    out->is_major = !key_signature.is_minor;
    out->found = true;
    return true;
}

/* Tries one candidate tonic against KeyEstimation (via the same Structure
   region/relative-pitch-class buffer every candidate reuses) and, on a
   match, fills *estimate and returns true. relative must have room for
   count entries; it is overwritten on every call regardless of outcome. */
static bool try_tonic(Structure *region, const Framework *framework, NoteEvent *const *events,
                       PitchNumericValue *const *pitch_values, size_t count, int *relative, int tonic,
                       KeyEstimate *estimate) {
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
    strncpy(estimate->label, claim, sizeof(estimate->label) - 1);
    estimate->label[sizeof(estimate->label) - 1] = '\0';
    estimate->tonic_pitch_class = tonic;
    estimate->is_major = (strstr(claim, "major") != NULL);
    estimate->found = true;
    analysis_result_destroy(result);
    return true;
}

KeyEstimate estimate_global_key(NoteEvent *const *events, PitchNumericValue *const *pitch_values, size_t count,
                                 MidiKeySignature key_signature, const int *priority_tonics,
                                 size_t priority_tonic_count) {
    KeyEstimate estimate = {false, "", 0, false};
    if (count == 0) {
        return estimate;
    }

    if (key_estimate_from_key_signature(key_signature, &estimate)) {
        return estimate;
    }

    Structure *region = build_whole_piece_region(events, count);
    if (!region) {
        return estimate;
    }

    const Framework *framework = common_practice_minimal_framework();
    if (!framework) {
        structure_destroy(region);
        return estimate;
    }

    int *relative = malloc(sizeof(int) * count);
    if (!relative) {
        structure_destroy(region);
        return estimate;
    }

    for (size_t p = 0; p < priority_tonic_count && !estimate.found; p++) {
        int tonic = priority_tonics[p];
        if (tonic < 0 || tonic > 11) {
            continue;
        }
        try_tonic(region, framework, events, pitch_values, count, relative, tonic, &estimate);
    }

    for (int tonic = 0; tonic < 12 && !estimate.found; tonic++) {
        try_tonic(region, framework, events, pitch_values, count, relative, tonic, &estimate);
    }

    free(relative);
    structure_destroy(region);
    return estimate;
}
