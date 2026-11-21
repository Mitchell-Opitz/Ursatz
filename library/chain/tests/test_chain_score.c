#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "analysis/notes/analysis_note_build.h"
#include "chain/analysis_chain.h"
#include "io/imported_score.h"
#include "io/midi_renderer.h"
#include "io/tests/io/midi_render_test_util.h"
#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/pitch_spelling.h"
#include "test_knowledge.h"
#include "ursatz_test.h"

/*
 * The chain run on an imported score (R-615): for Bach 114 and Mozart 545 the collected result's counts equal what the
 * Analyzer's report held at 069e65c (measured with the Analyzer before the report moved into the chain); analysis_chain_run
 * collects nothing, a partial stage set collects nothing, and a score with no notes gives an empty result. A rendered
 * fragment heard through imported_score_view gives the result its events give when imported from a MIDI file written from them,
 * and its voice faults (chain_result_voice_faults, R-618) are found, hard, at their measure and beat.
 */

typedef struct {
    const char *file;
    size_t entries;
    size_t cadences;
    size_t key_regions;
    size_t phrases;
    size_t themes;
    size_t motives;
    size_t sections;
    size_t densities;
} Counts;

static const Counts EXPECTED[] = {
    {"Bach 114.mid", 54, 0, 1, 2, 0, 36, 1, 8},
    {"Mozart 545.mid", 210, 16, 12, 16, 10, 116, 10, 64},
};

static AnalysisChainConfig config(void) {
    AnalysisChainConfig c = analysis_chain_config_default();
    c.knowledge = test_knowledge();
    c.stages = ANALYSIS_CHAIN_STAGE_ALL;
    c.refine_keys = true;
    return c;
}

static void import(const char *file, ImportedScore *score) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, file);
    ImportedScoreError error = {{0}};
    assert(imported_score_import_file(path, IMPORTED_FORMAT_AUTO, score, &error) == IMPORTED_SCORE_OK);
}

static void test_counts_equal_the_analyzers(void) {
    for (size_t i = 0; i < sizeof EXPECTED / sizeof EXPECTED[0]; i++) {
        ImportedScore score;
        import(EXPECTED[i].file, &score);
        AnalysisChainConfig c = config();
        AnalysisChain *chain = NULL;
        assert(analysis_chain_run_score(&score, &c, &chain) == ANALYSIS_CHAIN_OK);
        const ChainResult *result = analysis_chain_result(chain);
        assert(analysis_chain_score(chain) == &score);
        const Counts *want = &EXPECTED[i];
        assert(result->entry_count == want->entries && result->cadence_count == want->cadences);
        assert(result->key_region_count == want->key_regions && result->phrase_count == want->phrases);
        assert(result->theme_count == want->themes && result->motive_count == want->motives);
        assert(result->section_count == want->sections && result->density_count == want->densities);
        analysis_chain_destroy(chain);
        imported_score_destroy(&score);
    }
}

/* analysis_chain_run collects nothing and keeps no score; a run of fewer than every chain stage collects nothing. */
static void test_only_a_full_run_collects(void) {
    ImportedScore score;
    import("Field 1.mid", &score);

    AnalysisNoteSource source = {.events = (const NoteEvent *const *)score.events,
                                 .pitch_values = score.pitch_values,
                                 .tracks = score.streams,
                                 .count = score.count,
                                 .use_spelling = score.source.spelling_notated,
                                 .map = score.meter.meter_map};
    AnalysisNoteSet set;
    size_t *order = NULL;
    assert(analysis_note_set_build(&source, &set, &order));
    AnalysisChainConfig c = config();
    AnalysisChain *plain = NULL;
    assert(analysis_chain_run(&set, NULL, 0, &c, &plain) == ANALYSIS_CHAIN_OK);
    assert(analysis_chain_chords(plain) != NULL);
    assert(analysis_chain_result(plain) != NULL && analysis_chain_result(plain)->entry_count == 0);
    assert(analysis_chain_score(plain) == NULL);
    analysis_chain_destroy(plain);
    analysis_note_set_built_destroy(&set, order);

    c.stages = ANALYSIS_CHAIN_STAGE_SEGMENTS | ANALYSIS_CHAIN_STAGE_KEYS | ANALYSIS_CHAIN_STAGE_CHORDS;
    AnalysisChain *partial = NULL;
    assert(analysis_chain_run_score(&score, &c, &partial) == ANALYSIS_CHAIN_OK);
    assert(analysis_chain_chords(partial) != NULL);
    assert(analysis_chain_result(partial)->entry_count == 0 && analysis_chain_result(partial)->key_region_count == 0);
    assert(analysis_chain_score(partial) == &score);
    analysis_chain_destroy(partial);

    /* Every chain stage without the result-only stages: the result is collected without them. The meter stage is the chain's
       own, so its results (the densities) are there; the progressions and the form plan are not. */
    ImportedScore bach;
    import("Bach 114.mid", &bach);
    c.stages = ANALYSIS_CHAIN_STAGE_PERCEPTION;
    AnalysisChain *perception = NULL;
    assert(analysis_chain_run_score(&bach, &c, &perception) == ANALYSIS_CHAIN_OK);
    assert(analysis_chain_result(perception)->entry_count == 54 && analysis_chain_result(perception)->density_count == 8);
    assert(analysis_chain_result(perception)->progression_count == 0 && analysis_chain_result(perception)->section_count == 0);
    analysis_chain_destroy(perception);
    imported_score_destroy(&bach);

    /* No knowledge, no perception stage. */
    AnalysisChain *refused = NULL;
    AnalysisChainConfig none = analysis_chain_config_default();
    assert(analysis_chain_run_score(&score, &none, &refused) == ANALYSIS_CHAIN_ERR_KNOWLEDGE && refused == NULL);
    c.stages = ANALYSIS_CHAIN_STAGE_PROGRESSIONS;
    assert(analysis_chain_run_score(&score, &c, &refused) == ANALYSIS_CHAIN_ERR_BAD_CONFIG && refused == NULL);
    assert(analysis_chain_run_score(NULL, &c, &refused) == ANALYSIS_CHAIN_ERR_NULL_ARGUMENT);
    imported_score_destroy(&score);
}

/* A score with no notes: an empty result. */
static void test_a_score_with_no_notes(void) {
    ImportedScoreView view = {0};
    ImportedScore score;
    assert(imported_score_view(&view, &score) == IMPORTED_SCORE_OK && score.count == 0);
    AnalysisChainConfig c = config();
    c.stages = ANALYSIS_CHAIN_STAGE_ALL;
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run_score(&score, &c, &chain) == ANALYSIS_CHAIN_OK);
    const ChainResult *result = analysis_chain_result(chain);
    assert(result->entry_count == 0 && result->key_region_count == 0 && result->home_duration.den == 1);
    assert(analysis_chain_segments(chain) == NULL && analysis_chain_segment_count(chain) == 0);
    analysis_chain_destroy(chain);
    imported_score_destroy(&score);
    /* A NULL chain reads as an empty result too. */
    assert(analysis_chain_result(NULL) != NULL && analysis_chain_result(NULL)->entry_count == 0);
}


/* ---- a constructed four-voice fragment (R-618): the Composer's way in ---- */

typedef struct {
    int midi;
    long start; /* in half notes */
    int voice;
} Planned;

/* C major then D minor, a half note each, in 4/4: the bass C3 to D3 and the tenor G3 to A3 are parallel fifths; the soprano G5 to F5
   and the alto E4 to F4 keep the other pairs out of parallels. Voices: soprano 0, alto 1, tenor 2, bass 3. The second chord starts on
   beat 3 of measure 1. */
static const Planned FRAGMENT[] = {{79, 0, 0}, {64, 0, 1}, {55, 0, 2}, {48, 0, 3},
                                   {77, 1, 0}, {65, 1, 1}, {57, 1, 2}, {50, 1, 3}};
#define FRAGMENT_COUNT (sizeof FRAGMENT / sizeof FRAGMENT[0])

static const char *const VOICES[] = {"soprano", "alto", "tenor", "bass"};

/* The fragment's events, spelled as the MIDI importer spells a note (sharps only: the chain reads the events' spellings), each in
   its voice, with the pitch values the renderer pairs them with. */
typedef struct {
    NoteEvent *events[FRAGMENT_COUNT];
    PitchNumericValue *values[FRAGMENT_COUNT];
    TuningSystem *tuning;
    int midi[FRAGMENT_COUNT];
    size_t streams[FRAGMENT_COUNT];
} Fragment;

static void build_fragment(Fragment *fragment) {
    memset(fragment, 0, sizeof(*fragment));
    EntityID *tuning_id = NULL;
    assert(entity_id_create("midi-note-number", &tuning_id) == ENTITY_ID_OK);
    assert(tuning_system_create(tuning_id, "MIDI note number", &fragment->tuning) == TUNING_SYSTEM_OK);
    for (size_t i = 0; i < FRAGMENT_COUNT; i++) {
        char name[32];
        snprintf(name, sizeof name, "fragment-note-%zu", i);
        int midi = FRAGMENT[i].midi;
        int fifths = (midi % PITCH_SEMITONES_PER_OCTAVE) * 7 % PITCH_SEMITONES_PER_OCTAVE;
        PitchLetter letter;
        int accidental;
        pitch_spelling_from_fifths(fifths == 11 ? -1 : fifths, &letter, &accidental);
        EntityID *id = NULL, *voice_id = NULL;
        MusicalTime position;
        Duration duration;
        PitchIdentity *pitch = NULL;
        PitchSpelling *spelling = NULL;
        VoiceReference *voice = NULL;
        ProvenanceReference *provenance = NULL;
        assert(entity_id_create(name, &id) == ENTITY_ID_OK && musical_time_make(FRAGMENT[i].start, 2, &position) == MUSICAL_TIME_OK &&
               duration_make(1, 2, &duration) == DURATION_OK &&
               pitch_numeric_value_create(fragment->tuning, midi, 1, &fragment->values[i]) == PITCH_NUMERIC_VALUE_OK &&
               pitch_numeric_value_to_identity(fragment->values[i], &pitch) == PITCH_NUMERIC_VALUE_TO_IDENTITY_OK &&
               pitch_spelling_create(letter, accidental, midi / PITCH_SEMITONES_PER_OCTAVE - 1, &spelling) == PITCH_SPELLING_OK &&
               entity_id_create(VOICES[FRAGMENT[i].voice], &voice_id) == ENTITY_ID_OK &&
               voice_reference_create(voice_id, &voice) == VOICE_REFERENCE_OK &&
               provenance_reference_create_absent(&provenance) == PROVENANCE_REFERENCE_OK &&
               note_event_create(id, position, duration, pitch, spelling, voice, provenance, &fragment->events[i]) == NOTE_EVENT_OK);
        fragment->midi[i] = midi;
        fragment->streams[i] = (size_t)FRAGMENT[i].voice;
    }
}

static void fragment_free(Fragment *fragment) {
    for (size_t i = 0; i < FRAGMENT_COUNT; i++) {
        note_event_destroy(fragment->events[i]);
        pitch_numeric_value_destroy(fragment->values[i]);
    }
    tuning_system_destroy(fragment->tuning);
}

/* How many of the faults are of the rule. */
static size_t count_rule(const ChainVoiceFault *faults, size_t total, const char *rule) {
    size_t found = 0;
    for (size_t i = 0; i < total; i++) {
        found += strcmp(faults[i].rule, rule) == 0;
    }
    return found;
}

/* The events a Composer hears, borrowed into a view, give what a MIDI file written from them gives. */
static void test_view_equals_import(void) {
    Fragment fragment;
    build_fragment(&fragment);

    unsigned char *data = NULL;
    size_t size = 0;
    assert(midi_renderer_render_with_options(fragment.events, fragment.values, FRAGMENT_COUNT, NULL, NULL, NULL, 0, NULL, 480,
                                             &data, &size) == MIDI_RENDER_OK);
    ImportedScore imported;
    ImportedScoreError error = {{0}};
    assert(imported_score_import_buffer(data, size, IMPORTED_FORMAT_MIDI, &imported, &error) == IMPORTED_SCORE_OK);
    free(data);
    assert(imported.count == FRAGMENT_COUNT);

    ImportedScoreView view = {.events = (const NoteEvent *const *)fragment.events,
                              .count = FRAGMENT_COUNT,
                              .midi = fragment.midi,
                              .streams = fragment.streams};
    ImportedScore viewed;
    assert(imported_score_view(&view, &viewed) == IMPORTED_SCORE_OK && viewed.events_borrowed);

    AnalysisChainConfig c = config();
    AnalysisChain *from_file = NULL;
    AnalysisChain *from_view = NULL;
    assert(analysis_chain_run_score(&imported, &c, &from_file) == ANALYSIS_CHAIN_OK);
    assert(analysis_chain_run_score(&viewed, &c, &from_view) == ANALYSIS_CHAIN_OK);
    const ChainResult *a = analysis_chain_result(from_file);
    const ChainResult *b = analysis_chain_result(from_view);
    assert(a->entry_count > 0 && a->entry_count == b->entry_count);
    assert(a->cadence_count == b->cadence_count && a->key_region_count == b->key_region_count);
    assert(a->voice_leading.violation_count == b->voice_leading.violation_count);
    for (size_t i = 0; i < a->entry_count; i++) {
        assert(rational_equals(a->entries[i].time, b->entries[i].time));
    }

    ChainVoiceFault *file_faults = NULL, *view_faults = NULL;
    size_t file_count = 0, view_count = 0;
    assert(chain_result_voice_faults(from_file, &file_faults, &file_count));
    assert(chain_result_voice_faults(from_view, &view_faults, &view_count));
    assert(file_count == view_count);
    for (size_t i = 0; i < file_count; i++) {
        assert(strcmp(file_faults[i].rule, view_faults[i].rule) == 0 && file_faults[i].hard == view_faults[i].hard);
        assert(file_faults[i].measure == view_faults[i].measure && file_faults[i].beat == view_faults[i].beat);
    }
    chain_voice_faults_free(file_faults, file_count);
    chain_voice_faults_free(view_faults, view_count);

    analysis_chain_destroy(from_file);
    analysis_chain_destroy(from_view);
    imported_score_destroy(&imported);
    imported_score_destroy(&viewed);
    fragment_free(&fragment);
}

/* A parallel fifth is a hard fault at the measure and beat where it arrives. */
static void test_voice_faults(void) {
    Fragment fragment;
    build_fragment(&fragment);
    ImportedScoreView view = {.events = (const NoteEvent *const *)fragment.events,
                              .count = FRAGMENT_COUNT,
                              .midi = fragment.midi,
                              .streams = fragment.streams,
                              .has_key = true,
                              .key_fifths = 0,
                              .key_minor = false};
    ImportedScore score;
    assert(imported_score_view(&view, &score) == IMPORTED_SCORE_OK);
    AnalysisChainConfig c = config();
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run_score(&score, &c, &chain) == ANALYSIS_CHAIN_OK);

    ChainVoiceFault *faults = NULL;
    size_t total = 0;
    assert(chain_result_voice_faults(chain, &faults, &total));
    assert(count_rule(faults, total, "parallel-fifths") == 1 && count_rule(faults, total, "parallel-octaves") == 0);
    for (size_t i = 0; i < total; i++) {
        if (strcmp(faults[i].rule, "parallel-fifths") == 0) {
            assert(faults[i].hard && faults[i].measure == 1 && faults[i].beat == 3.0 && faults[i].note_id_count == 4);
            for (size_t n = 0; n < faults[i].note_id_count; n++) {
                assert(faults[i].note_ids[n] < FRAGMENT_COUNT);
            }
        }
    }
    chain_voice_faults_free(faults, total);

    /* No faults is a NULL array; a chain that ran on no score, or no chain, is refused. */
    AnalysisChain *plain = NULL;
    AnalysisNoteSource source = {.events = (const NoteEvent *const *)score.events,
                                 .pitch_values = score.pitch_values,
                                 .tracks = score.streams,
                                 .count = score.count,
                                 .use_spelling = false,
                                 .map = score.meter.meter_map};
    AnalysisNoteSet set;
    size_t *order = NULL;
    assert(analysis_note_set_build(&source, &set, &order));
    assert(analysis_chain_run(&set, NULL, 0, &c, &plain) == ANALYSIS_CHAIN_OK);
    faults = (ChainVoiceFault *)1;
    total = 7;
    assert(!chain_result_voice_faults(plain, &faults, &total) && faults == NULL && total == 0);
    assert(!chain_result_voice_faults(NULL, &faults, &total));
    assert(!chain_result_voice_faults(chain, NULL, &total) && !chain_result_voice_faults(chain, &faults, NULL));
    analysis_chain_destroy(plain);
    analysis_note_set_built_destroy(&set, order);
    chain_voice_faults_free(NULL, 0);

    analysis_chain_destroy(chain);
    imported_score_destroy(&score);
    fragment_free(&fragment);
}

int main(void) {
    test_counts_equal_the_analyzers();
    test_only_a_full_run_collects();
    test_a_score_with_no_notes();
    test_view_equals_import();
    test_voice_faults();
    test_knowledge_release();
    return 0;
}
