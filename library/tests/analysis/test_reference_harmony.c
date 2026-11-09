#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "weighted_pipeline.h"
#include "semantics/analysis_key.h"
#include "ursatz_chain/analysis_chain.h"

/*
 * The chain's harmony against Michelle's hand harmony of the Clementi and Mozart reference pieces
 * (tests/fixtures/annotations/clementi36_harmony.txt and mozart545_harmony.txt, the chord syntax of the cadences files).
 *
 * The chain is analysis_chain_run to the form stage with the file's key signature events and the piece's home key as the key of the
 * boundary cadences, every default the library's. For each annotated chord this reports whether the chain's chord sounding at the
 * chord's onset has the same ROOT and the same TRIAD QUALITY (major, minor, diminished, augmented); the seventh is not compared, so a V7
 * annotation matches a V triad. An applied numeral (V/V, viio7/V) is compared the same way, by the root and quality of the applied chord,
 * until P40 reads applied chords as their own. It also counts the chord changes the chain makes inside each annotated stretch (a
 * change is a matched segment whose root or triad quality differs from the matched segment before it) against the changes the
 * annotation has, so the surplus shows how finely the chain cuts the harmony.
 *
 * P39 asserts the target bars of R-371 (the chain reads chords on harmonic spans): in Clementi measures 1 to 5, 8 and 9 and in Mozart
 * measures 1 to 6, 11 and 12 every annotated chord must match by root and triad quality, and the chain makes no chord change inside
 * Clementi measure 8 or Mozart measures 1 and 8 (Mozart measure 8 is one chord; which chord is reported, not asserted, R-375). P40 B4
 * (the bass evidence rules of the harmonic spans, R-381 and R-383) also asserts that a chain segment starts at Clementi measure 6 beat 4
 * (the F# bass), that none starts at Mozart measure 10 beat 4 (the chromatic F#3 under the F chord is no new chord), and that the F#3 of
 * Mozart measure 10 is a leftover tone of its segment when that segment is matched (reported when it is not, R-384). P40 B5 (the chain
 * reads applied chords, R-380) adds Clementi measures 6 and 7 and Mozart measure 9 to the target bars (Clementi measure 6 beat 4 is the
 * F# diminished seventh of V, asserted by its root and triad quality as every chord is), with Clementi measure 7 beat 2 inside the bar
 * but reported, not asserted (R-384), and asserts that the C#5 notes of Mozart measure 9 are leftover tones of its chord. Every other
 * annotated bar and the extra changes are reported only. Each report line also prints the chain's Roman numeral at that chord.
 * URSATZ_CORPUS_DIR and URSATZ_ANNOTATION_DIR come from CMake.
 */

#define MAX_CHORDS 64

typedef enum { Q_MAJOR, Q_MINOR, Q_DIMINISHED, Q_AUGMENTED } Quality;

typedef struct {
    double at;    /* whole notes */
    double until; /* whole notes */
    int measure;
    double beat;
    int root;     /* pitch class */
    Quality quality;
    char text[32]; /* tonic and numeral as written, for the report */
} HandChord;

typedef struct {
    int beats;
    long beat_num;
    long beat_den;
    int home;
    HandChord chords[MAX_CHORDS];
    size_t count;
} Reference;

typedef struct {
    int measure;
    double beat;
} BeatPoint;

typedef struct {
    int measure;
    int pitch;
} LeftoverTone;

typedef struct {
    const char *name;
    const char *midi;
    const char *fixture;
    const int *bars;       /* the target bars: every annotated chord in them must match */
    size_t bar_count;
    const int *one_chord;  /* the bars in which the chain makes no chord change */
    size_t one_chord_count;
    const BeatPoint *segment_starts;    /* beats at which a chain segment must start (P40 B4) */
    size_t segment_start_count;
    const BeatPoint *no_segment_starts; /* beats at which none may start */
    size_t no_segment_start_count;
    const BeatPoint *reported;          /* annotated chords inside a target bar that are reported, not asserted (P40 B5, R-384) */
    size_t reported_count;
    const LeftoverTone *leftovers;      /* the notes of a measure that must be leftover tones of their segment (when matched) */
    size_t leftover_count;
} Piece;

static const int CLEMENTI_BARS[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
static const int CLEMENTI_ONE_CHORD[] = {8};
static const int MOZART_BARS[] = {1, 2, 3, 4, 5, 6, 9, 11, 12};
static const int MOZART_ONE_CHORD[] = {1, 8};

static const BeatPoint CLEMENTI_SEGMENT_STARTS[] = {{6, 4.0}};
static const BeatPoint MOZART_NO_SEGMENT_STARTS[] = {{10, 4.0}};
static const BeatPoint CLEMENTI_REPORTED[] = {{7, 2.0}}; /* annotated C major, the chain reads A minor (R-384) */
static const LeftoverTone MOZART_LEFTOVERS[] = {{9, 73}, {10, 54}}; /* the C#5 notes of measure 9, the F#3 of measure 10 */

static const Piece PIECES[2] = {
    {"Clementi 36", "Clementi 36.mid", "clementi36_harmony.txt", CLEMENTI_BARS, sizeof(CLEMENTI_BARS) / sizeof(int), CLEMENTI_ONE_CHORD,
     sizeof(CLEMENTI_ONE_CHORD) / sizeof(int), CLEMENTI_SEGMENT_STARTS, sizeof(CLEMENTI_SEGMENT_STARTS) / sizeof(BeatPoint), NULL, 0,
     CLEMENTI_REPORTED, sizeof(CLEMENTI_REPORTED) / sizeof(BeatPoint), NULL, 0},
    {"Mozart 545", "Mozart 545.mid", "mozart545_harmony.txt", MOZART_BARS, sizeof(MOZART_BARS) / sizeof(int), MOZART_ONE_CHORD,
     sizeof(MOZART_ONE_CHORD) / sizeof(int), NULL, 0, MOZART_NO_SEGMENT_STARTS, sizeof(MOZART_NO_SEGMENT_STARTS) / sizeof(BeatPoint), NULL, 0,
     MOZART_LEFTOVERS, sizeof(MOZART_LEFTOVERS) / sizeof(LeftoverTone)}};

static bool listed(const int *bars, size_t count, int measure) {
    for (size_t i = 0; i < count; i++) {
        if (bars[i] == measure) {
            return true;
        }
    }
    return false;
}

static bool is_reported(const Piece *piece, int measure, double beat) {
    for (size_t i = 0; i < piece->reported_count; i++) {
        if (piece->reported[i].measure == measure && fabs(piece->reported[i].beat - beat) < 1e-9) {
            return true;
        }
    }
    return false;
}

static const char *const PC_NAMES[12] = {"C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
static const int MAJOR_SCALE[7] = {0, 2, 4, 5, 7, 9, 11};

static int tonic_of(const char *name) {
    for (int pc = 0; pc < 12; pc++) {
        if (strcmp(name, PC_NAMES[pc]) == 0) {
            return pc;
        }
    }
    assert(!"unknown tonic");
    return -1;
}

/* The scale degree 1..7 of a Roman numeral's letters, whatever the case. */
static int degree_of(const char *letters, size_t n) {
    static const char *const NAMES[7] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
    char lower[8] = {0};
    assert(n > 0 && n < 8);
    for (size_t i = 0; i < n; i++) {
        lower[i] = (char)tolower((unsigned char)letters[i]);
    }
    for (int d = 0; d < 7; d++) {
        if (strcmp(lower, NAMES[d]) == 0) {
            return d + 1;
        }
    }
    assert(!"unknown numeral");
    return -1;
}

/* The root and triad quality of a numeral in the major key on `tonic`. An applied numeral (X/Y) is the chord X in the key of degree Y. */
static void read_numeral(int tonic, const char *text, int *root, Quality *quality) {
    size_t n = 0;
    while (text[n] == 'I' || text[n] == 'V' || text[n] == 'i' || text[n] == 'v') {
        n++;
    }
    int degree = degree_of(text, n);
    bool upper = isupper((unsigned char)text[0]) != 0;
    const char *rest = text + n;
    bool diminished = *rest == 'o';
    const char *slash = strchr(rest, '/');
    int key = tonic;
    if (slash != NULL && !isdigit((unsigned char)slash[1])) {
        /* an applied chord: the figure before the slash is read by the loop above, the target after it */
        const char *target = slash + 1;
        size_t t = 0;
        while (target[t] == 'I' || target[t] == 'V' || target[t] == 'i' || target[t] == 'v') {
            t++;
        }
        assert(target[t] == '\0');
        key = (tonic + MAJOR_SCALE[degree_of(target, t) - 1]) % 12;
    }
    *root = (key + MAJOR_SCALE[degree - 1]) % 12;
    *quality = diminished ? Q_DIMINISHED : (upper ? Q_MAJOR : Q_MINOR);
}

static double time_of(const Reference *r, int measure, double beat) {
    return (((double)(measure - 1) * (double)r->beats + (beat - 1.0)) * (double)r->beat_num) / (double)r->beat_den;
}

static Reference load_reference(const char *fixture) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_ANNOTATION_DIR, fixture);
    size_t size = 0;
    unsigned char *text = pipeline_read_file(path, &size);
    Reference r;
    memset(&r, 0, sizeof(r));
    for (char *line = strtok((char *)text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        if (line[0] == '#' || line[0] == '\0') {
            continue;
        }
        char word[16] = "";
        sscanf(line, "%15s", word);
        if (strcmp(word, "meter") == 0) {
            assert(sscanf(line, "%*s %d %ld/%ld", &r.beats, &r.beat_num, &r.beat_den) == 3);
        } else if (strcmp(word, "home") == 0) {
            char name[4] = "";
            assert(sscanf(line, "%*s %3s", name) == 1);
            r.home = tonic_of(name);
        } else if (strcmp(word, "chord") == 0) {
            int measure = 0;
            double beat = 0.0;
            char tonic[4] = "";
            char numeral[24] = "";
            assert(sscanf(line, "%*s %d %lf %3s %23s", &measure, &beat, tonic, numeral) == 4);
            assert(r.count < MAX_CHORDS && r.beats > 0);
            HandChord *c = &r.chords[r.count++];
            c->measure = measure;
            c->beat = beat;
            c->at = time_of(&r, measure, beat);
            c->until = c->at;
            read_numeral(tonic_of(tonic), numeral, &c->root, &c->quality);
            snprintf(c->text, sizeof(c->text), "%s %s", tonic, numeral);
            if (r.count >= 2 && r.chords[r.count - 2].until == r.chords[r.count - 2].at) {
                r.chords[r.count - 2].until = c->at;
            }
        } else if (strcmp(word, "end") == 0) {
            int measure = 0;
            double beat = 0.0;
            assert(sscanf(line, "%*s %d %lf", &measure, &beat) == 2 && r.count > 0);
            r.chords[r.count - 1].until = time_of(&r, measure, beat);
        }
    }
    free(text);
    assert(r.beats > 0 && r.count > 0);
    return r;
}

static const char *quality_name(Quality q) {
    return q == Q_MAJOR ? "major" : q == Q_MINOR ? "minor" : q == Q_DIMINISHED ? "diminished" : "augmented";
}

/* The root and triad quality of a result's chord; false when the segment has no match. */
static bool chain_chord(const CommonPracticeWeightedResult *result, int *root, Quality *quality) {
    if (!result->matched) {
        return false;
    }
    int intervals[4] = {0};
    size_t n = common_practice_ranked_chord_intervals(&result->best, intervals);
    assert(n >= 3);
    *root = result->best.root_pitch_class;
    if (intervals[1] == 3) {
        *quality = intervals[2] == 6 ? Q_DIMINISHED : Q_MINOR;
    } else {
        *quality = intervals[2] == 8 ? Q_AUGMENTED : Q_MAJOR;
    }
    return true;
}

/* The chain's Roman numeral of segment s as written in the report ("none" when it has no numeral). */
static const char *numeral_label(const AnalysisChain *chain, size_t s, char *buffer, size_t size) {
    const StandardTonalityRomanResult *numerals = analysis_chain_numerals(chain);
    if (numerals == NULL || !numerals[s].has_numeral || !standard_tonality_format_roman_reading(&numerals[s].reading, buffer, size)) {
        snprintf(buffer, size, "none");
    }
    return buffer;
}

/* The chord changes between the matched segments that start inside `measure`, and the first such segment's chord (for the report). */
static size_t changes_in_measure(const Reference *ref, const ChainSegments *segments, const CommonPracticeWeightedResult *chords, size_t count,
                                 int measure, bool *has_chord, int *first_root, Quality *first_quality) {
    double from = time_of(ref, measure, 1.0);
    double to = time_of(ref, measure + 1, 1.0);
    size_t changes = 0;
    *has_chord = false;
    int previous_root = 0;
    Quality previous_quality = Q_MAJOR;
    for (size_t s = 0; s < count; s++) {
        if (segments->starts[s] < from - 1e-9 || segments->starts[s] >= to - 1e-9) {
            continue;
        }
        int root = 0;
        Quality quality = Q_MAJOR;
        if (!chain_chord(&chords[s], &root, &quality)) {
            continue;
        }
        if (!*has_chord) {
            *first_root = root;
            *first_quality = quality;
        } else if (root != previous_root || quality != previous_quality) {
            changes++;
        }
        *has_chord = true;
        previous_root = root;
        previous_quality = quality;
    }
    return changes;
}

/* The number of asserted expectations that failed. */
static int report_piece(const Piece *piece) {
    int failures = 0;
    Reference ref = load_reference(piece->fixture);
    WeightedPipeline p = pipeline_run(URSATZ_CORPUS_DIR, piece->midi, NULL);

    size_t key_event_count = p.import.key_signatures.count;
    AnalysisKeyEvent *key_events = (AnalysisKeyEvent *)calloc(key_event_count + 1, sizeof(AnalysisKeyEvent));
    assert(key_events != NULL);
    for (size_t i = 0; i < key_event_count; i++) {
        const MidiKeySignatureEvent *event = &p.import.key_signatures.events[i];
        key_events[i].time = event->position;
        assert(analysis_key_from_fifths(event->sharps_flats, KEY_STRUCTURE_MODE_MAJOR, &key_events[i].key));
    }
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = ANALYSIS_CHAIN_STAGE_FORM;
    config.first_measure = 1;
    config.home_key_index = common_practice_key_index_of(ref.home, COMMON_PRACTICE_KEY_MAJOR);
    AnalysisChain *chain = NULL;
    assert(analysis_chain_run(&p.set, key_events, key_event_count, &config, &chain) == ANALYSIS_CHAIN_OK);

    const ChainSegments *segments = analysis_chain_segments(chain);
    const CommonPracticeWeightedResult *chords = analysis_chain_chords(chain);
    size_t count = analysis_chain_segment_count(chain);
    assert(count > 0 && segments->count >= count);

    printf("%s, the chain's harmony against Michelle's hand harmony (%zu chords)\n", piece->name, ref.count);
    size_t matched = 0;
    for (size_t i = 0; i < ref.count; i++) {
        const HandChord *c = &ref.chords[i];
        int root = -1;
        Quality quality = Q_MAJOR;
        bool found = false;
        size_t at_segment = count;
        for (size_t s = 0; s < count; s++) {
            if (segments->starts[s] <= c->at + 1e-9 && c->at < segments->ends[s] - 1e-9) {
                found = chain_chord(&chords[s], &root, &quality);
                at_segment = s;
                break;
            }
        }
        if (found && root == c->root && quality == c->quality) {
            matched++;
        } else {
            bool in_bar = listed(piece->bars, piece->bar_count, c->measure);
            bool asserted = in_bar && !is_reported(piece, c->measure, c->beat);
            char label[STANDARD_TONALITY_ROMAN_LABEL_MAX];
            printf("  %s: measure %d beat %g (%s): annotated %s %s, chain %s", asserted ? "FAIL (target bar)" : in_bar ? "reported (R-384)" : "miss",
                   c->measure, c->beat, c->text, PC_NAMES[c->root], quality_name(c->quality), found ? PC_NAMES[root] : "no chord");
            if (found) {
                printf(" %s", quality_name(quality));
            }
            printf(", numeral %s\n", at_segment < count ? numeral_label(chain, at_segment, label, sizeof(label)) : "none");
            failures += asserted ? 1 : 0;
        }
    }

    /* The changes inside each annotated stretch, the chain's against the annotation's. */
    size_t chain_changes = 0;
    size_t annotated_changes = 0;
    size_t first = 0;
    while (first < ref.count) {
        size_t last = first;
        while (last + 1 < ref.count && ref.chords[last].until == ref.chords[last + 1].at) {
            last++;
        }
        for (size_t i = first; i < last; i++) {
            if (ref.chords[i].root != ref.chords[i + 1].root || ref.chords[i].quality != ref.chords[i + 1].quality) {
                annotated_changes++;
            }
        }
        bool have_previous = false;
        int previous_root = 0;
        Quality previous_quality = Q_MAJOR;
        for (size_t s = 0; s < count; s++) {
            if (segments->starts[s] < ref.chords[first].at - 1e-9 || segments->starts[s] >= ref.chords[last].until - 1e-9) {
                continue;
            }
            int root = 0;
            Quality quality = Q_MAJOR;
            if (!chain_chord(&chords[s], &root, &quality)) {
                continue;
            }
            if (have_previous && (root != previous_root || quality != previous_quality)) {
                chain_changes++;
            }
            have_previous = true;
            previous_root = root;
            previous_quality = quality;
        }
        first = last + 1;
    }
    printf("  matched %zu / %zu annotated chords; chain changes %zu, annotated changes %zu, extra changes %ld\n", matched, ref.count,
           chain_changes, annotated_changes, (long)chain_changes - (long)annotated_changes);

    for (size_t i = 0; i < piece->one_chord_count; i++) {
        bool has_chord = false;
        int root = 0;
        Quality quality = Q_MAJOR;
        size_t changes = changes_in_measure(&ref, segments, chords, count, piece->one_chord[i], &has_chord, &root, &quality);
        printf("  measure %d: %zu chord change(s) inside; first chord %s%s%s\n", piece->one_chord[i], changes,
               has_chord ? PC_NAMES[root] : "none", has_chord ? " " : "", has_chord ? quality_name(quality) : "");
        if (!has_chord || changes != 0) {
            printf("  FAIL (one chord): measure %d\n", piece->one_chord[i]);
            failures++;
        }
    }

    for (size_t i = 0; i < piece->segment_start_count; i++) {
        double at = time_of(&ref, piece->segment_starts[i].measure, piece->segment_starts[i].beat);
        bool starts = false;
        for (size_t s = 0; s < count; s++) {
            starts = starts || fabs(segments->starts[s] - at) < 1e-9;
        }
        printf("  a segment %s at measure %d beat %g\n", starts ? "starts" : "does not start", piece->segment_starts[i].measure,
               piece->segment_starts[i].beat);
        if (!starts) {
            printf("  FAIL (segment start): measure %d beat %g\n", piece->segment_starts[i].measure, piece->segment_starts[i].beat);
            failures++;
        }
    }
    for (size_t i = 0; i < piece->no_segment_start_count; i++) {
        double at = time_of(&ref, piece->no_segment_starts[i].measure, piece->no_segment_starts[i].beat);
        bool starts = false;
        for (size_t s = 0; s < count; s++) {
            starts = starts || fabs(segments->starts[s] - at) < 1e-9;
        }
        printf("  no segment starts at measure %d beat %g: %s\n", piece->no_segment_starts[i].measure, piece->no_segment_starts[i].beat,
               starts ? "FAIL" : "ok");
        if (starts) {
            printf("  FAIL (no segment start): measure %d beat %g\n", piece->no_segment_starts[i].measure,
                   piece->no_segment_starts[i].beat);
            failures++;
        }
    }
    for (size_t l = 0; l < piece->leftover_count; l++) {
        int measure = piece->leftovers[l].measure;
        int pitch = piece->leftovers[l].pitch;
        double from = time_of(&ref, measure, 1.0);
        double to = time_of(&ref, measure + 1, 1.0);
        size_t seen = 0;
        for (size_t s = 0; s < count; s++) {
            if (segments->starts[s] < from - 1e-9 || segments->starts[s] >= to - 1e-9) {
                continue;
            }
            const CommonPracticeSegment *segment = &segments->segments[s];
            for (size_t m = 0; m < segment->member_count; m++) {
                const CommonPracticeMember *member = &segment->members[m];
                if (member->pitch != pitch) {
                    continue;
                }
                seen++;
                if (!chords[s].matched) {
                    printf("  measure %d: the segment is not matched; the note %d is not labelled (reported, R-384)\n", measure, pitch);
                    continue;
                }
                bool leftover = false;
                for (size_t k = 0; k < chords[s].leftover_count; k++) {
                    leftover = leftover || (chords[s].leftover[k].note_id != NULL && member->note_id != NULL &&
                                            strcmp(chords[s].leftover[k].note_id, member->note_id) == 0);
                }
                printf("  measure %d: the note %d is %s a leftover tone of its segment\n", measure, pitch, leftover ? "" : "NOT");
                if (!leftover) {
                    printf("  FAIL (leftover tone): measure %d note %d\n", measure, pitch);
                    failures++;
                }
            }
        }
        if (seen == 0) {
            printf("  FAIL (leftover tone): measure %d has no note %d\n", measure, pitch);
            failures++;
        }
    }

    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
    return failures;
}

int main(void) {
    int failures = 0;
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        failures += report_piece(&PIECES[i]);
    }
    if (failures > 0) {
        printf("test_reference_harmony: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_reference_harmony: ok\n");
    return 0;
}
