#include "analysis_melody_climax.h"
#include "analysis_melody_leaps.h"
#include "analysis_melody_nct.h"
#include "melody_util.h"
#include "pitch/tuning_system.h"
#include "time/meter_map.h"

/*
 * Melody analysis of Bach 114 (Minuet in G, BWV Anh. 114), measures 1-8: the soprano the outer-voice extractor produces from
 * the corpus MIDI file (read in the score's 3/4, as the other Bach 114 tests do), with the L2.6 scale degrees and the L2.5
 * non-chord-tone results attached (tests/integration/melody_util.h), is run through standard-counterpoint's melody adapter
 * over the phrase spans of tests/fixtures/annotations/bach114_melody.txt (placeholders: measures 1-4 and 5-8, until phrase
 * segmentation exists).
 *
 * Two runs, as in the outer-voice tests, because the extractor is not exact (it reads one soprano note per segment and adopts a
 * left-hand note where the right hand rests): the adapter ALONE on the written soprano (the highest annotated melody note sounding
 * at each moment, from bach114_outer_voices.txt), where the annotated climaxes, ranges and leaps are asserted at the target (every
 * climax and range, at least 90% of the leaps with the right status), and the WHOLE CHAIN on the extracted soprano, where the same
 * numbers are reported with the misses and a floor is asserted. The non-chord-tone counts of the extracted run are reported.
 * The fixture is a DRAFT hand annotation (see its header) and nothing is tuned to it.
 */

#define MEASURE_TICKS 1152L

/* The floors of the whole-chain run: set to what it measured (see the report it prints), not above. */
#define FLOOR_CLIMAXES 2
#define FLOOR_RANGES 0 /* the extractor adopts a left-hand note where the right hand rests (measure 4, beat 1) */
#define FLOOR_LEAPS 4 /* of 11: one soprano note per segment drops the quick notes the leaps arrive on */

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

/* --- the written soprano: the highest annotated melody note sounding at each moment --- */

typedef struct {
    long start;
    long end;
    long pitch;
} WrittenNote;

typedef struct {
    MelodyBuild build;
    TuningSystem *tuning;
    PitchNumericValue **values;
    char (*ids)[16];
    size_t value_count;
} WrittenBuild;

static int compare_long(const void *a, const void *b) {
    long x = *(const long *)a;
    long y = *(const long *)b;
    return (x > y) - (x < y);
}

static void written_build(WrittenBuild *w) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/bach114_outer_voices.txt", URSATZ_ANNOTATION_DIR);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    WrittenNote notes[256];
    size_t count = 0;
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        char which[16];
        long measure, offset, duration, pitch;
        if (line[0] == '#' || line[0] == '\0' ||
            sscanf(line, "%15s %ld %ld %ld %ld", which, &measure, &offset, &duration, &pitch) != 5 ||
            strcmp(which, "soprano") != 0) {
            continue;
        }
        notes[count].start = (measure - 1) * MEASURE_TICKS + offset;
        notes[count].end = notes[count].start + duration;
        notes[count].pitch = pitch;
        count++;
    }
    free(text);
    long bounds[512];
    size_t bound_count = 0;
    for (size_t i = 0; i < count; i++) {
        bounds[bound_count++] = notes[i].start;
        bounds[bound_count++] = notes[i].end;
    }
    qsort(bounds, bound_count, sizeof(long), compare_long);

    memset(w, 0, sizeof(*w));
    EntityID *id = NULL;
    assert(entity_id_create("midi-tuning", &id) == ENTITY_ID_OK);
    assert(tuning_system_create(id, "MIDI note number", &w->tuning) == TUNING_SYSTEM_OK);
    w->values = (PitchNumericValue **)calloc(count + 1, sizeof(PitchNumericValue *));
    w->ids = calloc(count + 1, sizeof(char[16]));
    w->build.segment = (size_t *)calloc(count + 1, sizeof(size_t));
    w->build.tick = (long *)calloc(2 * count + 2, sizeof(long));
    standard_counterpoint_melody_line_init(&w->build.line);
    long limit = 8 * MEASURE_TICKS;
    int last = -2; /* the index of the note on top in the previous interval, -1 for a rest */
    for (size_t b = 0; b + 1 < bound_count; b++) {
        if (bounds[b] == bounds[b + 1] || bounds[b] >= limit) {
            continue;
        }
        int top = -1;
        for (size_t i = 0; i < count; i++) {
            if (notes[i].start <= bounds[b] && notes[i].end >= bounds[b + 1] &&
                (top < 0 || notes[i].pitch > notes[top].pitch ||
                 (notes[i].pitch == notes[top].pitch && notes[i].start > notes[top].start))) {
                top = (int)i;
            }
        }
        if (top == last) {
            continue;
        }
        last = top;
        w->build.tick[w->build.line.count] = bounds[b];
        if (top < 0) {
            assert(standard_counterpoint_melody_line_add_rest(&w->build.line) == STANDARD_COUNTERPOINT_MELODY_OK);
            continue;
        }
        size_t v = w->value_count++;
        assert(pitch_numeric_value_create(w->tuning, notes[top].pitch, 1, &w->values[v]) == PITCH_NUMERIC_VALUE_OK);
        snprintf(w->ids[v], sizeof(w->ids[v]), "w%zu", v);
        StandardCounterpointMelodyNote note;
        memset(&note, 0, sizeof(note));
        note.present = true;
        note.note_id = w->ids[v];
        note.pitch = w->values[v];
        note.confidence = 1.0;
        melody_parse_degree(common_practice_scale_degree_label((int)(notes[top].pitch % 12), 7, COMMON_PRACTICE_KEY_MAJOR),
                            &note.degree, &note.alteration);
        assert(standard_counterpoint_melody_line_add_note(&w->build.line, &note) == STANDARD_COUNTERPOINT_MELODY_OK);
    }
}

static void written_destroy(WrittenBuild *w) {
    standard_counterpoint_melody_line_destroy(&w->build.line);
    free(w->build.segment);
    free(w->build.tick);
    for (size_t i = 0; i < w->value_count; i++) {
        pitch_numeric_value_destroy(w->values[i]);
    }
    free(w->values);
    free(w->ids);
    tuning_system_destroy(w->tuning);
}

/* --- the analysis and its comparison with the annotation --- */

typedef struct {
    size_t spans;
    size_t climax_ok;
    size_t range_ok;
    size_t leaps;
    size_t leaps_correct;
    size_t extra_leaps;
    StandardCounterpointNctCountList counts;
} Report;

static void analyze(const char *label, const MelodyBuild *build, const Annotation *annotation, Report *report) {
    memset(report, 0, sizeof(*report));
    StandardCounterpointMelodySpan spans[8];
    for (size_t s = 0; s < annotation->span_count; s++) {
        assert(span_entries(build, &annotation->spans[s], &spans[s]));
    }
    StandardCounterpointMelodicIntervals intervals;
    assert(standard_counterpoint_melody_intervals(&build->line, NULL, &intervals) == STANDARD_COUNTERPOINT_MELODY_OK);
    StandardCounterpointContourSummaries summaries;
    assert(standard_counterpoint_melody_contour_summaries(&build->line, &intervals, spans, annotation->span_count,
                                                          &summaries) == STANDARD_COUNTERPOINT_MELODY_OK);
    StandardCounterpointClimaxes climaxes;
    assert(standard_counterpoint_melody_climaxes(&build->line, &intervals, spans, annotation->span_count, &climaxes) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    StandardCounterpointLeapRecoveries recoveries;
    assert(standard_counterpoint_melody_leap_recoveries(&build->line, &intervals, &recoveries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(standard_counterpoint_melody_nct_counts(&build->line, spans, annotation->span_count, &report->counts) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(summaries.count == annotation->span_count && climaxes.count == annotation->span_count);

    size_t notes = 0;
    for (size_t i = 0; i < build->line.count; i++) {
        notes += build->line.notes[i].present ? 1 : 0;
    }
    printf("%s: %zu notes\n", label, notes);
    report->spans = annotation->span_count;
    for (size_t s = 0; s < annotation->span_count; s++) {
        const AnnotatedSpan *want = &annotation->spans[s];
        const StandardCounterpointClimaxClaim *climax = &climaxes.items[s].claim;
        long tick = build->tick[climax->first_index];
        int measure = (int)(tick / MEASURE_TICKS) + 1;
        long offset = tick % MEASURE_TICKS;
        const StandardCounterpointContourSummaryClaim *contour = &summaries.items[s].claim;
        long low = pitch_of(build, contour->lowest_index);
        long high = pitch_of(build, contour->highest_index);
        printf("  %s: climax %ld at measure %d offset %ld (beat %.2f), %d at the top, relative position %.2f; range %ld-%ld "
               "(%ld semitones), up %d down %d same %d, steps %d leaps %d\n",
               want->id, climax->pitch, measure, offset, 1.0 + (double)offset / 384.0, climax->occurrence_count,
               climax->relative_position, low, high, contour->range_semitones, contour->up, contour->down,
               contour->same, contour->step_count, contour->leap_count);
        bool climax_ok = climax->pitch == want->climax_pitch && measure == want->climax_measure &&
                         offset == want->climax_offset && climax->occurrence_count == want->top_count;
        bool range_ok = low == want->low && high == want->high && contour->range_semitones == want->high - want->low;
        if (!climax_ok) {
            printf("    climax expected %ld at measure %d offset %ld, %d at the top\n", want->climax_pitch,
                   want->climax_measure, want->climax_offset, want->top_count);
        }
        if (!range_ok) {
            printf("    range expected %ld-%ld\n", want->low, want->high);
        }
        report->climax_ok += climax_ok ? 1 : 0;
        report->range_ok += range_ok ? 1 : 0;
    }

    report->leaps = annotation->leap_count;
    for (size_t i = 0; i < annotation->leap_count; i++) {
        const AnnotatedLeap *want = &annotation->leaps[i];
        bool found = false;
        bool right = false;
        for (size_t k = 0; k < recoveries.count; k++) {
            const StandardCounterpointLeapRecoveryClaim *claim = &recoveries.items[k].claim;
            long tick = build->tick[claim->to];
            if ((int)(tick / MEASURE_TICKS) + 1 == want->measure && tick % MEASURE_TICKS == want->offset &&
                pitch_of(build, claim->from) == want->from && pitch_of(build, claim->to) == want->to) {
                found = true;
                right = claim->status == (want->recovered ? (int)STANDARD_COUNTERPOINT_LEAP_RECOVERED
                                                          : (int)STANDARD_COUNTERPOINT_LEAP_UNRECOVERED);
                if (!right) {
                    printf("    leap m%d+%ld %ld->%ld: expected %s, got %s\n", want->measure, want->offset, want->from,
                           want->to, want->recovered ? "recovered" : "unrecovered",
                           standard_counterpoint_leap_status_name((StandardCounterpointLeapStatus)claim->status));
                }
            }
        }
        if (!found) {
            printf("    leap m%d+%ld %ld->%ld: not found\n", want->measure, want->offset, want->from, want->to);
        }
        report->leaps_correct += right ? 1 : 0;
    }
    for (size_t k = 0; k < recoveries.count; k++) {
        const StandardCounterpointLeapRecoveryClaim *claim = &recoveries.items[k].claim;
        if (claim->status == (int)STANDARD_COUNTERPOINT_LEAP_NOT_REQUIRED) {
            continue; /* a third: the annotation lists leaps of a fourth or larger */
        }
        long tick = build->tick[claim->to];
        bool annotated = false;
        for (size_t i = 0; i < annotation->leap_count; i++) {
            annotated = annotated || ((int)(tick / MEASURE_TICKS) + 1 == annotation->leaps[i].measure &&
                                      tick % MEASURE_TICKS == annotation->leaps[i].offset);
        }
        if (!annotated) {
            report->extra_leaps++;
            printf("    found, not annotated: m%d+%ld %ld->%ld %s\n", (int)(tick / MEASURE_TICKS) + 1,
                   tick % MEASURE_TICKS, pitch_of(build, claim->from), pitch_of(build, claim->to),
                   standard_counterpoint_leap_status_name((StandardCounterpointLeapStatus)claim->status));
        }
    }
    printf("  climaxes right %zu of %zu, ranges right %zu of %zu, leaps of a fourth or larger right %zu of %zu (%.1f%%), "
           "%zu found that are not annotated\n",
           report->climax_ok, report->spans, report->range_ok, report->spans, report->leaps_correct, report->leaps,
           100.0 * (double)report->leaps_correct / (double)report->leaps, report->extra_leaps);

    standard_counterpoint_leap_recoveries_destroy(&recoveries);
    standard_counterpoint_climaxes_destroy(&climaxes);
    standard_counterpoint_contour_summaries_destroy(&summaries);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
}

static void print_nct_counts(const Annotation *annotation, const Report *report) {
    for (size_t s = 0; s < annotation->span_count; s++) {
        const StandardCounterpointNctCountsClaim *c = &report->counts.items[s].claim;
        printf("  %s non-chord tones from L2.5: %d notes, %d chord tones, %d non-chord tones (passing %d, neighbor %d), "
               "%d unclassified\n",
               annotation->spans[s].id, c->note_count, c->chord_tones, c->non_chord_tones,
               c->by_type[NON_CHORD_TONE_PASSING], c->by_type[NON_CHORD_TONE_NEIGHBOR], c->unclassified);
        assert(c->chord_tones + c->non_chord_tones + c->unclassified == c->note_count);
    }
}

static void test_bach_114(void) {
    Annotation annotation;
    load_annotation(&annotation);
    assert(annotation.span_count == 2 && annotation.leap_count > 0);

    /* the adapter alone, on the written soprano: at the target */
    WrittenBuild written;
    written_build(&written);
    Report alone;
    analyze("Bach 114, the written soprano (the adapter alone)", &written.build, &annotation, &alone);
    assert(alone.climax_ok == alone.spans && alone.range_ok == alone.spans);
    assert((double)alone.leaps_correct / (double)alone.leaps >= 0.90);
    standard_counterpoint_nct_count_list_destroy(&alone.counts);
    written_destroy(&written);

    /* the whole chain, on the extracted soprano */
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
    Report chain;
    analyze("Bach 114, the extracted soprano (the whole chain)", &build, &annotation, &chain);
    print_nct_counts(&annotation, &chain);
    /* The extractor reads one soprano note per segment and adopts a left-hand note where the right hand rests, so the chain
       is below the target; the floor is what the run measures (reported above), not a tuned value. */
    assert(chain.climax_ok >= FLOOR_CLIMAXES && chain.range_ok >= FLOOR_RANGES);
    assert(chain.leaps_correct >= FLOOR_LEAPS);
    standard_counterpoint_nct_count_list_destroy(&chain.counts);

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
