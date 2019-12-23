#include "pipeline/segmentation.h"

#include <stdlib.h>

#include "time/musical_time.h"
#include "time/temporal_anchor.h"

static bool onset_equals(const NoteEvent *a, const NoteEvent *b) {
    const MusicalTime *ta = temporal_anchor_position(note_event_temporal_anchor(a));
    const MusicalTime *tb = temporal_anchor_position(note_event_temporal_anchor(b));
    long long lhs = (long long)musical_time_numerator(ta) * (long long)musical_time_denominator(tb);
    long long rhs = (long long)musical_time_numerator(tb) * (long long)musical_time_denominator(ta);
    return lhs == rhs;
}

bool segment_by_onset(NoteEvent *const *events, size_t count, SonorityGrouping *out) {
    if (count == 0) {
        out->groups = NULL;
        out->group_count = 0;
        return true;
    }

    SonorityGroup *groups = malloc(sizeof(SonorityGroup) * count);
    if (!groups) {
        return false;
    }
    size_t group_count = 0;

    size_t i = 0;
    while (i < count) {
        size_t j = i + 1;
        while (j < count && onset_equals(events[i], events[j])) {
            j++;
        }
        size_t group_size = j - i;

        size_t *indices = malloc(sizeof(size_t) * group_size);
        if (!indices) {
            for (size_t k = 0; k < group_count; k++) {
                free(groups[k].note_indices);
            }
            free(groups);
            return false;
        }
        for (size_t k = 0; k < group_size; k++) {
            indices[k] = i + k;
        }

        groups[group_count].note_indices = indices;
        groups[group_count].note_index_count = group_size;
        group_count++;
        i = j;
    }

    out->groups = groups;
    out->group_count = group_count;
    return true;
}

void sonority_grouping_destroy(SonorityGrouping *grouping) {
    if (!grouping) {
        return;
    }
    for (size_t i = 0; i < grouping->group_count; i++) {
        free(grouping->groups[i].note_indices);
    }
    free(grouping->groups);
    grouping->groups = NULL;
    grouping->group_count = 0;
}
