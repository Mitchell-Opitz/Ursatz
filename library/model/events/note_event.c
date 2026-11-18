#include "model/events/note_event.h"

#include <stdlib.h>

#include "model/time/tuplet.h"

struct NoteEvent {
    EntityID *id;
    TemporalAnchor *temporal_anchor;
    Duration duration;
    PitchIdentity *pitch;
    PitchSpelling *optional_pitch_spelling;
    VoiceReference *optional_voice;
    ProvenanceReference *provenance;
    bool has_tuplet;
    unsigned tuplet_actual;
    unsigned tuplet_normal;
};

NoteEventResult note_event_create(EntityID *id, MusicalTime onset, Duration duration,
                                   PitchIdentity *pitch, PitchSpelling *optional_pitch_spelling,
                                   VoiceReference *optional_voice, ProvenanceReference *provenance,
                                   NoteEvent **out) {
    if (id == NULL) {
        return NOTE_EVENT_ERR_NULL_ID;
    }
    if (pitch == NULL) {
        return NOTE_EVENT_ERR_NULL_PITCH;
    }
    if (provenance == NULL) {
        return NOTE_EVENT_ERR_NULL_PROVENANCE;
    }

    NoteEvent *note_event = (NoteEvent *)malloc(sizeof(NoteEvent));
    if (note_event == NULL) {
        return NOTE_EVENT_ERR_ALLOC;
    }
    TemporalAnchor *temporal_anchor = NULL;
    if (temporal_anchor_create(onset, &temporal_anchor) != TEMPORAL_ANCHOR_OK) {
        free(note_event);
        return NOTE_EVENT_ERR_ALLOC;
    }

    note_event->id = id;
    note_event->temporal_anchor = temporal_anchor;
    note_event->duration = duration;
    note_event->pitch = pitch;
    note_event->optional_pitch_spelling = optional_pitch_spelling;
    note_event->optional_voice = optional_voice;
    note_event->provenance = provenance;
    note_event->has_tuplet = false;
    note_event->tuplet_actual = 0;
    note_event->tuplet_normal = 0;

    *out = note_event;
    return NOTE_EVENT_OK;
}

void note_event_destroy(NoteEvent *note_event) {
    if (note_event == NULL) {
        return;
    }
    entity_id_destroy(note_event->id);
    temporal_anchor_destroy(note_event->temporal_anchor);
    pitch_identity_destroy(note_event->pitch);
    pitch_spelling_destroy(note_event->optional_pitch_spelling);
    voice_reference_destroy(note_event->optional_voice);
    provenance_reference_destroy(note_event->provenance);
    free(note_event);
}

const EntityID *note_event_id(const NoteEvent *note_event) {
    return note_event->id;
}

const TemporalAnchor *note_event_temporal_anchor(const NoteEvent *note_event) {
    return note_event->temporal_anchor;
}

const Duration *note_event_duration(const NoteEvent *note_event) {
    return &note_event->duration;
}

const PitchIdentity *note_event_pitch(const NoteEvent *note_event) {
    return note_event->pitch;
}

const PitchSpelling *note_event_optional_pitch_spelling(const NoteEvent *note_event) {
    return note_event->optional_pitch_spelling;
}

const VoiceReference *note_event_optional_voice(const NoteEvent *note_event) {
    return note_event->optional_voice;
}

const ProvenanceReference *note_event_provenance(const NoteEvent *note_event) {
    return note_event->provenance;
}

bool note_event_tuplet(const NoteEvent *note_event, unsigned *actual, unsigned *normal) {
    if (!note_event->has_tuplet) {
        return false;
    }
    if (actual != NULL) {
        *actual = note_event->tuplet_actual;
    }
    if (normal != NULL) {
        *normal = note_event->tuplet_normal;
    }
    return true;
}

NoteEventResult note_event_set_tuplet(NoteEvent *note_event, unsigned actual, unsigned normal) {
    Tuplet *tuplet = NULL;
    TupletResult result = tuplet_create(actual, normal, &tuplet);
    if (result == TUPLET_ERR_ALLOC) {
        return NOTE_EVENT_ERR_ALLOC;
    }
    if (result != TUPLET_OK) {
        return NOTE_EVENT_ERR_BAD_TUPLET;
    }
    note_event->has_tuplet = true;
    note_event->tuplet_actual = tuplet_actual(tuplet);
    note_event->tuplet_normal = tuplet_normal(tuplet);
    tuplet_destroy(tuplet);
    return NOTE_EVENT_OK;
}
