#include "framework_v1_chain.h"

#include "analysis_motive.h"
#include "motive_util.h"

#include "interpretation/chord.h"
#include "interpretation/key.h"
#include "persistence/persistence.h"
#include "persistence/persistence_cadence.h"
#include "persistence/persistence_chord.h"
#include "persistence/persistence_interpretation.h"
#include "persistence/persistence_key.h"
#include "persistence/persistence_motif.h"
#include "persistence/persistence_motif_occurrence.h"
#include "persistence/persistence_note_events.h"
#include "persistence/persistence_phrase_occurrence.h"
#include "persistence/persistence_relationship.h"

/*
 * A whole analysis persisted and reloaded. Mozart 545 goes through the Framework v1 chain (tests/integration/
 * framework_v1_chain.h): note events, key regions (Keys), chords, cadences (Cadences and their Interpretations),
 * phrases (PhraseOccurrences), themes (a Relationship between a theme's parts). The motive analysis then runs on the same
 * piece with the chain's cadences and adds Motifs, MotifOccurrences, and the "occurrence-of" and "contained-in"
 * Relationships. Every one of those objects is the analysis' own output: nothing is built here from the shell
 * constructors. All of it is stored in one database; the database is closed and reopened; everything is loaded
 * back and compared with the original: counts, ids, order and field values, and the list functions' results.
 */

static unsigned stored_keys;
static unsigned stored_chords;
static unsigned stored_cadences;
static unsigned stored_phrases;
static unsigned stored_motifs;
static unsigned stored_motif_occurrences;
static unsigned stored_relationships;
static unsigned stored_interpretations;

static long count_rows(PersistenceHandle *handle, const char *table) {
    char sql[96];
    snprintf(sql, sizeof(sql), "SELECT COUNT(*) FROM %s", table);
    sqlite3_stmt *stmt = NULL;
    assert(sqlite3_prepare_v2(persistence_db(handle), sql, -1, &stmt, NULL) == SQLITE_OK);
    assert(sqlite3_step(stmt) == SQLITE_ROW);
    long count = (long)sqlite3_column_int64(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

/* --- comparing --- */

static void same_uncertainty(const Uncertainty *a, const Uncertainty *b) {
    assert(uncertainty_kind(a) == uncertainty_kind(b));
    if (uncertainty_kind(a) == UNCERTAINTY_KIND_SCALAR_CONFIDENCE) {
        assert(uncertainty_scalar_confidence_numerator(a) == uncertainty_scalar_confidence_numerator(b));
        assert(uncertainty_scalar_confidence_denominator(a) == uncertainty_scalar_confidence_denominator(b));
    } else if (uncertainty_kind(a) == UNCERTAINTY_KIND_CANDIDATE_SET) {
        assert(uncertainty_candidate_count(a) == uncertainty_candidate_count(b));
        for (size_t i = 0; i < uncertainty_candidate_count(a); i++) {
            assert(strcmp(uncertainty_candidate_label_at(a, i), uncertainty_candidate_label_at(b, i)) == 0);
            assert(uncertainty_candidate_weight_numerator_at(a, i) == uncertainty_candidate_weight_numerator_at(b, i));
            assert(uncertainty_candidate_weight_denominator_at(a, i) == uncertainty_candidate_weight_denominator_at(b, i));
        }
    }
}

static void same_interpretation(const Interpretation *a, const Interpretation *b) {
    assert(entity_id_equals(interpretation_id(a), interpretation_id(b)));
    assert(entity_id_equals(interpretation_target_id(a), interpretation_target_id(b)));
    assert(value_equals(interpretation_claim(a), interpretation_claim(b)));
    assert((interpretation_context_id(a) == NULL) == (interpretation_context_id(b) == NULL));
    if (interpretation_context_id(a) != NULL) {
        assert(entity_id_equals(interpretation_context_id(a), interpretation_context_id(b)));
    }
    assert((interpretation_evidence_id(a) == NULL) == (interpretation_evidence_id(b) == NULL));
    if (interpretation_evidence_id(a) != NULL) {
        assert(entity_id_equals(interpretation_evidence_id(a), interpretation_evidence_id(b)));
    }
    assert((interpretation_framework_reference(a) == NULL) == (interpretation_framework_reference(b) == NULL));
    if (interpretation_framework_reference(a) != NULL) {
        assert(entity_id_equals(framework_reference_target_id(interpretation_framework_reference(a)),
                                framework_reference_target_id(interpretation_framework_reference(b))));
    }
    same_uncertainty(interpretation_uncertainty(a), interpretation_uncertainty(b));
    assert(strcmp(interpretation_author(a), interpretation_author(b)) == 0);
    assert(strcmp(interpretation_method(a), interpretation_method(b)) == 0);
    assert(interpretation_status(a) == interpretation_status(b));
    assert(provenance_reference_equals(interpretation_provenance(a), interpretation_provenance(b)));
}

/* --- storing --- */

static void store_interpretation(PersistenceHandle *handle, const Interpretation *interpretation) {
    if (interpretation == NULL) {
        return;
    }
    PersistenceInterpretationResult stored = persistence_store_interpretation(handle, interpretation);
    if (stored != PERSISTENCE_INTERPRETATION_OK) {
        printf("interpretation %s: store failed (%d)\n", entity_id_value(interpretation_id(interpretation)), (int)stored);
    }
    assert(stored == PERSISTENCE_INTERPRETATION_OK);
    stored_interpretations++;
}

static void store_relationship(PersistenceHandle *handle, const Relationship *relationship) {
    if (relationship == NULL) {
        return;
    }
    PersistenceRelationshipResult stored = persistence_store_relationship(handle, relationship);
    if (stored != PERSISTENCE_RELATIONSHIP_OK) {
        printf("relationship %s: store failed (%d)\n", entity_id_value(relationship_id(relationship)), (int)stored);
    }
    assert(stored == PERSISTENCE_RELATIONSHIP_OK);
    stored_relationships++;
}

static void store_all(PersistenceHandle *handle, const Chain *c, const StandardFormMotives *motives) {
    assert(persistence_store_note_events(handle, c->p.events, c->p.note_count) == PERSISTENCE_NOTE_EVENTS_OK);

    for (size_t i = 0; i < c->regions.region_count; i++) {
        assert(persistence_store_key(handle, c->regions.regions[i].key) == PERSISTENCE_KEY_OK);
        stored_keys++;
    }
    for (size_t s = 0; s < c->count; s++) {
        if (c->chords[s].matched) {
            assert(persistence_store_chord(handle, c->chords[s].best_chord) == PERSISTENCE_CHORD_OK);
            stored_chords++;
        }
    }
    for (size_t i = 0; i < c->cadences.count; i++) {
        assert(persistence_store_cadence(handle, c->cadences.items[i].cadence) == PERSISTENCE_CADENCE_OK);
        stored_cadences++;
        store_interpretation(handle, c->cadences.items[i].interpretation);
    }
    for (size_t i = 0; i < c->form.phrases.count; i++) {
        const StandardFormPhrase *phrase = &c->form.phrases.items[i];
        assert(persistence_store_phrase_occurrence(handle, phrase->occurrence) == PERSISTENCE_PHRASE_OCCURRENCE_OK);
        stored_phrases++;
        store_interpretation(handle, phrase->interpretation);
    }
    for (size_t i = 0; i < c->form.themes.count; i++) {
        store_relationship(handle, c->form.themes.items[i].relationship);
        store_interpretation(handle, c->form.themes.items[i].interpretation);
    }
    for (size_t f = 0; f < motives->family_count; f++) {
        const StandardFormMotiveFamily *family = &motives->families[f];
        assert(persistence_store_motif(handle, family->motif) == PERSISTENCE_MOTIF_OK);
        stored_motifs++;
        store_interpretation(handle, family->interpretation);
        for (size_t o = 0; o < family->occurrence_count; o++) {
            const StandardFormMotiveOccurrence *occurrence = &family->occurrences[o];
            assert(persistence_store_motif_occurrence(handle, occurrence->occurrence) == PERSISTENCE_MOTIF_OCCURRENCE_OK);
            stored_motif_occurrences++;
            store_interpretation(handle, occurrence->interpretation);
            store_relationship(handle, occurrence->family_relationship);
            store_relationship(handle, occurrence->contained_relationship);
        }
    }
}

/* --- reloading --- */

static void check_relationship(PersistenceHandle *handle, const Relationship *original) {
    if (original == NULL) {
        return;
    }
    Relationship *loaded = NULL;
    assert(persistence_load_relationship(handle, entity_id_value(relationship_id(original)), &loaded) ==
           PERSISTENCE_RELATIONSHIP_OK);
    assert(entity_id_equals(relationship_id(original), relationship_id(loaded)));
    assert(entity_id_equals(relationship_source_id(original), relationship_source_id(loaded)));
    assert(strcmp(relationship_predicate_name(original), relationship_predicate_name(loaded)) == 0);
    assert(entity_id_equals(relationship_target_id(original), relationship_target_id(loaded)));
    assert((relationship_scope(original) == NULL) == (relationship_scope(loaded) == NULL));
    if (relationship_scope(original) != NULL) {
        assert(value_equals(relationship_scope(original), relationship_scope(loaded)));
    }
    assert((relationship_qualifiers(original) == NULL) == (relationship_qualifiers(loaded) == NULL));
    if (relationship_qualifiers(original) != NULL) {
        assert(value_equals(relationship_qualifiers(original), relationship_qualifiers(loaded)));
    }
    assert((relationship_uncertainty(original) == NULL) == (relationship_uncertainty(loaded) == NULL));
    if (relationship_uncertainty(original) != NULL) {
        assert(value_equals(relationship_uncertainty(original), relationship_uncertainty(loaded)));
    }
    assert((relationship_context_id(original) == NULL) == (relationship_context_id(loaded) == NULL));
    assert((relationship_evidence_id(original) == NULL) == (relationship_evidence_id(loaded) == NULL));
    assert(provenance_reference_equals(relationship_provenance(original), relationship_provenance(loaded)));
    relationship_destroy(loaded);
}

static void check_interpretation(PersistenceHandle *handle, const Interpretation *original) {
    if (original == NULL) {
        return;
    }
    Interpretation *loaded = NULL;
    assert(persistence_load_interpretation(handle, entity_id_value(interpretation_id(original)), &loaded) ==
           PERSISTENCE_INTERPRETATION_OK);
    same_interpretation(original, loaded);
    interpretation_destroy(loaded);
}

static void check_all(PersistenceHandle *handle, const Chain *c, const StandardFormMotives *motives) {
    /* the note events: same count, ids and order */
    NoteEvent **events = NULL;
    size_t event_count = 0;
    assert(persistence_load_note_events(handle, &events, &event_count) == PERSISTENCE_NOTE_EVENTS_OK);
    assert(event_count == c->p.note_count);
    for (size_t i = 0; i < event_count; i++) {
        assert(entity_id_equals(note_event_id(events[i]), note_event_id(c->p.events[i])));
        note_event_destroy(events[i]);
    }
    free(events);

    for (size_t i = 0; i < c->regions.region_count; i++) {
        const Key *original = c->regions.regions[i].key;
        Key *loaded = NULL;
        assert(persistence_load_key(handle, entity_id_value(interpretation_id(key_interpretation(original))), &loaded) ==
               PERSISTENCE_KEY_OK);
        same_interpretation(key_interpretation(original), key_interpretation(loaded));
        assert(strcmp(key_label(original), key_label(loaded)) == 0);
        key_destroy(loaded);
    }
    for (size_t s = 0; s < c->count; s++) {
        if (!c->chords[s].matched) {
            continue;
        }
        const Chord *original = c->chords[s].best_chord;
        Chord *loaded = NULL;
        assert(persistence_load_chord(handle, entity_id_value(interpretation_id(chord_interpretation(original))),
                                      &loaded) == PERSISTENCE_CHORD_OK);
        same_interpretation(chord_interpretation(original), chord_interpretation(loaded));
        assert(strcmp(chord_label(original), chord_label(loaded)) == 0);
        chord_destroy(loaded);
    }
    for (size_t i = 0; i < c->cadences.count; i++) {
        Cadence *loaded = NULL;
        assert(persistence_load_cadence(handle, entity_id_value(cadence_id(c->cadences.items[i].cadence)), &loaded) ==
               PERSISTENCE_CADENCE_OK);
        assert(entity_id_equals(cadence_id(c->cadences.items[i].cadence), cadence_id(loaded)));
        assert(cadence_event_id_count(c->cadences.items[i].cadence) == cadence_event_id_count(loaded));
        for (size_t e = 0; e < cadence_event_id_count(loaded); e++) {
            assert(entity_id_equals(cadence_event_id_at(c->cadences.items[i].cadence, e), cadence_event_id_at(loaded, e)));
        }
        assert((cadence_optional_harmonic_context(c->cadences.items[i].cadence) == NULL) ==
               (cadence_optional_harmonic_context(loaded) == NULL));
        cadence_destroy(loaded);
        check_interpretation(handle, c->cadences.items[i].interpretation);
    }

    for (size_t i = 0; i < c->form.phrases.count; i++) {
        const PhraseOccurrence *original = c->form.phrases.items[i].occurrence;
        PhraseOccurrence *loaded = NULL;
        assert(persistence_load_phrase_occurrence(handle, entity_id_value(phrase_occurrence_id(original)), &loaded) ==
               PERSISTENCE_PHRASE_OCCURRENCE_OK);
        assert(entity_id_equals(phrase_occurrence_phrase_id(original), phrase_occurrence_phrase_id(loaded)));
        assert(phrase_occurrence_region_event_id_count(original) == phrase_occurrence_region_event_id_count(loaded));
        for (size_t e = 0; e < phrase_occurrence_region_event_id_count(loaded); e++) {
            assert(entity_id_equals(phrase_occurrence_region_event_id_at(original, e),
                                    phrase_occurrence_region_event_id_at(loaded, e)));
        }
        phrase_occurrence_destroy(loaded);
        check_interpretation(handle, c->form.phrases.items[i].interpretation);

        /* the occurrences of this phrase id, in the order they were stored */
        PhraseOccurrence **same_phrase = NULL;
        size_t same_count = 0;
        assert(persistence_list_phrase_occurrences_by_phrase_id(
                   handle, entity_id_value(phrase_occurrence_phrase_id(original)), &same_phrase, &same_count) ==
               PERSISTENCE_PHRASE_OCCURRENCE_OK);
        size_t expected = 0;
        for (size_t j = 0; j < c->form.phrases.count; j++) {
            if (entity_id_equals(phrase_occurrence_phrase_id(c->form.phrases.items[j].occurrence),
                                 phrase_occurrence_phrase_id(original))) {
                assert(expected < same_count);
                assert(entity_id_equals(phrase_occurrence_id(same_phrase[expected]),
                                        phrase_occurrence_id(c->form.phrases.items[j].occurrence)));
                expected++;
            }
        }
        assert(expected == same_count);
        for (size_t j = 0; j < same_count; j++) {
            phrase_occurrence_destroy(same_phrase[j]);
        }
        free(same_phrase);
    }
    for (size_t i = 0; i < c->form.themes.count; i++) {
        check_relationship(handle, c->form.themes.items[i].relationship);
        check_interpretation(handle, c->form.themes.items[i].interpretation);
    }

    for (size_t f = 0; f < motives->family_count; f++) {
        const StandardFormMotiveFamily *family = &motives->families[f];
        Motif *loaded = NULL;
        assert(persistence_load_motif(handle, entity_id_value(motif_id(family->motif)), &loaded) == PERSISTENCE_MOTIF_OK);
        assert(motif_event_id_count(family->motif) == motif_event_id_count(loaded));
        for (size_t e = 0; e < motif_event_id_count(loaded); e++) {
            assert(entity_id_equals(motif_event_id_at(family->motif, e), motif_event_id_at(loaded, e)));
        }
        motif_destroy(loaded);
        check_interpretation(handle, family->interpretation);

        MotifOccurrence **listed = NULL;
        size_t listed_count = 0;
        assert(persistence_list_motif_occurrences_by_motif_id(handle, entity_id_value(motif_id(family->motif)), &listed,
                                                              &listed_count) == PERSISTENCE_MOTIF_OCCURRENCE_OK);
        assert(listed_count == family->occurrence_count); /* the family's own order */
        for (size_t o = 0; o < family->occurrence_count; o++) {
            const StandardFormMotiveOccurrence *occurrence = &family->occurrences[o];
            const MotifOccurrence *original = occurrence->occurrence;
            assert(entity_id_equals(motif_occurrence_id(original), motif_occurrence_id(listed[o])));
            assert(entity_id_equals(motif_occurrence_motif_id(original), motif_occurrence_motif_id(listed[o])));
            assert(motif_occurrence_region_event_id_count(original) == motif_occurrence_region_event_id_count(listed[o]));
            for (size_t e = 0; e < motif_occurrence_region_event_id_count(original); e++) {
                assert(entity_id_equals(motif_occurrence_region_event_id_at(original, e),
                                        motif_occurrence_region_event_id_at(listed[o], e)));
            }
            check_interpretation(handle, occurrence->interpretation);
            check_relationship(handle, occurrence->family_relationship);
            check_relationship(handle, occurrence->contained_relationship);
        }
        for (size_t o = 0; o < listed_count; o++) {
            motif_occurrence_destroy(listed[o]);
        }
        free(listed);

        /* the family's "occurrence-of" relationships, by source, in the order stored */
        Relationship **from_motif = NULL;
        size_t from_count = 0;
        assert(persistence_list_relationships_by_source(handle, entity_id_value(motif_id(family->motif)), &from_motif,
                                                        &from_count) == PERSISTENCE_RELATIONSHIP_OK);
        size_t expected_from = 0;
        for (size_t o = 0; o < family->occurrence_count; o++) {
            if (family->occurrences[o].family_relationship != NULL) {
                assert(expected_from < from_count);
                assert(entity_id_equals(relationship_id(from_motif[expected_from]),
                                        relationship_id(family->occurrences[o].family_relationship)));
                expected_from++;
            }
        }
        assert(expected_from == from_count);
        for (size_t r = 0; r < from_count; r++) {
            relationship_destroy(from_motif[r]);
        }
        free(from_motif);
    }

    assert((unsigned long)count_rows(handle, "keys") == stored_keys);
    assert((unsigned long)count_rows(handle, "chords") == stored_chords);
    assert((unsigned long)count_rows(handle, "cadences") == stored_cadences);
    assert((unsigned long)count_rows(handle, "phrase_occurrences") == stored_phrases);
    assert((unsigned long)count_rows(handle, "motifs") == stored_motifs);
    assert((unsigned long)count_rows(handle, "motif_occurrences") == stored_motif_occurrences);
    assert((unsigned long)count_rows(handle, "relationships") == stored_relationships);
}

int main(void) {
    Chain c;
    fv1_chain_run(&c, &FORM_PIECES[1]); /* Mozart 545 */

    /* the motive analysis on the same piece, with the chain's cadences */
    FormBuild build;
    form_build_init(&build, c.map);
    motive_build_soprano(&build, &c.p, c.x.keys_first, c.x.keys_last, 0, COMMON_PRACTICE_KEY_MAJOR);
    add_chain_cadences(&build, &c.p, &c.cadences);
    assert(standard_form_input_finish(&build.input) == STANDARD_FORM_ANALYSIS_OK);
    StandardFormPhrases phrases;
    StandardFormSegmentation segmentation;
    StandardFormThemes themes;
    StandardFormMotives motives;
    assert(standard_form_segment_phrases(&build.input, NULL, form_kernel_detect, NULL, &phrases) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(&build.input, &phrases, NULL, &segmentation) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_identify_themes(&build.input, &phrases, &segmentation, NULL, &themes) ==
           STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_find_motives(&build.input, &phrases, &segmentation, &themes, NULL, form_kernel_match_windows, NULL,
                                      &motives) == STANDARD_FORM_ANALYSIS_OK);

    const char *path = "test_persistence_full_analysis.sqlite";
    remove(path);
    PersistenceHandle *handle = NULL;
    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    store_all(handle, &c, &motives);
    printf("stored: %zu note events, %u keys, %u chords, %u cadences, %u phrases, %u motifs, %u motif occurrences, "
           "%u relationships, %u interpretations\n",
           c.p.note_count, stored_keys, stored_chords, stored_cadences, stored_phrases, stored_motifs,
           stored_motif_occurrences, stored_relationships, stored_interpretations);
    assert(stored_keys > 0 && stored_chords > 0 && stored_cadences > 0 && stored_phrases > 0);
    persistence_close(handle);

    assert(persistence_open(path, &handle) == PERSISTENCE_OK);
    check_all(handle, &c, &motives);
    persistence_close(handle);
    remove(path);

    standard_form_motives_destroy(&motives);
    standard_form_themes_destroy(&themes);
    standard_form_segmentation_destroy(&segmentation);
    standard_form_phrases_destroy(&phrases);
    fv1_chain_destroy(&c);
    printf("test_persistence_full_analysis: all tests passed\n");
    return 0;
}
