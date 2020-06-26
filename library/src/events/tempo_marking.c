#include "events/tempo_marking.h"

#include <stdlib.h>
#include <string.h>

struct TempoMarking {
    TemporalAnchor *temporal_anchor;
    char *text;
    MetronomeMarking *metronome_marking;
};

TempoMarkingResult tempo_marking_create(TemporalAnchor *temporal_anchor, const char *text,
                                        MetronomeMarking *metronome_marking, TempoMarking **out) {
    if (temporal_anchor == NULL) {
        return TEMPO_MARKING_ERR_NULL_TEMPORAL_ANCHOR;
    }
    bool has_text = text != NULL && text[0] != '\0';
    if (!has_text && metronome_marking == NULL) {
        return TEMPO_MARKING_ERR_EMPTY;
    }

    TempoMarking *tempo_marking = (TempoMarking *)malloc(sizeof(TempoMarking));
    if (tempo_marking == NULL) {
        return TEMPO_MARKING_ERR_ALLOC;
    }

    char *text_copy = NULL;
    if (has_text) {
        size_t len = strlen(text);
        text_copy = (char *)malloc(len + 1);
        if (text_copy == NULL) {
            free(tempo_marking);
            return TEMPO_MARKING_ERR_ALLOC;
        }
        memcpy(text_copy, text, len + 1);
    }

    tempo_marking->temporal_anchor = temporal_anchor;
    tempo_marking->text = text_copy;
    tempo_marking->metronome_marking = metronome_marking;

    *out = tempo_marking;
    return TEMPO_MARKING_OK;
}

void tempo_marking_destroy(TempoMarking *tempo_marking) {
    if (tempo_marking == NULL) {
        return;
    }
    temporal_anchor_destroy(tempo_marking->temporal_anchor);
    free(tempo_marking->text);
    metronome_marking_destroy(tempo_marking->metronome_marking);
    free(tempo_marking);
}

const TemporalAnchor *tempo_marking_temporal_anchor(const TempoMarking *tempo_marking) {
    return tempo_marking->temporal_anchor;
}

const char *tempo_marking_text(const TempoMarking *tempo_marking) {
    return tempo_marking->text;
}

bool tempo_marking_has_text(const TempoMarking *tempo_marking) {
    return tempo_marking->text != NULL;
}

const MetronomeMarking *tempo_marking_metronome_marking(const TempoMarking *tempo_marking) {
    return tempo_marking->metronome_marking;
}

bool tempo_marking_has_metronome_marking(const TempoMarking *tempo_marking) {
    return tempo_marking->metronome_marking != NULL;
}
