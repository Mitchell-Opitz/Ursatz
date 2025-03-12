#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "service/piece_metadata.h"

static int failures = 0;

#define CHECK(cond, msg)                                            \
    do {                                                            \
        if (!(cond)) {                                              \
            fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
            failures++;                                             \
        }                                                           \
    } while (0)

static void check_value(const char *input, const char *expected, const char *label) {
    bool too_long = true;
    char *out = ursatz_gui_meta_normalize_value(input, &too_long);
    CHECK(out != NULL && !too_long && strcmp(out, expected) == 0, label);
    free(out);
}

static void test_normalize_value(void) {
    check_value("John  Field ", "John Field", "trims and collapses spaces");
    check_value("  \t John \n\n  Field\t", "John Field", "tabs and newlines count as whitespace");
    check_value("Jo\x01hn\x7f Field", "John Field", "control characters are dropped");
    check_value("", "", "empty is valid");
    check_value("   ", "", "blank normalizes to empty");
    check_value(NULL, "", "NULL normalizes to empty");

    char text[200];
    bool too_long = false;
    memset(text, 'a', 120);
    text[120] = '\0';
    char *out = ursatz_gui_meta_normalize_value(text, &too_long);
    CHECK(out != NULL && strlen(out) == 120, "120 bytes are accepted");
    free(out);
    memset(text, 'a', 121);
    text[121] = '\0';
    out = ursatz_gui_meta_normalize_value(text, &too_long);
    CHECK(out == NULL && too_long, "121 bytes are too long");
    /* Length is measured after normalizing. */
    memset(text, 'a', 120);
    memcpy(text + 120, "      ", 7);
    too_long = true;
    out = ursatz_gui_meta_normalize_value(text, &too_long);
    CHECK(out != NULL && !too_long, "trailing whitespace does not count toward the limit");
    free(out);
}

static void test_normalize_tag(void) {
    bool invalid = true;
    char *tag = ursatz_gui_meta_normalize_tag("  Whimsical   Mood ", &invalid);
    CHECK(tag && !invalid && strcmp(tag, "whimsical mood") == 0, "tag is trimmed, collapsed and lowercased");
    free(tag);

    tag = ursatz_gui_meta_normalize_tag("   ", &invalid);
    CHECK(tag == NULL && invalid, "an empty tag is invalid");
    tag = ursatz_gui_meta_normalize_tag("a,b", &invalid);
    CHECK(tag == NULL && invalid, "a comma makes a tag invalid");

    char text[80];
    memset(text, 'A', 48);
    text[48] = '\0';
    tag = ursatz_gui_meta_normalize_tag(text, &invalid);
    CHECK(tag && !invalid && strlen(tag) == 48 && tag[0] == 'a', "48 bytes are accepted and lowercased");
    free(tag);
    memset(text, 'A', 49);
    text[49] = '\0';
    tag = ursatz_gui_meta_normalize_tag(text, &invalid);
    CHECK(tag == NULL && invalid, "49 bytes are too long for a tag");
}

static void test_values_equal(void) {
    CHECK(ursatz_gui_meta_values_equal("John Field", "  john   FIELD"), "equal after normalizing, ignoring case");
    CHECK(!ursatz_gui_meta_values_equal("John Field", "John Fields"), "different values differ");
    CHECK(ursatz_gui_meta_values_equal(NULL, "  "), "NULL equals blank");
}

static void test_tag_list(void) {
    char **tags = NULL;
    size_t count = 0;
    CHECK(ursatz_gui_meta_tags_add(&tags, &count, "b") == URSATZ_GUI_META_OK, "add first tag");
    CHECK(ursatz_gui_meta_tags_add(&tags, &count, "a") == URSATZ_GUI_META_OK, "add second tag");
    CHECK(ursatz_gui_meta_tags_add(&tags, &count, "B") == URSATZ_GUI_META_OK && count == 2,
          "a duplicate is skipped without error");
    CHECK(count == 2 && strcmp(tags[0], "b") == 0 && strcmp(tags[1], "a") == 0, "insertion order is kept");
    CHECK(ursatz_gui_meta_tags_remove(&tags, &count, "zzz") == URSATZ_GUI_META_OK && count == 2,
          "removing an absent tag is not an error");
    CHECK(ursatz_gui_meta_tags_remove(&tags, &count, "b") == URSATZ_GUI_META_OK && count == 1 &&
              strcmp(tags[0], "a") == 0,
          "remove drops the tag");
    CHECK(ursatz_gui_meta_tags_remove(&tags, &count, "a") == URSATZ_GUI_META_OK && count == 0 && tags == NULL,
          "removing the last tag empties the list");

    for (int i = 0; i < URSATZ_GUI_META_MAX_TAGS; i++) {
        char name[16];
        snprintf(name, sizeof(name), "tag%d", i);
        CHECK(ursatz_gui_meta_tags_add(&tags, &count, name) == URSATZ_GUI_META_OK, "add up to 32 tags");
    }
    CHECK(ursatz_gui_meta_tags_add(&tags, &count, "one-too-many") == URSATZ_GUI_META_ERR_TOO_MANY_TAGS &&
              count == 32,
          "the 33rd tag fails");
    CHECK(ursatz_gui_meta_tags_add(&tags, &count, "tag3") == URSATZ_GUI_META_OK && count == 32,
          "a duplicate at the limit is still not an error");
    ursatz_gui_meta_tags_free(tags, count);
}

typedef struct {
    const char *composer;
    const char *source;
    const char *tags[4];
    size_t tag_count;
} Spec;

static UrsatzGuiPieceSummary *build_pieces(const Spec *specs, size_t count) {
    UrsatzGuiPieceSummary *pieces = calloc(count, sizeof(UrsatzGuiPieceSummary));
    for (size_t i = 0; i < count; i++) {
        pieces[i].composer = strdup(specs[i].composer);
        pieces[i].source = strdup(specs[i].source);
        pieces[i].tag_count = specs[i].tag_count;
        if (specs[i].tag_count > 0) {
            pieces[i].tags = calloc(specs[i].tag_count, sizeof(char *));
            for (size_t t = 0; t < specs[i].tag_count; t++) {
                pieces[i].tags[t] = strdup(specs[i].tags[t]);
            }
        }
    }
    return pieces;
}

static void free_pieces(UrsatzGuiPieceSummary *pieces, size_t count) {
    for (size_t i = 0; i < count; i++) {
        free(pieces[i].composer);
        free(pieces[i].source);
        ursatz_gui_meta_tags_free(pieces[i].tags, pieces[i].tag_count);
    }
    free(pieces);
}

static const Spec kSpecs[] = {
    {"John Field", "Nocturnes", {"slow", "romantic"}, 2},
    {"john field", "Nocturnes", {"slow"}, 1},
    {"John Field", "nocturnes", {"slow", "SLOW"}, 2},
    {"Clementi", "Sonatinas", {"fast"}, 1},
    {"Mozart", "", {}, 0},
    {"", "Sonatinas", {"fast", "romantic"}, 2},
    {"Alpha", "", {"zeta"}, 1},
    {"beta", "", {"zeta"}, 1},
};

static void test_grouping(void) {
    size_t count = sizeof(kSpecs) / sizeof(kSpecs[0]);
    UrsatzGuiPieceSummary *pieces = build_pieces(kSpecs, count);
    UrsatzGuiMetaGroups groups;
    CHECK(ursatz_gui_meta_group(pieces, count, &groups) == URSATZ_GUI_META_OK, "grouping succeeds");

    CHECK(groups.total == 8, "total counts every piece");
    CHECK(groups.composer_count == 5, "composers are distinct case-insensitively");
    CHECK(groups.composer_count > 0 && strcmp(groups.composers[0].value, "John Field") == 0 &&
              groups.composers[0].count == 3,
          "the most used spelling is shown (John Field twice, john field once)");
    CHECK(groups.composer_count == 5 && strcmp(groups.composers[1].value, "Alpha") == 0 &&
              groups.composers[2].value && strcmp(groups.composers[2].value, "beta") == 0,
          "equal counts sort by case-insensitive value");
    CHECK(groups.unassigned_composer == 1, "one piece has no composer");
    CHECK(groups.unassigned_source == 3, "three pieces have no source");

    CHECK(groups.source_count == 2 && strcmp(groups.sources[0].value, "Nocturnes") == 0 &&
              groups.sources[0].count == 3 && groups.sources[1].count == 2,
          "sources group case-insensitively, most used spelling shown");

    CHECK(groups.tag_count == 4, "tags are distinct");
    CHECK(groups.tag_count == 4 && strcmp(groups.tags[0].value, "slow") == 0 && groups.tags[0].count == 3,
          "a tag repeated within one piece counts once");
    CHECK(groups.untagged == 1, "one piece is untagged");

    ursatz_gui_meta_groups_destroy(&groups);
    free_pieces(pieces, count);
}

static void test_spelling_tie(void) {
    static const Spec specs[] = {{"bach", "", {}, 0}, {"Bach", "", {}, 0}};
    UrsatzGuiPieceSummary *pieces = build_pieces(specs, 2);
    UrsatzGuiMetaGroups groups;
    CHECK(ursatz_gui_meta_group(pieces, 2, &groups) == URSATZ_GUI_META_OK, "grouping a tie succeeds");
    CHECK(groups.composer_count == 1 && groups.composers[0].count == 2 &&
              strcmp(groups.composers[0].value, "Bach") == 0,
          "a spelling tie is broken by byte order");
    ursatz_gui_meta_groups_destroy(&groups);
    free_pieces(pieces, 2);

    UrsatzGuiMetaGroups empty;
    CHECK(ursatz_gui_meta_group(NULL, 0, &empty) == URSATZ_GUI_META_OK && empty.total == 0 &&
              empty.composers == NULL,
          "no pieces gives empty groups");
    ursatz_gui_meta_groups_destroy(&empty);
}

static size_t count_matches(const UrsatzGuiPieceSummary *pieces, size_t count, const UrsatzGuiMetaFilter *filter) {
    size_t matched = 0;
    for (size_t i = 0; i < count; i++) {
        matched += ursatz_gui_meta_matches(&pieces[i], filter) ? 1 : 0;
    }
    return matched;
}

static void test_filters(void) {
    size_t count = sizeof(kSpecs) / sizeof(kSpecs[0]);
    UrsatzGuiPieceSummary *pieces = build_pieces(kSpecs, count);

    UrsatzGuiMetaFilter none = {0};
    CHECK(count_matches(pieces, count, &none) == count, "an empty filter matches everything");

    UrsatzGuiMetaFilter composer = {0};
    composer.composer = "JOHN FIELD";
    CHECK(count_matches(pieces, count, &composer) == 3, "composer matches case-insensitively");

    UrsatzGuiMetaFilter tag = {0};
    tag.tag = "Romantic";
    CHECK(count_matches(pieces, count, &tag) == 2, "tag matches when any tag equals it");

    UrsatzGuiMetaFilter combined = {0};
    combined.composer = "john field";
    combined.tag = "romantic";
    CHECK(count_matches(pieces, count, &combined) == 1, "all given fields must match");

    UrsatzGuiMetaFilter unassigned = {0};
    unassigned.composer_unassigned = true;
    CHECK(count_matches(pieces, count, &unassigned) == 1, "composer_unassigned matches an empty composer");
    unassigned.composer_unassigned = false;
    unassigned.source_unassigned = true;
    CHECK(count_matches(pieces, count, &unassigned) == 3, "source_unassigned matches an empty source");

    UrsatzGuiMetaFilter untagged = {0};
    untagged.untagged = true;
    CHECK(count_matches(pieces, count, &untagged) == 1, "untagged matches a piece without tags");

    UrsatzGuiMetaFilter source_and_unassigned = {0};
    source_and_unassigned.source = "sonatinas";
    source_and_unassigned.composer_unassigned = true;
    CHECK(count_matches(pieces, count, &source_and_unassigned) == 1, "source and unassigned composer combine");

    free_pieces(pieces, count);
}

int main(void) {
    test_normalize_value();
    test_normalize_tag();
    test_values_equal();
    test_tag_list();
    test_grouping();
    test_spelling_tie();
    test_filters();
    if (failures == 0) {
        printf("piece_metadata: all checks passed\n");
    }
    return failures == 0 ? 0 : 1;
}
