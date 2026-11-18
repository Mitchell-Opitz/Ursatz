#include <assert.h>
#include <cJSON.h>
#include <stdio.h>
#include <string.h>

#include "model/pitch/region.h"
#include "test_knowledge.h"
#include "theory/harmony/harmony_related_key.h"
#include "theory/knowledge/knowledge_embedded.h"
#include "theory/tonality/brightness.h"
#include "theory/tonality/region_relation.h"

/*
 * Regions named by a relation (R-594): every embedded relation entry's examples resolve to their targets (read from the embedded
 * entries, as tools/ci/check-knowledge.py does), the specific cases of the plan, brightness over the seven modes, the agreement of
 * the degree relations with harmony_related_key_of, the set relations (built by hand: no entry exists yet), and the refusals with
 * their messages.
 */

static const char *const NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};

static const PitchMaterial *material(const char *word) {
    const PitchMaterial *found = knowledge_snapshot_material_named(test_knowledge(), word);
    assert(found != NULL);
    return found;
}

/* "C major", "Eb dorian", "F# natural minor" as a region; false when the tonic or the word is unknown. */
static bool parse_region(const char *text, Region *out) {
    static const int LETTERS[7] = {9, 11, 0, 2, 4, 5, 7}; /* A to G */
    if (text[0] < 'A' || text[0] > 'G') {
        return false;
    }
    int center = LETTERS[text[0] - 'A'];
    const char *rest = text + 1;
    if (*rest == '#') {
        center++, rest++;
    } else if (*rest == 'b') {
        center--, rest++;
    }
    if (*rest != ' ') {
        return false;
    }
    const PitchMaterial *found = knowledge_snapshot_material_named(test_knowledge(), rest + 1);
    if (found == NULL) {
        return false;
    }
    *out = region_tonal(center, found);
    return true;
}

static Region key(const char *text) {
    Region region;
    assert(parse_region(text, &region));
    return region;
}

static const Relation *relation(const char *id) {
    const Relation *found = knowledge_snapshot_relation(test_knowledge(), id);
    assert(found != NULL);
    return found;
}

/* The relation resolved from the anchor, as a "C major" style text; "refused" or "error" when it is not resolved. */
static const char *show(const char *relation_id, const char *anchor_text, char *buffer, size_t size) {
    Region anchor = key(anchor_text);
    Region out;
    char message[256];
    RegionRelationStatus status = region_resolve(test_knowledge(), relation(relation_id), &anchor, &out, message, sizeof(message));
    if (status == REGION_RELATION_REFUSED) {
        return "refused";
    }
    assert(status == REGION_RELATION_OK && out.kind == REGION_TONAL_MODAL && out.strength == REGION_STRENGTH_ESTABLISHED);
    snprintf(buffer, size, "%s %s", NAMES[out.center], out.material->key_word);
    return buffer;
}

#define RESOLVES(id, from, to)                                                                                                    \
    do {                                                                                                                          \
        char buffer[64];                                                                                                          \
        assert(strcmp(show("omt.relation." id, from, buffer, sizeof(buffer)), to) == 0);                                         \
    } while (0)

/* Every example of every embedded relation entry resolves to its target. */
static void test_embedded_examples(void) {
    size_t relations = 0;
    size_t examples = 0;
    for (size_t f = 0; f < knowledge_embedded_file_count; f++) {
        if (strncmp(knowledge_embedded_files[f].path, "relation/", 9) != 0) {
            continue;
        }
        cJSON *file = cJSON_Parse(knowledge_embedded_files[f].text);
        assert(file != NULL);
        const cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, cJSON_GetObjectItemCaseSensitive(file, "entries")) {
            const char *id = cJSON_GetObjectItemCaseSensitive(entry, "id")->valuestring;
            const char *subtype = cJSON_GetObjectItemCaseSensitive(entry, "subtype")->valuestring;
            relations++;
            const cJSON *list = cJSON_GetObjectItemCaseSensitive(entry, "examples");
            const cJSON *example = NULL;
            cJSON_ArrayForEach(example, list) {
                const char *anchor_text = cJSON_GetObjectItemCaseSensitive(example, "anchor")->valuestring;
                const char *target_text = cJSON_GetObjectItemCaseSensitive(example, "target")->valuestring;
                Region anchor, target, out;
                char message[256];
                assert(parse_region(anchor_text, &anchor) && parse_region(target_text, &target));
                RegionRelationStatus status = region_resolve(test_knowledge(), relation(id), &anchor, &out, message, sizeof(message));
                if (status != REGION_RELATION_OK || !region_equals(&out, &target)) {
                    fprintf(stderr, "%s (%s): %s to %s: %s\n", id, subtype, anchor_text, target_text, message);
                    assert(false);
                }
                examples++;
            }
        }
        cJSON_Delete(file);
    }
    assert(relations == 18);
    assert(examples == 36);
}

static void test_named_relations(void) {
    Region anchor = key("C major");
    Region out;
    char message[256];
    assert(region_resolve_named(test_knowledge(), "relative minor", &anchor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(region_equals(&out, &(Region){REGION_TONAL_MODAL, 9, material("minor"), REGION_STRENGTH_ESTABLISHED}));
    assert(region_resolve_named(test_knowledge(), "Dominant", &anchor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(out.center == 7 && strcmp(out.material->id, "omt.material.major") == 0);
    assert(region_resolve_named(test_knowledge(), "parallel key", &anchor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(out.center == 0 && strcmp(out.material->id, "omt.material.natural-minor") == 0);
    Region before = out;
    assert(region_resolve_named(test_knowledge(), "no such relation", &anchor, &out, message, sizeof(message)) ==
           REGION_RELATION_ERR_UNKNOWN_RELATION);
    assert(strstr(message, "no such relation") != NULL && region_equals(&out, &before));
    assert(region_resolve_named(test_knowledge(), NULL, &anchor, &out, message, sizeof(message)) == REGION_RELATION_ERR_ARGUMENT);
}

static void test_specific_cases(void) {
    char buffer[64];
    /* brightness steps: locrian 1, phrygian 2, aeolian 3, dorian 4, mixolydian 5, major 6, lydian 7 */
    RESOLVES("two-steps-brighter", "C locrian", "C minor");
    RESOLVES("two-steps-brighter", "C aeolian", "C mixolydian");
    RESOLVES("two-steps-brighter", "C dorian", "C major");
    RESOLVES("two-steps-brighter", "C phrygian", "C dorian");
    RESOLVES("two-steps-brighter", "C mixolydian", "C lydian");
    assert(strcmp(show("omt.relation.two-steps-brighter", "C major", buffer, sizeof(buffer)), "refused") == 0);
    assert(strcmp(show("omt.relation.two-steps-brighter", "C lydian", buffer, sizeof(buffer)), "refused") == 0);
    RESOLVES("one-step-brighter", "C locrian", "C phrygian");
    RESOLVES("one-step-brighter", "C major", "C lydian");
    assert(strcmp(show("omt.relation.one-step-brighter", "C lydian", buffer, sizeof(buffer)), "refused") == 0);
    RESOLVES("one-step-darker", "C major", "C mixolydian");
    RESOLVES("one-step-darker", "D dorian", "D minor");
    assert(strcmp(show("omt.relation.one-step-darker", "C locrian", buffer, sizeof(buffer)), "refused") == 0);
    RESOLVES("two-steps-darker", "C lydian", "C mixolydian");
    RESOLVES("two-steps-darker", "C major", "C dorian");
    assert(strcmp(show("omt.relation.two-steps-darker", "C phrygian", buffer, sizeof(buffer)), "refused") == 0);

    /* chromatic mediants keep the material */
    RESOLVES("chromatic-mediant", "C major", "E major");
    RESOLVES("chromatic-mediant-lowered", "C major", "Eb major");
    RESOLVES("chromatic-submediant", "C major", "Ab major");
    RESOLVES("chromatic-submediant", "A minor", "F minor");
    RESOLVES("chromatic-submediant-raised", "C major", "A major");
    RESOLVES("chromatic-submediant-raised", "C minor", "A minor");

    /* degrees: the closely related keys */
    RESOLVES("submediant", "C major", "A minor");
    RESOLVES("mediant", "A minor", "C major");
    RESOLVES("mediant", "C major", "E minor");
    RESOLVES("dominant", "C minor", "G minor");
    RESOLVES("subdominant", "C major", "F major");
    RESOLVES("subtonic", "A minor", "G major");
    assert(strcmp(show("omt.relation.supertonic", "A minor", buffer, sizeof(buffer)), "refused") == 0);
    assert(strcmp(show("omt.relation.subtonic", "C major", buffer, sizeof(buffer)), "refused") == 0);
    assert(strcmp(show("omt.relation.supertonic", "C major", buffer, sizeof(buffer)), "D minor") == 0);

    /* parallel: major and the minor forms swap; other modes have none */
    RESOLVES("parallel", "C major", "C minor");
    RESOLVES("parallel", "A minor", "A major");
    RESOLVES("parallel", "C harmonic minor", "C major");
    RESOLVES("parallel", "Eb melodic minor", "Eb major");
    assert(strcmp(show("omt.relation.parallel", "C dorian", buffer, sizeof(buffer)), "refused") == 0);

    /* neo-Riemannian: P, L and R from C major */
    RESOLVES("neo-riemannian-p", "C major", "C minor");
    RESOLVES("neo-riemannian-l", "C major", "E minor");
    RESOLVES("neo-riemannian-r", "C major", "A minor");
    RESOLVES("neo-riemannian-p", "C minor", "C major");
    RESOLVES("neo-riemannian-l", "C minor", "Ab major");
    RESOLVES("neo-riemannian-r", "C minor", "Eb major");
    assert(strcmp(show("omt.relation.neo-riemannian-p", "C lydian", buffer, sizeof(buffer)), "refused") == 0);
    assert(strcmp(show("omt.relation.neo-riemannian-r", "C harmonic minor", buffer, sizeof(buffer)), "refused") == 0);
}

/* A neo-Riemannian chain and the set relations are built by hand: the entries carry single moves and no set relations yet. */
static void test_hand_built_relations(void) {
    Region anchor = key("C major");
    Region out;
    char message[256];

    Relation chain;
    memset(&chain, 0, sizeof(chain));
    chain.id = "test.relation.pl";
    chain.name = "PL";
    chain.family = RELATION_FAMILY_NEO_RIEMANNIAN;
    chain.neo_riemannian.move_count = 2;
    chain.neo_riemannian.moves[0] = 'P';
    chain.neo_riemannian.moves[1] = 'L';
    assert(region_resolve(test_knowledge(), &chain, &anchor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(region_equals(&out, &(Region){REGION_TONAL_MODAL, 8, material("major"), REGION_STRENGTH_ESTABLISHED}));
    chain.neo_riemannian.moves[1] = 'X';
    assert(region_resolve(test_knowledge(), &chain, &anchor, &out, message, sizeof(message)) == REGION_RELATION_REFUSED);
    assert(strstr(message, "'X'") != NULL && strstr(message, "PL") != NULL);

    Relation set;
    memset(&set, 0, sizeof(set));
    set.id = "test.relation.t7";
    set.name = "T7";
    set.family = RELATION_FAMILY_SET;
    set.set.n = 7;
    assert(region_resolve(test_knowledge(), &set, &anchor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(out.center == 7 && strcmp(out.material->id, "omt.material.major") == 0);
    set.set.n = -5;
    Region again;
    assert(region_resolve(test_knowledge(), &set, &anchor, &again, message, sizeof(message)) == REGION_RELATION_OK);
    assert(again.center == 7 && strcmp(again.material->id, "omt.material.major") == 0);
    set.set.n = 0;
    Region minor_anchor = key("D minor");
    assert(region_resolve(test_knowledge(), &set, &minor_anchor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(out.center == 2 && strcmp(out.material->id, "omt.material.natural-minor") == 0);

    /* In: every pitch class p becomes n - p, the center too, and the collection is matched to a material by its mask */
    set.id = "test.relation.i0";
    set.name = "I0";
    set.set.inversion = true;
    set.set.n = 0;
    assert(region_resolve(test_knowledge(), &set, &anchor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(out.center == 0 && strcmp(out.material->id, "omt.material.phrygian") == 0);
    set.set.n = 7;
    assert(region_resolve(test_knowledge(), &set, &anchor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(out.center == 7 && strcmp(out.material->id, "omt.material.phrygian") == 0);
    assert(pitch_material_mask(out.material, out.center) == (1u << 7 | 1u << 8 | 1u << 10 | 1u << 0 | 1u << 2 | 1u << 3 | 1u << 5));
    Region harmonic = key("C harmonic minor");
    Region untouched = out;
    assert(region_resolve(test_knowledge(), &set, &harmonic, &out, message, sizeof(message)) == REGION_RELATION_REFUSED);
    assert(strstr(message, "I0") != NULL && strstr(message, "C harmonic minor") != NULL && region_equals(&out, &untouched));

    Relation serial;
    memset(&serial, 0, sizeof(serial));
    serial.id = "test.relation.serial";
    serial.name = "row form";
    serial.family = RELATION_FAMILY_SERIAL;
    assert(region_resolve(test_knowledge(), &serial, &anchor, &out, message, sizeof(message)) == REGION_RELATION_REFUSED);
    assert(strstr(message, "row form") != NULL && region_equals(&out, &untouched));
}

/* A degree relation from a home key agrees with harmony_related_key_of for every degree the entries cover (2 to 7). Degree 1 has no
   entry: the tonic is no related key, and a degree 1 relation would give the home key again. */
static void test_agrees_with_related_keys(void) {
    static const char *const DEGREE_IDS[8] = {NULL,          NULL,          "omt.relation.supertonic", "omt.relation.mediant",
                                              "omt.relation.subdominant", "omt.relation.dominant", "omt.relation.submediant",
                                              "omt.relation.subtonic"};
    static const char *const HOMES[2] = {"C major", "A minor"};
    for (int home = 0; home < 2; home++) {
        HarmonyMode mode = home == 0 ? HARMONY_MODE_MAJOR : HARMONY_MODE_MINOR;
        Region anchor = key(HOMES[home]);
        for (int degree = 2; degree <= 7; degree++) {
            const Relation *by_degree = relation(DEGREE_IDS[degree]);
            assert(by_degree->family == RELATION_FAMILY_TONAL && by_degree->tonal.by_degree && by_degree->tonal.degree == degree);
            Region out;
            char message[256];
            RegionRelationStatus status = region_resolve(test_knowledge(), by_degree, &anchor, &out, message, sizeof(message));
            HarmonyRelatedKey related;
            if (harmony_related_key_of(mode, degree, &related)) {
                assert(status == REGION_RELATION_OK);
                int semitone = harmony_related_key_semitone(mode, &related);
                assert(out.center == (anchor.center + semitone) % 12);
                assert(strcmp(out.material->id, related.mode == HARMONY_MODE_MAJOR ? "omt.material.major" : "omt.material.natural-minor") == 0);
            } else {
                assert(status == REGION_RELATION_REFUSED);
            }
        }
    }
    /* the home key as a minor key on another tonic agrees too: C minor, F# major */
    Region c_minor = key("C minor");
    Region out;
    char message[256];
    assert(region_resolve(test_knowledge(), relation("omt.relation.mediant"), &c_minor, &out, message, sizeof(message)) == REGION_RELATION_OK);
    assert(out.center == 3 && strcmp(out.material->id, "omt.material.major") == 0);
}

static void test_brightness(void) {
    static const char *const MODES[7] = {"locrian", "phrygian", "aeolian", "dorian", "mixolydian", "major", "lydian"};
    char message[256];
    for (int rank = 1; rank <= 7; rank++) {
        int got = 0;
        assert(brightness_rank_of(material(MODES[rank - 1]), &got, message, sizeof(message)) && got == rank);
        for (int steps = -6; steps <= 6; steps++) {
            const PitchMaterial *shifted = brightness_shift(test_knowledge(), material(MODES[rank - 1]), steps, message, sizeof(message));
            if (rank + steps >= 1 && rank + steps <= 7) {
                assert(shifted == material(MODES[rank + steps - 1]));
            } else {
                assert(shifted == NULL && strstr(message, "no diatonic mode has brightness rank") != NULL);
            }
        }
    }
    int rank = 99;
    assert(!brightness_rank_of(material("harmonic minor"), &rank, message, sizeof(message)));
    assert(strcmp(message, "harmonic minor has no brightness: only the diatonic modes rank") == 0 && rank == 99);
    assert(!brightness_rank_of(material("melodic minor"), &rank, message, sizeof(message)));
    assert(strcmp(message, "melodic minor has no brightness: only the diatonic modes rank") == 0);
    assert(brightness_shift(test_knowledge(), material("harmonic minor"), 1, message, sizeof(message)) == NULL);
    assert(strcmp(message, "harmonic minor has no brightness: only the diatonic modes rank") == 0);
    assert(brightness_shift(test_knowledge(), material("lydian"), 1, NULL, 0) == NULL);
    assert(!brightness_rank_of(NULL, &rank, message, sizeof(message)) && !brightness_rank_of(material("lydian"), NULL, NULL, 0));
    assert(brightness_shift(NULL, material("lydian"), 0, message, sizeof(message)) == NULL);
    assert(brightness_shift(test_knowledge(), NULL, 0, message, sizeof(message)) == NULL);
    assert(brightness_shift(test_knowledge(), material("dorian"), 0, message, sizeof(message)) == material("dorian"));

    /* the same through a relation: refused on harmonic minor, naming the relation and the anchor */
    Region anchor = key("A harmonic minor");
    Region out = region_tonal(5, material("lydian"));
    Region before = out;
    assert(region_resolve(test_knowledge(), relation("omt.relation.one-step-brighter"), &anchor, &out, message, sizeof(message)) ==
           REGION_RELATION_REFUSED);
    assert(strstr(message, "'one step brighter'") != NULL && strstr(message, "A harmonic minor") != NULL &&
           strstr(message, "harmonic minor has no brightness: only the diatonic modes rank") != NULL);
    assert(region_equals(&out, &before));
}

static void test_refusal_messages_and_arguments(void) {
    char message[256];
    Region anchor = key("A minor");
    Region out = region_tonal(1, material("major"));
    Region before = out;
    assert(region_resolve(test_knowledge(), relation("omt.relation.supertonic"), &anchor, &out, message, sizeof(message)) ==
           REGION_RELATION_REFUSED);
    assert(strcmp(message, "relation 'supertonic' from A minor: the triad on degree 2 is not major or minor, so it gives no key") == 0);
    assert(region_equals(&out, &before));
    assert(region_resolve(test_knowledge(), relation("omt.relation.supertonic"), &anchor, &out, NULL, 0) == REGION_RELATION_REFUSED);
    char tiny[12];
    assert(region_resolve(test_knowledge(), relation("omt.relation.supertonic"), &anchor, &out, tiny, sizeof(tiny)) ==
           REGION_RELATION_REFUSED);
    assert(strlen(tiny) == sizeof(tiny) - 1);
    Region mixolydian = key("G mixolydian");
    assert(region_resolve(test_knowledge(), relation("omt.relation.parallel"), &mixolydian, &out, message, sizeof(message)) ==
           REGION_RELATION_REFUSED);
    assert(strstr(message, "relation 'parallel' from G mixolydian") != NULL);

    /* a degree beyond the material's steps: a five-step material has no degree 6 */
    PitchMaterial pentatonic;
    memset(&pentatonic, 0, sizeof(pentatonic));
    pentatonic.id = "test.material.pentatonic";
    pentatonic.name = "pentatonic";
    pentatonic.key_word = "pentatonic";
    pentatonic.step_count = 5;
    memcpy(pentatonic.steps, (int[]){0, 2, 4, 7, 9}, 5 * sizeof(int));
    Region penta = region_tonal(0, &pentatonic);
    assert(region_resolve(test_knowledge(), relation("omt.relation.submediant"), &penta, &out, message, sizeof(message)) ==
           REGION_RELATION_REFUSED);
    assert(strcmp(message, "relation 'submediant' from C pentatonic: the material has no degree 6") == 0);

    /* not a tonal or modal region with a center and a material */
    Region field = {REGION_PITCH_FIELD, -1, &pentatonic, REGION_STRENGTH_ESTABLISHED};
    assert(region_resolve(test_knowledge(), relation("omt.relation.parallel"), &field, &out, message, sizeof(message)) ==
           REGION_RELATION_ERR_ARGUMENT);
    Region none = {REGION_NONE, -1, NULL, REGION_STRENGTH_ABSENT};
    assert(region_resolve(test_knowledge(), relation("omt.relation.parallel"), &none, &out, message, sizeof(message)) ==
           REGION_RELATION_ERR_ARGUMENT);
    Region no_material = {REGION_TONAL_MODAL, 0, NULL, REGION_STRENGTH_ESTABLISHED};
    assert(region_resolve(test_knowledge(), relation("omt.relation.parallel"), &no_material, &out, message, sizeof(message)) ==
           REGION_RELATION_ERR_ARGUMENT);
    assert(region_resolve(NULL, relation("omt.relation.parallel"), &anchor, &out, message, sizeof(message)) == REGION_RELATION_ERR_ARGUMENT);
    assert(region_resolve(test_knowledge(), NULL, &anchor, &out, message, sizeof(message)) == REGION_RELATION_ERR_ARGUMENT);
    assert(region_resolve(test_knowledge(), relation("omt.relation.parallel"), NULL, &out, message, sizeof(message)) ==
           REGION_RELATION_ERR_ARGUMENT);
    assert(region_resolve(test_knowledge(), relation("omt.relation.parallel"), &anchor, NULL, message, sizeof(message)) ==
           REGION_RELATION_ERR_ARGUMENT);
    assert(region_equals(&out, &before));
}

static void test_region_values(void) {
    Region c_major = region_tonal(0, material("major"));
    assert(c_major.kind == REGION_TONAL_MODAL && c_major.center == 0 && c_major.strength == REGION_STRENGTH_ESTABLISHED);
    assert(region_tonal(-1, material("major")).center == 11 && region_tonal(14, material("major")).center == 2);
    assert(region_equals(&c_major, &c_major));
    Region same = region_tonal(12, material("major"));
    assert(region_equals(&c_major, &same));
    Region weaker = c_major;
    weaker.strength = REGION_STRENGTH_FRAGILE;
    assert(!region_equals(&c_major, &weaker));
    assert(!region_equals(&c_major, &(Region){REGION_TONAL_MODAL, 0, material("minor"), REGION_STRENGTH_ESTABLISHED}));
    assert(!region_equals(&c_major, &(Region){REGION_TONAL_MODAL, 7, material("major"), REGION_STRENGTH_ESTABLISHED}));
    assert(!region_equals(&c_major, &(Region){REGION_PITCH_FIELD, 0, material("major"), REGION_STRENGTH_ESTABLISHED}));
    assert(region_equals(&(Region){REGION_NONE, -1, NULL, REGION_STRENGTH_ABSENT}, &(Region){REGION_NONE, -1, NULL, REGION_STRENGTH_ABSENT}));
    assert(!region_equals(&c_major, NULL) && region_equals(NULL, NULL));
}

int main(void) {
    test_region_values();
    test_embedded_examples();
    test_named_relations();
    test_specific_cases();
    test_hand_built_relations();
    test_agrees_with_related_keys();
    test_brightness();
    test_refusal_messages_and_arguments();
    return 0;
}
