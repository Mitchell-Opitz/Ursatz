#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ursatz_analyzer/analysis_document.h>
#include <ursatz_analyzer/analysis_pipeline.h>
#include <ursatz_analyzer/midi_pipeline.h>

#include "cJSON.h"
#include "pitch/pitch_numeric_value.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

/*
 * The melody of the report, for Bach 114 (Minuet in G, BWV Anh. 114).
 *
 * Structure first (no argument): every phrase with soprano notes has one melody claim, in phrase order, whose notes are
 * soprano notes in time order; its range, climax and contour agree with its notes (the climax is a note at the highest
 * pitch, the contour counts the intervals between consecutive notes, the leaps are the intervals of a third or larger,
 * every leap carries one of the library's statuses, the non-chord tones add up to the notes, the tendencies are the
 * library's five).
 *
 * Then the results Ursatz's tests/integration/test_melody_bach114.c asserts, against the same hand annotation
 * (tests/fixtures/annotations/bach114_melody.txt, a DRAFT reading of measures 1 to 8 of the 3/4 score; its phrase spans
 * are placeholders), run as `bach-114-target`: every annotated climax and range is found among the soprano notes
 * of the annotated span, and at least 90% of the annotated leaps have the annotated recovery status. It misses today, so it
 * is WILL_FAIL in CMake with the measured values: the climaxes are right, the ranges are not (the extractor adopts a
 * left-hand note where the right hand rests), and 4 of 11 leaps have the right status. The default run asserts the floors
 * of what is measured. The Analyzer reads the file's 4/4 meter and takes its own phrases, so times are compared as whole
 * notes (a measure of the 3/4 score is 3/4 of a whole note), not by measure number.
 */

#define TARGET_LEAPS 0.90
/* The floors of the default run: what the extracted soprano measured (Ursatz's whole-chain run measures the same: both
   climaxes, no range, 4 of 11 leaps), not above. The extractor adopts a left-hand note where the right hand rests, and keeps
   one soprano note per segment, which drops the quick notes the other leaps arrive on. */
#define FLOOR_CLIMAXES 2
#define FLOOR_LEAPS 4
#define TICKS_PER_WHOLE 1536.0
#define MEASURE_TICKS 1152.0
#define TIME_TOLERANCE 1e-6

static int failures = 0;

#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);          \
            failures++;                                                                            \
        }                                                                                          \
    } while (0)

static const cJSON *member(const cJSON *object, const char *key) {
    return cJSON_GetObjectItemCaseSensitive(object, key);
}

static double number(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    return cJSON_IsNumber(item) ? item->valuedouble : -1e9;
}

static const char *text(const cJSON *object, const char *key) {
    const cJSON *item = member(object, key);
    return cJSON_IsString(item) ? item->valuestring : "";
}

/* A note's onset in whole notes and its pitch as a MIDI note number. */
static double onset_of(const ImportedNotes *notes, size_t index) {
    const MusicalTime *start = temporal_anchor_position(note_event_temporal_anchor(notes->events[index]));
    return (double)musical_time_numerator(start) / (double)musical_time_denominator(start);
}

static long pitch_of(const ImportedNotes *notes, size_t index) {
    return pitch_numeric_value_position_numerator(notes->pitch_values[index]);
}

static bool same_time(double a, double b) {
    return fabs(a - b) < TIME_TOLERANCE;
}

static bool known_status(const char *status) {
    return strcmp(status, "recovered") == 0 || strcmp(status, "unrecovered") == 0 ||
           strcmp(status, "not-required") == 0 || strcmp(status, "not-judged") == 0;
}

/* --- the structure of the report and the document --- */

static void check_melody(const ImportedNotes *notes, const AnalysisReport *report, const cJSON *document) {
    const cJSON *melody = member(document, "melody");
    const cJSON *phrases = member(document, "phrases");
    CHECK(report->melody_count > 0);
    CHECK((size_t)cJSON_GetArraySize(melody) == report->melody_count);
    double previous_phrase = -1.0;
    for (size_t m = 0; m < report->melody_count; m++) {
        const AnalysisMelody *entry = &report->melodies[m];
        const cJSON *claim = cJSON_GetArrayItem(melody, (int)m);
        CHECK(claim != NULL);
        if (!claim) {
            continue;
        }
        CHECK(strcmp(text(claim, "framework"), "standard-counterpoint-v1") == 0);
        CHECK(strcmp(text(claim, "label"), "melody") == 0);
        CHECK((double)entry->phrase > previous_phrase && entry->phrase < report->phrase_count);
        CHECK((int)number(claim, "index") == (int)entry->phrase);
        CHECK(cJSON_GetArrayItem(phrases, (int)entry->phrase) != NULL);
        previous_phrase = (double)entry->phrase;
        CHECK(entry->note_id_count > 0 && entry->note_id_count == (size_t)number(claim, "note_count"));

        /* the notes: in time order, inside the phrase's span, and the pitch range they span */
        const AnalysisPhrase *phrase = &report->phrases[entry->phrase];
        double start = (double)phrase->start_numerator / (double)phrase->start_denominator;
        double end = (double)phrase->end_numerator / (double)phrase->end_denominator;
        long lowest = 1000;
        long highest = -1;
        int at_top = 0;
        int ups = 0;
        int downs = 0;
        int sames = 0;
        for (size_t n = 0; n < entry->note_id_count; n++) {
            double onset = onset_of(notes, entry->note_ids[n]);
            long pitch = pitch_of(notes, entry->note_ids[n]);
            CHECK(onset >= start - TIME_TOLERANCE && onset < end);
            CHECK(n == 0 || onset > onset_of(notes, entry->note_ids[n - 1]) - TIME_TOLERANCE);
            lowest = pitch < lowest ? pitch : lowest;
            if (pitch > highest) {
                highest = pitch;
                at_top = 0;
            }
            at_top += pitch == highest ? 1 : 0;
            if (n > 0) {
                long before = pitch_of(notes, entry->note_ids[n - 1]);
                ups += pitch > before ? 1 : 0;
                downs += pitch < before ? 1 : 0;
                sames += pitch == before ? 1 : 0;
            }
        }
        CHECK(entry->lowest_pitch == lowest && entry->highest_pitch == highest);
        CHECK(entry->range_semitones == highest - lowest);
        CHECK(pitch_of(notes, entry->highest_note) == highest && pitch_of(notes, entry->lowest_note) == lowest);
        CHECK(pitch_of(notes, entry->climax_note) == highest);
        CHECK(entry->climax_occurrences == at_top && entry->climax_unique == (at_top == 1));
        CHECK(entry->climax_relative_position >= 0.0 && entry->climax_relative_position <= 1.0);

        /* the contour counts the intervals between consecutive notes (a rest in the line would break the chain, so the
           counts can be lower than the notes' own, never higher) */
        CHECK(entry->up <= ups && entry->down <= downs && entry->same <= sames);
        CHECK(entry->up + entry->down + entry->same == (int)entry->direction_count);
        CHECK(entry->step_count + entry->leap_count <= (int)entry->direction_count);
        CHECK(strcmp(entry->net_direction, "up") == 0 || strcmp(entry->net_direction, "down") == 0 ||
              strcmp(entry->net_direction, "same") == 0);

        /* the leaps: every one of a third or larger, with one of the library's statuses */
        CHECK((int)entry->leap_total == entry->leap_count);
        for (size_t l = 0; l < entry->leap_total; l++) {
            const AnalysisMelodyLeap *leap = &entry->leaps[l];
            CHECK(known_status(leap->status));
            /* a leap is a third or larger in letter names; with no letter names or degrees, 3 semitones or more */
            CHECK(leap->generic_size >= 3 || (leap->generic_size == 0 && leap->approximate && labs(leap->semitones) >= 3));
            CHECK(leap->semitones == pitch_of(notes, leap->to_note) - pitch_of(notes, leap->from_note));
            CHECK(strcmp(leap->direction, leap->semitones > 0 ? "up" : "down") == 0);
            CHECK(leap->uncertainty >= 0.0 && leap->uncertainty <= 1.0);
        }
        const cJSON *leaps = member(claim, "leaps");
        CHECK(cJSON_GetArraySize(member(leaps, "items")) == (int)entry->leap_total);
        CHECK((int)(number(leaps, "recovered") + number(leaps, "unrecovered") + number(leaps, "not_required") +
                    number(leaps, "not_judged")) == (int)entry->leap_total);
        CHECK(cJSON_GetArraySize(member(claim, "consecutive_leaps")) == (int)entry->consecutive_leap_count);

        /* the non-chord tones */
        const cJSON *tones = member(claim, "non_chord_tones");
        CHECK(entry->chord_tone_count + entry->non_chord_tone_count + entry->unclassified_count ==
              (int)entry->note_id_count);
        CHECK(entry->nct_total == (size_t)entry->non_chord_tone_count);
        CHECK(cJSON_GetArraySize(member(tones, "items")) == entry->non_chord_tone_count);
        int by_type = 0;
        for (int t = 0; t < ANALYSIS_MELODY_NCT_TYPES; t++) {
            by_type += entry->nct_by_type[t];
        }
        CHECK(by_type == entry->non_chord_tone_count);

        /* the tendencies: the library's five, and the seventh degree judged by its rule */
        const cJSON *tendencies = member(claim, "tendencies");
        CHECK(cJSON_GetArraySize(member(tendencies, "line")) == ANALYSIS_MELODY_TENDENCIES);
        const cJSON *seventh = member(tendencies, "seventh_degree");
        CHECK(cJSON_IsObject(seventh));
        CHECK(entry->seventh_followed >= 0 && entry->seventh_broken >= 0);
        CHECK(cJSON_GetArraySize(member(seventh, "breaks")) == entry->seventh_broken);
        printf("  phrase %zu: %zu notes, %s to %s, climax %ld x%d, %d leaps (%d unrecovered), %d non-chord tones, "
               "seventh degree followed %d broken %d\n",
               entry->phrase + 1, entry->note_id_count, entry->lowest_name, entry->highest_name, highest,
               entry->climax_occurrences, entry->leap_count, (int)number(leaps, "unrecovered"),
               entry->non_chord_tone_count, entry->seventh_followed, entry->seventh_broken);
    }
}

/* --- the annotation --- */

typedef struct {
    char id[32];
    int first_measure;
    int last_measure;
    int climax_measure;
    long climax_offset;
    long climax_pitch;
    int top_count;
    long low;
    long high;
} AnnotatedSpan;

typedef struct {
    int measure;
    long offset;
    long from;
    long to;
    bool recovered;
} AnnotatedLeap;

typedef struct {
    AnnotatedSpan spans[8];
    size_t span_count;
    AnnotatedLeap leaps[64];
    size_t leap_count;
} Annotation;

static AnnotatedSpan *span_named(Annotation *a, const char *id) {
    for (size_t i = 0; i < a->span_count; i++) {
        if (strcmp(a->spans[i].id, id) == 0) {
            return &a->spans[i];
        }
    }
    return NULL;
}

static bool load_annotation(const char *path, Annotation *a) {
    FILE *file = fopen(path, "r");
    if (!file) {
        return false;
    }
    memset(a, 0, sizeof(*a));
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char kind[16];
        char id[32];
        if (line[0] == '#' || sscanf(line, "%15s", kind) != 1) {
            continue;
        }
        if (strcmp(kind, "span") == 0 && a->span_count < 8) {
            AnnotatedSpan *s = &a->spans[a->span_count++];
            if (sscanf(line, "span %31s %d %d", s->id, &s->first_measure, &s->last_measure) != 3) {
                fclose(file);
                return false;
            }
        } else if (strcmp(kind, "climax") == 0) {
            int measure;
            long offset;
            long pitch;
            AnnotatedSpan *s = sscanf(line, "climax %31s %d %ld %ld", id, &measure, &offset, &pitch) == 4
                                   ? span_named(a, id)
                                   : NULL;
            if (s) {
                s->climax_measure = measure;
                s->climax_offset = offset;
                s->climax_pitch = pitch;
            }
        } else if (strcmp(kind, "top-count") == 0) {
            int count;
            AnnotatedSpan *s = sscanf(line, "top-count %31s %d", id, &count) == 2 ? span_named(a, id) : NULL;
            if (s) {
                s->top_count = count;
            }
        } else if (strcmp(kind, "range") == 0) {
            long low;
            long high;
            AnnotatedSpan *s = sscanf(line, "range %31s %ld %ld", id, &low, &high) == 3 ? span_named(a, id) : NULL;
            if (s) {
                s->low = low;
                s->high = high;
            }
        } else if (strcmp(kind, "leap") == 0 && a->leap_count < 64) {
            AnnotatedLeap *l = &a->leaps[a->leap_count];
            char status[16];
            if (sscanf(line, "leap %d %ld %ld %ld %15s", &l->measure, &l->offset, &l->from, &l->to, status) == 5) {
                l->recovered = strcmp(status, "recovered") == 0;
                a->leap_count++;
            }
        }
    }
    fclose(file);
    return true;
}

static double tick_time(int measure, long offset) {
    return ((double)(measure - 1) * MEASURE_TICKS + (double)offset) / TICKS_PER_WHOLE;
}

/* The climaxes and ranges: among the soprano notes of the report that start in the annotated span, the highest pitch, the
   first note at it, how many are at it, and the lowest. Counts the climaxes right and the ranges right. */
static void check_spans(const ImportedNotes *notes, const AnalysisReport *report, const Annotation *annotation,
                        size_t *climaxes_right, size_t *ranges_right) {
    *climaxes_right = 0;
    *ranges_right = 0;
    for (size_t s = 0; s < annotation->span_count; s++) {
        const AnnotatedSpan *want = &annotation->spans[s];
        double start = tick_time(want->first_measure, 0);
        double end = tick_time(want->last_measure + 1, 0);
        long low = 1000;
        long high = -1;
        double high_time = 0.0;
        int top = 0;
        for (size_t m = 0; m < report->melody_count; m++) {
            for (size_t n = 0; n < report->melodies[m].note_id_count; n++) {
                size_t index = report->melodies[m].note_ids[n];
                double onset = onset_of(notes, index);
                long pitch = pitch_of(notes, index);
                if (onset < start - TIME_TOLERANCE || onset >= end - TIME_TOLERANCE) {
                    continue;
                }
                low = pitch < low ? pitch : low;
                if (pitch > high) {
                    high = pitch;
                    high_time = onset;
                    top = 0;
                }
                top += pitch == high ? 1 : 0;
            }
        }
        bool climax_ok = high == want->climax_pitch && same_time(high_time, tick_time(want->climax_measure, want->climax_offset)) &&
                         top == want->top_count;
        bool range_ok = low == want->low && high == want->high;
        printf("  %s: climax %ld x%d at %.4f (annotated %ld x%d at %.4f)%s; range %ld-%ld (annotated %ld-%ld)%s\n", want->id,
               high, top, high_time, want->climax_pitch, want->top_count,
               tick_time(want->climax_measure, want->climax_offset), climax_ok ? "" : " MISSED", low, high, want->low,
               want->high, range_ok ? "" : " MISSED");
        *climaxes_right += climax_ok ? 1 : 0;
        *ranges_right += range_ok ? 1 : 0;
    }
}

/* The leaps: the report has a leap arriving on the annotated note, between the annotated pitches, with the annotated
   status. Returns how many are right. */
static size_t check_leaps(const ImportedNotes *notes, const AnalysisReport *report, const Annotation *annotation) {
    size_t right = 0;
    for (size_t i = 0; i < annotation->leap_count; i++) {
        const AnnotatedLeap *want = &annotation->leaps[i];
        double arrival = tick_time(want->measure, want->offset);
        const char *found = NULL;
        for (size_t m = 0; m < report->melody_count; m++) {
            for (size_t l = 0; l < report->melodies[m].leap_total; l++) {
                const AnalysisMelodyLeap *leap = &report->melodies[m].leaps[l];
                if (same_time(onset_of(notes, leap->to_note), arrival) && pitch_of(notes, leap->from_note) == want->from &&
                    pitch_of(notes, leap->to_note) == want->to) {
                    found = leap->status;
                }
            }
        }
        bool ok = found && strcmp(found, want->recovered ? "recovered" : "unrecovered") == 0;
        if (!ok) {
            printf("    leap %ld->%ld at %.4f: expected %s, report has %s\n", want->from, want->to, arrival,
                   want->recovered ? "recovered" : "unrecovered", found ? found : "no such leap");
        }
        right += ok ? 1 : 0;
    }
    return right;
}

/* The annotation against the report: the floors in the default run, the target when target is set. */
static void check_annotation(const ImportedNotes *notes, const AnalysisReport *report, const char *annotation_path,
                             bool target) {
    Annotation annotation;
    CHECK(load_annotation(annotation_path, &annotation));
    CHECK(annotation.span_count == 2 && annotation.leap_count > 0);
    size_t climaxes_right = 0;
    size_t ranges_right = 0;
    check_spans(notes, report, &annotation, &climaxes_right, &ranges_right);
    size_t leaps_right = check_leaps(notes, report, &annotation);
    double rate = annotation.leap_count > 0 ? (double)leaps_right / (double)annotation.leap_count : 0.0;
    printf("  climaxes right %zu of %zu, ranges right %zu of %zu, leaps of a fourth or larger with the right status %zu of "
           "%zu (%.1f%%)\n",
           climaxes_right, annotation.span_count, ranges_right, annotation.span_count, leaps_right,
           annotation.leap_count, 100.0 * rate);
    if (target) {
        CHECK(climaxes_right == annotation.span_count && ranges_right == annotation.span_count);
        CHECK(rate >= TARGET_LEAPS);
    } else {
        CHECK(climaxes_right >= FLOOR_CLIMAXES);
        CHECK(leaps_right >= FLOOR_LEAPS);
    }
}

int main(int argc, char **argv) {
    bool target = argc > 1 && strcmp(argv[1], "bach-114-target") == 0;
    char path[1024];
    snprintf(path, sizeof(path), "%s/framework-v1-corpus/Bach 114.mid", URSATZ_FIXTURE_DIR);
    ImportedNotes notes;
    AnalysisReport report;
    if (midi_pipeline_import_file(path, &notes) != MIDI_PIPELINE_OK || !run_analysis_pipeline(&notes, &report)) {
        fprintf(stderr, "cannot analyze %s\n", path);
        return 1;
    }
    char *json = analysis_document_render(path, &notes, &report);
    cJSON *document = json ? cJSON_Parse(json) : NULL;
    free(json);
    CHECK(document != NULL);
    printf("Bach 114: %zu phrases with melody\n", report.melody_count);
    if (document && !target) {
        check_melody(&notes, &report, document);
    }
    if (document) {
        snprintf(path, sizeof(path), "%s/annotations/bach114_melody.txt", URSATZ_FIXTURE_DIR);
        check_annotation(&notes, &report, path, target);
    }
    cJSON_Delete(document);
    analysis_report_destroy(&report);
    imported_notes_destroy(&notes);
    if (failures > 0) {
        fprintf(stderr, "test_melody: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_melody: ok\n");
    return 0;
}
