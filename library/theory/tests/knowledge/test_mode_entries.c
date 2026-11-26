#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The modal knowledge of P69 B1 (R-725 to R-727, R-731): the five church modes' chords, characteristic degrees and cadence, the
 * cadence kinds that move through them, the construction of every color chord type and the four shell voicings with their fits. A
 * user file that names a chord numeral with an alteration, a voicing whose members do not rise or one that fits no chord type is
 * refused with the file, the id and the field.
 */

static KnowledgeSnapshot *build(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

typedef struct {
    const char *id;
    const char *cadence;
    size_t primary_count;
    int primary[3];
    size_t avoided_count;
    int avoided[1];
    size_t characteristic_count;
    int characteristic[2];
    int approach;
    int arrival;
} Mode;

static const Mode MODES[] = {
    {"omt.material.lydian", "persichetti.vocabulary.lydian-cadence", 3, {1, 2, 7}, 1, {4}, 1, {4, 0}, 2, 1},
    {"omt.material.mixolydian", "persichetti.vocabulary.mixolydian-cadence", 3, {1, 5, 7}, 1, {3}, 1, {7, 0}, 7, 1},
    {"omt.material.dorian", "persichetti.vocabulary.dorian-cadence", 3, {1, 2, 4}, 1, {6}, 1, {6, 0}, 4, 1},
    {"omt.material.phrygian", "persichetti.vocabulary.phrygian-cadence", 3, {1, 2, 7}, 1, {5}, 1, {2, 0}, 2, 1},
    {"omt.material.locrian", "persichetti.vocabulary.locrian-cadence", 3, {1, 2, 5}, 0, {0}, 2, {2, 5}, 5, 1},
};

static void test_modes(void) {
    KnowledgeSnapshot *snapshot = build();
    for (size_t m = 0; m < sizeof(MODES) / sizeof(MODES[0]); m++) {
        const Mode *mode = &MODES[m];
        const PitchMaterial *material = knowledge_snapshot_material(snapshot, mode->id);
        assert(material != NULL && knowledge_snapshot_entry(snapshot, mode->id)->version == 2);
        assert(material->primary_count == mode->primary_count && material->avoided_count == mode->avoided_count);
        for (size_t i = 0; i < mode->primary_count; i++) {
            assert(material->primary_degrees[i] == mode->primary[i]);
        }
        for (size_t i = 0; i < mode->avoided_count; i++) {
            assert(material->avoided_degrees[i] == mode->avoided[i]);
        }
        assert(material->characteristic_count == mode->characteristic_count);
        for (size_t i = 0; i < mode->characteristic_count; i++) {
            assert(material->characteristic_degrees[i] == mode->characteristic[i]);
        }
        assert(material->cadence != NULL && strcmp(material->cadence, mode->cadence) == 0);
        /* the cadence kind the mode names moves through the mode: its approach, then the arrival */
        const VocabularyEntry *kind = knowledge_snapshot_vocabulary(snapshot, material->cadence);
        assert(kind != NULL && kind->subtype == VOCABULARY_CADENCE_KIND);
        assert(kind->cadence.material == material && kind->cadence.approach_count == 1);
        assert(kind->cadence.approach_degrees[0] == mode->approach && kind->cadence.arrival_degree == mode->arrival);
        assert(strcmp(kind->cadence.core, "perfect-authentic") == 0 && kind->cadence.strength == 4 && kind->cadence.conclusive);
    }
    /* the four forms the Composer already wrote carry no modal field, and the older kinds no mode */
    const char *plain[] = {"omt.material.major", "omt.material.natural-minor", "omt.material.harmonic-minor", "omt.material.melodic-minor"};
    for (size_t i = 0; i < 4; i++) {
        const PitchMaterial *material = knowledge_snapshot_material(snapshot, plain[i]);
        assert(material != NULL && material->primary_count == 0 && material->avoided_count == 0);
        assert(material->characteristic_count == 0 && material->cadence == NULL);
    }
    const VocabularyEntry *authentic = knowledge_snapshot_vocabulary(snapshot, "omt.vocabulary.perfect-authentic");
    assert(authentic != NULL && authentic->cadence.material == NULL && authentic->cadence.approach_count == 0);
    knowledge_snapshot_destroy(snapshot);
}

static void test_synthetic_scales(void) {
    KnowledgeSnapshot *snapshot = build();
    const PitchMaterial *overtone = knowledge_snapshot_material(snapshot, "persichetti.material.overtone");
    assert(overtone != NULL && overtone->step_count == 7 && overtone->steps[3] == 6 && overtone->steps[6] == 10);
    assert(strcmp(overtone->family, "synthetic") == 0 && !overtone->has_brightness);
    assert(knowledge_snapshot_material_named(snapshot, "lydian dominant") == overtone);
    const PitchMaterial *enigmatic = knowledge_snapshot_material(snapshot, "persichetti.material.enigmatic");
    assert(enigmatic != NULL && enigmatic->steps[1] == 1 && enigmatic->steps[6] == 11);
    assert(knowledge_snapshot_entry(snapshot, "persichetti.material.hungarian-major")->status == KNOWLEDGE_STATUS_REFERENCE);
    knowledge_snapshot_destroy(snapshot);
}

static void test_chord_types(void) {
    static const struct {
        const char *id;
        ChordConstruction construction;
        size_t members;
    } TYPES[] = {{"persichetti.chord-type.add9", CHORD_CONSTRUCTION_ADDED_NOTE, 4},
                 {"persichetti.chord-type.minor-add9", CHORD_CONSTRUCTION_ADDED_NOTE, 4},
                 {"persichetti.chord-type.add6", CHORD_CONSTRUCTION_ADDED_NOTE, 4},
                 {"persichetti.chord-type.minor-add6", CHORD_CONSTRUCTION_ADDED_NOTE, 4},
                 {"persichetti.chord-type.six-nine", CHORD_CONSTRUCTION_ADDED_NOTE, 5},
                 {"persichetti.chord-type.dominant-ninth", CHORD_CONSTRUCTION_TERTIAN, 5},
                 {"persichetti.chord-type.major-ninth", CHORD_CONSTRUCTION_TERTIAN, 5},
                 {"persichetti.chord-type.minor-ninth", CHORD_CONSTRUCTION_TERTIAN, 5},
                 {"persichetti.chord-type.quartal-three", CHORD_CONSTRUCTION_QUARTAL, 3},
                 {"persichetti.chord-type.quartal-four", CHORD_CONSTRUCTION_QUARTAL, 4},
                 {"omt.chord-type.major-triad", CHORD_CONSTRUCTION_TERTIAN, 3}};
    KnowledgeSnapshot *snapshot = build();
    assert(knowledge_snapshot_chord_type_count(snapshot) == 21);
    for (size_t i = 0; i < sizeof(TYPES) / sizeof(TYPES[0]); i++) {
        const ChordType *type = knowledge_snapshot_chord_type(snapshot, TYPES[i].id);
        assert(type != NULL && type->construction == TYPES[i].construction && type->member_count == TYPES[i].members);
    }
    const ChordType *four = knowledge_snapshot_chord_type(snapshot, "persichetti.chord-type.quartal-four");
    assert(four->members[1].role == CHORD_MEMBER_SUSPENDED_FOURTH && four->members[1].semitones == 5);
    assert(four->members[3].role == CHORD_MEMBER_THIRD && four->members[3].semitones == 15);
    const ChordType *six_nine = knowledge_snapshot_chord_type(snapshot, "persichetti.chord-type.six-nine");
    assert(six_nine->members[3].role == CHORD_MEMBER_ADDED_SIXTH && six_nine->members[4].semitones == 14);
    /* in id order, the accessors list every one of them */
    assert(strcmp(knowledge_snapshot_chord_type_at(snapshot, 0)->id, "omt.chord-type.augmented-triad") == 0);
    assert(knowledge_snapshot_chord_type_at(snapshot, 21) == NULL);
    knowledge_snapshot_destroy(snapshot);
}

static void test_voicings(void) {
    static const struct {
        const char *id;
        size_t members;
        int semitones[4];
        const char *fit;
    } SHELLS[] = {{"omt.voicing.shell-dominant", 3, {0, 4, 10, 0}, "omt.chord-type.dominant-seventh"},
                  {"omt.voicing.shell-major", 3, {0, 4, 11, 0}, "omt.chord-type.major-seventh"},
                  {"omt.voicing.shell-minor", 3, {0, 3, 10, 0}, "omt.chord-type.minor-seventh"},
                  {"omt.voicing.shell-dominant-ninth", 4, {0, 4, 10, 14}, "omt.chord-type.dominant-seventh"}};
    KnowledgeSnapshot *snapshot = build();
    assert(knowledge_snapshot_voicing_count(snapshot) == 4);
    for (size_t i = 0; i < 4; i++) {
        const VoicingEntry *voicing = knowledge_snapshot_voicing(snapshot, SHELLS[i].id);
        assert(voicing != NULL && voicing->member_count == SHELLS[i].members && voicing->fit_count == 1);
        assert(voicing->members[0].role == CHORD_MEMBER_ROOT);
        for (size_t m = 0; m < voicing->member_count; m++) {
            assert(voicing->members[m].semitones == SHELLS[i].semitones[m]);
        }
        assert(voicing->fits[0] == knowledge_snapshot_chord_type(snapshot, SHELLS[i].fit));
    }
    assert(knowledge_snapshot_voicing(snapshot, "omt.voicing.shell-dominant")->members[2].role == CHORD_MEMBER_SEVENTH);
    assert(knowledge_snapshot_voicing(snapshot, "omt.voicing.shell-dominant-ninth")->members[3].role == CHORD_MEMBER_NINTH);
    assert(strcmp(knowledge_snapshot_voicing_at(snapshot, 0)->id, "omt.voicing.shell-dominant") == 0);
    assert(knowledge_snapshot_voicing_at(snapshot, 4) == NULL && knowledge_snapshot_voicing(snapshot, "omt.chord-type.major-triad") == NULL);
    knowledge_snapshot_destroy(snapshot);
}

/* A user file is refused: the status is not OK and the message names the file, the id and the field. */
static void refused(const char *text, const char *id, const char *field) {
    KnowledgeUserFile file = {"mine.json", text};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&file, 1, &snapshot, message, sizeof(message)) != KNOWLEDGE_OK);
    assert(snapshot == NULL);
    assert(strstr(message, "mine.json") != NULL && strstr(message, id) != NULL && strstr(message, field) != NULL);
}

static void test_user_files(void) {
    refused("{\"entries\":[{\"id\":\"user.material.bad\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"bad\","
            "\"content\":{\"steps\":[0,2,4,5,7,9,11],\"primary_chords\":[\"I\",\"bII\"]},\"status\":\"reference\",\"version\":1}]}",
            "user.material.bad", "content.primary_chords");
    refused("{\"entries\":[{\"id\":\"user.material.high\",\"kind\":\"material\",\"subtype\":\"unordered\",\"name\":\"high\","
            "\"content\":{\"steps\":[0,2,4],\"chords_to_avoid\":[\"vi\"]},\"status\":\"reference\",\"version\":1}]}",
            "user.material.high", "content.chords_to_avoid");
    refused("{\"entries\":[{\"id\":\"user.voicing.down\",\"kind\":\"voicing\",\"name\":\"down\","
            "\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0},{\"role\":\"third\",\"semitones\":10},"
            "{\"role\":\"seventh\",\"semitones\":4}],\"fits\":[\"omt.chord-type.dominant-seventh\"]},\"status\":\"reference\",\"version\":1}]}",
            "user.voicing.down", "content.members");
    refused("{\"entries\":[{\"id\":\"user.voicing.lost\",\"kind\":\"voicing\",\"name\":\"lost\","
            "\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0},{\"role\":\"third\",\"semitones\":4}],"
            "\"fits\":[\"user.chord-type.none\"]},\"status\":\"reference\",\"version\":1}]}",
            "user.voicing.lost", "content.fits");
    /* a user voicing that fits a repository chord type loads, and keeps its members */
    KnowledgeUserFile good = {"mine.json",
        "{\"entries\":[{\"id\":\"user.voicing.open\",\"kind\":\"voicing\",\"name\":\"open\","
        "\"content\":{\"members\":[{\"role\":\"root\",\"semitones\":0},{\"role\":\"seventh\",\"semitones\":10},"
        "{\"role\":\"third\",\"semitones\":16}],\"fits\":[\"omt.chord-type.dominant-seventh\"]},\"status\":\"reference\",\"version\":1}]}"};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&good, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    const VoicingEntry *open = knowledge_snapshot_voicing(snapshot, "user.voicing.open");
    assert(open != NULL && open->member_count == 3 && open->members[2].semitones == 16);
    assert(knowledge_snapshot_voicing_count(snapshot) == 5);
    knowledge_snapshot_destroy(snapshot);
}

int main(void) {
    test_modes();
    test_synthetic_scales();
    test_chord_types();
    test_voicings();
    test_user_files();
    puts("test_mode_entries: ok");
    return 0;
}
