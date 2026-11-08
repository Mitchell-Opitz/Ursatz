#include "harmonic_spans.h"

#define BASS_PERSIST_SEMITONES 12
#define TIME_TOLERANCE 1e-9

/* The lowest pitch sounding at the cell's start, or -1 when nothing does. */
static int lowest_at_start(const HarmonicSpanCell *cell) {
    int lowest = -1;
    for (size_t i = 0; i < cell->segment->member_count; i++) {
        const CommonPracticeMember *member = &cell->segment->members[i];
        if (!member->held && member->onset > cell->start + TIME_TOLERANCE) {
            continue; /* attacked after the start */
        }
        if (lowest < 0 || member->pitch < lowest) {
            lowest = member->pitch;
        }
    }
    return lowest;
}

bool harmonic_span_basses(const HarmonicSpanCell *cells, size_t count, int *out_bass) {
    if (count == 0) {
        return true;
    }
    if (cells == NULL || out_bass == NULL) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (cells[i].segment == NULL || (cells[i].segment->members == NULL && cells[i].segment->member_count > 0)) {
            return false;
        }
    }

    int recent = -1;
    for (size_t i = 0; i < count; i++) {
        bool first_of_measure = i == 0 || cells[i].measure != cells[i - 1].measure;
        int lowest = lowest_at_start(&cells[i]);
        int bass;
        if (lowest < 0) {
            bass = recent;
        } else if (!first_of_measure && recent >= 0 && lowest - recent >= BASS_PERSIST_SEMITONES) {
            bass = recent;
        } else {
            bass = lowest;
        }
        out_bass[i] = bass;
        recent = bass;
    }
    return true;
}
