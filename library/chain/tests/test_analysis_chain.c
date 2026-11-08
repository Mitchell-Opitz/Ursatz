#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "identity/entity_id.h"
#include "io/import_notes.h"
#include "io/midi_importer.h"
#include "pitch/pitch_numeric_value.h"
#include "time/meter_map.h"
#include "time/note_value.h"
#include "time/time_signature.h"
#include "time/time_span.h"
#include "ursatz_chain/analysis_chain.h"
#include "../../tests/integration/reference_pieces.h"

/*
 * analysis_chain_run on four reference pieces (Bach 114, Mozart 545, Clementi 36 and Field 1 from tests/fixtures) against fixed expected
 * values. The values were captured from the chain at the commit where it was proven field by field equal to the test glue that ran the
 * chain before it existed (B4 of P22); the glue is gone and these are the record. Each stage of a piece is reduced to one digest over its
 * results (every field the glue comparison checked, doubles rounded to 1e-6, order-free sets sorted) beside the counts that make a
 * mismatch readable. A change in a digest is a change in the analysis: if it is deliberate, capture the new values in their own commit with
 * `URSATZ_CHAIN_DUMP=1 test_analysis_chain` (R-015). Then a stage subset runs only what it needs.
 */

#define CHECK(condition)                                                                                                   \
    do {                                                                                                                   \
        if (!(condition)) {                                                                                                \
            fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, context, #condition);                                    \
            abort();                                                                                                       \
        }                                                                                                                  \
    } while (0)

/* --- the pieces and their expected values --- */

enum { STAGE_SEGMENTS, STAGE_KEYS, STAGE_CHORDS, STAGE_NUMERALS, STAGE_OUTER, STAGE_CADENCES, STAGE_FORM, STAGE_MOTIVES, STAGE_METER,
       STAGE_TEXTURE, STAGE_MELODY, STAGE_SCHEMAS, STAGE_VOICE_LEADING, STAGE_COUNT };

static const char *const STAGE_NAMES[STAGE_COUNT] = {"segments", "keys",    "chords", "numerals", "outer voices", "cadences", "form",
                                                     "motives",  "meter",   "texture", "melody",  "schemas",      "voice leading"};

typedef struct {
    const char *name;
    const char *midi;
    bool three_four; /* Bach 114's file declares 4/4 and the piece is 3/4: a 3/4 map replaces it */
    int tonic_pitch_class;
    int first_measure;
    int last_measure;
    size_t segments;
    size_t key_regions;
    size_t cadences;
    size_t phrases;
    uint64_t digest[STAGE_COUNT];
} Reference;

static const Reference REFERENCES[4] = {
    {"Bach 114", "Bach 114.mid", true, 7, 1, 32, 52, 5, 4, 4,
     {0x416b3f18e1019eb5ULL, 0xd85242dd48e0db8eULL, 0xe1c68259d823c4eeULL,
      0x71624d5bdd42ff6fULL, 0x7dcbf37f1948fe14ULL, 0xe06a317e4a1716a5ULL,
      0x25e054bd0635de94ULL, 0xbf6b5ec9ca0e1519ULL, 0x6ea6a8f59942dd36ULL,
      0x6e5efa0dc17f5c39ULL, 0xe14473ae2c7aeeb9ULL, 0x131b3fae40ad5e0bULL,
      0xb7c363c1e56cf00bULL}},
    {"Mozart 545", "Mozart 545.mid", false, 0, 1, 146, 224, 25, 13, 21,
     {0x69761056580e36b1ULL, 0x35fb407c898c63c8ULL, 0x7376e484ab66ac3bULL,
      0x53cef783e3e562eaULL, 0x4ada091110983702ULL, 0x7fdf5ac4aea6cf80ULL,
      0xb388d4895a66c95eULL, 0x080a94ae1ee0c03dULL, 0xd0f9e0cf36c74271ULL,
      0x031e15b34d36bf99ULL, 0xa7880702d4b6ca76ULL, 0xc50b929baeb6abd3ULL,
      0xefbf609bf7b0b958ULL}},
    {"Clementi 36", "Clementi 36.mid", false, 0, 1, 38, 47, 5, 5, 7,
     {0xb5d9fcc8cd306106ULL, 0xaf564f595ab6274cULL, 0xf6cd7742f0442773ULL,
      0x9958e15185957937ULL, 0x4d10887062cc5a59ULL, 0xfd625e6553f9fd12ULL,
      0x39da996555bb40d0ULL, 0x47fe0d7eaf8e51e3ULL, 0x50739a843985b4e4ULL,
      0x63f983fedb650d5fULL, 0xdc004de08e7bd956ULL, 0xa31e272015f12c43ULL,
      0x1ffa7355bd5400a3ULL}},
    {"Field 1", "Field 1.mid", false, 3, 1, 16, 17, 1, 4, 4,
     {0xb9f6bd087c744c91ULL, 0xae97569ebba2d9bfULL, 0xa30c99182995d3b1ULL,
      0xc2fa7068d90ec461ULL, 0xc121b0921a83f0afULL, 0x9f7eb8f7e1907ec3ULL,
      0x30b381fad3756ac8ULL, 0x47fe0d7eaf8e51e3ULL, 0x32f2fbae6b786e3bULL,
      0x20c7011fc0a70dc0ULL, 0x5dd961f284530e0dULL, 0xa31e272015f12c43ULL,
      0xdf2f30c0d9e6b245ULL}},
};

/* --- digests --- */

typedef struct {
    uint64_t h;
} Digest;

static void digest_init(Digest *d) {
    d->h = 1469598103934665603ULL;
}

static void digest_bytes(Digest *d, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < size; i++) {
        d->h = (d->h ^ p[i]) * 1099511628211ULL;
    }
}

static void digest_int(Digest *d, long long value) {
    digest_bytes(d, &value, sizeof(value));
}

/* Doubles agree to within rounding: scores within 1e-9 are tied by definition in the library, so a digest holds 1e-6. */
static void digest_double(Digest *d, double value) {
    digest_int(d, llround(value * 1e6));
}

static void digest_string(Digest *d, const char *text) {
    digest_bytes(d, text != NULL ? text : "", text != NULL ? strlen(text) + 1 : 1);
    digest_int(d, text != NULL);
}

static void digest_time(Digest *d, const MusicalTime *time) {
    digest_int(d, (long long)musical_time_numerator(time));
    digest_int(d, (long long)musical_time_denominator(time));
}

static void digest_span(Digest *d, const TimeSpan *span) {
    digest_time(d, time_span_start(span));
    digest_time(d, time_span_end(span));
}

/* --- the chain's input: a file's notes and key signature events --- */

typedef struct {
    MidiImport import;
    MeterMap *own;
    AnalysisNote *notes;
    size_t note_count;
    AnalysisKeyEvent *events;
    size_t event_count;
    AnalysisNoteSet set;
} ChainInput;

static unsigned char *read_file(const char *path, size_t *out_size) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    assert(length > 0);
    unsigned char *data = (unsigned char *)malloc((size_t)length + 1);
    assert(data != NULL);
    size_t read_count = fread(data, 1, (size_t)length, file);
    fclose(file);
    assert(read_count == (size_t)length);
    *out_size = (size_t)length;
    return data;
}

static void chain_input_init(ChainInput *in, const Reference *ref) {
    memset(in, 0, sizeof(*in));
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, ref->midi);
    size_t size = 0;
    unsigned char *data = read_file(path, &size);
    assert(midi_importer_import_complete(data, size, &in->import) == MIDI_IMPORT_OK);
    free(data);
    assert(midi_import_analysis_notes(&in->import, &in->notes, &in->note_count));
    in->event_count = in->import.key_signatures.count;
    in->events = calloc(in->event_count + 1, sizeof(AnalysisKeyEvent));
    assert(in->events != NULL);
    for (size_t i = 0; i < in->event_count; i++) {
        const MidiKeySignatureEvent *event = &in->import.key_signatures.events[i];
        in->events[i].time = event->position;
        assert(analysis_key_from_fifths(event->sharps_flats, KEY_STRUCTURE_MODE_MAJOR, &in->events[i].key));
    }
    in->own = ref->three_four ? three_four_map() : NULL;
    in->set.notes = in->notes;
    in->set.count = in->note_count;
    in->set.map = in->own != NULL ? in->own : in->import.meter.meter_map;
    assert(analysis_note_set_valid(&in->set));
}

static void chain_input_destroy(ChainInput *in) {
    free(in->events);
    import_analysis_notes_destroy(in->notes, in->note_count);
    if (in->own != NULL) {
        meter_map_destroy(in->own);
    }
    midi_import_destroy(&in->import);
}

static AnalysisChainConfig reference_config(const Reference *ref) {
    AnalysisChainConfig config = analysis_chain_config_default();
    config.first_measure = ref->first_measure;
    config.last_measure = ref->last_measure;
    config.home_key_index = common_practice_key_index_of(ref->tonic_pitch_class, COMMON_PRACTICE_KEY_MAJOR);
    return config;
}

/* --- the stages --- */

static int compare_strings(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

static void digest_segments(Digest *d, const AnalysisChain *c) {
    const ChainSegments *segs = analysis_chain_segments(c);
    digest_int(d, (long long)analysis_chain_segment_count(c));
    digest_int(d, (long long)segs->count);
    for (size_t s = 0; s < segs->count; s++) {
        digest_string(d, segs->ids[s]);
        digest_double(d, segs->starts[s]);
        digest_double(d, segs->ends[s]);
        const CommonPracticeSegment *seg = &segs->segments[s];
        digest_int(d, (long long)seg->member_count);
        digest_int(d, seg->beat);
        /* the members of a segment are a set; they are read in the notes' order */
        const char **ids = calloc(seg->member_count + 1, sizeof(char *));
        assert(ids != NULL);
        for (size_t j = 0; j < seg->member_count; j++) {
            ids[j] = seg->members[j].note_id;
        }
        qsort(ids, seg->member_count, sizeof(char *), compare_strings);
        for (size_t k = 0; k < seg->member_count; k++) {
            for (size_t j = 0; j < seg->member_count; j++) {
                if (seg->members[j].note_id != ids[k]) {
                    continue;
                }
                const CommonPracticeMember *m = &seg->members[j];
                digest_string(d, m->note_id);
                digest_int(d, m->pitch);
                digest_int(d, m->held);
                digest_double(d, m->duration);
                digest_double(d, m->onset);
                digest_int(d, m->strength);
                digest_string(d, m->voice_id);
            }
        }
        free(ids);
    }
}

static void digest_keys(Digest *d, const AnalysisChain *c) {
    const CommonPracticeKeyRegions *keys = analysis_chain_key_regions(c);
    digest_int(d, (long long)keys->region_count);
    digest_int(d, (long long)keys->change_count);
    for (size_t i = 0; i < keys->region_count; i++) {
        const CommonPracticeKeyRegion *r = &keys->regions[i];
        digest_int(d, r->key_index);
        digest_int(d, r->first_measure);
        digest_int(d, r->last_measure);
        digest_double(d, r->uncertainty);
        digest_int(d, r->source);
        digest_span(d, r->span);
    }
    for (size_t i = 0; i < keys->change_count; i++) {
        const CommonPracticeKeyChange *k = &keys->changes[i];
        digest_int(d, (long long)k->from_region);
        digest_int(d, (long long)k->to_region);
        digest_int(d, k->extent);
        digest_int(d, k->is_return);
        digest_int(d, k->cadence_confirmed);
        digest_int(d, k->has_pivot);
        digest_int(d, k->has_pivot ? (long long)k->pivot_segment : -1);
    }
}

static void digest_chords(Digest *d, const AnalysisChain *c) {
    const CommonPracticeWeightedResult *chords = analysis_chain_chords(c);
    for (size_t s = 0; s < analysis_chain_segment_count(c); s++) {
        const CommonPracticeWeightedResult *r = &chords[s];
        digest_int(d, r->matched);
        digest_double(d, r->best_score);
        digest_double(d, r->runner_up_score);
        digest_double(d, r->uncertainty);
        digest_string(d, r->best_chord != NULL ? chord_label(r->best_chord) : NULL);
        digest_int(d, (long long)r->leftover_count);
        const char **ids = calloc(r->leftover_count + 1, sizeof(char *));
        assert(ids != NULL);
        for (size_t k = 0; k < r->leftover_count; k++) {
            ids[k] = r->leftover[k].note_id;
        }
        qsort(ids, r->leftover_count, sizeof(char *), compare_strings);
        for (size_t k = 0; k < r->leftover_count; k++) {
            for (size_t m = 0; m < r->leftover_count; m++) {
                if (r->leftover[m].note_id != ids[k]) {
                    continue;
                }
                digest_string(d, r->leftover[m].note_id);
                digest_int(d, r->leftover[m].classified);
                digest_int(d, r->leftover[m].classified ? (long long)r->leftover[m].tone : -1);
            }
        }
        free(ids);
    }
}

static void digest_numerals(Digest *d, const AnalysisChain *c) {
    const StandardTonalityChordInput *inputs = analysis_chain_chord_inputs(c);
    const StandardTonalityRomanResult *romans = analysis_chain_numerals(c);
    for (size_t s = 0; s < analysis_chain_segment_count(c); s++) {
        digest_int(d, inputs[s].has_numeral);
        digest_int(d, inputs[s].root_pitch_class);
        digest_int(d, inputs[s].bass_pitch_class);
        digest_int(d, (long long)inputs[s].interval_count);
        digest_bytes(d, inputs[s].intervals, sizeof(inputs[s].intervals));
        digest_int(d, inputs[s].tonic_pitch_class);
        digest_int(d, inputs[s].mode);
        digest_int(d, (long long)inputs[s].region_index);
        digest_double(d, inputs[s].chord_uncertainty);
        digest_double(d, inputs[s].key_uncertainty);
        digest_int(d, romans[s].has_numeral);
        digest_double(d, romans[s].uncertainty);
        if (romans[s].has_numeral) {
            digest_string(d, romans[s].claim.label);
            digest_bytes(d, &romans[s].claim, sizeof(romans[s].claim));
            digest_int(d, romans[s].deceptive);
        }
    }
}

static void digest_entries(Digest *d, const OuterVoiceEntry *entries, size_t count) {
    for (size_t s = 0; s < count; s++) {
        digest_int(d, entries[s].present);
        digest_int(d, entries[s].bridged);
        digest_int(d, entries[s].guarded);
        if (entries[s].present) {
            digest_string(d, entity_id_value(entries[s].note_id));
            digest_int(d, (long long)pitch_numeric_value_position_numerator(entries[s].pitch));
        }
        digest_span(d, entries[s].span);
    }
}

static void digest_outer_voices(Digest *d, const AnalysisChain *c) {
    const OuterVoiceLines *lines = analysis_chain_outer_voices(c);
    digest_int(d, (long long)lines->count);
    digest_entries(d, lines->soprano, lines->count);
    digest_entries(d, lines->bass, lines->count);
}

static void digest_cadences(Digest *d, const AnalysisChain *c) {
    const HarmonyCadences *cadences = analysis_chain_cadences(c);
    digest_int(d, (long long)cadences->count);
    for (size_t k = 0; k < cadences->count; k++) {
        const HarmonyCadenceResult *a = &cadences->items[k];
        digest_int(d, a->type);
        digest_int(d, (long long)a->dominant_first);
        digest_int(d, (long long)a->dominant_last);
        digest_int(d, a->has_six_four);
        digest_int(d, (long long)a->arrival_first);
        digest_int(d, (long long)a->arrival_last);
        digest_int(d, a->soprano_degree);
        digest_int(d, a->post_cadential);
        digest_int(d, a->arrival_beat_strength);
        digest_double(d, a->uncertainty);
        digest_int(d, a->share);
        digest_span(d, a->span);
        digest_int(d, a->alternative.share);
        digest_int(d, a->alternative.interpretation != NULL);
    }
}

static void digest_form(Digest *d, const AnalysisChain *c) {
    const StandardFormPhrases *phrases = analysis_chain_phrases(c);
    const StandardFormSegmentation *ideas = analysis_chain_ideas(c);
    const StandardFormThemes *themes = analysis_chain_themes(c);
    const StandardFormExpansions *expansions = analysis_chain_expansions(c);
    digest_int(d, (long long)phrases->count);
    for (size_t i = 0; i < phrases->count; i++) {
        digest_bytes(d, &phrases->items[i].claim, sizeof(StandardFormPhraseClaim));
        digest_double(d, phrases->items[i].uncertainty);
        digest_span(d, phrases->items[i].span);
    }
    digest_int(d, (long long)ideas->idea_count);
    digest_int(d, (long long)ideas->subphrase_count);
    for (size_t i = 0; i < ideas->idea_count; i++) {
        digest_bytes(d, &ideas->ideas[i].claim, sizeof(StandardFormIdeaClaim));
        digest_double(d, ideas->ideas[i].uncertainty);
    }
    for (size_t i = 0; i < ideas->subphrase_count; i++) {
        digest_bytes(d, &ideas->subphrases[i].claim, sizeof(StandardFormSubphraseClaim));
    }
    digest_int(d, (long long)themes->count);
    for (size_t i = 0; i < themes->count; i++) {
        digest_bytes(d, &themes->items[i].claim, sizeof(StandardFormThemeClaim));
        digest_double(d, themes->items[i].uncertainty);
    }
    digest_int(d, (long long)expansions->count);
    for (size_t i = 0; i < expansions->count; i++) {
        digest_bytes(d, &expansions->items[i].claim, sizeof(StandardFormExpansionClaim));
        digest_double(d, expansions->items[i].uncertainty);
    }
}

static void digest_motives(Digest *d, const AnalysisChain *c) {
    const StandardFormMotives *motives = analysis_chain_motives(c);
    digest_int(d, (long long)motives->family_count);
    for (size_t f = 0; f < motives->family_count; f++) {
        const StandardFormMotiveFamily *family = &motives->families[f];
        digest_bytes(d, &family->claim, sizeof(family->claim));
        digest_int(d, (long long)family->occurrence_count);
        for (size_t o = 0; o < family->occurrence_count; o++) {
            digest_bytes(d, &family->occurrences[o].claim, sizeof(family->occurrences[o].claim));
            digest_double(d, family->occurrences[o].uncertainty);
        }
    }
}

static void digest_meter(Digest *d, const AnalysisChain *c) {
    const AnalysisChainMeter *meter = analysis_chain_meter(c);
    digest_int(d, (long long)meter->syncopations.count);
    digest_int(d, (long long)meter->hemiolas.count);
    /* simultaneous notes come in another order: the syncopations are a set, ordered by note */
    const char **ids = calloc(meter->syncopations.count + 1, sizeof(char *));
    assert(ids != NULL);
    for (size_t i = 0; i < meter->syncopations.count; i++) {
        ids[i] = meter->syncopations.items[i].claim.note_id;
    }
    qsort(ids, meter->syncopations.count, sizeof(char *), compare_strings);
    for (size_t k = 0; k < meter->syncopations.count; k++) {
        for (size_t i = 0; i < meter->syncopations.count; i++) {
            const StandardMeterSyncopation *a = &meter->syncopations.items[i];
            if (a->claim.note_id != ids[k]) {
                continue;
            }
            digest_string(d, a->claim.note_id);
            digest_int(d, a->claim.line);
            digest_int(d, a->claim.measure);
            digest_int(d, a->claim.beat);
            digest_int(d, a->claim.attack_level);
            digest_int(d, a->claim.through_level);
            digest_int(d, a->claim.strength);
            digest_int(d, a->claim.through_measure);
            digest_int(d, a->claim.through_beat);
            digest_double(d, a->uncertainty);
        }
    }
    free(ids);
    for (size_t i = 0; i < meter->hemiolas.count; i++) {
        digest_bytes(d, &meter->hemiolas.items[i].claim, sizeof(meter->hemiolas.items[i].claim));
    }
    digest_int(d, (long long)meter->hypermeter.phrase_count);
    digest_int(d, (long long)meter->hypermeter.hypermeasure_count);
    digest_int(d, (long long)meter->hypermeter.reinterpretation_count);
    for (size_t i = 0; i < meter->hypermeter.hypermeasure_count; i++) {
        digest_bytes(d, &meter->hypermeter.hypermeasures[i].claim, sizeof(meter->hypermeter.hypermeasures[i].claim));
    }
    digest_int(d, (long long)meter->densities.count);
    for (size_t i = 0; i < meter->densities.count; i++) {
        digest_bytes(d, &meter->densities.items[i].claim, sizeof(meter->densities.items[i].claim));
    }
}

/* The voice separation reads simultaneous notes in the order it is given them, and the chain's order (onset, pitch, id) is not the
   importer's order the numbers were first compared in: the digest holds the textures, passages and changes, not the numbers that carry a
   voice confidence. */
static void digest_texture(Digest *d, const AnalysisChain *c) {
    const StandardTextureAnalysis *texture = analysis_chain_texture(c);
    digest_int(d, (long long)texture->windows.count);
    digest_int(d, (long long)texture->passage_count);
    digest_int(d, (long long)texture->change_count);
    for (size_t w = 0; w < texture->windows.count; w++) {
        const StandardTextureWindowFeatures *a = &texture->windows.items[w];
        digest_int(d, a->measure);
        digest_int(d, (long long)a->events);
        digest_int(d, (long long)a->attacks);
        digest_int(d, texture->classes[w].has_texture);
        digest_int(d, texture->classes[w].has_texture ? (long long)texture->classes[w].texture : -1);
        digest_int(d, texture->window_texture[w]);
    }
    for (size_t i = 0; i < texture->passage_count; i++) {
        const StandardTexturePassageClaim *a = &texture->passages[i].claim;
        digest_int(d, (long long)a->index);
        digest_int(d, a->first_measure);
        digest_int(d, a->last_measure);
        digest_int(d, a->texture);
        digest_int(d, a->category);
        digest_int(d, (long long)a->windows);
        digest_int(d, (long long)a->unclassified_windows);
    }
    for (size_t i = 0; i < texture->change_count; i++) {
        const StandardTextureChangeClaim *a = &texture->changes[i].claim;
        digest_int(d, (long long)a->from);
        digest_int(d, (long long)a->to);
        digest_int(d, a->boundary_measure);
        digest_int(d, a->from_texture);
        digest_int(d, a->to_texture);
        digest_int(d, a->category_changed);
    }
}

static void digest_melody(Digest *d, const AnalysisChain *c) {
    const AnalysisChainMelody *melody = analysis_chain_melody(c);
    digest_int(d, (long long)melody->line.line.count);
    for (size_t i = 0; i < melody->line.line.count; i++) {
        const StandardCounterpointMelodyNote *a = &melody->line.line.notes[i];
        digest_int(d, a->present);
        digest_int(d, (long long)melody->line.segment[i]);
        if (!a->present) {
            continue;
        }
        digest_string(d, a->note_id);
        digest_int(d, a->degree);
        digest_int(d, a->alteration);
        digest_int(d, a->has_strength);
        digest_int(d, a->strength);
        digest_int(d, a->nct);
        digest_int(d, a->nct == STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE ? (long long)a->nct_type : -1);
        digest_int(d, (long long)pitch_numeric_value_position_numerator(a->pitch));
    }
    digest_int(d, (long long)melody->intervals.count);
    digest_int(d, (long long)melody->summaries.count);
    digest_int(d, (long long)melody->counts.count);
    for (size_t i = 0; i < melody->intervals.count; i++) {
        digest_bytes(d, &melody->intervals.items[i].claim, sizeof(melody->intervals.items[i].claim));
        digest_double(d, melody->intervals.items[i].uncertainty);
    }
    for (size_t i = 0; i < melody->summaries.count; i++) {
        digest_bytes(d, &melody->summaries.items[i].claim, sizeof(melody->summaries.items[i].claim));
    }
}

static void digest_schemas(Digest *d, const AnalysisChain *c) {
    const GalantSchemaMatches *schemas = analysis_chain_schemas(c);
    const ScaleSequenceMatches *sequences = analysis_chain_sequences(c);
    digest_int(d, (long long)schemas->count);
    digest_int(d, (long long)sequences->count);
    for (size_t i = 0; i < schemas->count; i++) {
        digest_bytes(d, &schemas->items[i].claim, sizeof(schemas->items[i].claim));
        digest_double(d, schemas->items[i].uncertainty);
    }
    for (size_t i = 0; i < sequences->count; i++) {
        digest_bytes(d, &sequences->items[i].claim, sizeof(sequences->items[i].claim));
    }
}

/* The voice-leading stage is held to its shape and its counts. */
static void digest_voice_leading(Digest *d, const AnalysisChain *c) {
    const AnalysisChainVoiceLeading *vl = analysis_chain_voice_leading(c);
    digest_int(d, (long long)vl->voice_count);
    digest_int(d, (long long)vl->motions.count);
    digest_int(d, (long long)vl->perfects.count);
    digest_int(d, (long long)vl->resolutions.count);
}

static void check_voice_leading(const char *context, const AnalysisChain *c) {
    const AnalysisChainVoiceLeading *vl = analysis_chain_voice_leading(c);
    size_t count = analysis_chain_segment_count(c);
    CHECK(vl != NULL && vl->voice_count >= 2 && vl->voice_count <= 8);
    CHECK(vl->motions.count > 0);
    for (size_t i = 0; i < vl->motions.count; i++) {
        CHECK(vl->motions.items[i].segment_to < count);
    }
    for (size_t i = 0; i < vl->perfects.count; i++) {
        CHECK(vl->perfects.items[i].segment_to < count && vl->perfects.items[i].upper_voice < vl->perfects.items[i].lower_voice);
    }
    for (size_t i = 0; i < vl->resolutions.count; i++) {
        CHECK(vl->resolutions.items[i].segment_to < count && vl->resolutions.items[i].voice < vl->voice_count);
    }
}

typedef void (*DigestFn)(Digest *, const AnalysisChain *);
static const DigestFn DIGESTS[STAGE_COUNT] = {digest_segments, digest_keys,    digest_chords,  digest_numerals, digest_outer_voices,
                                              digest_cadences, digest_form,    digest_motives, digest_meter,    digest_texture,
                                              digest_melody,   digest_schemas, digest_voice_leading};

static uint64_t stage_digest(int stage, const AnalysisChain *c) {
    Digest d;
    digest_init(&d);
    DIGESTS[stage](&d, c);
    return d.h;
}

/* A run of a subset of the stages gives the stages asked for and what they need, and nothing else. */
static void check_subsets(const char *context, ChainInput *input, const Reference *ref, const AnalysisChain *full) {
    AnalysisChainConfig config = reference_config(ref);

    config.stages = ANALYSIS_CHAIN_STAGE_CADENCES;
    AnalysisChain *cadences = NULL;
    assert(analysis_chain_run(&input->set, input->events, input->event_count, &config, &cadences) == ANALYSIS_CHAIN_OK);
    CHECK(analysis_chain_segments(cadences) != NULL && analysis_chain_key_regions(cadences) != NULL && analysis_chain_chords(cadences) != NULL);
    CHECK(analysis_chain_numerals(cadences) != NULL && analysis_chain_outer_voices(cadences) != NULL && analysis_chain_cadences(cadences) != NULL);
    CHECK(analysis_chain_phrases(cadences) == NULL && analysis_chain_motives(cadences) == NULL && analysis_chain_meter(cadences) == NULL);
    CHECK(analysis_chain_texture(cadences) == NULL && analysis_chain_voice_leading(cadences) == NULL && analysis_chain_melody(cadences) == NULL);
    CHECK(analysis_chain_schemas(cadences) == NULL && analysis_chain_sequences(cadences) == NULL);
    /* with no form there is no second pass: the keys are the first pass's, and the cadences their own */
    CHECK(analysis_chain_home_key(cadences) == config.home_key_index);
    analysis_chain_destroy(cadences);

    config.stages = ANALYSIS_CHAIN_STAGE_TEXTURE;
    AnalysisChain *texture = NULL;
    assert(analysis_chain_run(&input->set, input->events, input->event_count, &config, &texture) == ANALYSIS_CHAIN_OK);
    CHECK(analysis_chain_texture(texture) != NULL && analysis_chain_segments(texture) == NULL && analysis_chain_key_regions(texture) == NULL);
    CHECK(stage_digest(STAGE_TEXTURE, texture) == stage_digest(STAGE_TEXTURE, full));
    analysis_chain_destroy(texture);

    config.stages = ANALYSIS_CHAIN_STAGE_FORM;
    AnalysisChain *form = NULL;
    assert(analysis_chain_run(&input->set, input->events, input->event_count, &config, &form) == ANALYSIS_CHAIN_OK);
    CHECK(stage_digest(STAGE_KEYS, form) == ref->digest[STAGE_KEYS]);
    CHECK(stage_digest(STAGE_FORM, form) == ref->digest[STAGE_FORM]);
    CHECK(analysis_chain_motives(form) != NULL && analysis_chain_meter(form) == NULL && analysis_chain_texture(form) == NULL);
    analysis_chain_destroy(form);

    /* the keys alone, and a run with refine_keys off, differ only in the second pass */
    config.stages = ANALYSIS_CHAIN_STAGE_KEYS;
    AnalysisChain *keys = NULL;
    assert(analysis_chain_run(&input->set, input->events, input->event_count, &config, &keys) == ANALYSIS_CHAIN_OK);
    CHECK(analysis_chain_key_regions(keys) != NULL && analysis_chain_chords(keys) == NULL && analysis_chain_cadences(keys) == NULL);
    CHECK(analysis_chain_key_regions(keys)->region_count >= 1);
    analysis_chain_destroy(keys);
}

/* The outer lines on the chord segments: an entry per chord segment, the voice segment sounding at its start. */
static void check_segment_lines(const char *context, const AnalysisChain *c) {
    const OuterVoiceLines *lines = analysis_chain_segment_outer_voices(c);
    const OuterVoiceLines *voice_lines = analysis_chain_outer_voices(c);
    const ChainSegments *segments = analysis_chain_segments(c);
    const ChainSegments *voice_segments = analysis_chain_voice_segments(c);
    CHECK(lines != NULL && voice_lines != NULL && lines->count == segments->count && voice_lines->count == voice_segments->count);
    for (size_t i = 0; i < lines->count; i++) {
        size_t v = 0;
        while (v + 1 < voice_segments->count && voice_segments->starts[v + 1] <= segments->starts[i] + 1e-12) {
            v++;
        }
        bool has = voice_segments->starts[0] <= segments->starts[i] + 1e-12;
        CHECK(lines->soprano[i].present == (has && voice_lines->soprano[v].present));
        CHECK(lines->bass[i].present == (has && voice_lines->bass[v].present));
        if (lines->soprano[i].present) {
            CHECK(strcmp(entity_id_value(lines->soprano[i].note_id), entity_id_value(voice_lines->soprano[v].note_id)) == 0);
        }
        if (lines->bass[i].present) {
            CHECK(strcmp(entity_id_value(lines->bass[i].note_id), entity_id_value(voice_lines->bass[v].note_id)) == 0);
        }
    }
}

/* The stage inputs the chain keeps: all there after a full run, none on a NULL chain, and the ones the stages read are the ones they built. */
static void check_stage_inputs(const char *context, const AnalysisChain *c) {
    CHECK(analysis_chain_pitches(NULL) == NULL && analysis_chain_harmony_inputs(NULL) == NULL && analysis_chain_prepared_harmony(NULL) == NULL);
    CHECK(analysis_chain_form_input(NULL) == NULL && analysis_chain_motive_input(NULL) == NULL && analysis_chain_voice_grid(NULL) == NULL);
    CHECK(analysis_chain_galant_segments(NULL) == NULL && analysis_chain_segment_outer_voices(NULL) == NULL);
    CHECK(analysis_chain_pitches(c) != NULL && analysis_chain_harmony_inputs(c) != NULL && analysis_chain_prepared_harmony(c) != NULL);
    CHECK(analysis_chain_form_input(c) != NULL && analysis_chain_motive_input(c) != NULL && analysis_chain_voice_grid(c) != NULL);
    CHECK(analysis_chain_galant_segments(c) != NULL);
    CHECK(analysis_chain_harmony_inputs(c)->count == analysis_chain_segment_count(c));
    CHECK(analysis_chain_form_input(c)->input.finished && analysis_chain_motive_input(c)->input.finished);
    CHECK(analysis_chain_form_input(c)->input.segment_count > 0 && analysis_chain_motive_input(c)->input.note_count > 0);
    CHECK(analysis_chain_voice_grid(c)->grid.voice_count == analysis_chain_voice_leading(c)->voice_count);
    CHECK(analysis_chain_galant_segments(c)->list.count > 0);
}

/* The distinct soprano note ids of a line, runs counted once (a present entry on the id before it is the same note). */
static size_t soprano_runs(const OuterVoiceLines *lines) {
    size_t runs = 0;
    const char *previous = NULL;
    for (size_t i = 0; i < lines->count; i++) {
        const char *id = lines->soprano[i].present ? entity_id_value(lines->soprano[i].note_id) : NULL;
        runs += id != NULL && (previous == NULL || strcmp(previous, id) != 0) ? 1 : 0;
        previous = id;
    }
    return runs;
}

/* The melody line and the meter input read the voice lines (R-377): one melody note per soprano note, not one per chord segment, and every
   soprano note of the voice lines is the meter input's soprano line. */
static void check_voice_line_inputs(const char *context, const ChainInput *input, const Reference *ref, const AnalysisChain *c) {
    const OuterVoiceLines *voice_lines = analysis_chain_outer_voices(c);
    const OuterVoiceLines *segment_lines = analysis_chain_segment_outer_voices(c);
    size_t runs = soprano_runs(voice_lines);
    CHECK(runs > soprano_runs(segment_lines));
    const StandardCounterpointMelodyLine *melody = &analysis_chain_melody(c)->line.line;
    size_t present = 0;
    for (size_t i = 0; i < melody->count; i++) {
        present += melody->notes[i].present ? 1 : 0;
    }
    CHECK(present == runs);

    ChainMeterInput meter;
    memset(&meter, 0, sizeof(meter));
    int last = ref->last_measure > 0 ? ref->last_measure : INT_MAX;
    CHECK(chain_build_meter_input(&input->set, voice_lines, analysis_chain_segments(c), NULL, ref->first_measure, last, NULL, NULL, NULL, &meter) ==
          CHAIN_INPUT_OK);
    size_t soprano_notes = 0;
    for (size_t n = 0; n < meter.input.note_count; n++) {
        bool in_line = false;
        for (size_t i = 0; i < voice_lines->count && !in_line; i++) {
            in_line = voice_lines->soprano[i].present && strcmp(entity_id_value(voice_lines->soprano[i].note_id), meter.input.notes[n].id) == 0;
        }
        CHECK((meter.input.notes[n].line == 0) == in_line);
        soprano_notes += in_line ? 1 : 0;
    }
    CHECK(soprano_notes > 0 && soprano_notes <= runs);
    chain_meter_input_destroy(&meter);
}

static void check_piece(const Reference *ref, bool dump) {
    ChainInput input;
    chain_input_init(&input, ref);
    const char *context = ref->name;

    AnalysisChainConfig config = reference_config(ref);
    AnalysisChain *chain = NULL;
    AnalysisChainStatus status = analysis_chain_run(&input.set, input.events, input.event_count, &config, &chain);
    if (status != ANALYSIS_CHAIN_OK) {
        fprintf(stderr, "%s: the chain failed at %s\n", context, analysis_chain_status_name(status));
        abort();
    }
    size_t segments = analysis_chain_segment_count(chain);
    size_t key_regions = analysis_chain_key_regions(chain)->region_count;
    size_t cadences = analysis_chain_cadences(chain)->count;
    size_t phrases = analysis_chain_phrases(chain)->count;
    if (dump) {
        printf("    {\"%s\", \"%s\", %s, %d, %d, %d, %zu, %zu, %zu, %zu,\n     {", ref->name, ref->midi, ref->three_four ? "true" : "false",
               ref->tonic_pitch_class, ref->first_measure, ref->last_measure, segments, key_regions, cadences, phrases);
        for (int s = 0; s < STAGE_COUNT; s++) {
            printf("%s0x%016llxULL", s == 0 ? "" : (s % 3 == 0 ? ",\n      " : ", "), (unsigned long long)stage_digest(s, chain));
        }
        printf("}},\n");
    } else {
        CHECK(segments == ref->segments && key_regions == ref->key_regions && cadences == ref->cadences && phrases == ref->phrases);
        for (int s = 0; s < STAGE_COUNT; s++) {
            if (stage_digest(s, chain) != ref->digest[s]) {
                fprintf(stderr, "%s: the %s stage no longer gives its expected results\n", context, STAGE_NAMES[s]);
                abort();
            }
        }
        check_segment_lines(context, chain);
        check_stage_inputs(context, chain);
        check_voice_line_inputs(context, &input, ref, chain);
        check_voice_leading(context, chain);
        check_subsets(context, &input, ref, chain);
        printf("  %s: %zu segments, %zu key regions, %zu cadences, %zu phrases; %d stages as expected\n", context, segments, key_regions,
               cadences, phrases, STAGE_COUNT);
    }
    analysis_chain_destroy(chain);
    chain_input_destroy(&input);
}

int main(void) {
    bool dump = getenv("URSATZ_CHAIN_DUMP") != NULL;
    const char *only = getenv("URSATZ_CHAIN_PIECE");
    static const char *const PREFIXES[4] = {"bach114", "mozart545", "clementi36", "field1"};
    if (!dump) {
        printf("the analysis chain against its expected results:\n");
    }
    for (size_t i = 0; i < 4; i++) {
        if (only != NULL && strcmp(PREFIXES[i], only) != 0) {
            continue;
        }
        check_piece(&REFERENCES[i], dump);
    }
    if (!dump) {
        printf("test_analysis_chain: ok\n");
    }
    return 0;
}
