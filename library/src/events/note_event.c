#include "events/note_event.h"

#include <stdlib.h>

struct NoteEvent {
    EntityID *id;
    TemporalAnchor *temporal_anchor;
    Duration *duration;
    PitchIdentity *pitch;
    PitchSpelling *optional_pitch_spelling;
    VoiceReference *optional_voice;
    ProvenanceReference *provenance;
};

NoteEventResult note_event_create(EntityID *id, TemporalAnchor *temporal_anchor, Duration *duration,
                                   PitchIdentity *pitch, PitchSpelling *optional_pitch_spelling,
                                   VoiceReference *optional_voice, ProvenanceReference *provenance,
                                   NoteEvent **out) {
    if (id == NULL) {
        return NOTE_EVENT_ERR_NULL_ID;
    }
    if (temporal_anchor == NULL) {
        return NOTE_EVENT_ERR_NULL_TEMPORAL_ANCHOR;
    }
    if (duration == NULL) {
        return NOTE_EVENT_ERR_NULL_DURATION;
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

    note_event->id = id;
    note_event->temporal_anchor = temporal_anchor;
    note_event->duration = duration;
    note_event->pitch = pitch;
    note_event->optional_pitch_spelling = optional_pitch_spelling;
    note_event->optional_voice = optional_voice;
    note_event->provenance = provenance;

    *out = note_event;
    return NOTE_EVENT_OK;
}

void note_event_destroy(NoteEvent *note_event) {
    if (note_event == NULL) {
        return;
    }
    entity_id_destroy(note_event->id);
    temporal_anchor_destroy(note_event->temporal_anchor);
    duration_destroy(note_event->duration);
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
    return note_event->duration;
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

bool note_event_equals_by_id(const NoteEvent *a, const NoteEvent *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}

bool note_event_equals_by_value(const NoteEvent *a, const NoteEvent *b) {
    if (a == NULL || b == NULL) {
        return false;
    }

    if (!temporal_anchor_equals(a->temporal_anchor, b->temporal_anchor)) {
        return false;
    }
    if (!duration_equals(a->duration, b->duration)) {
        return false;
    }
    if (!pitch_identity_equals(a->pitch, b->pitch)) {
        return false;
    }

    if ((a->optional_pitch_spelling == NULL) != (b->optional_pitch_spelling == NULL)) {
        return false;
    }
    if (a->optional_pitch_spelling != NULL &&
        !pitch_spelling_equals(a->optional_pitch_spelling, b->optional_pitch_spelling)) {
        return false;
    }

    if ((a->optional_voice == NULL) != (b->optional_voice == NULL)) {
        return false;
    }
    if (a->optional_voice != NULL && !voice_reference_equals(a->optional_voice, b->optional_voice)) {
        return false;
    }

    return provenance_reference_equals(a->provenance, b->provenance);
}