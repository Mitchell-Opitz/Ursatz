#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/provenance_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "pitch/pitch_numeric_value.h"
#include "pitch/tuning_system.h"
#include "time/duration.h"
#include "time/temporal_anchor.h"
#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/composer_api.h>
#include <ursatz_analyzer/corpus.h>
#include <ursatz_analyzer/midi_pipeline.h>
#include <ursatz_analyzer/style_profile.h>

/*
 * The API the Composer calls (composer_api.h). Bach 114 imported from its file and the same events passed as a
 * fragment give the same report; a constructed four-voice fragment with a parallel fifth gives one parallel-fifths
 * violation at its measure and beat; Mozart 545 is closer to the sonatinas model than to the chorales; the caller's
 * events are not changed. With the argument "smoke" the style models (which analyze two corpora) are skipped, so the
 * valgrind run stays short.
 */

static int failures = 0;

#define CHECK(condition, ...)                                                                                          \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            fprintf(stderr, "FAIL line %d: ", __LINE__);                                                               \
            fprintf(stderr, __VA_ARGS__);                                                                              \
            fprintf(stderr, "\n");                                                                                     \
            failures++;                                                                                                \
        }                                                                                                              \
    } while (0)

static void piece_path(char *out, size_t size, const char *file) {
    snprintf(out, size, "%s/%s", URSATZ_FIXTURE_DIR, file);
}

/* A fingerprint of the events: id, onset, duration and pitch token of each, to see they are not changed. */
static char *fingerprint(const NoteEvent *const *events, size_t count) {
    size_t capacity = count * 192 + 16;
    char *text = calloc(capacity, 1);
    size_t at = 0;
    for (size_t i = 0; text && i < count; i++) {
        const MusicalTime *onset = temporal_anchor_position(note_event_temporal_anchor(events[i]));
        const Duration *length = note_event_duration(events[i]);
        size_t token_size = 0;
        const unsigned char *token = pitch_identity_token(note_event_pitch(events[i]), &token_size);
        char line[160];
        int written = snprintf(line, sizeof line, "%s@%ld/%lu+%ld/%lu:", entity_id_value(note_event_id(events[i])),
                               musical_time_numerator(onset), musical_time_denominator(onset),
                               duration_numerator(length), duration_denominator(length));
        for (size_t k = 0; k < token_size && written > 0 && (size_t)written + 3 < sizeof line; k++) {
            written += snprintf(line + written, sizeof line - (size_t)written, "%02x", token[k]);
        }
        if (written > 0 && at + (size_t)written < capacity) {
            memcpy(text + at, line, (size_t)written);
            at += (size_t)written;
        }
    }
    return text;
}

/* ---- Bach 114 through the file and through the API ---- */

/* The document with the file's ticks per quarter note masked: a fragment has no SMF header, so it reports null. */
static char *mask_ticks(const char *json) {
    char *copy = malloc(strlen(json) + 8);
    if (copy) {
        strcpy(copy, json);
    }
    char *at = copy ? strstr(copy, "\"ticks_per_quarter\": ") : NULL;
    if (at) {
        at += strlen("\"ticks_per_quarter\": ");
        char *end = at;
        while (*end && *end != ',' && *end != '\n') {
            end++;
        }
        memmove(at + 4, end, strlen(end) + 1);
        memcpy(at, "null", 4);
    }
    return copy;
}

/* The tonic pitch class of the first key signature event of an imported file. */
static int tonic_of(const ImportedNotes *notes) {
    int fifths = notes->key_signature.sharps_flats;
    int major = (((fifths * 7) % 12) + 12) % 12;
    return notes->key_signature.is_minor ? (major + 9) % 12 : major;
}

static void test_bach_round_trip(void) {
    char path[1024];
    piece_path(path, sizeof(path), "Bach 114.mid");
    ImportedNotes notes;
    CHECK(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK, "cannot import %s", path);
    AnalysisReport direct;
    CHECK(run_analysis_pipeline(&notes, &direct), "direct analysis");
    /* A fragment has no parts: leave the file's out of the document it is compared with. */
    for (size_t i = 0; i < notes.source.part_count; i++) {
        free(notes.source.parts[i].id);
        free(notes.source.parts[i].name);
    }
    free(notes.source.parts);
    notes.source.parts = NULL;
    notes.source.part_count = 0;
    char *direct_json = analysis_document_render("<fragment>", &notes, &direct);

    char *before = fingerprint((const NoteEvent *const *)notes.events, notes.count);
    AnalyzerFragment fragment = {(const NoteEvent *const *)notes.events,
                                 notes.count,
                                 notes.meter.meter_map,
                                 notes.key_signature.found,
                                 tonic_of(&notes),
                                 notes.key_signature.is_minor,
                                 notes.streams};
    AnalyzerFragmentAnalysis analysis;
    CHECK(analyzer_analyze_fragment(&fragment, &analysis) == ANALYZER_FRAGMENT_OK, "fragment analysis");
    const AnalysisReport *report = analyzer_fragment_report(&analysis);
    CHECK(report != NULL, "report accessor");
    if (report) {
        CHECK(report->entry_count == direct.entry_count, "%zu chords against %zu", report->entry_count,
              direct.entry_count);
        for (size_t i = 0; i < report->entry_count && i < direct.entry_count; i++) {
            CHECK(strcmp(report->entries[i].chord_label, direct.entries[i].chord_label) == 0, "chord %zu differs", i);
        }
        CHECK(report->cadence_count == direct.cadence_count, "%zu cadences against %zu", report->cadence_count,
              direct.cadence_count);
        for (size_t i = 0; i < report->cadence_count && i < direct.cadence_count; i++) {
            CHECK(report->cadences[i].arrival_first == direct.cadences[i].arrival_first &&
                      strcmp(report->cadences[i].type, direct.cadences[i].type) == 0,
                  "cadence %zu differs", i);
        }
        CHECK(report->phrase_count == direct.phrase_count, "%zu phrases against %zu", report->phrase_count,
              direct.phrase_count);
        for (size_t i = 0; i < report->phrase_count && i < direct.phrase_count; i++) {
            CHECK(report->phrases[i].first_entry == direct.phrases[i].first_entry &&
                      report->phrases[i].entry_count == direct.phrases[i].entry_count,
                  "phrase %zu differs", i);
        }
    }
    const char *json = analyzer_fragment_document_json(&analysis);
    char *masked = json ? mask_ticks(json) : NULL;
    char *direct_masked = direct_json ? mask_ticks(direct_json) : NULL;
    CHECK(masked && direct_masked && strcmp(masked, direct_masked) == 0, "the document differs from the file's");
    free(masked);
    free(direct_masked);

    char *after = fingerprint((const NoteEvent *const *)notes.events, notes.count);
    CHECK(before && after && strcmp(before, after) == 0 && fragment.count == notes.count, "the events were changed");
    free(before);
    free(after);
    free(direct_json);
    analyzer_fragment_analysis_destroy(&analysis);
    analysis_report_destroy(&direct);
    imported_notes_destroy(&notes);
}

/* ---- a constructed four-voice fragment ---- */

typedef struct {
    int midi;
    long start; /* in half notes */
    int voice;
} Planned;

static NoteEvent *make_event(TuningSystem *tuning, size_t number, int midi, long start_half, long length_half) {
    char name[32];
    snprintf(name, sizeof name, "fragment-note-%zu", number);
    EntityID *id = NULL;
    MusicalTime *position = NULL;
    TemporalAnchor *anchor = NULL;
    Duration *duration = NULL;
    PitchNumericValue *value = NULL;
    PitchIdentity *pitch = NULL;
    ProvenanceReference *provenance = NULL;
    NoteEvent *event = NULL;
    if (entity_id_create(name, &id) != ENTITY_ID_OK || musical_time_create(start_half, 2, &position) != MUSICAL_TIME_OK ||
        temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK ||
        duration_create(length_half, 2, &duration) != DURATION_OK ||
        pitch_numeric_value_create(tuning, midi, 1, &value) != PITCH_NUMERIC_VALUE_OK ||
        pitch_numeric_value_to_identity(value, &pitch) != PITCH_NUMERIC_VALUE_TO_IDENTITY_OK ||
        provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK ||
        note_event_create(id, anchor, duration, pitch, NULL, NULL, provenance, &event) != NOTE_EVENT_OK) {
        return NULL;
    }
    pitch_numeric_value_destroy(value);
    return event;
}

static size_t count_rule(const AnalyzerViolation *violations, size_t count, const char *rule) {
    size_t found = 0;
    for (size_t i = 0; i < count; i++) {
        found += strcmp(violations[i].rule, rule) == 0;
    }
    return found;
}

static void test_parallel_fifth(void) {
    /* C major then D minor, a half note each, in 4/4: the bass C3 to D3 and the tenor G3 to A3 are parallel fifths;
       the soprano G5 to F5 and the alto E4 to F4 keep the other pairs out of parallels. Voices: soprano 0, alto 1,
       tenor 2, bass 3. The second chord starts on beat 3 of measure 1. */
    static const Planned plan[] = {{79, 0, 0}, {64, 0, 1}, {55, 0, 2}, {48, 0, 3},
                                   {77, 1, 0}, {65, 1, 1}, {57, 1, 2}, {50, 1, 3}};
    EntityID *tuning_id = NULL;
    TuningSystem *tuning = NULL;
    CHECK(entity_id_create("midi-note-number", &tuning_id) == ENTITY_ID_OK &&
              tuning_system_create(tuning_id, "MIDI note number", &tuning) == TUNING_SYSTEM_OK,
          "tuning system");
    NoteEvent *events[8];
    size_t streams[8];
    for (size_t i = 0; i < 8; i++) {
        events[i] = make_event(tuning, i, plan[i].midi, plan[i].start, 1);
        streams[i] = (size_t)plan[i].voice;
        CHECK(events[i] != NULL, "event %zu", i);
    }
    char *before = fingerprint((const NoteEvent *const *)events, 8);
    AnalyzerFragment fragment = {(const NoteEvent *const *)events, 8, NULL, true, 0, false, streams};
    AnalyzerFragmentAnalysis analysis;
    CHECK(analyzer_analyze_fragment(&fragment, &analysis) == ANALYZER_FRAGMENT_OK, "fragment analysis");
    AnalyzerViolation *violations = NULL;
    size_t count = 0;
    CHECK(analyzer_fragment_violations(&analysis, &violations, &count) == ANALYZER_FRAGMENT_OK, "violations");
    CHECK(count_rule(violations, count, "parallel-fifths") == 1, "%zu parallel fifths", count_rule(violations, count, "parallel-fifths"));
    CHECK(count_rule(violations, count, "parallel-octaves") == 0, "no parallel octaves");
    for (size_t i = 0; i < count; i++) {
        if (strcmp(violations[i].rule, "parallel-fifths") == 0) {
            CHECK(violations[i].hard, "a parallel fifth is a hard fault");
            CHECK(violations[i].measure == 1 && violations[i].beat == 3.0, "at measure %d beat %.2f", violations[i].measure,
                  violations[i].beat);
            CHECK(violations[i].note_id_count == 4, "four notes");
        }
    }
    analyzer_violations_free(violations, count);
    char *after = fingerprint((const NoteEvent *const *)events, 8);
    CHECK(before && after && strcmp(before, after) == 0, "the events were changed");
    free(before);
    free(after);
    analyzer_fragment_analysis_destroy(&analysis);
    for (size_t i = 0; i < 8; i++) {
        note_event_destroy(events[i]);
    }
    tuning_system_destroy(tuning);
}

/* ---- the argument checks ---- */

static void test_refusals(void) {
    AnalyzerFragmentAnalysis analysis;
    CHECK(analyzer_analyze_fragment(NULL, &analysis) == ANALYZER_FRAGMENT_ERR_ARGUMENT, "no fragment");
    AnalyzerFragment bad_key = {NULL, 0, NULL, true, 12, false, NULL};
    CHECK(analyzer_analyze_fragment(&bad_key, &analysis) == ANALYZER_FRAGMENT_ERR_ARGUMENT, "tonic out of range");
    AnalyzerFragment empty = {NULL, 0, NULL, false, 0, false, NULL};
    CHECK(analyzer_analyze_fragment(&empty, &analysis) == ANALYZER_FRAGMENT_OK, "an empty fragment");
    CHECK(analyzer_fragment_report(&analysis) && analyzer_fragment_report(&analysis)->entry_count == 0, "no chords");
    AnalyzerViolation *violations = NULL;
    size_t count = 7;
    CHECK(analyzer_fragment_violations(&analysis, &violations, &count) == ANALYZER_FRAGMENT_OK && count == 0 &&
              violations == NULL,
          "no violations");
    analyzer_fragment_analysis_destroy(&analysis);
    CHECK(analyzer_fragment_report(&analysis) == NULL && analyzer_fragment_document_json(NULL) == NULL,
          "a destroyed analysis has no report");
    analyzer_fragment_analysis_destroy(NULL);
}

/* ---- style distance ---- */

static char work_dir[512];

static const char *const motive_categories[] = {STYLE_CATEGORY_MOTIVE_DEGREES, STYLE_CATEGORY_MOTIVE_INTERVALS,
                                                STYLE_CATEGORY_MOTIVE_RHYTHM, STYLE_CATEGORY_MOTIVE_NCT_FIGURE};
#define MOTIVE_COUNT (sizeof motive_categories / sizeof *motive_categories)

/* The corpus's style model, read without the named categories (none when count is 0). */
static StyleModel *corpus_model_without(const char *name, size_t pieces, const char *const *without, size_t count) {
    char input[1024], output[1024], file[1024];
    snprintf(input, sizeof(input), "%s/%s", URSATZ_CORPORA_DIR, name);
    snprintf(output, sizeof(output), "%s/%s", work_dir, name);
    CorpusRunOptions options = {input, output, false, false, NULL, NULL};
    CorpusRunSummary summary;
    CHECK(corpus_run(&options, &summary) == CORPUS_RUN_OK && summary.analyzed == pieces && summary.failed == 0,
          "%s analyzed", name);
    StyleProfile *profile = style_profile_create();
    CHECK(profile && style_profile_add_corpus(profile, output) == STYLE_PROFILE_OK, "%s profile", name);
    snprintf(file, sizeof(file), "%s/%s.style.json", work_dir, name);
    StyleModel *model = NULL;
    CHECK(profile && style_model_file_write(profile, name, file) == STYLE_MODEL_FILE_OK &&
              style_model_file_read_without(file, without, count, &model, NULL) == STYLE_MODEL_FILE_OK,
          "%s model", name);
    style_profile_destroy(profile);
    return model;
}

static StyleModel *corpus_model(const char *name, size_t pieces) {
    return corpus_model_without(name, pieces, NULL, 0);
}

static bool same_rational(StyleRational a, StyleRational b) {
    return a.numerator == b.numerator && a.denominator == b.denominator;
}

static bool has_motive(const StyleComparison *comparison, bool *fragment_side_only) {
    bool found = false;
    *fragment_side_only = true;
    for (size_t i = 0; i < comparison->category_count; i++) {
        for (size_t m = 0; m < MOTIVE_COUNT; m++) {
            if (strcmp(comparison->categories[i].category, motive_categories[m]) == 0) {
                found = true;
                *fragment_side_only = *fragment_side_only && comparison->categories[i].in_a &&
                                      !comparison->categories[i].in_b;
            }
        }
    }
    return found;
}

/* The distance without the motive categories: the same against the full model as against the model read without
   them, and no motive category in the result. The plain call against the cut model still lists them. */
static void check_without(const AnalyzerFragmentAnalysis *analysis, const StyleModel *full, const StyleModel *cut,
                          const char *name) {
    StyleComparison against_full = {0}, against_cut = {0}, plain = {0};
    CHECK(analyzer_fragment_style_distance_without(analysis, full, motive_categories, MOTIVE_COUNT, &against_full) ==
                  ANALYZER_FRAGMENT_OK &&
              analyzer_fragment_style_distance_without(analysis, cut, motive_categories, MOTIVE_COUNT, &against_cut) ==
                  ANALYZER_FRAGMENT_OK,
          "%s without", name);
    CHECK(analyzer_fragment_style_distance(analysis, cut, &plain) == ANALYZER_FRAGMENT_OK, "%s plain", name);
    bool only = false;
    CHECK(!has_motive(&against_full, &only) && !has_motive(&against_cut, &only), "%s: motive category left in", name);
    CHECK(against_full.category_count == against_cut.category_count, "%s: category counts differ", name);
    CHECK(against_full.overall_present == against_cut.overall_present &&
              (!against_full.overall_present || same_rational(against_full.overall, against_cut.overall)),
          "%s: overall differs", name);
    for (int d = 0; d < STYLE_DIMENSION_COUNT; d++) {
        CHECK(against_full.dimension_present[d] == against_cut.dimension_present[d] &&
                  (!against_full.dimension_present[d] ||
                   same_rational(against_full.dimension[d], against_cut.dimension[d])),
              "%s: dimension %d differs", name, d);
    }
    /* Why the call exists: the plain call lists the fragment's motive categories with in_b false. */
    CHECK(has_motive(&plain, &only) && only, "%s: plain call does not list motive categories as missing", name);
    style_comparison_destroy(&against_full);
    style_comparison_destroy(&against_cut);
    style_comparison_destroy(&plain);
}

static void test_style_distance(void) {
    StyleModel *sonatinas = corpus_model("sonatinas", 2);
    StyleModel *chorales = corpus_model("chorales", 10);
    StyleModel *sonatinas_cut = corpus_model_without("sonatinas", 2, motive_categories, MOTIVE_COUNT);
    StyleModel *chorales_cut = corpus_model_without("chorales", 10, motive_categories, MOTIVE_COUNT);
    char path[1024];
    piece_path(path, sizeof(path), "Mozart 545.mid");
    ImportedNotes notes;
    CHECK(midi_pipeline_import_file(path, &notes) == MIDI_PIPELINE_OK, "cannot import %s", path);
    AnalyzerFragment fragment = {(const NoteEvent *const *)notes.events,
                                 notes.count,
                                 notes.meter.meter_map,
                                 notes.key_signature.found,
                                 tonic_of(&notes),
                                 notes.key_signature.is_minor,
                                 notes.streams};
    AnalyzerFragmentAnalysis analysis;
    CHECK(analyzer_analyze_fragment(&fragment, &analysis) == ANALYZER_FRAGMENT_OK, "fragment analysis");
    StyleComparison near_style = {0}, far_style = {0};
    if (sonatinas && chorales) {
        CHECK(analyzer_fragment_style_distance(&analysis, sonatinas, &near_style) == ANALYZER_FRAGMENT_OK, "near");
        CHECK(analyzer_fragment_style_distance(&analysis, chorales, &far_style) == ANALYZER_FRAGMENT_OK, "far");
        CHECK(near_style.overall_present && far_style.overall_present, "overall distances");
        if (near_style.overall_present && far_style.overall_present) {
            double near_distance = style_rational_value(near_style.overall);
            double far_distance = style_rational_value(far_style.overall);
            printf("Mozart 545 against sonatinas %.4f, against chorales %.4f\n", near_distance, far_distance);
            CHECK(near_distance < far_distance, "Mozart 545 is not closer to the sonatinas");
        }
    }
    style_comparison_destroy(&near_style);
    style_comparison_destroy(&far_style);
    if (sonatinas && chorales && sonatinas_cut && chorales_cut) {
        check_without(&analysis, sonatinas, sonatinas_cut, "sonatinas");
        check_without(&analysis, chorales, chorales_cut, "chorales");
        StyleComparison refused = {0};
        const char *unknown[] = {"no-such-category"};
        const char *known[] = {STYLE_CATEGORY_MOTIVE_RHYTHM};
        CHECK(analyzer_fragment_style_distance_without(&analysis, sonatinas, unknown, 1, &refused) ==
                  ANALYZER_FRAGMENT_ERR_ARGUMENT,
              "unknown category name");
        CHECK(analyzer_fragment_style_distance_without(&analysis, sonatinas, NULL, 1, &refused) ==
                  ANALYZER_FRAGMENT_ERR_ARGUMENT,
              "NULL categories with a count");
        CHECK(analyzer_fragment_style_distance_without(&analysis, sonatinas, known, 1, &refused) ==
                  ANALYZER_FRAGMENT_OK,
              "one category left out");
        style_comparison_destroy(&refused);
    }
    analyzer_fragment_analysis_destroy(&analysis);
    imported_notes_destroy(&notes);

    /* A fragment too short for most categories lists them as missing and leaves them out of the means. */
    EntityID *tuning_id = NULL;
    TuningSystem *tuning = NULL;
    CHECK(entity_id_create("midi-note-number", &tuning_id) == ENTITY_ID_OK &&
              tuning_system_create(tuning_id, "MIDI note number", &tuning) == TUNING_SYSTEM_OK,
          "tuning system");
    NoteEvent *events[2] = {make_event(tuning, 0, 60, 0, 1), make_event(tuning, 1, 62, 1, 1)};
    AnalyzerFragment tiny = {(const NoteEvent *const *)events, 2, NULL, false, 0, false, NULL};
    CHECK(analyzer_analyze_fragment(&tiny, &analysis) == ANALYZER_FRAGMENT_OK, "tiny fragment");
    StyleComparison tiny_style = {0};
    if (sonatinas) {
        CHECK(analyzer_fragment_style_distance(&analysis, sonatinas, &tiny_style) == ANALYZER_FRAGMENT_OK, "tiny distance");
        size_t missing = 0, shared = 0;
        for (size_t i = 0; i < tiny_style.category_count; i++) {
            missing += !tiny_style.categories[i].in_a && tiny_style.categories[i].in_b;
            shared += tiny_style.categories[i].in_a && tiny_style.categories[i].in_b;
        }
        CHECK(missing > 0, "categories the fragment lacks are listed");
        printf("tiny fragment: %zu categories missing, %zu shared\n", missing, shared);
        if (shared == 0) {
            CHECK(!tiny_style.overall_present, "no shared category, no overall distance");
        }
    }
    style_comparison_destroy(&tiny_style);
    analyzer_fragment_analysis_destroy(&analysis);
    note_event_destroy(events[0]);
    note_event_destroy(events[1]);
    tuning_system_destroy(tuning);
    style_model_destroy(sonatinas);
    style_model_destroy(chorales);
    style_model_destroy(sonatinas_cut);
    style_model_destroy(chorales_cut);
}

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "smoke") == 0;
    snprintf(work_dir, sizeof(work_dir), "%s", URSATZ_COMPOSER_API_WORK_DIR);
    char command[600];
    snprintf(command, sizeof(command), "mkdir -p '%s'", work_dir);
    if (system(command) != 0) {
        fprintf(stderr, "cannot create %s\n", work_dir);
        return 1;
    }
    test_bach_round_trip();
    test_parallel_fifth();
    test_refusals();
    if (!smoke) {
        test_style_distance();
    }
    if (failures > 0) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("composer api: all checks passed\n");
    return 0;
}
