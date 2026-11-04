#include "pipeline/outer_voice_pipeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/rational.h"
#include "analysis/outer_voice.h"
#include "common-practice-minimal/key_vocabulary.h"
#include "common-practice-minimal/outer_scale_degrees.h"
#include "pipeline/pitch_class_util.h"
#include "pipeline/spelling_util.h"

/* The position of the segment's member that is the entry's note, or segment->member_count when it is none. */
static size_t member_of(const Segment *segment, const OuterVoiceEntry *entry) {
    const char *id = entity_id_value(entry->note_id);
    for (size_t m = 0; m < segment->member_count; m++) {
        if (strcmp(segment->members[m].note_id, id) == 0) {
            return m;
        }
    }
    return segment->member_count;
}

/* "E5" for a MIDI note number, spelled with flats or sharps. */
static char *pitch_name_create(int midi, bool prefer_flats) {
    char name[16];
    note_name(midi, prefer_flats, name);
    return strdup(name);
}

/* Fills one outer note from the library's entry for the segment. False on allocation failure. */
static bool fill_note(const ImportedNotes *notes, const Segment *segment, const OuterVoiceEntry *entry, const AnalysisKeyRegion *region,
                      AnalysisOuterNote *out) {
    if (!entry->present) {
        out->status = entry->bridged ? ANALYSIS_OUTER_BRIDGED
                                     : (entry->guarded ? ANALYSIS_OUTER_GUARDED : ANALYSIS_OUTER_ABSENT);
        return true;
    }
    size_t member = member_of(segment, entry);
    if (member >= segment->member_count) {
        out->status = ANALYSIS_OUTER_ABSENT;
        return true;
    }
    out->status = ANALYSIS_OUTER_PRESENT;
    out->note_id = segment->note_indices[member];
    out->pitch = segment->members[member].pitch;
    out->held = segment->members[member].held;
    char spelled[16];
    if (notes->source.spelling_notated &&
        spelled_note_name(notes->events[out->note_id], out->pitch, true, spelled)) {
        out->pitch_name = strdup(spelled);
    } else {
        out->pitch_name = pitch_name_create(out->pitch, region ? region->prefer_flats : false);
    }
    if (!out->pitch_name) {
        return false;
    }
    if (region) {
        const char *degree = common_practice_scale_degree_label(
            pitch_class_of(out->pitch), region->tonic_pitch_class,
            region->is_major ? COMMON_PRACTICE_KEY_MAJOR : COMMON_PRACTICE_KEY_MINOR);
        if (degree) {
            out->degree = strdup(degree);
            if (!out->degree) {
                return false;
            }
        }
    }
    return true;
}

/* a/b < c/d for positive denominators. */
static bool rational_less(long a, unsigned long b, long c, unsigned long d) {
    return rational_cmp(rational_ratio(a, (long long)b), rational_ratio(c, (long long)d)) < 0;
}

/* The voice segment holding the chord segment's start: the line's entry at the chord's onset. voices->segment_count
   when none does. */
static size_t voice_segment_at(const Segmentation *voices, const Segment *chord, size_t from) {
    for (size_t v = from; v < voices->segment_count; v++) {
        const Segment *voice = &voices->segments[v];
        if (!rational_less(chord->start_numerator, chord->start_denominator, voice->start_numerator,
                           voice->start_denominator) &&
            rational_less(chord->start_numerator, chord->start_denominator, voice->end_numerator,
                          voice->end_denominator)) {
            return v;
        }
    }
    return voices->segment_count;
}

bool collect_outer_voices(const ImportedNotes *notes, const Segmentation *voices, const Segmentation *segmentation,
                          const ChordEstimate *chords, const AnalysisKeyRegion *regions, const OuterVoiceLines *lines,
                          AnalysisReportEntry *entries) {
    size_t count = segmentation->segment_count;
    for (size_t i = 0; i < count; i++) {
        entries[i].in_region = chords[i].in_region;
        entries[i].region = chords[i].region;
        entries[i].soprano.status = ANALYSIS_OUTER_ABSENT;
        entries[i].bass.status = ANALYSIS_OUTER_ABSENT;
    }
    if (count == 0 || voices->segment_count == 0 || !lines || lines->count != voices->segment_count) {
        return true; /* the library gave no lines for this piece: every line stays absent */
    }

    bool ok = true;
    size_t v = 0;
    for (size_t i = 0; ok && i < count; i++) {
        v = voice_segment_at(voices, &segmentation->segments[i], v);
        if (v >= voices->segment_count) {
            v = 0;
            continue;
        }
        const AnalysisKeyRegion *region = chords[i].in_region ? &regions[chords[i].region] : NULL;
        ok = fill_note(notes, &voices->segments[v], &lines->soprano[v], region, &entries[i].soprano) &&
             fill_note(notes, &voices->segments[v], &lines->bass[v], region, &entries[i].bass);
    }
    return ok;
}
