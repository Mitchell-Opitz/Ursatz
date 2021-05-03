#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "identity/entity_id.h"
#include "mixed_texture_rules.h"
#include "rule_check.h"

#define MONO STANDARD_TEXTURE_MONOPHONY
#define HETERO STANDARD_TEXTURE_HETEROPHONY
#define HOMO STANDARD_TEXTURE_HOMOPHONY
#define HOMORHYTHM STANDARD_TEXTURE_HOMORHYTHM
#define MELACC STANDARD_TEXTURE_MELODY_AND_ACCOMPANIMENT
#define POLY STANDARD_TEXTURE_POLYPHONY

static void test_rules_reference_framework(void) {
    CHECK_RULES_REFERENCE_FRAMEWORK(standard_texture_mixed_rule, standard_texture_mixed_constraint,
                                    STANDARD_TEXTURE_MIXED_RULE_COUNT);
}

static EntityID *passage(const char *value) {
    EntityID *id;
    assert(entity_id_create(value, &id) == ENTITY_ID_OK);
    return id;
}

/* A work that opens with a solo voice, then changes to melody and
   accompaniment, then to polyphony. */
static void test_a_work_that_moves_between_textures(void) {
    EntityID *a = passage("intro");
    EntityID *b = passage("theme");
    EntityID *c = passage("development");
    StandardTextureSegment segments[] = {{a, MONO}, {b, MELACC}, {c, POLY}};

    assert(standard_texture_segments_valid(segments, 3));
    assert(standard_texture_is_mixed(segments, 3));
    assert(standard_texture_distinct_count(segments, 3, false) == 3);
    assert(standard_texture_distinct_count(segments, 3, true) == 3);

    size_t points[4] = {99, 99, 99, 99};
    assert(standard_texture_change_points(segments, 3, false, points, 4) == 2);
    assert(points[0] == 1 && points[1] == 2 && points[2] == 99);

    entity_id_destroy(a);
    entity_id_destroy(b);
    entity_id_destroy(c);
}

/* A change between the two homophony subcategories is a change of texture
   but not of category: the work is not mixed. */
static void test_a_change_inside_a_category(void) {
    EntityID *a = passage("a");
    EntityID *b = passage("b");
    EntityID *c = passage("c");
    StandardTextureSegment segments[] = {{a, HOMORHYTHM}, {b, MELACC}, {c, HOMO}};

    assert(standard_texture_change_points(segments, 3, false, NULL, 0) == 2);
    assert(standard_texture_change_points(segments, 3, true, NULL, 0) == 0);
    assert(standard_texture_distinct_count(segments, 3, false) == 3);
    assert(standard_texture_distinct_count(segments, 3, true) == 1);
    assert(!standard_texture_is_mixed(segments, 3));

    entity_id_destroy(a);
    entity_id_destroy(b);
    entity_id_destroy(c);
}

/* One texture throughout is not mixed; a return to an earlier texture is a
   change each way. */
static void test_unchanged_and_returning_textures(void) {
    EntityID *a = passage("a");
    EntityID *b = passage("b");
    EntityID *c = passage("c");
    StandardTextureSegment steady[] = {{a, POLY}, {b, POLY}, {c, POLY}};
    assert(!standard_texture_is_mixed(steady, 3));
    assert(standard_texture_change_points(steady, 3, false, NULL, 0) == 0);
    assert(standard_texture_distinct_count(steady, 3, false) == 1);

    StandardTextureSegment returning[] = {{a, MONO}, {b, HETERO}, {c, MONO}};
    assert(standard_texture_is_mixed(returning, 3));
    assert(standard_texture_change_points(returning, 3, false, NULL, 0) == 2);
    assert(standard_texture_distinct_count(returning, 3, false) == 2);

    StandardTextureSegment single[] = {{a, MONO}};
    assert(!standard_texture_is_mixed(single, 1));
    assert(standard_texture_distinct_count(single, 1, true) == 1);

    entity_id_destroy(a);
    entity_id_destroy(b);
    entity_id_destroy(c);
}

/* A small capacity still counts every change point. */
static void test_change_point_capacity(void) {
    EntityID *id = passage("p");
    StandardTextureSegment segments[] = {{id, MONO}, {id, POLY}, {id, MONO}, {id, POLY}, {id, MONO}};
    size_t points[2] = {99, 99};
    assert(standard_texture_change_points(segments, 5, false, points, 2) == 4);
    assert(points[0] == 1 && points[1] == 2);
    assert(standard_texture_change_points(segments, 5, false, points, 0) == 4);
    assert(points[0] == 1);
    entity_id_destroy(id);
}

static void test_invalid_lists(void) {
    EntityID *id = passage("p");
    StandardTextureSegment valid[] = {{id, MONO}, {id, POLY}};
    StandardTextureSegment no_passage[] = {{id, MONO}, {NULL, POLY}};
    StandardTextureSegment bad_texture[] = {{id, MONO}, {id, STANDARD_TEXTURE_COUNT}};

    assert(standard_texture_segments_valid(valid, 2));
    assert(!standard_texture_segments_valid(NULL, 2));
    assert(!standard_texture_segments_valid(valid, 0));
    assert(!standard_texture_segments_valid(no_passage, 2));
    assert(!standard_texture_segments_valid(bad_texture, 2));

    assert(standard_texture_change_points(no_passage, 2, false, NULL, 0) == 0);
    assert(standard_texture_distinct_count(bad_texture, 2, false) == 0);
    assert(!standard_texture_is_mixed(NULL, 2));
    assert(!standard_texture_is_mixed(no_passage, 2));
    entity_id_destroy(id);
}

static void test_build_texture(void) {
    for (int t = 0; t < STANDARD_TEXTURE_COUNT; t++) {
        EntityID *target = passage("section-1");
        Texture *texture = NULL;
        assert(standard_texture_build_texture((StandardTexture)t, target, &texture) == TEXTURE_OK);
        assert(strcmp(texture_label(texture), standard_texture_name((StandardTexture)t)) == 0);

        const Interpretation *interpretation = texture_interpretation(texture);
        assert(interpretation_framework_reference(interpretation) != NULL);
        assert(entity_id_equals(interpretation_target_id(interpretation), target));
        assert(strcmp(interpretation_author(interpretation), "standard-texture-v1") == 0);
        texture_destroy(texture);
    }
}

static void test_build_texture_rejects_invalid_textures(void) {
    EntityID *target = passage("section-2");
    Texture *texture = NULL;
    assert(standard_texture_build_texture(STANDARD_TEXTURE_COUNT, target, &texture) == TEXTURE_ERR_NULL_LABEL);
    assert(standard_texture_build_texture((StandardTexture)-1, target, &texture) == TEXTURE_ERR_NULL_LABEL);
    assert(texture == NULL);
    /* The caller still owns the target on failure. */
    entity_id_destroy(target);
}

int main(void) {
    test_rules_reference_framework();
    test_a_work_that_moves_between_textures();
    test_a_change_inside_a_category();
    test_unchanged_and_returning_textures();
    test_change_point_capacity();
    test_invalid_lists();
    test_build_texture();
    test_build_texture_rejects_invalid_textures();
    printf("test_mixed_texture_rules (standard-texture): ok\n");
    return 0;
}
