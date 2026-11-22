#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_snapshot.h"
#include "model/pitch/pitch_material.h"

/*
 * The knowledge snapshot: the four tonality modes the build embeds, found by id with their fields and steps; the entries it holds by
 * kind (nine materials, eighteen relations, nine chord types) and the steps of the seven diatonic modes; the hash; a user's own
 * file; the files and entries it refuses, each with its status and a message naming the file, the id and the field. The material
 * details (key word, family, brightness, color notes, links), the lookup by word, the chord types by id and by pitch classes, and the
 * relations of each family are read from the embedded entries and from a user file; the refusals name the file, the id and the field.
 */

static const char *MODE_IDS[4] = {"omt.material.major", "omt.material.natural-minor", "omt.material.harmonic-minor",
                                  "omt.material.melodic-minor"};
static const int MODE_STEPS[4][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 3, 5, 7, 9, 11}};
static const int MELODIC_DESCENDING[7] = {0, 2, 3, 5, 7, 8, 10};

static const char *USER_FILE =
    "{\"schema\":\"ursatz-knowledge\",\"schema_version\":1,\"source\":\"user\",\"entries\":["
    "{\"id\":\"user.material.test\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"test\","
    "\"content\":{\"steps\":[0,2,7]},\"citations\":[],\"status\":\"active\",\"version\":1},"
    "{\"id\":\"user.chord-type.test\",\"kind\":\"chord-type\",\"name\":\"test chord\",\"content\":{\"construction\":\"tertian\",\"members\":[{\"role\":\"root\",\"semitones\":0},"
    "{\"role\":\"third\",\"semitones\":4}]},\"citations\":[],"
    "\"status\":\"reference\",\"version\":2}]}";

static KnowledgeSnapshot *build(const KnowledgeUserFile *files, size_t count) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(files, count, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

static bool lower_hex16(const char *text) {
    if (text == NULL || strlen(text) != 16) {
        return false;
    }
    for (size_t i = 0; i < 16; i++) {
        if (!((text[i] >= '0' && text[i] <= '9') || (text[i] >= 'a' && text[i] <= 'f'))) {
            return false;
        }
    }
    return true;
}

static void test_embedded_modes(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    assert(knowledge_snapshot_entry_count(snapshot) == 89);
    /* in id order */
    assert(strcmp(knowledge_snapshot_entry_at(snapshot, 0)->id, "omt.chord-type.augmented-triad") == 0);
    assert(strcmp(knowledge_snapshot_entry_at(snapshot, 88)->id, "omt.vocabulary.tonic") == 0);
    assert(knowledge_snapshot_entry_at(snapshot, 89) == NULL);

    for (int mode = 0; mode < 4; mode++) {
        const KnowledgeEntry *entry = knowledge_snapshot_entry(snapshot, MODE_IDS[mode]);
        assert(entry != NULL);
        assert(strcmp(entry->id, MODE_IDS[mode]) == 0 && strcmp(entry->kind, "material") == 0);
        assert(entry->subtype != NULL && strcmp(entry->subtype, "unordered") == 0);
        assert(entry->status == KNOWLEDGE_STATUS_ACTIVE && entry->version == 1);
        assert(lower_hex16(entry->hash));

        const PitchMaterial *material = knowledge_snapshot_material(snapshot, MODE_IDS[mode]);
        assert(material != NULL && strcmp(material->id, MODE_IDS[mode]) == 0);
        assert(material->step_count == 7);
        for (int d = 0; d < 7; d++) {
            assert(material->steps[d] == MODE_STEPS[mode][d]);
            assert(pitch_material_step(material, d + 1) == MODE_STEPS[mode][d]);
        }
        assert(material->descending_step_count == (mode == 3 ? 7u : 0u));
    }
    assert(strcmp(knowledge_snapshot_entry(snapshot, "omt.material.natural-minor")->name, "natural minor") == 0);
    const PitchMaterial *melodic = knowledge_snapshot_material(snapshot, "omt.material.melodic-minor");
    for (int d = 0; d < 7; d++) {
        assert(melodic->descending_steps[d] == MELODIC_DESCENDING[d]);
    }

    assert(knowledge_snapshot_entry(snapshot, "omt.material.missing") == NULL);
    assert(knowledge_snapshot_material(snapshot, "omt.material.missing") == NULL);
    assert(knowledge_snapshot_entry(snapshot, NULL) == NULL);
    knowledge_snapshot_destroy(snapshot);
}

static const char *DIATONIC_IDS[7] = {"omt.material.locrian", "omt.material.phrygian", "omt.material.natural-minor",
                                      "omt.material.dorian", "omt.material.mixolydian", "omt.material.major",
                                      "omt.material.lydian"};
static const int DIATONIC_STEPS[7][7] = {{0, 1, 3, 5, 6, 8, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 10},
                                         {0, 2, 3, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 10}, {0, 2, 4, 5, 7, 9, 11},
                                         {0, 2, 4, 6, 7, 9, 11}};

static void test_embedded_counts(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    size_t materials = 0, relations = 0, chord_types = 0, instruments = 0, ensembles = 0, vocabulary = 0, functions = 0, theme_types = 0, others = 0;
    for (size_t i = 0; i < knowledge_snapshot_entry_count(snapshot); i++) {
        const KnowledgeEntry *entry = knowledge_snapshot_entry_at(snapshot, i);
        assert(entry != NULL && (entry->status == KNOWLEDGE_STATUS_ACTIVE || strcmp(entry->kind, "vocabulary") == 0 ||
                           strcmp(entry->kind, "function") == 0));
        if (strcmp(entry->kind, "material") == 0) {
            materials++;
        } else if (strcmp(entry->kind, "relation") == 0) {
            relations++;
        } else if (strcmp(entry->kind, "chord-type") == 0) {
            chord_types++;
        } else if (strcmp(entry->kind, "instrument") == 0 && strcmp(entry->subtype, "instrument") == 0) {
            instruments++;
        } else if (strcmp(entry->kind, "instrument") == 0 && strcmp(entry->subtype, "ensemble") == 0) {
            ensembles++;
        } else if (strcmp(entry->kind, "vocabulary") == 0) {
            vocabulary++;
        } else if (strcmp(entry->kind, "function") == 0) {
            functions++;
        } else if (strcmp(entry->kind, "theme-type") == 0) {
            theme_types++;
        } else {
            others++;
        }
    }
    assert(materials == 9 && relations == 18 && chord_types == 9 && instruments == 1 && ensembles == 1 && vocabulary == 32 &&
           functions == 10 && theme_types == 9 && others == 0);
    /* the seven diatonic modes, darkest to brightest: each a rotation of the major scale with its own steps */
    for (int mode = 0; mode < 7; mode++) {
        const PitchMaterial *material = knowledge_snapshot_material(snapshot, DIATONIC_IDS[mode]);
        assert(material != NULL && material->step_count == 7 && material->descending_step_count == 0);
        for (int d = 0; d < 7; d++) {
            assert(material->steps[d] == DIATONIC_STEPS[mode][d]);
        }
    }
    /* relations and chord types are entries, not pitch materials */
    assert(knowledge_snapshot_entry(snapshot, "omt.relation.supertonic") != NULL);
    assert(knowledge_snapshot_material(snapshot, "omt.relation.supertonic") == NULL);
    assert(knowledge_snapshot_entry(snapshot, "omt.chord-type.diminished-seventh") != NULL);
    assert(knowledge_snapshot_material(snapshot, "omt.chord-type.diminished-seventh") == NULL);
    /* the instrument entries are loaded, each under its own subtype only */
    assert(knowledge_snapshot_instrument(snapshot, "omt.instrument.piano") != NULL);
    assert(knowledge_snapshot_instrument(snapshot, "omt.instrument.solo-piano") == NULL);
    assert(knowledge_snapshot_ensemble(snapshot, "omt.instrument.solo-piano") != NULL);
    assert(knowledge_snapshot_ensemble(snapshot, "omt.instrument.piano") == NULL);
    knowledge_snapshot_destroy(snapshot);
}

static void test_material_queries(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    const PitchMaterial *major = knowledge_snapshot_material(snapshot, "omt.material.major");
    assert(pitch_material_step(major, 0) == -1 && pitch_material_step(major, 8) == -1 && pitch_material_step(major, -1) == -1);
    assert(pitch_material_step(major, 1) == 0 && pitch_material_step(major, 7) == 11);
    assert(pitch_material_step(NULL, 1) == -1);
    assert(pitch_material_contains(major, 0) && pitch_material_contains(major, 11) && pitch_material_contains(major, 7));
    assert(!pitch_material_contains(major, 1) && !pitch_material_contains(major, 6));
    assert(pitch_material_contains(major, 12) && pitch_material_contains(major, -1) && pitch_material_contains(major, -13));
    assert(!pitch_material_contains(major, 13) && !pitch_material_contains(major, -12 + 1));
    assert(!pitch_material_contains(NULL, 0));
    knowledge_snapshot_destroy(snapshot);
}

static void expect_refused(const char *text, KnowledgeStatus status, const char *needle_a, const char *needle_b,
                           const char *needle_c) {
    KnowledgeUserFile file = {"broken.json", text};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&file, 1, &snapshot, message, sizeof(message)) == status);
    assert(snapshot == NULL);
    assert(strstr(message, "broken.json") != NULL);
    assert(needle_a == NULL || strstr(message, needle_a) != NULL);
    assert(needle_b == NULL || strstr(message, needle_b) != NULL);
    assert(needle_c == NULL || strstr(message, needle_c) != NULL);
}

static KnowledgeSnapshot *build_user(const char *text) {
    KnowledgeUserFile file = {"mine.json", text};
    return build(&file, 1);
}

static void test_material_details(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    const PitchMaterial *major = knowledge_snapshot_material(snapshot, "omt.material.major");
    assert(strcmp(major->key_word, "major") == 0 && strcmp(major->family, "diatonic") == 0);
    assert(major->has_brightness && major->brightness_rank == 6 && major->color_note_count == 0);
    assert(major->variant_of == NULL && major->rotation_of == NULL);

    const PitchMaterial *minor = knowledge_snapshot_material(snapshot, "omt.material.natural-minor");
    assert(strcmp(minor->name, "natural minor") == 0 && strcmp(minor->key_word, "minor") == 0);
    assert(minor->has_brightness && minor->brightness_rank == 3);
    assert(minor->color_note_count == 3);
    assert(minor->color_notes[0].degree == 3 && minor->color_notes[0].alteration == -1);
    assert(minor->color_notes[1].degree == 6 && minor->color_notes[1].alteration == -1);
    assert(minor->color_notes[2].degree == 7 && minor->color_notes[2].alteration == -1);
    assert(minor->variant_of == major && minor->rotation_of == NULL);

    const PitchMaterial *lydian = knowledge_snapshot_material(snapshot, "omt.material.lydian");
    assert(lydian->has_brightness && lydian->brightness_rank == 7 && lydian->color_note_count == 1);
    assert(lydian->color_notes[0].degree == 4 && lydian->color_notes[0].alteration == 1);
    assert(lydian->rotation_of == major && lydian->variant_of == NULL);
    assert(strcmp(lydian->key_word, "lydian") == 0); /* the name when the entry gives no key word */
    const PitchMaterial *locrian = knowledge_snapshot_material(snapshot, "omt.material.locrian");
    assert(locrian->brightness_rank == 1 && locrian->color_note_count == 5 && locrian->rotation_of == major);

    /* the ranks of the seven diatonic modes are 1 to 7, each once */
    unsigned ranks = 0;
    for (int mode = 0; mode < 7; mode++) {
        const PitchMaterial *material = knowledge_snapshot_material(snapshot, DIATONIC_IDS[mode]);
        assert(material->brightness_rank == mode + 1 && pitch_material_is_diatonic(material));
        ranks |= 1u << material->brightness_rank;
    }
    assert(ranks == 0xFEu);

    const PitchMaterial *harmonic = knowledge_snapshot_material(snapshot, "omt.material.harmonic-minor");
    assert(strcmp(harmonic->key_word, "harmonic minor") == 0 && strcmp(harmonic->family, "minor") == 0);
    assert(!harmonic->has_brightness && !pitch_material_is_diatonic(harmonic) && harmonic->variant_of == minor);
    assert(pitch_material_third(harmonic) == 3 && pitch_material_third(major) == 4);
    assert(pitch_material_mask(major, 0) == 0xAB5u && pitch_material_mask(minor, 9) == pitch_material_mask(major, 0));
    knowledge_snapshot_destroy(snapshot);
}

static void test_material_named(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    const PitchMaterial *major = knowledge_snapshot_material(snapshot, "omt.material.major");
    const PitchMaterial *minor = knowledge_snapshot_material(snapshot, "omt.material.natural-minor");
    assert(knowledge_snapshot_material_named(snapshot, "major") == major);
    assert(knowledge_snapshot_material_named(snapshot, "MAJOR") == major);
    assert(knowledge_snapshot_material_named(snapshot, "ionian") == major);
    assert(knowledge_snapshot_material_named(snapshot, "Major Scale") == major);
    assert(knowledge_snapshot_material_named(snapshot, "omt.material.major") == major);
    assert(knowledge_snapshot_material_named(snapshot, "minor") == minor); /* the key word, also an alias */
    assert(knowledge_snapshot_material_named(snapshot, "natural minor") == minor);
    assert(knowledge_snapshot_material_named(snapshot, "aeolian") == minor);
    assert(knowledge_snapshot_material_named(snapshot, "harmonic minor") ==
           knowledge_snapshot_material(snapshot, "omt.material.harmonic-minor"));
    assert(knowledge_snapshot_material_named(snapshot, "Dorian") == knowledge_snapshot_material(snapshot, "omt.material.dorian"));
    assert(knowledge_snapshot_material_named(snapshot, "locrian") != NULL);
    assert(knowledge_snapshot_material_named(snapshot, "wholetone") == NULL);
    assert(knowledge_snapshot_material_named(snapshot, "") == NULL);
    assert(knowledge_snapshot_material_named(snapshot, NULL) == NULL);
    assert(knowledge_snapshot_material_named(NULL, "major") == NULL);
    /* a relation or a chord type is not a material, by id or by name */
    assert(knowledge_snapshot_material_named(snapshot, "omt.relation.supertonic") == NULL);
    assert(knowledge_snapshot_material_named(snapshot, "dominant seventh") == NULL);

    /* in id order */
    assert(knowledge_snapshot_material_count(snapshot) == 9);
    assert(strcmp(knowledge_snapshot_material_at(snapshot, 0)->id, "omt.material.dorian") == 0);
    assert(strcmp(knowledge_snapshot_material_at(snapshot, 4)->id, "omt.material.major") == 0);
    assert(strcmp(knowledge_snapshot_material_at(snapshot, 8)->id, "omt.material.phrygian") == 0);
    assert(knowledge_snapshot_material_at(snapshot, 9) == NULL);
    assert(knowledge_snapshot_material_count(NULL) == 0 && knowledge_snapshot_material_at(NULL, 0) == NULL);
    knowledge_snapshot_destroy(snapshot);

    /* two materials with the same key word, name or alias are ambiguous: NULL; a unique one is found */
    KnowledgeSnapshot *with_user = build_user(
        "{\"entries\":["
        "{\"id\":\"user.material.first\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"first\","
        "\"aliases\":[\"twin\"],\"content\":{\"steps\":[0,2,7],\"key_word\":\"major\"},\"status\":\"active\",\"version\":1},"
        "{\"id\":\"user.material.second\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"second\","
        "\"aliases\":[\"twin\",\"solo\"],\"content\":{\"steps\":[0,3,7]},\"status\":\"active\",\"version\":1}]}");
    assert(knowledge_snapshot_material_count(with_user) == 11);
    assert(knowledge_snapshot_material_named(with_user, "twin") == NULL);
    assert(knowledge_snapshot_material_named(with_user, "major") == NULL); /* its key word and the repository's */
    assert(knowledge_snapshot_material_named(with_user, "solo") == knowledge_snapshot_material(with_user, "user.material.second"));
    assert(knowledge_snapshot_material_named(with_user, "first") == knowledge_snapshot_material(with_user, "user.material.first"));
    assert(knowledge_snapshot_material_named(with_user, "omt.material.major") ==
           knowledge_snapshot_material(with_user, "omt.material.major"));
    knowledge_snapshot_destroy(with_user);
}

static unsigned semitone_mask(const ChordType *type) {
    unsigned mask = 0;
    for (size_t i = 1; i < type->member_count; i++) {
        mask |= 1u << (type->members[i].semitones % 12);
    }
    return mask;
}

static void test_chord_types(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    const ChordType *major = knowledge_snapshot_chord_type(snapshot, "omt.chord-type.major-triad");
    assert(major != NULL && strcmp(major->id, "omt.chord-type.major-triad") == 0 && strcmp(major->name, "major triad") == 0);
    assert(major->member_count == 3);
    assert(major->members[0].role == CHORD_MEMBER_ROOT && major->members[0].semitones == 0);
    assert(major->members[1].role == CHORD_MEMBER_THIRD && major->members[1].semitones == 4);
    assert(major->members[2].role == CHORD_MEMBER_FIFTH && major->members[2].semitones == 7);
    const ChordType *dominant = knowledge_snapshot_chord_type(snapshot, "omt.chord-type.dominant-seventh");
    assert(dominant->member_count == 4 && dominant->members[3].role == CHORD_MEMBER_SEVENTH && dominant->members[3].semitones == 10);
    const ChordType *diminished = knowledge_snapshot_chord_type(snapshot, "omt.chord-type.diminished-seventh");
    assert(diminished->member_count == 4 && diminished->members[3].semitones == 9);

    /* each of the nine is found by the pitch classes above its root, and by no other mask */
    static const char *IDS[9] = {"major-triad", "minor-triad", "diminished-triad", "augmented-triad", "dominant-seventh",
                                 "major-seventh", "minor-seventh", "half-diminished-seventh", "diminished-seventh"};
    for (int i = 0; i < 9; i++) {
        char id[64];
        snprintf(id, sizeof(id), "omt.chord-type.%s", IDS[i]);
        const ChordType *type = knowledge_snapshot_chord_type(snapshot, id);
        assert(type != NULL);
        unsigned mask = semitone_mask(type);
        assert(knowledge_snapshot_chord_type_of(snapshot, mask) == type);
        assert(knowledge_snapshot_chord_type_of(snapshot, mask | 1u) == type); /* the root bit is ignored */
    }
    assert(knowledge_snapshot_chord_type_of(snapshot, (1u << 4) | (1u << 7)) == major);
    assert(knowledge_snapshot_chord_type_of(snapshot, (1u << 4) | (1u << 7) | (1u << 10)) == dominant);
    assert(knowledge_snapshot_chord_type_of(snapshot, 0) == NULL);
    assert(knowledge_snapshot_chord_type_of(snapshot, 1u << 4) == NULL);
    assert(knowledge_snapshot_chord_type_of(snapshot, (1u << 2) | (1u << 7)) == NULL);
    assert(knowledge_snapshot_chord_type_of(snapshot, 1u << 12) == NULL);
    assert(knowledge_snapshot_chord_type_of(NULL, 0x90) == NULL);

    assert(knowledge_snapshot_chord_type(snapshot, "omt.chord-type.missing") == NULL);
    assert(knowledge_snapshot_chord_type(snapshot, "omt.material.major") == NULL);
    assert(knowledge_snapshot_chord_type(snapshot, NULL) == NULL && knowledge_snapshot_chord_type(NULL, "x") == NULL);
    knowledge_snapshot_destroy(snapshot);

    /* a user chord type with a ninth keeps its semitones; its mask reduces them to one octave */
    KnowledgeSnapshot *with_user = build_user(
        "{\"entries\":[{\"id\":\"user.chord-type.add9\",\"kind\":\"chord-type\",\"name\":\"add nine\","
        "\"content\":{\"construction\":\"added-note\",\"members\":[{\"role\":\"root\",\"semitones\":0},"
        "{\"role\":\"third\",\"semitones\":4},{\"role\":\"fifth\",\"semitones\":7},{\"role\":\"ninth\",\"semitones\":14}]},"
        "\"status\":\"active\",\"version\":1}]}");
    const ChordType *add9 = knowledge_snapshot_chord_type(with_user, "user.chord-type.add9");
    assert(add9 != NULL && add9->member_count == 4 && add9->members[3].role == CHORD_MEMBER_NINTH && add9->members[3].semitones == 14);
    assert(knowledge_snapshot_chord_type_of(with_user, (1u << 2) | (1u << 4) | (1u << 7)) == add9);
    knowledge_snapshot_destroy(with_user);
}

static void test_relations(void) {
    KnowledgeSnapshot *snapshot = build(NULL, 0);
    const Relation *supertonic = knowledge_snapshot_relation(snapshot, "omt.relation.supertonic");
    assert(supertonic != NULL && supertonic->family == RELATION_FAMILY_TONAL && strcmp(supertonic->name, "supertonic") == 0);
    assert(supertonic->tonal.by_degree && supertonic->tonal.degree == 2 && supertonic->tonal.material == RELATION_MATERIAL_DIATONIC);
    assert(supertonic->tonal.material_entry == NULL && supertonic->anchor_default == RELATION_ANCHOR_HOME);
    const Relation *subtonic = knowledge_snapshot_relation(snapshot, "omt.relation.subtonic");
    assert(subtonic->tonal.by_degree && subtonic->tonal.degree == 7);

    const Relation *parallel = knowledge_snapshot_relation(snapshot, "omt.relation.parallel");
    assert(parallel->family == RELATION_FAMILY_TONAL && !parallel->tonal.by_degree && parallel->tonal.root_semitones == 0);
    assert(parallel->tonal.material == RELATION_MATERIAL_PARALLEL);
    const Relation *mediant = knowledge_snapshot_relation(snapshot, "omt.relation.chromatic-mediant");
    assert(!mediant->tonal.by_degree && mediant->tonal.root_semitones == 4 && mediant->tonal.material == RELATION_MATERIAL_SAME);
    assert(knowledge_snapshot_relation(snapshot, "omt.relation.chromatic-submediant")->tonal.root_semitones == 8);

    const Relation *brighter = knowledge_snapshot_relation(snapshot, "omt.relation.one-step-brighter");
    assert(brighter->family == RELATION_FAMILY_MODAL && brighter->modal.same_center && brighter->modal.brightness_steps == 1);
    assert(knowledge_snapshot_relation(snapshot, "omt.relation.two-steps-darker")->modal.brightness_steps == -2);
    const Relation *neo = knowledge_snapshot_relation(snapshot, "omt.relation.neo-riemannian-l");
    assert(neo->family == RELATION_FAMILY_NEO_RIEMANNIAN && neo->neo_riemannian.move_count == 1 && neo->neo_riemannian.moves[0] == 'L');

    /* by id, name or alias, without regard to case */
    assert(knowledge_snapshot_relation_named(snapshot, "supertonic") == supertonic);
    assert(knowledge_snapshot_relation_named(snapshot, "Relative Major") == knowledge_snapshot_relation(snapshot, "omt.relation.mediant"));
    assert(knowledge_snapshot_relation_named(snapshot, "relative minor") ==
           knowledge_snapshot_relation(snapshot, "omt.relation.submediant"));
    assert(knowledge_snapshot_relation_named(snapshot, "parallel key") == parallel);
    assert(knowledge_snapshot_relation_named(snapshot, "omt.relation.parallel") == parallel);
    assert(knowledge_snapshot_relation_named(snapshot, "lowered submediant") ==
           knowledge_snapshot_relation(snapshot, "omt.relation.chromatic-submediant"));
    assert(knowledge_snapshot_relation_named(snapshot, "nothing") == NULL && knowledge_snapshot_relation_named(snapshot, "") == NULL);
    assert(knowledge_snapshot_relation_named(snapshot, NULL) == NULL && knowledge_snapshot_relation_named(NULL, "parallel") == NULL);
    assert(knowledge_snapshot_relation(snapshot, "omt.material.major") == NULL);
    assert(knowledge_snapshot_relation(snapshot, "omt.relation.missing") == NULL);
    assert(knowledge_snapshot_relation(snapshot, NULL) == NULL && knowledge_snapshot_relation(NULL, "x") == NULL);
    knowledge_snapshot_destroy(snapshot);
}

static void test_user_relations(void) {
    KnowledgeSnapshot *snapshot = build_user(
        "{\"entries\":["
        "{\"id\":\"user.material.test\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"test\","
        "\"content\":{\"steps\":[0,2,7]},\"status\":\"active\",\"version\":1},"
        "{\"id\":\"user.relation.to-entry\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"to entry\","
        "\"aliases\":[\"into test\"],\"content\":{\"root_semitones\":5,\"material\":\"user.material.test\","
        "\"anchor_default\":\"previous\"},\"status\":\"active\",\"version\":1},"
        "{\"id\":\"user.relation.to-dorian\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"to dorian\","
        "\"content\":{\"degree\":2,\"material\":\"omt.material.dorian\"},\"status\":\"active\",\"version\":1},"
        "{\"id\":\"user.relation.modal-seven\",\"kind\":\"relation\",\"subtype\":\"modal\",\"name\":\"modal seven\","
        "\"content\":{\"center\":7,\"brightness_steps\":-1},\"status\":\"active\",\"version\":1},"
        "{\"id\":\"user.relation.invert-five\",\"kind\":\"relation\",\"subtype\":\"set\",\"name\":\"invert five\","
        "\"content\":{\"operation\":\"I\",\"n\":5},\"status\":\"active\",\"version\":1},"
        "{\"id\":\"user.relation.transpose-two\",\"kind\":\"relation\",\"subtype\":\"set\",\"name\":\"transpose two\","
        "\"content\":{\"operation\":\"T\",\"n\":2},\"status\":\"active\",\"version\":1},"
        "{\"id\":\"user.relation.chain\",\"kind\":\"relation\",\"subtype\":\"neo-riemannian\",\"name\":\"chain\","
        "\"content\":{\"moves\":[\"P\",\"L\",\"R\"]},\"status\":\"active\",\"version\":1},"
        "{\"id\":\"user.relation.row\",\"kind\":\"relation\",\"subtype\":\"serial\",\"name\":\"row\","
        "\"content\":{\"form\":\"RI\",\"n\":3},\"status\":\"reference\",\"version\":1}]}");
    const Relation *to_entry = knowledge_snapshot_relation(snapshot, "user.relation.to-entry");
    assert(to_entry->tonal.material == RELATION_MATERIAL_ENTRY && !to_entry->tonal.by_degree && to_entry->tonal.root_semitones == 5);
    assert(to_entry->tonal.material_entry == knowledge_snapshot_material(snapshot, "user.material.test"));
    assert(to_entry->anchor_default == RELATION_ANCHOR_PREVIOUS);
    assert(knowledge_snapshot_relation_named(snapshot, "into test") == to_entry);
    const Relation *to_dorian = knowledge_snapshot_relation(snapshot, "user.relation.to-dorian");
    assert(to_dorian->tonal.by_degree && to_dorian->tonal.degree == 2);
    assert(to_dorian->tonal.material_entry == knowledge_snapshot_material(snapshot, "omt.material.dorian"));
    assert(to_dorian->anchor_default == RELATION_ANCHOR_HOME);
    const Relation *modal = knowledge_snapshot_relation(snapshot, "user.relation.modal-seven");
    assert(modal->family == RELATION_FAMILY_MODAL && !modal->modal.same_center && modal->modal.center == 7);
    assert(modal->modal.brightness_steps == -1);
    const Relation *invert = knowledge_snapshot_relation(snapshot, "user.relation.invert-five");
    assert(invert->family == RELATION_FAMILY_SET && invert->set.inversion && invert->set.n == 5);
    const Relation *transpose = knowledge_snapshot_relation(snapshot, "user.relation.transpose-two");
    assert(!transpose->set.inversion && transpose->set.n == 2);
    const Relation *chain = knowledge_snapshot_relation(snapshot, "user.relation.chain");
    assert(chain->neo_riemannian.move_count == 3 && memcmp(chain->neo_riemannian.moves, "PLR", 3) == 0);
    /* a serial entry is kept by its common fields only */
    const Relation *row = knowledge_snapshot_relation(snapshot, "user.relation.row");
    assert(row != NULL && row->family == RELATION_FAMILY_SERIAL && strcmp(row->name, "row") == 0);
    assert(knowledge_snapshot_entry(snapshot, "user.relation.row")->status == KNOWLEDGE_STATUS_REFERENCE);
    knowledge_snapshot_destroy(snapshot);
}

static void test_refused_details(void) {
    /* a brightness rank needs the diatonic family */
    expect_refused("{\"entries\":[{\"id\":\"user.material.bright\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"bright\",\"content\":{\"steps\":[0,2,4],\"brightness_rank\":3},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.bright", "content.brightness_rank", "diatonic");
    expect_refused("{\"entries\":[{\"id\":\"user.material.bright\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"bright\",\"content\":{\"steps\":[0,2,4],\"family\":\"minor\",\"brightness_rank\":3},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.bright", "content.brightness_rank", "diatonic");
    expect_refused("{\"entries\":[{\"id\":\"user.material.bright\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"bright\",\"content\":{\"steps\":[0,2,4],\"family\":\"diatonic\",\"brightness_rank\":\"high\"},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.bright", "content.brightness_rank", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.material.kw\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"kw\","
                   "\"content\":{\"steps\":[0,2],\"key_word\":\"Minor Key\"},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.kw", "content.key_word", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.material.fam\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"fam\","
                   "\"content\":{\"steps\":[0,2],\"family\":3},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.fam", "content.family", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.material.notes\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"notes\","
                   "\"content\":{\"steps\":[0,2],\"color_notes\":[\"b3\",\"third\"]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.notes", "content.color_notes", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.material.alias\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"alias\",\"aliases\":[\"ok\",\"\"],\"content\":{\"steps\":[0,2]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.alias", "aliases", NULL);
    /* a link names a material that is there */
    expect_refused("{\"entries\":[{\"id\":\"user.material.link\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"link\","
                   "\"content\":{\"steps\":[0,2]},\"relations\":[{\"type\":\"rotation-of\",\"target\":\"user.material.gone\"}],"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.link", "relations", "user.material.gone");
    expect_refused("{\"entries\":[{\"id\":\"user.material.link\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"link\","
                   "\"content\":{\"steps\":[0,2]},\"relations\":[{\"type\":\"variant-of\",\"target\":\"omt.chord-type.major-triad\"}],"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.link", "relations", "not a pitch material");
}

static void test_refused_relations(void) {
    /* both degree and root_semitones */
    expect_refused("{\"entries\":[{\"id\":\"user.relation.both\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"both\","
                   "\"content\":{\"degree\":5,\"root_semitones\":7,\"material\":\"same\"},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.both", "content", "root_semitones");
    /* neither */
    expect_refused("{\"entries\":[{\"id\":\"user.relation.neither\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"neither\","
                   "\"content\":{\"material\":\"same\"},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.neither", "content", "exactly one");
    /* an unknown material, and an entry that is not a material */
    expect_refused("{\"entries\":[{\"id\":\"user.relation.unknown\",\"kind\":\"relation\",\"subtype\":\"tonal\","
                   "\"name\":\"unknown\",\"content\":{\"root_semitones\":7,\"material\":\"user.material.nowhere\"},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.unknown", "content.material", "user.material.nowhere");
    expect_refused("{\"entries\":[{\"id\":\"user.relation.chord\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"chord\","
                   "\"content\":{\"root_semitones\":7,\"material\":\"omt.chord-type.major-triad\"},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.chord", "content.material", "not a pitch material");
    /* "diatonic" needs a degree; a degree and a root are in range */
    expect_refused("{\"entries\":[{\"id\":\"user.relation.dia\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"dia\","
                   "\"content\":{\"root_semitones\":7,\"material\":\"diatonic\"},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.dia", "content.material", "degree");
    expect_refused("{\"entries\":[{\"id\":\"user.relation.deg\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"deg\","
                   "\"content\":{\"degree\":8,\"material\":\"same\"},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.deg", "content.degree", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.relation.root\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"root\","
                   "\"content\":{\"root_semitones\":12,\"material\":\"same\"},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.root", "content.root_semitones", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.relation.anchor\",\"kind\":\"relation\",\"subtype\":\"tonal\",\"name\":\"anchor\","
                   "\"content\":{\"root_semitones\":7,\"material\":\"same\",\"anchor_default\":\"next\"},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.anchor", "content.anchor_default", NULL);
    /* the other families and the subtype */
    expect_refused("{\"entries\":[{\"id\":\"user.relation.modal\",\"kind\":\"relation\",\"subtype\":\"modal\",\"name\":\"modal\","
                   "\"content\":{\"center\":\"same\"},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.modal", "content.brightness_steps", "missing");
    expect_refused("{\"entries\":[{\"id\":\"user.relation.neo\",\"kind\":\"relation\",\"subtype\":\"neo-riemannian\",\"name\":\"neo\","
                   "\"content\":{\"moves\":[\"P\",\"X\"]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.neo", "content.moves", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.relation.set\",\"kind\":\"relation\",\"subtype\":\"set\",\"name\":\"set\","
                   "\"content\":{\"operation\":\"T\",\"n\":12},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.set", "content.n", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.relation.odd\",\"kind\":\"relation\",\"subtype\":\"melodic\",\"name\":\"odd\","
                   "\"content\":{},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.odd", "subtype", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.relation.none\",\"kind\":\"relation\",\"name\":\"none\","
                   "\"content\":{},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.relation.none", "subtype", "missing");
}

static void test_refused_chord_types(void) {
    expect_refused("{\"entries\":[{\"id\":\"user.chord-type.none\",\"kind\":\"chord-type\",\"name\":\"none\",\"content\":{},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.chord-type.none", "content.members", "missing");
    expect_refused("{\"entries\":[{\"id\":\"user.chord-type.first\",\"kind\":\"chord-type\",\"name\":\"first\","
                   "\"content\":{\"members\":[{\"role\":\"third\",\"semitones\":4}]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.chord-type.first", "content.members", "root");
    expect_refused("{\"entries\":[{\"id\":\"user.chord-type.role\",\"kind\":\"chord-type\",\"name\":\"role\","
                   "\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0},{\"role\":\"second\",\"semitones\":2}]},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.chord-type.role", "content.members", "role");
    expect_refused("{\"entries\":[{\"id\":\"user.chord-type.far\",\"kind\":\"chord-type\",\"name\":\"far\","
                   "\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0},{\"role\":\"ninth\",\"semitones\":24}]},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.chord-type.far", "content.members", "semitones");
    expect_refused("{\"entries\":[{\"id\":\"user.chord-type.many\",\"kind\":\"chord-type\",\"name\":\"many\","
                   "\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0},{\"role\":\"third\",\"semitones\":3},"
                   "{\"role\":\"fifth\",\"semitones\":6},{\"role\":\"seventh\",\"semitones\":9},{\"role\":\"ninth\",\"semitones\":14},"
                   "{\"role\":\"eleventh\",\"semitones\":17},{\"role\":\"thirteenth\",\"semitones\":21},"
                   "{\"role\":\"added-sixth\",\"semitones\":8},{\"role\":\"added-second\",\"semitones\":2}]},"
                   "\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.chord-type.many", "content.members", "1 to 8");
}

static void test_hash(void) {
    KnowledgeSnapshot *first = build(NULL, 0);
    KnowledgeSnapshot *second = build(NULL, 0);
    assert(lower_hex16(knowledge_snapshot_hash(first)));
    assert(strcmp(knowledge_snapshot_hash(first), knowledge_snapshot_hash(second)) == 0);
    /* two snapshots in one process are independent: their entries are not shared */
    assert(knowledge_snapshot_entry(first, "omt.material.major") != knowledge_snapshot_entry(second, "omt.material.major"));
    knowledge_snapshot_destroy(first);
    assert(knowledge_snapshot_entry_count(second) == 89);
    knowledge_snapshot_destroy(second);
}

static void test_user_file(void) {
    KnowledgeSnapshot *plain = build(NULL, 0);
    KnowledgeUserFile file = {"mine.json", USER_FILE};
    KnowledgeSnapshot *with_user = build(&file, 1);
    assert(knowledge_snapshot_entry_count(with_user) == 91);
    assert(strcmp(knowledge_snapshot_hash(plain), knowledge_snapshot_hash(with_user)) != 0);
    const PitchMaterial *test = knowledge_snapshot_material(with_user, "user.material.test");
    assert(test != NULL && test->step_count == 3 && test->steps[2] == 7 && test->descending_step_count == 0);
    const KnowledgeEntry *chord = knowledge_snapshot_entry(with_user, "user.chord-type.test");
    assert(chord != NULL && chord->subtype == NULL && chord->status == KNOWLEDGE_STATUS_REFERENCE && chord->version == 2);
    assert(knowledge_snapshot_material(with_user, "user.chord-type.test") == NULL);
    /* the repository entries are unchanged by it */
    assert(strcmp(knowledge_snapshot_entry(plain, "omt.material.major")->hash,
                  knowledge_snapshot_entry(with_user, "omt.material.major")->hash) == 0);
    knowledge_snapshot_destroy(with_user);
    knowledge_snapshot_destroy(plain);
}

static void test_refused(void) {
    expect_refused("{\"entries\":[{\"id\":\"omt.material.major\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"again\",\"content\":{\"steps\":[0,2]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_DUPLICATE, "omt.material.major", "id", NULL);
    expect_refused("{\"entries\":[{\"id\":\"omt.material.other\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"other\",\"content\":{\"steps\":[0,2]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_NAMESPACE, "omt.material.other", "id", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.material.nosteps\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"no steps\",\"content\":{},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.nosteps", "content.steps", "missing");
    expect_refused("{\"entries\":[{\"id\":\"user.material.late\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"late\",\"content\":{\"steps\":[1,2]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.late", "content.steps", "ascending from 0");
    expect_refused("{\"entries\":[{\"id\":\"user.material.bad\",\"kind\":\"material\",\"subtype\":\"unordered\","
                   "\"name\":\"bad\",\"content\":{\"steps\":[0,12]},\"status\":\"active\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.material.bad", "content.steps", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.term.x\",\"kind\":\"term\",\"name\":\"x\",\"status\":\"soon\",\"version\":1}]}",
                   KNOWLEDGE_ERR_FIELD, "user.term.x", "status", NULL);
    expect_refused("{\"entries\":[{\"id\":\"user.term.x\",\"kind\":\"term\",\"name\":\"x\",\"status\":\"active\",\"version\":0}]}",
                   KNOWLEDGE_ERR_FIELD, "user.term.x", "version", NULL);
    expect_refused("{\"entries\":[{\"kind\":\"term\"}]}", KNOWLEDGE_ERR_FIELD, "id", "missing", NULL);
    expect_refused("{\"entries\":3}", KNOWLEDGE_ERR_FIELD, "entries", NULL, NULL);
    expect_refused("this is not JSON", KNOWLEDGE_ERR_PARSE, "not valid JSON", NULL, NULL);

    /* a user id reused by a second user file is a duplicate too */
    KnowledgeUserFile files[2] = {{"one.json", USER_FILE}, {"two.json", USER_FILE}};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(files, 2, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_DUPLICATE);
    assert(snapshot == NULL && strstr(message, "two.json") != NULL && strstr(message, "user.material.test") != NULL);
    /* the message is optional */
    assert(knowledge_snapshot_build(files, 2, &snapshot, NULL, 0) == KNOWLEDGE_ERR_DUPLICATE);
}

static void test_null_arguments(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[64];
    KnowledgeUserFile file = {"x.json", USER_FILE};
    KnowledgeUserFile no_text = {"x.json", NULL};
    assert(knowledge_snapshot_build(NULL, 0, NULL, message, sizeof(message)) == KNOWLEDGE_ERR_NULL_ARGUMENT);
    assert(knowledge_snapshot_build(NULL, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_NULL_ARGUMENT);
    assert(knowledge_snapshot_build(&no_text, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_ERR_NULL_ARGUMENT);
    assert(snapshot == NULL);
    assert(knowledge_snapshot_build(&file, 0, &snapshot, NULL, 0) == KNOWLEDGE_OK);
    knowledge_snapshot_destroy(snapshot);
    knowledge_snapshot_destroy(NULL);
    assert(knowledge_snapshot_hash(NULL) == NULL && knowledge_snapshot_entry_count(NULL) == 0);
    assert(knowledge_snapshot_entry_at(NULL, 0) == NULL && knowledge_snapshot_entry(NULL, "x") == NULL);
    assert(knowledge_snapshot_material(NULL, "x") == NULL);
    assert(knowledge_snapshot_chord_type(NULL, "x") == NULL && knowledge_snapshot_relation(NULL, "x") == NULL);
}

int main(void) {
    test_embedded_modes();
    test_embedded_counts();
    test_material_queries();
    test_material_details();
    test_material_named();
    test_chord_types();
    test_relations();
    test_user_relations();
    test_hash();
    test_user_file();
    test_refused();
    test_refused_details();
    test_refused_relations();
    test_refused_chord_types();
    test_null_arguments();
    puts("knowledge snapshot: ok");
    return 0;
}
