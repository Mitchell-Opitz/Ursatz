#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/form_spec.h>
#include <ursatz_composer/given.h>
#include <ursatz_composer/harmony_schema.h>
#include <ursatz_composer/piece.h>
#include <ursatz_composer/piece_perform.h>
#include <plan/plan_lanes.h>
#include <plan/plan_state.h>
#include "support/test_knowledge.h"

/*
 * The logic of piece.h that the requests reach but nothing asserted on its own: the regions a section asks for and the harmony it is given.
 *
 *   regions    piece_resolve_regions: a relation from home, a key, home again, and the four refusals (previous at section 0, an unknown
 *              material, a material the Composer does not write yet) with their words
 *   overlays   piece_resolve_overlay_regions: no overlay, an overlay on one section, a refused anchor
 *   harmony    piece_give_harmony (a progression, a schema, none, refusals) and piece_check_harmony (what fits a four bar section)
 *   names      piece_fill_name, piece_fill_from_name and piece_part_name
 *   sizes      piece_basic_idea against the sizes the built-in forms give, and piece_parts_source
 */

static void set_relation(SectionRegion *region, const char *word, SectionAnchor anchor) {
    memset(region, 0, sizeof(*region));
    region->kind = SECTION_REGION_RELATION;
    snprintf(region->relation, sizeof(region->relation), "%s", word);
    region->anchor = anchor;
}

static void set_key(SectionRegion *region, const char *tonic, const char *material) {
    memset(region, 0, sizeof(*region));
    region->kind = SECTION_REGION_KEY;
    snprintf(region->tonic, sizeof(region->tonic), "%s", tonic);
    snprintf(region->material, sizeof(region->material), "%s", material);
}

static PieceRequest two_sections(PieceSection *sections) {
    PieceRequest request;
    memset(&request, 0, sizeof(request));
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    for (size_t i = 0; i < 2; i++) {
        piece_section_init(&sections[i]);
        snprintf(sections[i].description.id, sizeof(sections[i].description.id), "%s", i == 0 ? "a" : "b");
    }
    request.sections = sections;
    request.section_count = 2;
    return request;
}

static void test_regions(void) {
    PieceSection sections[2];
    PieceRequest request = two_sections(sections);
    PieceRegion regions[2];
    size_t refused = 99;
    char message[256] = "";
    /* A dominant of home at the end of A: a region that changes the key, on G (pitch class 7), reached by the relation's id. */
    set_relation(&sections[0].description.key_end, "dominant", SECTION_ANCHOR_HOME);
    assert(piece_resolve_regions(&request, test_knowledge(), regions, &refused, message, sizeof(message)) == PIECE_OK);
    assert(regions[0].has_region && regions[0].changes_key);
    assert(strcmp(regions[0].relation, "omt.relation.dominant") == 0);
    assert(regions[0].anchor == PLAN_KEY_ANCHOR_HOME);
    assert(strcmp(regions[0].text, "dominant of home") == 0);
    assert(spelled_key_tonic_pitch_class(&regions[0].key) == 7);
    /* B has no key end: no region, home. */
    assert(!regions[1].has_region && !regions[1].changes_key);
    assert(strcmp(regions[1].text, "home") == 0);
    assert(spelled_key_tonic_pitch_class(&regions[1].key) == 0);
    /* HOME after a section away from home is a change of key back. */
    memset(&sections[1].description.key_end, 0, sizeof(SectionRegion));
    sections[1].description.key_end.kind = SECTION_REGION_HOME;
    assert(piece_resolve_regions(&request, test_knowledge(), regions, &refused, message, sizeof(message)) == PIECE_OK);
    assert(regions[1].has_region && regions[1].changes_key && strcmp(regions[1].text, "home") == 0);
    /* A key given as a tonic and a material: the text names both, no relation. */
    memset(&sections[1].description.key_end, 0, sizeof(SectionRegion));
    set_key(&sections[0].description.key_end, "G", "major");
    assert(piece_resolve_regions(&request, test_knowledge(), regions, &refused, message, sizeof(message)) == PIECE_OK);
    assert(regions[0].has_region && regions[0].changes_key && strcmp(regions[0].text, "G major") == 0);
    assert(regions[0].relation[0] == '\0' && spelled_key_tonic_pitch_class(&regions[0].key) == 7);
    /* The refusals: the section's index and the words. */
    set_relation(&sections[0].description.key_end, "dominant", SECTION_ANCHOR_PREVIOUS);
    assert(piece_resolve_regions(&request, test_knowledge(), regions, &refused, message, sizeof(message)) == PIECE_ERR_BAD_REQUEST);
    assert(refused == 0 && strcmp(message, "the first section has no previous region") == 0);
    set_key(&sections[0].description.key_end, "G", "nosuch");
    assert(piece_resolve_regions(&request, test_knowledge(), regions, &refused, message, sizeof(message)) == PIECE_ERR_BAD_REQUEST);
    assert(refused == 0 && strcmp(message, "section 'a': no material is named 'nosuch'") == 0);
    /* The snapshot holds dorian; the Composer writes major and natural minor until P69 (R-611). */
    set_key(&sections[0].description.key_end, "D", "dorian");
    assert(piece_resolve_regions(&request, test_knowledge(), regions, &refused, message, sizeof(message)) == PIECE_ERR_BAD_REQUEST);
    assert(refused == 0 && strstr(message, "until P69") != NULL && strstr(message, "dorian") != NULL);
    /* NULL arguments. */
    assert(piece_resolve_regions(NULL, test_knowledge(), regions, &refused, message, sizeof(message)) == PIECE_ERR_NULL);
    assert(piece_resolve_regions(&request, NULL, regions, &refused, message, sizeof(message)) == PIECE_ERR_NULL);
    assert(piece_resolve_regions(&request, test_knowledge(), NULL, &refused, message, sizeof(message)) == PIECE_ERR_NULL);
}

static void test_overlays(void) {
    FormSpec *form = NULL;
    assert(form_spec_builtin("ternary", &form) == FORM_SPEC_OK && form);
    const FormSpecSize *size = form_spec_size(form, 24, 0);
    assert(size && size->bars == 24);
    size_t a = form_spec_section_index(size, "A");
    size_t b = form_spec_section_index(size, "B");
    assert(a == 0 && b == 1 && b < size->section_count);
    PieceSection overlay;
    PieceRequest request;
    PieceRegion regions[FORM_SPEC_MAX_SECTIONS];
    bool regional = true;
    size_t refused = 99;
    char message[256] = "";
    memset(&request, 0, sizeof(request));
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    /* No overlay: nothing regional. */
    assert(piece_resolve_overlay_regions(&request, size, test_knowledge(), regions, &regional, &refused, message, sizeof(message)) == PIECE_OK);
    assert(!regional);
    /* A dominant overlay on B: a region on B only. */
    piece_section_init(&overlay);
    snprintf(overlay.description.name, sizeof(overlay.description.name), "B");
    set_relation(&overlay.description.key_end, "dominant", SECTION_ANCHOR_HOME);
    request.overlays = &overlay;
    request.overlay_count = 1;
    assert(piece_resolve_overlay_regions(&request, size, test_knowledge(), regions, &regional, &refused, message, sizeof(message)) == PIECE_OK);
    assert(regional);
    for (size_t i = 0; i < size->section_count; i++) {
        assert(regions[i].has_region == (i == b));
    }
    assert(regions[b].changes_key && spelled_key_tonic_pitch_class(&regions[b].key) == 7);
    /* An overlay on A anchored on the section before: refused at section 0. */
    snprintf(overlay.description.name, sizeof(overlay.description.name), "A");
    set_relation(&overlay.description.key_end, "dominant", SECTION_ANCHOR_PREVIOUS);
    assert(piece_resolve_overlay_regions(&request, size, test_knowledge(), regions, &regional, &refused, message, sizeof(message)) ==
           PIECE_ERR_BAD_REQUEST);
    assert(refused == 0 && strcmp(message, "the first section has no previous region") == 0);
    form_spec_destroy(form);
}

static Rational time_of(long numerator, unsigned long denominator) {
    Rational time;
    assert(rational_make(numerator, denominator, &time) == RATIONAL_OK);
    return time;
}

static NodeId section_a(const Plan *plan) {
    NodeId root = plan_root(plan);
    for (size_t i = 0; i < plan_node_child_count(plan, root); i++) {
        NodeId node = plan_node_child_at(plan, root, i);
        const SectionDescription *section = plan_node_section(plan, node);
        if (section && strcmp(section->letter, "A") == 0) {
            return node;
        }
    }
    return PLAN_NODE_NONE;
}

static Plan *new_plan(NodeId *section) {
    PieceRequest request;
    Plan *plan = NULL;
    memset(&request, 0, sizeof(request));
    request.form = "rounded-binary";
    request.key = test_key(0, "major");
    request.knowledge = test_knowledge();
    request.meter.beats_per_bar = 4;
    request.meter.beat_unit = 4;
    request.seed = 1;
    assert(piece_build_plan(&request, &plan) == PIECE_OK);
    *section = section_a(plan);
    assert(*section != PLAN_NODE_NONE);
    return plan;
}

static size_t harmony_spans(const Plan *plan, NodeId section, LaneSpanId *ids, size_t capacity) {
    return plan_lane_find(plan, PLAN_LANE_HARMONY, 0, plan_node_span(plan, section), ids, capacity);
}

static void test_give_harmony(void) {
    NodeId section;
    Plan *plan = new_plan(&section);
    PlanMeter meter = {4, 4, 0};
    SpelledKey key = test_key(0, "major");
    SectionHarmony harmony;
    LaneSpanId ids[64];
    char message[256] = "";
    memset(&harmony, 0, sizeof(harmony));
    /* A progression of five numerals: one Given span each, a bar long (cells 0 is the cells of a bar), first and last "I". */
    harmony.kind = SECTION_HARMONY_PROGRESSION;
    snprintf(harmony.numerals, sizeof(harmony.numerals), "I vi IV V I");
    assert(piece_give_harmony(plan, section, &harmony, &meter, &key, message, sizeof(message)) == PIECE_OK);
    assert(harmony_spans(plan, section, ids, 64) == 5);
    Rational bar = time_of(1, 1);
    Rational start = plan_node_span(plan, section).start;
    static const char *const NUMERALS[5] = {"I", "vi", "IV", "V", "I"};
    for (size_t i = 0; i < 5; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        Rational at;
        assert(rational_add(start, time_of((long)i, 1), &at) == RATIONAL_OK);
        assert(rational_cmp(span.start, at) == 0 && rational_cmp(span.length, bar) == 0);
        assert(plan_lane_span_state(plan, ids[i]) == PLAN_STATE_GIVEN);
        assert(strcmp(plan_lane_span_payload(plan, ids[i])->harmony.roman_numeral, NUMERALS[i]) == 0);
    }
    plan_destroy(plan);

    /* A schema: one Given span for each stage the key's chords have, each a cell long. */
    plan = new_plan(&section);
    GalantSchemaId schema;
    char chords[GALANT_MAX_STAGES][PLAN_ROMAN_NUMERAL_MAX];
    size_t stages = 0;
    assert(harmony_schema_find("prinner", &schema) && harmony_schema_chords(&key, schema, chords, &stages) && stages > 0);
    memset(&harmony, 0, sizeof(harmony));
    harmony.kind = SECTION_HARMONY_SCHEMA;
    snprintf(harmony.schema, sizeof(harmony.schema), "prinner");
    Rational cell;
    assert(given_cell_length(&meter, &cell));
    assert(piece_give_harmony(plan, section, &harmony, &meter, &key, message, sizeof(message)) == PIECE_OK);
    assert(harmony_spans(plan, section, ids, 64) == stages);
    for (size_t i = 0; i < stages; i++) {
        PlanSpan span = plan_lane_span_span(plan, ids[i]);
        assert(rational_cmp(span.length, cell) == 0);
        assert(plan_lane_span_state(plan, ids[i]) == PLAN_STATE_GIVEN);
    }
    plan_destroy(plan);

    /* A schema that is not one is refused with its name and gives nothing. */
    plan = new_plan(&section);
    snprintf(harmony.schema, sizeof(harmony.schema), "no-such-schema");
    assert(piece_give_harmony(plan, section, &harmony, &meter, &key, message, sizeof(message)) == PIECE_ERR_BAD_REQUEST);
    assert(strncmp(message, "the schema 'no-such-schema' is not one", strlen("the schema 'no-such-schema' is not one")) == 0);
    assert(harmony_spans(plan, section, ids, 64) == 0);
    /* No harmony gives nothing and is fine; NULL arguments are refused. */
    memset(&harmony, 0, sizeof(harmony));
    assert(piece_give_harmony(plan, section, &harmony, &meter, &key, message, sizeof(message)) == PIECE_OK);
    assert(harmony_spans(plan, section, ids, 64) == 0);
    assert(piece_give_harmony(NULL, section, &harmony, &meter, &key, message, sizeof(message)) == PIECE_ERR_NULL);
    assert(piece_give_harmony(plan, section, NULL, &meter, &key, message, sizeof(message)) == PIECE_ERR_NULL);
    plan_destroy(plan);
}

static void test_check_harmony(void) {
    PlanMeter meter = {4, 4, 0};
    SpelledKey key = test_key(0, "major");
    Rational four_bars = time_of(4, 1);
    SectionHarmony harmony;
    char message[256] = "";
    memset(&harmony, 0, sizeof(harmony));
    assert(piece_check_harmony(&meter, &key, four_bars, &harmony, message, sizeof(message)));
    harmony.kind = SECTION_HARMONY_PROGRESSION;
    snprintf(harmony.numerals, sizeof(harmony.numerals), "I vi IV V");
    assert(piece_check_harmony(&meter, &key, four_bars, &harmony, message, sizeof(message)));
    memset(&harmony, 0, sizeof(harmony));
    harmony.kind = SECTION_HARMONY_SCHEMA;
    snprintf(harmony.schema, sizeof(harmony.schema), "prinner");
    assert(piece_check_harmony(&meter, &key, four_bars, &harmony, message, sizeof(message)));
    /* What does not fit says why. */
    memset(&harmony, 0, sizeof(harmony));
    harmony.kind = SECTION_HARMONY_PROGRESSION;
    message[0] = '\0';
    assert(!piece_check_harmony(&meter, &key, four_bars, &harmony, message, sizeof(message)) && message[0] != '\0');
    snprintf(harmony.numerals, sizeof(harmony.numerals), "I I I I I I I I I I I I I I I I I I I I");
    message[0] = '\0';
    assert(!piece_check_harmony(&meter, &key, four_bars, &harmony, message, sizeof(message)));
    assert(strstr(message, "past the section's end") != NULL);
    memset(&harmony, 0, sizeof(harmony));
    harmony.kind = SECTION_HARMONY_SCHEMA;
    snprintf(harmony.schema, sizeof(harmony.schema), "no-such-schema");
    message[0] = '\0';
    assert(!piece_check_harmony(&meter, &key, four_bars, &harmony, message, sizeof(message)) && strstr(message, "no-such-schema") != NULL);
    memset(&harmony, 0, sizeof(harmony));
    harmony.kind = SECTION_HARMONY_CHORDS;
    harmony.chord_count = 1;
    harmony.chords[0].bar = 9;
    snprintf(harmony.chords[0].text, sizeof(harmony.chords[0].text), "I");
    message[0] = '\0';
    assert(!piece_check_harmony(&meter, &key, four_bars, &harmony, message, sizeof(message)));
    assert(strstr(message, "has bars 1 to 4") != NULL);
}

static void test_names(void) {
    static const char *const FILLS[] = {"empty", "motif", "theme"};
    PieceFillKind fill = PIECE_FILL_THEME;
    for (int i = 0; i < 3; i++) {
        assert(strcmp(piece_fill_name((PieceFillKind)i), FILLS[i]) == 0);
        assert(piece_fill_from_name(FILLS[i], &fill) && (int)fill == i);
    }
    assert(strcmp(piece_fill_name((PieceFillKind)3), "unknown") == 0);
    fill = PIECE_FILL_MOTIF;
    assert(!piece_fill_from_name("bogus", &fill) && fill == PIECE_FILL_MOTIF);
    assert(!piece_fill_from_name(NULL, &fill) && fill == PIECE_FILL_MOTIF);
    assert(piece_fill_from_name("empty", NULL));
    static const char *const PARTS[] = {"intro", "body", "turnaround", "ending"};
    for (int i = 0; i < 4; i++) {
        assert(strcmp(piece_part_name((PiecePart)i), PARTS[i]) == 0);
    }
    assert(strcmp(piece_part_name(PIECE_PART_COUNT), "unknown") == 0);
    assert(strcmp(piece_part_name((PiecePart)-1), "unknown") == 0);
}

static void test_sizes(void) {
    /* The shortest theme section of each size and the basic idea in it (1 of 4 bars, 2 of 8), as the sections of test_form_spec.c's sizes lay out. */
    static const struct {
        const char *form;
        size_t bars, theme, idea;
    } ROWS[] = {{"binary", 8, 4, 1}, {"binary", 16, 8, 2}, {"ternary", 12, 4, 1}, {"ternary", 24, 8, 2}};
    for (size_t i = 0; i < sizeof(ROWS) / sizeof(ROWS[0]); i++) {
        PieceRequest request;
        size_t theme = 0, idea = 0;
        memset(&request, 0, sizeof(request));
        request.form = ROWS[i].form;
        request.bars = ROWS[i].bars;
        assert(piece_basic_idea(&request, &theme, &idea));
        assert(theme == ROWS[i].theme && idea == ROWS[i].idea);
    }
    PieceRequest request;
    size_t theme = 0, idea = 0;
    memset(&request, 0, sizeof(request));
    request.form = "no-such-form";
    assert(!piece_basic_idea(&request, &theme, &idea));
    assert(!piece_basic_idea(NULL, &theme, &idea));
    /* A part that repeats an earlier one has it as its source; the others are their own. */
    FormSpec *form = NULL;
    assert(form_spec_builtin("minuet-and-trio", &form) == FORM_SPEC_OK && form && form->part_count == 3);
    assert(piece_parts_source(form, 0) == 0 && piece_parts_source(form, 1) == 1 && piece_parts_source(form, 2) == 0);
    form_spec_destroy(form);
}

int main(void) {
    test_regions();
    test_overlays();
    test_give_harmony();
    test_check_harmony();
    test_names();
    test_sizes();
    printf("test_piece_regions: ok\n");
    return 0;
}
