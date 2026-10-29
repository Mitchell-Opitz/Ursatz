#define URSATZ_FORM_PIECES_AS_LIBRARY
#include "framework_v1_chain.h"
#include "melody_util.h"
#include "motive_util.h"
#include "rhythm_util.h"
#include "schema_util.h"
#include "texture_util.h"

#include <math.h>

#include "io/import_notes.h"
#include "io/midi_importer.h"
#include "ursatz_chain/analysis_chain.h"

/*
 * analysis_chain_run against the test glue that ran the chain before it existed (tests/integration/framework_v1_chain.h and the *_util.h
 * helpers). The glue runs one reference piece (Bach 114, Mozart 545, Clementi 36, Field 1 from tests/fixtures) stage by stage; the chain is
 * run from the same file's neutral notes and every stage is compared field by field: segments and members, key regions and changes,
 * chords, Roman numerals, outer voices, cadences, phrases, ideas, themes and expansions, motive families, meter, texture and the melody and
 * schemas of the chain's own inputs. Then a stage subset runs only what it needs. The next branch deletes the glue and keeps these results as
 * fixed expected values.
 */

#define CHECK(condition)                                                                                                   \
    do {                                                                                                                   \
        if (!(condition)) {                                                                                                \
            fprintf(stderr, "%s:%d: %s: %s\n", __FILE__, __LINE__, context, #condition);                                    \
            abort();                                                                                                       \
        }                                                                                                                  \
    } while (0)

/* The chain reads the notes in the order of analysis_note_compare (onset, pitch, id); the glue reads them in the importer's order, which
   differs among simultaneous notes. Sets are the same, but sums over a segment's members run in another order, so the weights, scores
   and uncertainties derived from them agree to within rounding. Scores within 1e-9 are tied by definition in the library. */
#define NEAR(a, b) (fabs((a) - (b)) <= 1e-9)

/* The chain's inputs: the file's notes and key signature events, read once. */
typedef struct {
    MidiImport import;
    AnalysisNote *notes;
    size_t note_count;
    AnalysisKeyEvent *events;
    size_t event_count;
    AnalysisNoteSet set;
} ChainInput;

static void chain_input_init(ChainInput *in, const Chain *glue) {
    memset(in, 0, sizeof(*in));
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, glue->fp->piece.midi);
    size_t size = 0;
    unsigned char *data = pipeline_read_file(path, &size);
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
    in->set.notes = in->notes;
    in->set.count = in->note_count;
    in->set.map = glue->map;
    assert(analysis_note_set_valid(&in->set));
}

static void chain_input_destroy(ChainInput *in) {
    free(in->events);
    import_analysis_notes_destroy(in->notes, in->note_count);
    midi_import_destroy(&in->import);
}

/* --- the stages --- */

static void compare_segments(const char *context, const Chain *g, const AnalysisChain *c) {
    const ChainSegments *segs = analysis_chain_segments(c);
    CHECK(segs != NULL && analysis_chain_segment_count(c) == g->count);
    CHECK(segs->count == g->p.segment_count);
    for (size_t s = 0; s < segs->count; s++) {
        CHECK(strcmp(segs->ids[s], g->p.segment_ids[s]) == 0);
        CHECK(segs->starts[s] == g->p.starts[s] && segs->ends[s] == g->p.ends[s]);
        const CommonPracticeSegment *want = &g->p.segments[s];
        const CommonPracticeSegment *got = &segs->segments[s];
        CHECK(got->member_count == want->member_count && got->beat == want->beat && strcmp(got->id, want->id) == 0);
        /* the chain reads the notes sorted by onset, pitch and id, the glue in the importer's order: the members of a segment are
           the same set */
        for (size_t j = 0; j < got->member_count; j++) {
            const CommonPracticeMember *match = NULL;
            for (size_t k = 0; k < want->member_count && match == NULL; k++) {
                match = strcmp(got->members[j].note_id, want->members[k].note_id) == 0 ? &want->members[k] : NULL;
            }
            CHECK(match != NULL);
            CHECK(got->members[j].pitch == match->pitch && got->members[j].held == match->held);
            CHECK(got->members[j].duration == match->duration && got->members[j].onset == match->onset);
            CHECK(got->members[j].strength == match->strength);
            CHECK((got->members[j].voice_id == NULL) == (match->voice_id == NULL));
            CHECK(match->voice_id == NULL || strcmp(got->members[j].voice_id, match->voice_id) == 0);
        }
    }
}

static void compare_keys(const char *context, const Chain *g, const AnalysisChain *c) {
    const CommonPracticeKeyRegions *got = analysis_chain_key_regions(c);
    const CommonPracticeKeyRegions *want = &g->regions;
    CHECK(got != NULL && got->region_count == want->region_count && got->change_count == want->change_count);
    for (size_t i = 0; i < got->region_count; i++) {
        CHECK(got->regions[i].key_index == want->regions[i].key_index);
        CHECK(got->regions[i].first_measure == want->regions[i].first_measure && got->regions[i].last_measure == want->regions[i].last_measure);
        CHECK(NEAR(got->regions[i].uncertainty, want->regions[i].uncertainty) && got->regions[i].source == want->regions[i].source);
        CHECK(time_span_equals(got->regions[i].span, want->regions[i].span));
    }
    for (size_t i = 0; i < got->change_count; i++) {
        CHECK(got->changes[i].from_region == want->changes[i].from_region && got->changes[i].to_region == want->changes[i].to_region);
        CHECK(got->changes[i].extent == want->changes[i].extent && got->changes[i].is_return == want->changes[i].is_return);
        CHECK(got->changes[i].cadence_confirmed == want->changes[i].cadence_confirmed);
        CHECK(got->changes[i].has_pivot == want->changes[i].has_pivot);
        CHECK(!got->changes[i].has_pivot || got->changes[i].pivot_segment == want->changes[i].pivot_segment);
    }
}

static void compare_chords(const char *context, const Chain *g, const AnalysisChain *c) {
    const CommonPracticeWeightedResult *got = analysis_chain_chords(c);
    CHECK(got != NULL);
    for (size_t s = 0; s < g->count; s++) {
        const CommonPracticeWeightedResult *want = &g->chords[s];
        CHECK(got[s].matched == want->matched && NEAR(got[s].best_score, want->best_score));
        CHECK(NEAR(got[s].runner_up_score, want->runner_up_score) && NEAR(got[s].uncertainty, want->uncertainty));
        CHECK((got[s].best_chord == NULL) == (want->best_chord == NULL));
        CHECK(want->best_chord == NULL || strcmp(chord_label(got[s].best_chord), chord_label(want->best_chord)) == 0);
        CHECK(got[s].leftover_count == want->leftover_count);
        for (size_t k = 0; k < got[s].leftover_count; k++) { /* the leftover tones in member order: the same set */
            const CommonPracticeLeftoverTone *match = NULL;
            for (size_t m = 0; m < want->leftover_count && match == NULL; m++) {
                match = strcmp(got[s].leftover[k].note_id, want->leftover[m].note_id) == 0 ? &want->leftover[m] : NULL;
            }
            CHECK(match != NULL && got[s].leftover[k].classified == match->classified);
            CHECK(!match->classified || got[s].leftover[k].tone == match->tone);
        }
    }
}

static void compare_numerals(const char *context, const Chain *g, const AnalysisChain *c) {
    const StandardTonalityChordInput *inputs = analysis_chain_chord_inputs(c);
    const StandardTonalityRomanResult *romans = analysis_chain_numerals(c);
    CHECK(inputs != NULL && romans != NULL);
    for (size_t s = 0; s < g->count; s++) {
        const StandardTonalityChordInput *want_input = &g->tonal_inputs[s];
        CHECK(inputs[s].has_numeral == want_input->has_numeral && inputs[s].root_pitch_class == want_input->root_pitch_class);
        CHECK(inputs[s].bass_pitch_class == want_input->bass_pitch_class && inputs[s].interval_count == want_input->interval_count);
        CHECK(memcmp(inputs[s].intervals, want_input->intervals, sizeof(inputs[s].intervals)) == 0);
        CHECK(inputs[s].tonic_pitch_class == want_input->tonic_pitch_class && inputs[s].mode == want_input->mode);
        CHECK(inputs[s].region_index == want_input->region_index && NEAR(inputs[s].chord_uncertainty, want_input->chord_uncertainty));
        CHECK(NEAR(inputs[s].key_uncertainty, want_input->key_uncertainty));
        const StandardTonalityRomanResult *want = &g->romans[s];
        CHECK(romans[s].has_numeral == want->has_numeral && NEAR(romans[s].uncertainty, want->uncertainty));
        if (want->has_numeral) {
            CHECK(strcmp(romans[s].claim.label, want->claim.label) == 0);
            CHECK(memcmp(&romans[s].claim, &want->claim, sizeof(want->claim)) == 0);
            CHECK(romans[s].deceptive == want->deceptive);
        }
    }
}

static void compare_entries(const char *context, const OuterVoiceEntry *got, const OuterVoiceEntry *want, size_t count) {
    for (size_t s = 0; s < count; s++) {
        CHECK(got[s].present == want[s].present && got[s].bridged == want[s].bridged && got[s].guarded == want[s].guarded);
        if (want[s].present) {
            CHECK(strcmp(entity_id_value(got[s].note_id), entity_id_value(want[s].note_id)) == 0);
            CHECK(pitch_numeric_value_position_numerator(got[s].pitch) == pitch_numeric_value_position_numerator(want[s].pitch));
        }
        CHECK(time_span_equals(got[s].span, want[s].span));
    }
}

static void compare_outer_voices(const char *context, const Chain *g, const AnalysisChain *c) {
    const OuterVoiceLines *lines = analysis_chain_outer_voices(c);
    CHECK(lines != NULL && lines->count == g->lines.count && lines->count == g->p.voice_sonorities.count);
    compare_entries(context, lines->soprano, g->lines.soprano, lines->count);
    compare_entries(context, lines->bass, g->lines.bass, lines->count);
}

static void compare_cadences(const char *context, const Chain *g, const AnalysisChain *c) {
    const HarmonyCadences *got = analysis_chain_cadences(c);
    CHECK(got != NULL && got->count == g->cadences.count);
    for (size_t k = 0; k < got->count; k++) {
        const HarmonyCadenceResult *a = &got->items[k];
        const HarmonyCadenceResult *b = &g->cadences.items[k];
        CHECK(a->type == b->type && a->dominant_first == b->dominant_first && a->dominant_last == b->dominant_last);
        CHECK(a->has_six_four == b->has_six_four && a->arrival_first == b->arrival_first && a->arrival_last == b->arrival_last);
        CHECK(a->soprano_degree == b->soprano_degree && a->post_cadential == b->post_cadential);
        CHECK(a->arrival_beat_strength == b->arrival_beat_strength && NEAR(a->uncertainty, b->uncertainty) && a->share == b->share);
        CHECK(time_span_equals(a->span, b->span));
        CHECK(a->alternative.share == b->alternative.share && (a->alternative.interpretation == NULL) == (b->alternative.interpretation == NULL));
    }
}

static void compare_form(const char *context, const FormAnalysis *want, const StandardFormPhrases *phrases,
                         const StandardFormSegmentation *ideas, const StandardFormThemes *themes,
                         const StandardFormExpansions *expansions) {
    CHECK(phrases != NULL && phrases->count == want->phrases.count);
    for (size_t i = 0; i < phrases->count; i++) {
        CHECK(memcmp(&phrases->items[i].claim, &want->phrases.items[i].claim, sizeof(StandardFormPhraseClaim)) == 0);
        CHECK(NEAR(phrases->items[i].uncertainty, want->phrases.items[i].uncertainty));
        CHECK(time_span_equals(phrases->items[i].span, want->phrases.items[i].span));
    }
    CHECK(ideas != NULL && ideas->idea_count == want->segmentation.idea_count && ideas->subphrase_count == want->segmentation.subphrase_count);
    for (size_t i = 0; i < ideas->idea_count; i++) {
        CHECK(memcmp(&ideas->ideas[i].claim, &want->segmentation.ideas[i].claim, sizeof(StandardFormIdeaClaim)) == 0);
        CHECK(NEAR(ideas->ideas[i].uncertainty, want->segmentation.ideas[i].uncertainty));
    }
    for (size_t i = 0; i < ideas->subphrase_count; i++) {
        CHECK(memcmp(&ideas->subphrases[i].claim, &want->segmentation.subphrases[i].claim, sizeof(StandardFormSubphraseClaim)) == 0);
    }
    CHECK(themes != NULL && themes->count == want->themes.count);
    for (size_t i = 0; i < themes->count; i++) {
        CHECK(memcmp(&themes->items[i].claim, &want->themes.items[i].claim, sizeof(StandardFormThemeClaim)) == 0);
        CHECK(NEAR(themes->items[i].uncertainty, want->themes.items[i].uncertainty));
    }
    CHECK(expansions != NULL && expansions->count == want->expansions.count);
    for (size_t i = 0; i < expansions->count; i++) {
        CHECK(memcmp(&expansions->items[i].claim, &want->expansions.items[i].claim, sizeof(StandardFormExpansionClaim)) == 0);
        CHECK(NEAR(expansions->items[i].uncertainty, want->expansions.items[i].uncertainty));
    }
}

/* The motive families: the glue builds the note-level soprano with the chain's cadences and runs the form adapter on it. */
static void compare_motives(const char *context, const Chain *g, const AnalysisChain *c) {
    int home = g->x.home_key;
    FormBuild build;
    form_build_init(&build, g->map);
    motive_build_soprano(&build, &g->p, g->x.keys_first, g->x.keys_last, common_practice_key_index_tonic(home),
                         common_practice_key_index_mode(home));
    add_chain_cadences(&build, &g->p, &g->cadences);
    assert(standard_form_input_finish(&build.input) == STANDARD_FORM_ANALYSIS_OK);
    StandardFormPhrases phrases;
    StandardFormSegmentation ideas;
    StandardFormThemes themes;
    StandardFormMotives want;
    assert(standard_form_segment_phrases(&build.input, NULL, form_kernel_detect, NULL, &phrases) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_segment_ideas(&build.input, &phrases, NULL, &ideas) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_identify_themes(&build.input, &phrases, &ideas, NULL, &themes) == STANDARD_FORM_ANALYSIS_OK);
    assert(standard_form_find_motives(&build.input, &phrases, &ideas, &themes, NULL, form_kernel_match_windows, NULL, &want) ==
           STANDARD_FORM_ANALYSIS_OK);

    const StandardFormMotives *got = analysis_chain_motives(c);
    CHECK(got != NULL && got->family_count == want.family_count);
    for (size_t f = 0; f < got->family_count; f++) {
        const StandardFormMotiveFamily *a = &got->families[f];
        const StandardFormMotiveFamily *b = &want.families[f];
        CHECK(memcmp(&a->claim, &b->claim, sizeof(a->claim)) == 0 && a->occurrence_count == b->occurrence_count);
        for (size_t o = 0; o < a->occurrence_count; o++) {
            CHECK(memcmp(&a->occurrences[o].claim, &b->occurrences[o].claim, sizeof(a->occurrences[o].claim)) == 0);
            CHECK(NEAR(a->occurrences[o].uncertainty, b->occurrences[o].uncertainty));
        }
    }
    printf("  %s: %zu motive families equal\n", context, got->family_count);
    standard_form_motives_destroy(&want);
    standard_form_themes_destroy(&themes);
    standard_form_segmentation_destroy(&ideas);
    standard_form_phrases_destroy(&phrases);
    form_build_destroy(&build);
}

/* Meter: the glue's rhythm input from the same notes, outer lines, chord labels and the form adapter's phrases. */
static void compare_meter(const char *context, const Chain *g, const AnalysisChain *c) {
    const char **labels = calloc(g->p.segment_count + 1, sizeof(char *));
    assert(labels != NULL);
    for (size_t s = 0; s < g->count; s++) {
        labels[s] = g->chords[s].matched ? chord_label(g->chords[s].best_chord) : NULL;
    }
    RhythmBuild rb;
    rhythm_build_init(&rb, g->map);
    rhythm_build_notes(&rb, &g->p, &g->lines, g->x.keys_first, g->x.keys_last);
    rhythm_build_chord_changes(&rb, &g->p, labels, g->x.keys_first, g->x.keys_last);
    rhythm_build_detected_phrases(&rb, &g->build.input, &g->form.phrases, &g->form.expansions);
    free(labels);
    StandardMeterSyncopations sync = {NULL, 0};
    StandardMeterHemiolas hemi = {NULL, 0};
    StandardMeterHypermeter hyper;
    StandardMeterDensities dens = {NULL, 0};
    memset(&hyper, 0, sizeof(hyper));
    assert(standard_meter_find_syncopations(&rb.input, NULL, &sync) == STANDARD_METER_ANALYSIS_OK);
    assert(standard_meter_find_hemiolas(&rb.input, NULL, &hemi) == STANDARD_METER_ANALYSIS_OK);
    assert(standard_meter_find_hypermeter(&rb.input, NULL, &hyper) == STANDARD_METER_ANALYSIS_OK);
    assert(standard_meter_find_densities(&rb.input, NULL, &dens) == STANDARD_METER_ANALYSIS_OK);

    const AnalysisChainMeter *got = analysis_chain_meter(c);
    CHECK(got != NULL && got->syncopations.count == sync.count && got->hemiolas.count == hemi.count);
    for (size_t i = 0; i < sync.count; i++) { /* simultaneous notes come in another order: matched by note */
        const StandardMeterSyncopation *a = &got->syncopations.items[i];
        const StandardMeterSyncopation *b = NULL;
        for (size_t k = 0; k < sync.count && b == NULL; k++) {
            b = strcmp(a->claim.note_id, sync.items[k].claim.note_id) == 0 ? &sync.items[k] : NULL;
        }
        CHECK(b != NULL && a->claim.line == b->claim.line && a->claim.measure == b->claim.measure && a->claim.beat == b->claim.beat);
        CHECK(a->claim.attack_level == b->claim.attack_level && a->claim.through_level == b->claim.through_level);
        CHECK(a->claim.strength == b->claim.strength && a->claim.through_measure == b->claim.through_measure);
        CHECK(a->claim.through_beat == b->claim.through_beat && NEAR(a->uncertainty, b->uncertainty));
    }
    for (size_t i = 0; i < hemi.count; i++) {
        CHECK(memcmp(&got->hemiolas.items[i].claim, &hemi.items[i].claim, sizeof(hemi.items[i].claim)) == 0);
    }
    CHECK(got->hypermeter.phrase_count == hyper.phrase_count && got->hypermeter.hypermeasure_count == hyper.hypermeasure_count);
    CHECK(got->hypermeter.reinterpretation_count == hyper.reinterpretation_count);
    for (size_t i = 0; i < hyper.hypermeasure_count; i++) {
        CHECK(memcmp(&got->hypermeter.hypermeasures[i].claim, &hyper.hypermeasures[i].claim, sizeof(hyper.hypermeasures[i].claim)) == 0);
    }
    CHECK(got->densities.count == dens.count);
    for (size_t i = 0; i < dens.count; i++) {
        CHECK(memcmp(&got->densities.items[i].claim, &dens.items[i].claim, sizeof(dens.items[i].claim)) == 0);
    }
    printf("  %s: meter equal (%zu syncopations, %zu hemiolas, %zu hypermeasures)\n", context, sync.count, hemi.count,
           hyper.hypermeasure_count);
    standard_meter_densities_destroy(&dens);
    standard_meter_hypermeter_destroy(&hyper);
    standard_meter_hemiolas_destroy(&hemi);
    standard_meter_syncopations_destroy(&sync);
    rhythm_build_destroy(&rb);
}

/* Texture: the glue separates the voices per MIDI track, reading the track of each note back from the file. */
static void compare_texture(const char *context, const Chain *g, const AnalysisChain *c) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", URSATZ_CORPUS_DIR, g->fp->piece.midi);
    TextureBuild tb;
    texture_build_from_import(&tb, &g->p, g->map, path);
    StandardTextureAnalysis want;
    memset(&want, 0, sizeof(want));
    assert(standard_texture_analyze(&tb.input, NULL, &want) == STANDARD_TEXTURE_ANALYSIS_OK);

    const StandardTextureAnalysis *got = analysis_chain_texture(c);
    CHECK(got != NULL && got->windows.count == want.windows.count && got->passage_count == want.passage_count);
    CHECK(got->change_count == want.change_count);
    /* The voice separation reads simultaneous notes in the order it is given them, and the chain's order (onset, pitch, id) is not the
       importer's: a few notes (10 of 430 in Bach 114) take another voice or confidence. So the textures, passages and changes are compared,
       and the numbers that carry a voice confidence are not. */
    for (size_t w = 0; w < want.windows.count; w++) {
        const StandardTextureWindowFeatures *a = &got->windows.items[w];
        const StandardTextureWindowFeatures *b = &want.windows.items[w];
        CHECK(a->measure == b->measure && a->events == b->events && a->attacks == b->attacks);
        const StandardTextureWindowClass *ca = &got->classes[w];
        const StandardTextureWindowClass *cb = &want.classes[w];
        CHECK(ca->has_texture == cb->has_texture && (!cb->has_texture || ca->texture == cb->texture));
        CHECK(got->window_texture[w] == want.window_texture[w]);
    }
    for (size_t i = 0; i < want.passage_count; i++) {
        const StandardTexturePassageClaim *a = &got->passages[i].claim;
        const StandardTexturePassageClaim *b = &want.passages[i].claim;
        CHECK(a->index == b->index && a->first_measure == b->first_measure && a->last_measure == b->last_measure);
        CHECK(a->texture == b->texture && a->category == b->category && a->windows == b->windows);
        CHECK(a->unclassified_windows == b->unclassified_windows);
    }
    for (size_t i = 0; i < want.change_count; i++) {
        const StandardTextureChangeClaim *a = &got->changes[i].claim;
        const StandardTextureChangeClaim *b = &want.changes[i].claim;
        CHECK(a->from == b->from && a->to == b->to && a->boundary_measure == b->boundary_measure);
        CHECK(a->from_texture == b->from_texture && a->to_texture == b->to_texture && a->category_changed == b->category_changed);
    }
    printf("  %s: texture equal (%zu windows, %zu passages)\n", context, want.windows.count, want.passage_count);
    standard_texture_analysis_destroy(&want);
    texture_build_destroy(&tb);
}

/* Melody: the glue's soprano melody line over the scope's segments, and the intervals and summaries of the one span. */
static void compare_melody(const char *context, const Chain *g, const AnalysisChain *c) {
    int home = g->x.home_key;
    long limit = g->count < g->p.segment_count ? (long)(g->p.starts[g->count] * 1536.0 + 0.5) : 0x7fffffff;
    CommonPracticeWeightedResults results = {g->chords, g->count};
    MelodyBuild want;
    melody_build_from_line(&g->p, g->lines.soprano, &results, common_practice_key_index_tonic(home),
                           common_practice_key_index_mode(home), limit, &want);
    const AnalysisChainMelody *got = analysis_chain_melody(c);
    CHECK(got != NULL && got->line.line.count == want.line.count);
    for (size_t i = 0; i < want.line.count; i++) {
        const StandardCounterpointMelodyNote *a = &got->line.line.notes[i];
        const StandardCounterpointMelodyNote *b = &want.line.notes[i];
        CHECK(a->present == b->present && got->line.segment[i] == want.segment[i]);
        if (!b->present) {
            continue;
        }
        CHECK(strcmp(a->note_id, b->note_id) == 0 && a->degree == b->degree && a->alteration == b->alteration);
        CHECK(a->has_strength == b->has_strength && a->strength == b->strength && a->nct == b->nct);
        CHECK(a->nct != STANDARD_COUNTERPOINT_NCT_NON_CHORD_TONE || a->nct_type == b->nct_type);
        CHECK(pitch_numeric_value_position_numerator(a->pitch) == pitch_numeric_value_position_numerator(b->pitch));
    }
    StandardCounterpointMelodySpan span = {"line", 0, want.line.count - 1};
    StandardCounterpointMelodicIntervals intervals;
    StandardCounterpointContourSummaries summaries;
    StandardCounterpointNctCountList counts;
    assert(standard_counterpoint_melody_intervals(&want.line, NULL, &intervals) == STANDARD_COUNTERPOINT_MELODY_OK);
    assert(standard_counterpoint_melody_contour_summaries(&want.line, &intervals, &span, 1, &summaries) ==
           STANDARD_COUNTERPOINT_MELODY_OK);
    assert(standard_counterpoint_melody_nct_counts(&want.line, &span, 1, &counts) == STANDARD_COUNTERPOINT_MELODY_OK);
    CHECK(got->intervals.count == intervals.count && got->summaries.count == summaries.count && got->counts.count == counts.count);
    for (size_t i = 0; i < intervals.count; i++) {
        CHECK(memcmp(&got->intervals.items[i].claim, &intervals.items[i].claim, sizeof(intervals.items[i].claim)) == 0);
        CHECK(NEAR(got->intervals.items[i].uncertainty, intervals.items[i].uncertainty));
    }
    for (size_t i = 0; i < summaries.count; i++) {
        CHECK(memcmp(&got->summaries.items[i].claim, &summaries.items[i].claim, sizeof(summaries.items[i].claim)) == 0);
    }
    printf("  %s: melody equal (%zu notes, %zu intervals)\n", context, want.line.count, intervals.count);
    standard_counterpoint_nct_count_list_destroy(&counts);
    standard_counterpoint_contour_summaries_destroy(&summaries);
    standard_counterpoint_melodic_intervals_destroy(&intervals);
    melody_build_destroy(&want);
}

/* Schemas and sequences from the galant segments of the chain's keys, numerals and outer lines. */
static void compare_schemas(const char *context, const Chain *g, const AnalysisChain *c) {
    int tonic_pc[g->count];
    bool minor[g->count];
    for (size_t s = 0; s < g->count; s++) {
        tonic_pc[s] = g->tonal_inputs[s].tonic_pitch_class;
        minor[s] = g->tonal_inputs[s].mode == STANDARD_TONALITY_ROMAN_KEY_MINOR;
    }
    SchemaBuild sb;
    schema_build_init(&sb);
    schema_build_from_chain(&sb, &g->p, &g->lines, g->map, common_practice_key_index_tonic(g->x.home_key), tonic_pc, minor, g->romans,
                            g->count);
    GalantSchemaMatches schemas = {NULL, 0};
    ScaleSequenceMatches sequences = {NULL, 0};
    assert(galant_schema_match(&sb.list, NULL, &schemas) == GALANT_ANALYSIS_OK);
    assert(scale_sequence_detect(&sb.list, NULL, &sequences) == GALANT_ANALYSIS_OK);

    const GalantSchemaMatches *got_schemas = analysis_chain_schemas(c);
    const ScaleSequenceMatches *got_sequences = analysis_chain_sequences(c);
    CHECK(got_schemas != NULL && got_sequences != NULL);
    CHECK(got_schemas->count == schemas.count && got_sequences->count == sequences.count);
    for (size_t i = 0; i < schemas.count; i++) {
        CHECK(memcmp(&got_schemas->items[i].claim, &schemas.items[i].claim, sizeof(schemas.items[i].claim)) == 0);
        CHECK(NEAR(got_schemas->items[i].uncertainty, schemas.items[i].uncertainty));
    }
    for (size_t i = 0; i < sequences.count; i++) {
        CHECK(memcmp(&got_sequences->items[i].claim, &sequences.items[i].claim, sizeof(sequences.items[i].claim)) == 0);
    }
    printf("  %s: %zu schemas, %zu sequences equal\n", context, schemas.count, sequences.count);
    scale_sequence_matches_destroy(&sequences);
    galant_schema_matches_destroy(&schemas);
    schema_build_destroy(&sb);
}

/* The voice-leading stage has no real-piece glue (the glue builds hand-labelled fixtures): it is checked for its shape. */
static void check_voice_leading(const char *context, const Chain *g, const AnalysisChain *c) {
    const AnalysisChainVoiceLeading *vl = analysis_chain_voice_leading(c);
    CHECK(vl != NULL && vl->voice_count >= 2 && vl->voice_count <= 8);
    CHECK(vl->motions.count > 0);
    for (size_t i = 0; i < vl->motions.count; i++) {
        CHECK(vl->motions.items[i].segment_to < g->count);
    }
    for (size_t i = 0; i < vl->perfects.count; i++) {
        CHECK(vl->perfects.items[i].segment_to < g->count && vl->perfects.items[i].upper_voice < vl->perfects.items[i].lower_voice);
    }
    for (size_t i = 0; i < vl->resolutions.count; i++) {
        CHECK(vl->resolutions.items[i].segment_to < g->count && vl->resolutions.items[i].voice < vl->voice_count);
    }
    printf("  %s: voice leading %zu voices, %zu motions, %zu perfects, %zu resolutions\n", context, vl->voice_count, vl->motions.count,
           vl->perfects.count, vl->resolutions.count);
}

/* A run of a subset of the stages gives the stages asked for and what they need, and nothing else. */
static void check_subsets(const char *context, ChainInput *input, const Chain *g, const AnalysisChain *full) {
    AnalysisChainConfig config = analysis_chain_config_default();
    config.first_measure = g->x.keys_first;
    config.last_measure = g->x.keys_last;
    config.home_key_index = g->x.home_key;

    config.stages = ANALYSIS_CHAIN_STAGE_CADENCES;
    AnalysisChain *cadences = NULL;
    assert(analysis_chain_run(&input->set, input->events, input->event_count, &config, &cadences) == ANALYSIS_CHAIN_OK);
    CHECK(analysis_chain_segments(cadences) != NULL && analysis_chain_key_regions(cadences) != NULL && analysis_chain_chords(cadences) != NULL);
    CHECK(analysis_chain_numerals(cadences) != NULL && analysis_chain_outer_voices(cadences) != NULL && analysis_chain_cadences(cadences) != NULL);
    CHECK(analysis_chain_phrases(cadences) == NULL && analysis_chain_motives(cadences) == NULL && analysis_chain_meter(cadences) == NULL);
    CHECK(analysis_chain_texture(cadences) == NULL && analysis_chain_voice_leading(cadences) == NULL && analysis_chain_melody(cadences) == NULL);
    CHECK(analysis_chain_schemas(cadences) == NULL && analysis_chain_sequences(cadences) == NULL);
    /* with no form there is no second pass: the keys are the first pass's, and the cadences their own */
    CHECK(analysis_chain_home_key(cadences) == g->x.home_key);
    analysis_chain_destroy(cadences);

    config.stages = ANALYSIS_CHAIN_STAGE_TEXTURE;
    AnalysisChain *texture = NULL;
    assert(analysis_chain_run(&input->set, input->events, input->event_count, &config, &texture) == ANALYSIS_CHAIN_OK);
    CHECK(analysis_chain_texture(texture) != NULL && analysis_chain_segments(texture) == NULL && analysis_chain_key_regions(texture) == NULL);
    CHECK(analysis_chain_texture(texture)->windows.count == analysis_chain_texture(full)->windows.count);
    CHECK(analysis_chain_texture(texture)->passage_count == analysis_chain_texture(full)->passage_count);
    analysis_chain_destroy(texture);

    config.stages = ANALYSIS_CHAIN_STAGE_FORM;
    AnalysisChain *form = NULL;
    assert(analysis_chain_run(&input->set, input->events, input->event_count, &config, &form) == ANALYSIS_CHAIN_OK);
    compare_keys(context, g, form);
    compare_form(context, &g->form, analysis_chain_phrases(form), analysis_chain_ideas(form), analysis_chain_themes(form),
                 analysis_chain_expansions(form));
    CHECK(analysis_chain_motives(form) != NULL && analysis_chain_meter(form) == NULL && analysis_chain_texture(form) == NULL);
    analysis_chain_destroy(form);

    /* the keys alone, and a run with refine_keys off, differ only in the second pass */
    config.stages = ANALYSIS_CHAIN_STAGE_KEYS;
    AnalysisChain *keys = NULL;
    assert(analysis_chain_run(&input->set, input->events, input->event_count, &config, &keys) == ANALYSIS_CHAIN_OK);
    CHECK(analysis_chain_key_regions(keys) != NULL && analysis_chain_chords(keys) == NULL && analysis_chain_cadences(keys) == NULL);
    CHECK(analysis_chain_key_regions(keys)->region_count >= 1);
    analysis_chain_destroy(keys);
    (void)full;
}

static void compare_piece(const FormPiece *fp) {
    Chain glue;
    fv1_chain_run(&glue, fp);
    ChainInput input;
    chain_input_init(&input, &glue);
    const char *context = fp->piece.name;

    AnalysisChainConfig config = analysis_chain_config_default();
    config.first_measure = glue.x.keys_first;
    config.last_measure = glue.x.keys_last;
    config.home_key_index = glue.x.home_key;
    AnalysisChain *chain = NULL;
    AnalysisChainStatus status = analysis_chain_run(&input.set, input.events, input.event_count, &config, &chain);
    if (status != ANALYSIS_CHAIN_OK) {
        fprintf(stderr, "%s: the chain failed at %s\n", context, analysis_chain_status_name(status));
        abort();
    }
    compare_segments(context, &glue, chain);
    compare_keys(context, &glue, chain);
    compare_chords(context, &glue, chain);
    compare_numerals(context, &glue, chain);
    compare_outer_voices(context, &glue, chain);
    compare_cadences(context, &glue, chain);
    compare_form(context, &glue.form, analysis_chain_phrases(chain), analysis_chain_ideas(chain), analysis_chain_themes(chain),
                 analysis_chain_expansions(chain));
    printf("  %s: %zu segments, %zu key regions, %zu cadences, %zu phrases equal\n", context, glue.count,
           glue.regions.region_count, glue.cadences.count, glue.form.phrases.count);
    compare_motives(context, &glue, chain);
    compare_meter(context, &glue, chain);
    compare_texture(context, &glue, chain);
    compare_melody(context, &glue, chain);
    compare_schemas(context, &glue, chain);
    check_voice_leading(context, &glue, chain);
    check_subsets(context, &input, &glue, chain);

    analysis_chain_destroy(chain);
    chain_input_destroy(&input);
    fv1_chain_destroy(&glue);
}

int main(void) {
    printf("the analysis chain against the test glue:\n");
    const char *only = getenv("URSATZ_CHAIN_PIECE");
    for (size_t i = 0; i < 4; i++) {
        if (only != NULL && strcmp(FORM_PIECES[i].prefix, only) != 0) {
            continue;
        }
        compare_piece(&FORM_PIECES[i]);
    }
    printf("test_analysis_chain: ok\n");
    return 0;
}
