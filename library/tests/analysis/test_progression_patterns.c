#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/progression_patterns.h"

/*
 * progression_patterns_find. The cases are the Analyzer's progression test cases (tests/test_progressions.c at
 * its pin) with the same expected labels and occurrences. A spec is tokens separated by spaces: "<group>:<label>"
 * for a chord in that group, "-" for an entry with no label.
 */

typedef struct {
    ProgressionEntry *entries;
    size_t count;
    char *text;
} Entries;

static Entries entries_of(const char *spec) {
    Entries e;
    e.text = strdup(spec);
    e.count = 1;
    for (const char *p = spec; *p; p++) {
        e.count += (*p == ' ') ? 1 : 0;
    }
    e.entries = calloc(e.count, sizeof(ProgressionEntry));
    size_t n = 0;
    char *save = NULL;
    for (char *token = strtok_r(e.text, " ", &save); token; token = strtok_r(NULL, " ", &save)) {
        e.entries[n].label = NULL;
        e.entries[n].group = -1;
        if (token[0] != '-') {
            char *colon = strchr(token, ':');
            e.entries[n].group = atol(token);
            e.entries[n].label = colon + 1;
        }
        n++;
    }
    return e;
}

static void entries_free(Entries *e) {
    free(e->entries);
    free(e->text);
}

static void assert_pattern(const ProgressionPattern *pattern, const char *label, size_t occurrences) {
    char *actual = progression_pattern_label(pattern, " > ");
    assert(actual != NULL && strcmp(actual, label) == 0);
    free(actual);
    assert(pattern->occurrence_count == occurrences);
}

static void assert_occurrence(const ProgressionPattern *pattern, size_t index, size_t first, size_t last) {
    assert(index < pattern->occurrence_count);
    assert(pattern->occurrences[index].first == first);
    assert(pattern->occurrences[index].last == last);
}

static ProgressionPatterns find(const Entries *e) {
    ProgressionPatterns patterns;
    assert(progression_patterns_find(e->entries, e->count, PROGRESSION_PATTERN_MIN_LENGTH,
                                     PROGRESSION_PATTERN_MAX_LENGTH, &patterns) == PROGRESSION_PATTERNS_OK);
    return patterns;
}

static void test_held_chords(void) {
    /* I I IV V V I I IV V I: held chords merge, so I IV V I occurs twice (entries 0-6 and 5-9). */
    Entries e = entries_of("0:I 0:I 0:IV 0:V 0:V 0:I 0:I 0:IV 0:V 0:I");
    ProgressionPatterns p = find(&e);
    assert(p.count == 1);
    assert_pattern(&p.items[0], "I > IV > V > I", 2);
    assert(p.items[0].length == 4);
    assert_occurrence(&p.items[0], 0, 0, 6);
    assert_occurrence(&p.items[0], 1, 5, 9);
    progression_patterns_destroy(&p);
    assert(p.items == NULL && p.count == 0);
    entries_free(&e);
}

static void test_group_break(void) {
    /* A run does not cross from group 0 into 1: I > V occurs once in each. */
    Entries e = entries_of("0:I 0:V 1:I 1:V");
    ProgressionPatterns p = find(&e);
    assert(p.count == 1);
    assert_pattern(&p.items[0], "I > V", 2);
    assert_occurrence(&p.items[0], 0, 0, 1);
    assert_occurrence(&p.items[0], 1, 2, 3);
    progression_patterns_destroy(&p);
    entries_free(&e);
}

static void test_unlabelled_break(void) {
    /* An unlabelled entry breaks a run, in the same group too. */
    Entries e = entries_of("0:I 0:V - 0:I 0:V");
    ProgressionPatterns p = find(&e);
    assert(p.count == 1);
    assert_pattern(&p.items[0], "I > V", 2);
    assert_occurrence(&p.items[0], 0, 0, 1);
    assert_occurrence(&p.items[0], 1, 3, 4);
    progression_patterns_destroy(&p);
    entries_free(&e);

    /* Group -1 breaks a run like a NULL label. */
    ProgressionEntry none[5] = {{"I", 0}, {"V", 0}, {"X", -1}, {"I", 0}, {"V", 0}};
    ProgressionPatterns q;
    assert(progression_patterns_find(none, 5, 2, 6, &q) == PROGRESSION_PATTERNS_OK);
    assert(q.count == 1);
    assert_pattern(&q.items[0], "I > V", 2);
    progression_patterns_destroy(&q);
}

static void test_containment(void) {
    /* I V I V I V: I > V occurs three times and stays; V > I, I > V > I and V > I > V occur twice, each only inside
       I > V > I > V (also twice), so only that one remains with I > V. */
    Entries e = entries_of("0:I 0:V 0:I 0:V 0:I 0:V");
    ProgressionPatterns p = find(&e);
    assert(p.count == 2);
    assert_pattern(&p.items[0], "I > V", 3);
    assert_pattern(&p.items[1], "I > V > I > V", 2);
    progression_patterns_destroy(&p);
    entries_free(&e);
}

static void test_order(void) {
    /* I > IV > V twice (I > IV, IV > V dropped as parts), vi > ii twice: equal counts rank the longer first. */
    Entries e = entries_of("0:I 0:IV 0:V 0:vi 0:ii 0:I 0:IV 0:V 0:vi 0:ii");
    ProgressionPatterns p = find(&e);
    assert(p.count >= 1);
    for (size_t i = 1; i < p.count; i++) {
        const ProgressionPattern *before = &p.items[i - 1];
        const ProgressionPattern *after = &p.items[i];
        assert(before->occurrence_count > after->occurrence_count ||
               (before->occurrence_count == after->occurrence_count && before->length >= after->length));
    }
    progression_patterns_destroy(&p);
    entries_free(&e);

    /* Most occurrences first: I > V three times, then A > B > C > D twice. */
    e = entries_of("0:I 0:V 0:A 0:B 0:C 0:D 0:I 0:V 0:A 0:B 0:C 0:D 0:I 0:V");
    p = find(&e);
    assert(p.count >= 2);
    assert_pattern(&p.items[0], "I > V", 3);
    assert(p.items[1].occurrence_count == 2);
    progression_patterns_destroy(&p);
    entries_free(&e);
}

static void test_length_limit(void) {
    /* Seven different chords twice: no pattern is longer than six chords. */
    Entries e = entries_of("0:I 0:ii 0:iii 0:IV 0:V 0:vi 0:vii 0:I 0:ii 0:iii 0:IV 0:V 0:vi 0:vii");
    ProgressionPatterns p = find(&e);
    assert(p.count > 0);
    for (size_t i = 0; i < p.count; i++) {
        assert(p.items[i].length >= 2 && p.items[i].length <= 6);
    }
    progression_patterns_destroy(&p);

    ProgressionPatterns q;
    assert(progression_patterns_find(e.entries, e.count, 3, 3, &q) == PROGRESSION_PATTERNS_OK);
    assert(q.count > 0);
    for (size_t i = 0; i < q.count; i++) {
        assert(q.items[i].length == 3);
    }
    progression_patterns_destroy(&q);
    entries_free(&e);
}

static void test_no_repeat(void) {
    Entries e = entries_of("0:I 0:IV 0:V 0:ii");
    ProgressionPatterns p = find(&e);
    assert(p.count == 0 && p.items == NULL);
    entries_free(&e);
    e = entries_of("0:I");
    p = find(&e);
    assert(p.count == 0 && p.items == NULL);
    entries_free(&e);

    ProgressionPatterns empty;
    assert(progression_patterns_find(NULL, 0, 2, 6, &empty) == PROGRESSION_PATTERNS_OK);
    assert(empty.count == 0 && empty.items == NULL);
}

static void test_invalid_and_label(void) {
    ProgressionEntry one = {"I", 0};
    ProgressionPatterns p;
    assert(progression_patterns_find(&one, 1, 2, 6, NULL) == PROGRESSION_PATTERNS_INVALID);
    assert(progression_patterns_find(NULL, 1, 2, 6, &p) == PROGRESSION_PATTERNS_INVALID);
    assert(progression_patterns_find(&one, 1, 0, 6, &p) == PROGRESSION_PATTERNS_INVALID);
    assert(progression_patterns_find(&one, 1, 4, 3, &p) == PROGRESSION_PATTERNS_INVALID);
    progression_patterns_destroy(NULL);
    assert(progression_pattern_label(NULL, " > ") == NULL);

    const char *labels[2] = {"ii", "V7"};
    ProgressionPattern pattern = {labels, 2, NULL, 0};
    char *text = progression_pattern_label(&pattern, "-");
    assert(text != NULL && strcmp(text, "ii-V7") == 0);
    free(text);
}

int main(void) {
    test_held_chords();
    test_group_break();
    test_unlabelled_break();
    test_containment();
    test_order();
    test_length_limit();
    test_no_repeat();
    test_invalid_and_label();
    return 0;
}
