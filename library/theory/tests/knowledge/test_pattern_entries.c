#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "theory/knowledge/knowledge_snapshot.h"

/*
 * The pattern knowledge of P69b B1 (R-744, R-745): the four Schoenberg accompaniment patterns, the five reference ones and the 23
 * rhythm cells, the two sus chord types with the suspended-second role, and the refusal of a user pattern whose rhythm does not
 * fill its bar or whose member is out of range.
 */

static KnowledgeSnapshot *build(void) {
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(NULL, 0, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    assert(snapshot != NULL && message[0] == '\0');
    return snapshot;
}

#define REST {PATTERN_STEP_REST, 0}
#define CHORD {PATTERN_STEP_CHORD, 0}
#define M(n) {PATTERN_STEP_MEMBER, n}
#define OCT {PATTERN_STEP_BASS_OCTAVE, 0}

static void same_steps(const PatternStep *steps, size_t count, const PatternStep *want, size_t want_count) {
    assert(count == want_count);
    for (size_t i = 0; i < count; i++) {
        assert(steps[i].kind == want[i].kind && steps[i].member == want[i].member);
    }
}

static const PatternAccompaniment *accompaniment(const KnowledgeSnapshot *snapshot, const char *id) {
    const PatternEntry *entry = knowledge_snapshot_pattern(snapshot, id);
    assert(entry != NULL && !entry->is_rhythm && strcmp(entry->id, id) == 0);
    return &entry->accompaniment;
}

static const PatternStep *meter_steps(const PatternAccompaniment *pattern, const char *meter, size_t *count) {
    for (size_t i = 0; i < pattern->meter_count; i++) {
        if (strcmp(pattern->meters[i].meter, meter) == 0) {
            *count = pattern->meters[i].step_count;
            return pattern->meters[i].steps;
        }
    }
    return NULL;
}

static void test_schoenberg(void) {
    KnowledgeSnapshot *snapshot = build();
    const PatternAccompaniment *block = accompaniment(snapshot, "schoenberg.pattern.block");
    assert(block->beat_grid && block->compound && block->meter_count == 0);
    const PatternStep block_steps[] = {CHORD};
    same_steps(block->steps, block->step_count, block_steps, 1);

    const PatternAccompaniment *arpeggiated = accompaniment(snapshot, "schoenberg.pattern.arpeggiated");
    assert(!arpeggiated->beat_grid && arpeggiated->compound);
    const PatternStep arpeggiated_steps[] = {M(0), M(1), M(2), M(1)};
    same_steps(arpeggiated->steps, arpeggiated->step_count, arpeggiated_steps, 4);

    const PatternAccompaniment *alberti = accompaniment(snapshot, "schoenberg.pattern.alberti");
    assert(!alberti->beat_grid && !alberti->compound);
    const PatternStep alberti_steps[] = {M(0), M(2), M(1), M(2)};
    same_steps(alberti->steps, alberti->step_count, alberti_steps, 4);

    const PatternAccompaniment *oom_pah = accompaniment(snapshot, "schoenberg.pattern.oom-pah");
    assert(oom_pah->beat_grid && oom_pah->compound && oom_pah->meter_count == 4);
    const PatternStep default_steps[] = {REST, CHORD};
    same_steps(oom_pah->steps, oom_pah->step_count, default_steps, 2);
    size_t count = 0;
    const PatternStep three_four[] = {REST, CHORD, CHORD};
    const PatternStep four_four[] = {REST, CHORD, REST, CHORD};
    const PatternStep two_four[] = {REST, CHORD};
    const PatternStep six_eight[] = {REST, CHORD};
    const PatternStep *steps = meter_steps(oom_pah, "3/4", &count);
    assert(steps != NULL);
    same_steps(steps, count, three_four, 3);
    steps = meter_steps(oom_pah, "4/4", &count);
    assert(steps != NULL);
    same_steps(steps, count, four_four, 4);
    steps = meter_steps(oom_pah, "2/4", &count);
    assert(steps != NULL);
    same_steps(steps, count, two_four, 2);
    steps = meter_steps(oom_pah, "6/8", &count);
    assert(steps != NULL);
    same_steps(steps, count, six_eight, 2);
    knowledge_snapshot_destroy(snapshot);
}

static void test_reference_patterns(void) {
    KnowledgeSnapshot *snapshot = build();
    size_t count = 0;
    const PatternAccompaniment *eight = accompaniment(snapshot, "michelle.pattern.arpeggio-eight");
    const PatternStep eight_steps[] = {REST, M(0), M(1), M(2), M(1), M(0), M(1), M(0)};
    assert(!eight->beat_grid && !eight->compound && eight->meter_count == 1);
    same_steps(eight->steps, eight->step_count, eight_steps, 8);
    const PatternStep eight_three[] = {REST, M(0), M(1), M(2), M(1), M(0)};
    const PatternStep *steps = meter_steps(eight, "3/4", &count);
    assert(steps != NULL);
    same_steps(steps, count, eight_three, 6);

    const PatternAccompaniment *wave = accompaniment(snapshot, "michelle.pattern.arpeggio-wave");
    const PatternStep wave_steps[] = {REST, M(0), M(1), M(2), M(1), M(0), M(1), M(2)};
    same_steps(wave->steps, wave->step_count, wave_steps, 8);
    assert(!wave->compound && meter_steps(wave, "3/4", &count) != NULL && count == 6);

    /* Planed chords (R-763): the only figure that planes, a chord on every beat of each meter. */
    const PatternAccompaniment *planed = accompaniment(snapshot, "michelle.pattern.planed-chords");
    assert(planed->planing && planed->beat_grid && !planed->compound && planed->step_count == 4 && planed->meter_count == 2);
    assert(!eight->planing && !wave->planing);
    const PatternAccompaniment *compound = accompaniment(snapshot, "michelle.pattern.compound-oom-pah");
    const PatternStep compound_steps[] = {REST, REST, CHORD, REST, REST, CHORD};
    assert(!compound->beat_grid && compound->compound);
    same_steps(compound->steps, compound->step_count, compound_steps, 6);

    const PatternAccompaniment *pulse = accompaniment(snapshot, "michelle.pattern.octave-pulse");
    const PatternStep pulse_steps[] = {REST, OCT};
    assert(pulse->compound && pulse->meter_count == 0);
    same_steps(pulse->steps, pulse->step_count, pulse_steps, 2);

    const PatternAccompaniment *ostinato = accompaniment(snapshot, "michelle.pattern.ostinato");
    const PatternStep ostinato_steps[] = {M(0), REST, M(0), M(0), M(1), REST, M(0), REST};
    assert(!ostinato->compound);
    same_steps(ostinato->steps, ostinato->step_count, ostinato_steps, 8);
    knowledge_snapshot_destroy(snapshot);
}

static void test_rhythm_cells(void) {
    KnowledgeSnapshot *snapshot = build();
    size_t rhythms = 0, accompaniments = 0, by_meter[3] = {0, 0, 0};
    for (size_t i = 0; i < knowledge_snapshot_pattern_count(snapshot); i++) {
        const PatternEntry *entry = knowledge_snapshot_pattern_at(snapshot, i);
        assert(entry != NULL && entry->id != NULL && entry->name != NULL);
        if (!entry->is_rhythm) {
            accompaniments++;
            continue;
        }
        rhythms++;
        assert(entry->rhythm.jobs != 0 && entry->rhythm.rhythm != NULL && entry->rhythm.rhythm[0] == 'x');
        by_meter[strcmp(entry->rhythm.meter, "4/4") == 0 ? 0 : strcmp(entry->rhythm.meter, "3/4") == 0 ? 1 : 2]++;
        assert(strcmp(entry->rhythm.meter, "4/4") == 0 || strcmp(entry->rhythm.meter, "3/4") == 0 || strcmp(entry->rhythm.meter, "6/8") == 0);
    }
    assert(knowledge_snapshot_pattern_count(snapshot) == 33 && accompaniments == 10 && rhythms == 23);
    assert(by_meter[0] == 12 && by_meter[1] == 6 && by_meter[2] == 5);
    /* in id order, the accessors list every one of them */
    assert(strcmp(knowledge_snapshot_pattern_at(snapshot, 0)->id, "michelle.pattern.arpeggio-eight") == 0);
    assert(strcmp(knowledge_snapshot_pattern_at(snapshot, 32)->id, "schoenberg.pattern.oom-pah") == 0);
    assert(knowledge_snapshot_pattern_at(snapshot, 33) == NULL);
    assert(knowledge_snapshot_pattern(snapshot, "omt.chord-type.sus2") == NULL && knowledge_snapshot_pattern(snapshot, "no.pattern.here") == NULL);

    const PatternEntry *groups = knowledge_snapshot_pattern(snapshot, "michelle.pattern.run-groups");
    assert(groups != NULL && groups->is_rhythm && strcmp(groups->rhythm.meter, "4/4") == 0);
    assert(strcmp(groups->rhythm.rhythm, "x:e x:e x:q x:e x:e x:q") == 0 && groups->rhythm.jobs == PATTERN_JOB_RUNNING);
    const PatternEntry *whole = knowledge_snapshot_pattern(snapshot, "michelle.pattern.end-whole");
    assert(whole != NULL && whole->rhythm.jobs == PATTERN_JOB_CADENCE && strcmp(whole->rhythm.rhythm, "x:w") == 0);
    const PatternEntry *long_first = knowledge_snapshot_pattern(snapshot, "michelle.pattern.long-first-34");
    assert(long_first != NULL && strcmp(long_first->rhythm.meter, "3/4") == 0 && long_first->rhythm.jobs == PATTERN_JOB_LONG);
    const PatternEntry *sync = knowledge_snapshot_pattern(snapshot, "michelle.pattern.sync-68");
    assert(sync != NULL && strcmp(sync->rhythm.meter, "6/8") == 0 && sync->rhythm.jobs == PATTERN_JOB_SYNCOPATED);
    knowledge_snapshot_destroy(snapshot);
}

static void test_sus_types(void) {
    KnowledgeSnapshot *snapshot = build();
    const ChordType *two = knowledge_snapshot_chord_type(snapshot, "omt.chord-type.sus2");
    assert(two != NULL && two->construction == CHORD_CONSTRUCTION_TERTIAN && two->member_count == 3);
    assert(two->members[0].role == CHORD_MEMBER_ROOT && two->members[0].semitones == 0);
    assert(two->members[1].role == CHORD_MEMBER_SUSPENDED_SECOND && two->members[1].semitones == 2);
    assert(two->members[2].role == CHORD_MEMBER_FIFTH && two->members[2].semitones == 7);
    const ChordType *four = knowledge_snapshot_chord_type(snapshot, "omt.chord-type.sus4");
    assert(four != NULL && four->construction == CHORD_CONSTRUCTION_TERTIAN && four->member_count == 3);
    assert(four->members[1].role == CHORD_MEMBER_SUSPENDED_FOURTH && four->members[1].semitones == 5);
    assert(four->members[2].role == CHORD_MEMBER_FIFTH && four->members[2].semitones == 7);
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
    refused("{\"entries\":[{\"id\":\"user.pattern.long\",\"kind\":\"pattern\",\"subtype\":\"rhythm\",\"name\":\"long\","
            "\"content\":{\"meter\":\"4/4\",\"rhythm\":\"x:q x:q x:q x:q x:q\",\"jobs\":[\"running\"]},\"status\":\"reference\",\"version\":1}]}",
            "user.pattern.long", "content.rhythm");
    refused("{\"entries\":[{\"id\":\"user.pattern.far\",\"kind\":\"pattern\",\"subtype\":\"accompaniment\",\"name\":\"far\","
            "\"content\":{\"grid\":\"eighth\",\"steps\":[{\"member\":9}]},\"status\":\"reference\",\"version\":1}]}",
            "user.pattern.far", "content.steps");
    KnowledgeUserFile good = {"mine.json",
        "{\"entries\":[{\"id\":\"user.pattern.fine\",\"kind\":\"pattern\",\"subtype\":\"rhythm\",\"name\":\"fine\","
        "\"content\":{\"meter\":\"3/4\",\"rhythm\":\"x:q. x:e r:q\",\"jobs\":[\"long\",\"cadence\"]},\"status\":\"reference\",\"version\":1}]}"};
    KnowledgeSnapshot *snapshot = NULL;
    char message[256];
    assert(knowledge_snapshot_build(&good, 1, &snapshot, message, sizeof(message)) == KNOWLEDGE_OK);
    const PatternEntry *fine = knowledge_snapshot_pattern(snapshot, "user.pattern.fine");
    assert(fine != NULL && fine->is_rhythm && fine->rhythm.jobs == (PATTERN_JOB_LONG | PATTERN_JOB_CADENCE));
    assert(knowledge_snapshot_pattern_count(snapshot) == 34);
    knowledge_snapshot_destroy(snapshot);
}

int main(void) {
    test_schoenberg();
    test_reference_patterns();
    test_rhythm_cells();
    test_sus_types();
    test_user_files();
    puts("test_pattern_entries: ok");
    return 0;
}
