#ifndef URSATZ_TESTS_ANALYSIS_WEIGHTED_PIPELINE_H
#define URSATZ_TESTS_ANALYSIS_WEIGHTED_PIPELINE_H

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "events/note_event.h"
#include "io/import_notes.h"
#include "io/midi_importer.h"
#include "pitch/pitch_arithmetic.h"
#include "segmentation_beat_grid.h"
#include "ursatz_chain/analysis_chain.h"
#include "../rational_test_util.h"

/*
 * Test helper for weighted chord identification: imports a corpus MIDI file and runs the segment and outer-voice stages of the analysis
 * chain on its notes (ursatz_chain), on the beat grid of a MeterMap (standard-meter's provider). The fields keep the names the tests
 * read: the importer's events and pitch values, the adaptive sonority segments, the segments with their CommonPracticeMembers, and the
 * segmentation without the merge over unchanged harmony (one segment per melody note) the outer lines are read on. They view the import
 * and the chain this struct owns.
 */

typedef struct {
    MidiImport import;
    NoteEvent **events;
    PitchNumericValue **pitch_values;
    size_t note_count;
    MidiMeterImport meter;
    AnalysisNote *notes;
    AnalysisChain *chain;
    AdaptiveSonorities sonorities;
    AdaptiveSonorities voice_sonorities;
    CommonPracticeMember *members; /* every segment's members, one after another */
    CommonPracticeSegment *segments;
    size_t segment_count;
    char (*segment_ids)[32];
    double *starts; /* each segment's window, in whole notes */
    double *ends;
} WeightedPipeline;

static inline unsigned char *pipeline_read_file(const char *path, size_t *out_size) {
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
    data[length] = '\0';
    *out_size = (size_t)length;
    return data;
}

/* Runs the pipeline over a corpus file. `map` is the MeterMap to segment and read strengths with, or NULL for the file's own. */
static inline WeightedPipeline pipeline_run(const char *corpus_dir, const char *file_name, const MeterMap *map) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", corpus_dir, file_name);
    size_t size = 0;
    unsigned char *data = pipeline_read_file(path, &size);

    WeightedPipeline p;
    memset(&p, 0, sizeof(p));
    assert(midi_importer_import_complete(data, size, &p.import) == MIDI_IMPORT_OK);
    free(data);
    p.events = p.import.events;
    p.pitch_values = p.import.pitch_values;
    p.note_count = p.import.count;
    p.meter = p.import.meter;
    if (map == NULL) {
        map = p.meter.meter_map;
    }

    size_t note_count = 0;
    assert(midi_import_analysis_notes(&p.import, &p.notes, &note_count));
    AnalysisNoteSet set = {p.notes, note_count, map};
    AnalysisChainConfig config = analysis_chain_config_default();
    config.stages = ANALYSIS_CHAIN_STAGE_SEGMENTS | ANALYSIS_CHAIN_STAGE_OUTER_VOICES;
    config.first_measure = 0;
    assert(analysis_chain_run(&set, NULL, 0, &config, &p.chain) == ANALYSIS_CHAIN_OK);

    const ChainSegments *segments = analysis_chain_segments(p.chain);
    p.sonorities = segments->sonorities;
    p.voice_sonorities = analysis_chain_voice_segments(p.chain)->sonorities;
    p.members = segments->members;
    p.segments = segments->segments;
    p.segment_count = segments->count;
    p.segment_ids = segments->ids;
    p.starts = segments->starts;
    p.ends = segments->ends;
    return p;
}

static inline void pipeline_destroy(WeightedPipeline *p) {
    analysis_chain_destroy(p->chain);
    import_analysis_notes_destroy(p->notes, p->import.count);
    midi_import_destroy(&p->import);
    memset(p, 0, sizeof(*p));
}

#endif
