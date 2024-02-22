#include "analysis_melody_climax.h"
#include "analysis_melody_leaps.h"
#include "analysis_melody_nct.h"
#include "melody_util.h"
#include "time/meter_map.h"

/*
 * Melody analysis of Bach 114 (Minuet in G, BWV Anh. 114), measures 1-8: the soprano the outer-voice extractor produces from
 * the corpus MIDI file (read in the score's 3/4, as the other Bach 114 tests do), with the L2.6 scale degrees and the L2.5
 * non-chord-tone results attached (tests/integration/melody_util.h), is run through standard-counterpoint's melody adapter
 * over the phrase spans of tests/fixtures/annotations/bach114_melody.txt (placeholders: measures 1-4 and 5-8, until phrase
 * segmentation exists).
 *
 * Asserted: the climax of each span (its measure, tick offset and pitch, and how many notes sound at the top), the range of
 * each span, and that at least 90% of the annotated leaps of a fourth or larger are found with the right recovery status.
 * Reported: every annotated leap's result, the leaps the adapter finds that are not annotated, the contour summary and the
 * non-chord-tone counts of each span. The fixture is a DRAFT hand annotation (see its header) and nothing is tuned to it.
 */

#define MEASURE_TICKS 1152L

static MeterMap *three_four_map(void) {
    NoteValue *quarter = NULL;
    assert(note_value_create(2, &quarter) == NOTE_VALUE_OK);
    TimeSignature *three_four = NULL;
    assert(time_signature_create(3, quarter, &three_four) == TIME_SIGNATURE_OK);
    note_value_destroy(quarter);
    MusicalTime *zero = NULL;
    assert(musical_time_create(0, 1, &zero) == MUSICAL_TIME_OK);
    MeterMapChangePoint point = {three_four, zero};
    EntityID *id = NULL;
    assert(entity_id_create("bach-114-score-meter", &id) == ENTITY_ID_OK);
    MeterMap *map = NULL;
    assert(meter_map_create(id, &point, 1, &map) == METER_MAP_OK);
    time_signature_destroy(three_four);
    musical_time_destroy(zero);
    return map;
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
    assert(!"unknown span in the annotation");
    return NULL;
}

static void load_annotation(Annotation *a) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/bach114_melody.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    memset(a, 0, sizeof(*a));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char kind[16];
        char id[32];
        if (sscanf(line, "%15s", kind) != 1) {
            continue;
        }
        if (strcmp(kind, "span") == 0) {
            AnnotatedSpan *s = &a->spans[a->span_count++];
            assert(sscanf(line, "span %31s %d %d", s->id, &s->first_measure, &s->last_measure) == 3);
        } else if (strcmp(kind, "climax") == 0) {
            int measure;
            long offset;
            long pitch;
            assert(sscanf(line, "climax %31s %d %ld %ld", id, &measure, &offset, &pitch) == 4);
            AnnotatedSpan *s = span_named(a, id);
            s->climax_measure = measure;
            s->climax_offset = offset;
            s->climax_pitch = pitch;
        } else if (strcmp(kind, "top-count") == 0) {
            int count;
            assert(sscanf(line, "top-count %31s %d", id, &count) == 2);
            span_named(a, id)->top_count = count;
        } else if (strcmp(kind, "range") == 0) {
            long low;
            long high;
            assert(sscanf(line, "range %31s %ld %ld", id, &low, &high) == 3);
            span_named(a, id)->low = low;
            span_named(a, id)->high = high;
        } else if (strcmp(kind, "leap") == 0) {
            AnnotatedLeap *l = &a->leaps[a->leap_count++];
            char status[16];
            assert(sscanf(line, "leap %d %ld %ld %ld %15s", &l->measure, &l->offset, &l->from, &l->to, status) == 5);
            assert(strcmp(status, "recovered") == 0 || strcmp(status, "unrecovered") == 0);
            l->recovered = strcmp(status, "recovered") == 0;
        }
    }
    free(text);
}

static long pitch_of(const MelodyBuild *build, int entry) {
    return pitch_numeric_value_position_numerator(build->line.notes[entry].pitch);
}

/* The entry where a span's measures begin and end (first and last entry whose start falls in them). */
static bool span_entries(const MelodyBuild *build, const AnnotatedSpan *span, StandardCounterpointMelodySpan *out) {
    bool found = false;
    for (size_t i = 0; i < build->line.count; i++) {
        int measure = (int)(build->tick[i] / MEASURE_TICKS) + 1;
        if (measure >= span->first_measure && measure <= span->last_measure) {
            if (!found) {
                out->first = i;
                found = true;
            }
            out->last = i;
        }
    }
    out->id = span->id;
    return found;
}

static void test_bach_114(void) {
    Annotation annotation;
    load_annotation(&annotation);
    assert(annotation.span_count == 2 && annotation.leap_count > 0);

    MeterMap *map = three_four_map();
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, "Bach 114.mid", map);
    OuterVoiceLines lines = {0};
    assert(outer_voice_extract(&p.sonorities, p.events, p.pitch_values, p.note_count, map,
                               standard_meter_segmentation_beat_grid, NULL, NULL, &lines) == OUTER_VOICE_OK);
    CommonPracticeWeightedResults results = {NULL, 0};
    assert(common_practice_identify_weighted_sequence(p.segments, p.segment_count, 7, COMMON_PRACTICE_KEY_MAJOR, NULL,
                                                      &results) == COMMON_PRACTICE_WEIGHTED_OK);

    MelodyBuild build;
    melody_build_from_line(&p, lines.soprano, &results, 7, COMMON_PRACTICE_KEY_MAJOR, 8 * MEASURE_TICKS, &build);
    size_t notes = 0;
    for (size_t i = 0; i < build.line.count; i++) {
        notes += build.line.notes[i].present ? 1 : 0;
    }
    printf("Bach 114 soprano, measures 1-8: %zu notes (%zu entries)\n", notes, build.line.count);

    StandardCounterpointMelodySpan spans[8];
    for (size_t s = 0; s < annotation.span_count; s++) {
        assert(span_entries(&build, &annotation.spans[s], &spans[s]));
    }
    StandardCounterpointMelodicIntervals intervals;
    assert(standard_counterpoint_melody_intervals(&build.line, NULL, &intervals) == STANDARD_COUNTERPOINT_MELODY_OK);
    StandardCounterpointContourSummaries summaries;
    assert(standard_counterpoint_melody_contour_summaries(&build.line, &intervals, spans, annotation.span_count,
                                                          &summaries) == STANDARD_COUNTERPOINT_MELODY_OK);
    StandardCounterpointClimaxes climaxes;
    assert(standard_counterpoint_melody_climaxes(&build.line, &intervals, spans, annotation.span_count, &climaxes) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    StandardCounterpointLeapRecoveries recoveries;
    assert(standard_counterpoint_melody_leap_recoveries(&build.line, &intervals, &recoveries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    StandardCounterpointNctCountList counts;
    assert(standard_counterpoint_melody_nct_counts(&build.line, spans, annotation.span_count, &counts) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(summaries.count == annotation.span_count && climaxes.count == annotation.span_count &&
           counts.count == annotation.span_count);

    /* the climax and the range of each span */
    for (size_t s = 0; s < annotation.span_count; s++) {
        const AnnotatedSpan *want = &annotation.spans[s];
        const StandardCounterpointClimaxClaim *climax = &climaxes.items[s].claim;
        long tick = build.tick[climax->first_index];
        int measure = (int)(tick / MEASURE_TICKS) + 1;
        long offset = tick % MEASURE_TICKS;
        const StandardCounterpointContourSummaryClaim *contour = &summaries.items[s].claim;
        long low = pitch_of(&build, contour->lowest_index);
        long high = pitch_of(&build, contour->highest_index);
        printf("  %s: climax %ld at measure %d offset %ld (beat %.2f), %d at the top, relative position %.2f, "
               "range %ld-%ld (%ld semitones), up %d down %d same %d, steps %d leaps %d\n",
               want->id, climax->pitch, measure, offset, 1.0 + (double)offset / 384.0, climax->occurrence_count,
               climax->relative_position, low, high, contour->range_semitones, contour->up, contour->down,
               contour->same, contour->step_count, contour->leap_count);
        assert(climax->pitch == want->climax_pitch);
        assert(measure == want->climax_measure && offset == want->climax_offset);
        assert(climax->occurrence_count == want->top_count && (climax->unique == 1) == (want->top_count == 1));
        assert(low == want->low && high == want->high && contour->range_semitones == want->high - want->low);
    }

    /* the leaps of a fourth or larger and their recovery status */
    size_t correct = 0;
    for (size_t i = 0; i < annotation.leap_count; i++) {
        const AnnotatedLeap *want = &annotation.leaps[i];
        bool found = false;
        bool right = false;
        for (size_t k = 0; k < recoveries.count; k++) {
            const StandardCounterpointLeapRecoveryClaim *claim = &recoveries.items[k].claim;
            long tick = build.tick[claim->to];
            if ((int)(tick / MEASURE_TICKS) + 1 == want->measure && tick % MEASURE_TICKS == want->offset &&
                pitch_of(&build, claim->from) == want->from && pitch_of(&build, claim->to) == want->to) {
                found = true;
                right = claim->status == (want->recovered ? (int)STANDARD_COUNTERPOINT_LEAP_RECOVERED
                                                          : (int)STANDARD_COUNTERPOINT_LEAP_UNRECOVERED);
                if (!right) {
                    printf("  leap at m%d+%ld %ld->%ld: expected %s, got %s\n", want->measure, want->offset, want->from,
                           want->to, want->recovered ? "recovered" : "unrecovered",
                           standard_counterpoint_leap_status_name(
                               (StandardCounterpointLeapStatus)claim->status));
                }
            }
        }
        if (!found) {
            printf("  leap at m%d+%ld %ld->%ld: not found\n", want->measure, want->offset, want->from, want->to);
        }
        correct += right ? 1 : 0;
    }
    size_t extra = 0;
    for (size_t k = 0; k < recoveries.count; k++) {
        const StandardCounterpointLeapRecoveryClaim *claim = &recoveries.items[k].claim;
        if (claim->status == (int)STANDARD_COUNTERPOINT_LEAP_NOT_REQUIRED) {
            continue; /* a third: not in the annotation, which lists leaps of a fourth or larger */
        }
        long tick = build.tick[claim->to];
        bool annotated = false;
        for (size_t i = 0; i < annotation.leap_count; i++) {
            annotated = annotated || ((int)(tick / MEASURE_TICKS) + 1 == annotation.leaps[i].measure &&
                                      tick % MEASURE_TICKS == annotation.leaps[i].offset);
        }
        if (!annotated) {
            extra++;
            printf("  found, not annotated: m%d+%ld %ld->%ld %s\n", (int)(tick / MEASURE_TICKS) + 1, tick % MEASURE_TICKS,
                   pitch_of(&build, claim->from), pitch_of(&build, claim->to),
                   standard_counterpoint_leap_status_name((StandardCounterpointLeapStatus)claim->status));
        }
    }
    double rate = (double)correct / (double)annotation.leap_count;
    printf("  leaps of a fourth or larger: %zu of %zu annotated found with the right status (%.1f%%), %zu found that are "
           "not annotated\n",
           correct, annotation.leap_count, 100.0 * rate, extra);
    assert(rate >= 0.90);

    /* the non-chord tones attached from L2.5 */
    for (size_t s = 0; s < annotation.span_count; s++) {
        const StandardCounterpointNctCountsClaim *c = &counts.items[s].claim;
        printf("  %s: %d notes, %d chord tones, %d non-chord tones (passing %d, neighbor %d), %d unclassified\n",
               annotation.spans[s].id, c->note_count, c->chord_tones, c->non_chord_tones,
               c->by_type[NON_CHORD_TONE_PASSING], c->by_type[NON_CHORD_TONE_NEIGHBOR], c->unclassified);
        assert(c->chord_tones + c->non_chord_tones + c->unclassified == c->note_count);
    }

    standard_counterpoint_nct_count_list_destroy(&counts);
    standard_counterpoint_leap_recoveries_destroy(&recoveries);
    standard_counterpoint_climaxes_destroy(&climaxes);
    standard_counterpoint_contour_summaries_destroy(&summaries);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    melody_build_destroy(&build);
    common_practice_weighted_results_destroy(&results);
    outer_voice_lines_destroy(&lines);
    pipeline_destroy(&p);
    meter_map_destroy(map);
}

int main(void) {
    test_bach_114();
    printf("test_melody_bach114: ok\n");
    return 0;
}
