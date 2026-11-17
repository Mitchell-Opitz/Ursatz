#include "analysis/notes/analysis_note.h"

#include <string.h>


bool analysis_note_valid(const AnalysisNote *note) {
    if (note == NULL || note->note_id == NULL || !time_span_holds(note->span)) {
        return false;
    }
    if (note->midi != ANALYSIS_PITCH_UNKNOWN && (note->midi < 0 || note->midi > 127)) {
        return false;
    }
    return note->voice_confidence >= 0.0 && note->voice_confidence <= 1.0;
}

int analysis_note_compare(const void *a, const void *b) {
    const AnalysisNote *x = a;
    const AnalysisNote *y = b;
    int c = musical_time_cmp(x->span.start, y->span.start);
    if (c != 0) {
        return c;
    }
    if (x->midi != y->midi) {
        return x->midi < y->midi ? -1 : 1;
    }
    return strcmp(x->note_id, y->note_id);
}

bool analysis_note_set_valid(const AnalysisNoteSet *set) {
    if (set == NULL || set->map == NULL || (set->count > 0 && set->notes == NULL)) {
        return false;
    }
    for (size_t i = 0; i < set->count; i++) {
        if (!analysis_note_valid(&set->notes[i])) {
            return false;
        }
        if (i > 0 && analysis_note_compare(&set->notes[i - 1], &set->notes[i]) > 0) {
            return false;
        }
    }
    return true;
}
