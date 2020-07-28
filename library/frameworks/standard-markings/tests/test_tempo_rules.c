#include <assert.h>
#include <stdio.h>

#include "framework.h"
#include "tempo_rules.h"

static int rank_of(const char *word) {
    int rank = -1;
    assert(standard_markings_tempo_word_rank(word, &rank));
    return rank;
}

static void test_rules_reference_framework(void) {
    for (int i = 0; i < STANDARD_MARKINGS_TEMPO_RULE_COUNT; i++) {
        const Rule *rule = standard_markings_tempo_rule((StandardMarkingsTempoRule)i);
        const Constraint *constraint = standard_markings_tempo_constraint((StandardMarkingsTempoRule)i);
        assert(rule != NULL && constraint != NULL);
        assert(framework_identity_equals(rule_framework(rule), standard_markings_framework()));
        assert(constraint_type(constraint) == CONSTRAINT_TYPE_HARD);
    }
    assert(standard_markings_tempo_rule(STANDARD_MARKINGS_TEMPO_RULE_COUNT) == NULL);
}

static void test_groups(void) {
    const char *fast[] = {"vivace", "presto", "allegro", "allegretto"};
    const char *medium[] = {"moderato", "andante"};
    const char *slow[] = {"adagio", "largo", "lento", "grave"};
    for (int i = 0; i < 4; i++) {
        assert(standard_markings_tempo_group(fast[i]) == TEMPO_GROUP_FAST);
        assert(standard_markings_tempo_group(slow[i]) == TEMPO_GROUP_SLOW);
    }
    for (int i = 0; i < 2; i++) {
        assert(standard_markings_tempo_group(medium[i]) == TEMPO_GROUP_MEDIUM);
    }
    assert(standard_markings_tempo_group("furioso") == TEMPO_GROUP_UNKNOWN);
    assert(standard_markings_tempo_group(NULL) == TEMPO_GROUP_UNKNOWN);
}

/* Slowest to fastest, every adjacent pair and the full order. */
static void test_order(void) {
    const char *order[] = {"grave", "largo", "lento", "adagio",     "andante",
                            "moderato", "allegretto", "allegro", "vivace", "presto"};
    for (int i = 0; i < STANDARD_MARKINGS_TEMPO_WORD_COUNT; i++) {
        assert(rank_of(order[i]) == i);
        for (int j = 0; j < STANDARD_MARKINGS_TEMPO_WORD_COUNT; j++) {
            assert(standard_markings_tempo_is_slower(order[i], order[j]) == (i < j));
        }
    }
    /* Slow words rank below medium, medium below fast. */
    assert(standard_markings_tempo_is_slower("adagio", "andante"));
    assert(standard_markings_tempo_is_slower("moderato", "allegretto"));
    assert(rank_of("ALLEGRO") == rank_of("allegro"));
    assert(!standard_markings_tempo_is_slower("furioso", "presto"));
    assert(!standard_markings_tempo_is_slower("grave", NULL));
}

/* -etto means "little": allegretto is a little allegro, slightly slower. */
static void test_etto_suffix(void) {
    assert(standard_markings_tempo_is_slower("allegretto", "allegro"));
}

static void test_modifiers(void) {
    assert(standard_markings_is_tempo_modifier("assai"));
    assert(standard_markings_is_tempo_modifier("espressivo"));
    assert(standard_markings_is_tempo_modifier("cantabile"));
    assert(!standard_markings_is_tempo_modifier("allegro"));
    assert(!standard_markings_is_tempo_modifier(NULL));

    int rank = -1;
    assert(standard_markings_tempo_text_rank("allegro assai", &rank) && rank == rank_of("allegro"));
    assert(standard_markings_tempo_text_rank("andante cantabile", &rank) && rank == rank_of("andante"));
    assert(standard_markings_tempo_text_rank("adagio espressivo", &rank) && rank == rank_of("adagio"));
    assert(standard_markings_tempo_text_rank("allegro", &rank) && rank == rank_of("allegro"));
    assert(standard_markings_tempo_text_rank("Allegro  Assai", &rank) && rank == rank_of("allegro"));

    rank = 42;
    assert(!standard_markings_tempo_text_rank("assai", &rank));           /* modifier alone */
    assert(!standard_markings_tempo_text_rank("allegro furioso", &rank)); /* unknown modifier */
    assert(!standard_markings_tempo_text_rank("allegro andante", &rank)); /* second tempo word */
    assert(!standard_markings_tempo_text_rank("", &rank));
    assert(!standard_markings_tempo_text_rank(NULL, &rank));
    assert(rank == 42);
}

/* Quarter = 60 is sixty quarters a minute: one per second. */
static void test_metronome(void) {
    Duration *quarter = NULL;
    assert(duration_create(1, 4, &quarter) == DURATION_OK);
    MetronomeMarking *sixty = NULL, *hundred_twenty = NULL;
    assert(metronome_marking_create(quarter, 60, 1, &sixty) == METRONOME_MARKING_OK);
    assert(metronome_marking_create(quarter, 120, 1, &hundred_twenty) == METRONOME_MARKING_OK);

    unsigned long n, d;
    standard_markings_metronome_seconds_per_beat(sixty, &n, &d);
    assert(n == 1 && d == 1);
    standard_markings_metronome_seconds_per_beat(hundred_twenty, &n, &d);
    assert(n == 1 && d == 2);

    duration_destroy(quarter);
    metronome_marking_destroy(sixty);
    metronome_marking_destroy(hundred_twenty);
}

static void test_tempo_change(void) {
    assert(standard_markings_tempo_change("rit.") == TEMPO_CHANGE_GRADUALLY_SLOWER);
    assert(standard_markings_tempo_change("ritardando") == TEMPO_CHANGE_GRADUALLY_SLOWER);
    assert(standard_markings_tempo_change("rit. . . .") == TEMPO_CHANGE_GRADUALLY_SLOWER);
    assert(standard_markings_tempo_change("accel.") == TEMPO_CHANGE_GRADUALLY_FASTER);
    assert(standard_markings_tempo_change("Accelerando") == TEMPO_CHANGE_GRADUALLY_FASTER);
    assert(standard_markings_tempo_change("a tempo") == TEMPO_CHANGE_RETURN);
    assert(standard_markings_tempo_change("A Tempo") == TEMPO_CHANGE_RETURN);

    assert(standard_markings_tempo_change("allegro") == TEMPO_CHANGE_NONE);
    assert(standard_markings_tempo_change("") == TEMPO_CHANGE_NONE);
    assert(standard_markings_tempo_change(NULL) == TEMPO_CHANGE_NONE);
}

/* "a tempo" returns to the tempo in force before the rit./accel. */
static void test_a_tempo_source(void) {
    const char *texts[] = {"allegro", "rit.", "a tempo", "andante", "accel.", "a tempo"};
    size_t expected[] = {0, 0, 0, 3, 3, 3};
    for (size_t i = 0; i < 6; i++) {
        size_t source = 99;
        assert(standard_markings_tempo_source_index(texts, 6, i, &source));
        assert(source == expected[i]);
    }

    /* A rit. with nothing before it has no source. */
    const char *no_setting[] = {"rit.", "a tempo"};
    size_t source = 99;
    assert(!standard_markings_tempo_source_index(no_setting, 2, 0, &source));
    assert(!standard_markings_tempo_source_index(no_setting, 2, 1, &source));
    assert(source == 99);

    /* Out of range and NULL entries. */
    assert(!standard_markings_tempo_source_index(texts, 6, 6, &source));
    assert(!standard_markings_tempo_source_index(NULL, 0, 0, &source));
    const char *with_null[] = {"allegro", NULL, "rit."};
    assert(!standard_markings_tempo_source_index(with_null, 3, 1, &source));
    assert(standard_markings_tempo_source_index(with_null, 3, 2, &source) && source == 0);
}

int main(void) {
    test_rules_reference_framework();
    test_groups();
    test_order();
    test_etto_suffix();
    test_modifiers();
    test_metronome();
    test_tempo_change();
    test_a_tempo_source();
    printf("test_tempo_rules (standard-markings): ok\n");
    return 0;
}
