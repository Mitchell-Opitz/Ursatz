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
 * REPORT ONLY. The chain's harmony against Michelle's hand harmony of the Clementi and Mozart reference pieces
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
 * Nothing is asserted but that the files parse and the chain runs (architecture conventions: a measure that is not a target is
 * reported). P39 turns this into an assertion. URSATZ_CORPUS_DIR and URSATZ_ANNOTATION_DIR come from CMake.
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
    const char *name;
    const char *midi;
    const char *fixture;
} Piece;

static const Piece PIECES[2] = {{"Clementi 36", "Clementi 36.mid", "clementi36_harmony.txt"},
                                {"Mozart 545", "Mozart 545.mid", "mozart545_harmony.txt"}};

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

static void report_piece(const Piece *piece) {
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
        for (size_t s = 0; s < count; s++) {
            if (segments->starts[s] <= c->at + 1e-9 && c->at < segments->ends[s] - 1e-9) {
                found = chain_chord(&chords[s], &root, &quality);
                break;
            }
        }
        if (found && root == c->root && quality == c->quality) {
            matched++;
        } else {
            printf("  miss: measure %d beat %g (%s): annotated %s %s, chain %s", c->measure, c->beat, c->text, PC_NAMES[c->root],
                   quality_name(c->quality), found ? PC_NAMES[root] : "no chord");
            if (found) {
                printf(" %s", quality_name(quality));
            }
            printf("\n");
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

    analysis_chain_destroy(chain);
    free(key_events);
    pipeline_destroy(&p);
}

int main(void) {
    for (size_t i = 0; i < sizeof(PIECES) / sizeof(PIECES[0]); i++) {
        report_piece(&PIECES[i]);
    }
    printf("test_reference_harmony: ok (report only)\n");
    return 0;
}
