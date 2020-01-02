#include "persistence/persistence_note_events.h"

#include <stdlib.h>

#include "core/provenance_reference.h"
#include "core/voice_reference.h"
#include "identity/entity_id.h"
#include "pitch/pitch_identity.h"
#include "time/duration.h"
#include "time/musical_time.h"
#include "time/temporal_anchor.h"

static PersistenceNoteEventsResult store_one(sqlite3_stmt *stmt, const NoteEvent *event, long sequence_order) {
    sqlite3_reset(stmt);
    sqlite3_clear_bindings(stmt);

    const EntityID *id = note_event_id(event);
    const TemporalAnchor *anchor = note_event_temporal_anchor(event);
    const MusicalTime *position = temporal_anchor_position(anchor);
    const Duration *duration = note_event_duration(event);
    const PitchIdentity *pitch = note_event_pitch(event);
    const VoiceReference *voice = note_event_optional_voice(event);
    const ProvenanceReference *provenance = note_event_provenance(event);

    size_t token_size = 0;
    const void *token = pitch_identity_token(pitch, &token_size);

    sqlite3_bind_text(stmt, 1, entity_id_value(id), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 2, sequence_order);
    sqlite3_bind_int64(stmt, 3, musical_time_numerator(position));
    sqlite3_bind_int64(stmt, 4, (sqlite3_int64)musical_time_denominator(position));
    sqlite3_bind_int64(stmt, 5, duration_numerator(duration));
    sqlite3_bind_int64(stmt, 6, (sqlite3_int64)duration_denominator(duration));
    sqlite3_bind_blob(stmt, 7, token, (int)token_size, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 8, (sqlite3_int64)token_size);

    if (voice != NULL) {
        sqlite3_bind_text(stmt, 9, entity_id_value(voice_reference_target_id(voice)), -1, SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 9);
    }

    sqlite3_bind_int(stmt, 10, (int)provenance_reference_state(provenance));
    if (provenance_reference_state(provenance) == PROVENANCE_REFERENCE_PRESENT) {
        sqlite3_bind_text(stmt, 11, entity_id_value(provenance_reference_target_id(provenance)), -1,
                           SQLITE_TRANSIENT);
    } else {
        sqlite3_bind_null(stmt, 11);
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        return PERSISTENCE_NOTE_EVENTS_ERR_DB;
    }
    return PERSISTENCE_NOTE_EVENTS_OK;
}

PersistenceNoteEventsResult persistence_store_note_events(PersistenceHandle *handle, NoteEvent *const *events,
                                                           size_t count) {
    if (handle == NULL || (events == NULL && count > 0)) {
        return PERSISTENCE_NOTE_EVENTS_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "INSERT INTO note_events "
                       "(id, sequence_order, anchor_numerator, anchor_denominator, "
                       " duration_numerator, duration_denominator, pitch_token, pitch_token_size, "
                       " voice_reference_id, provenance_state, provenance_id) "
                       "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_NOTE_EVENTS_ERR_DB;
    }

    PersistenceNoteEventsResult result = PERSISTENCE_NOTE_EVENTS_OK;
    for (size_t i = 0; i < count; i++) {
        result = store_one(stmt, events[i], (long)i);
        if (result != PERSISTENCE_NOTE_EVENTS_OK) {
            break;
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

static PersistenceNoteEventsResult load_one(sqlite3_stmt *stmt, NoteEvent **out_event) {
    const unsigned char *id_text = sqlite3_column_text(stmt, 0);
    sqlite3_int64 anchor_num = sqlite3_column_int64(stmt, 1);
    sqlite3_int64 anchor_den = sqlite3_column_int64(stmt, 2);
    sqlite3_int64 duration_num = sqlite3_column_int64(stmt, 3);
    sqlite3_int64 duration_den = sqlite3_column_int64(stmt, 4);
    const void *token = sqlite3_column_blob(stmt, 5);
    sqlite3_int64 token_size = sqlite3_column_int64(stmt, 6);
    const unsigned char *voice_id_text = sqlite3_column_text(stmt, 7);
    int provenance_state = sqlite3_column_int(stmt, 8);
    const unsigned char *provenance_id_text = sqlite3_column_text(stmt, 9);

    EntityID *id = NULL;
    TemporalAnchor *anchor = NULL;
    MusicalTime *position = NULL;
    Duration *duration = NULL;
    PitchIdentity *pitch = NULL;
    VoiceReference *voice = NULL;
    ProvenanceReference *provenance = NULL;
    NoteEvent *event = NULL;

    if (entity_id_create((const char *)id_text, &id) != ENTITY_ID_OK) {
        goto fail;
    }
    if (musical_time_create((long)anchor_num, (unsigned long)anchor_den, &position) != MUSICAL_TIME_OK) {
        goto fail;
    }
    if (temporal_anchor_create(position, &anchor) != TEMPORAL_ANCHOR_OK) {
        musical_time_destroy(position);
        goto fail;
    }
    if (duration_create((long)duration_num, (unsigned long)duration_den, &duration) != DURATION_OK) {
        goto fail;
    }
    if (pitch_identity_create(token, (size_t)token_size, &pitch) != PITCH_IDENTITY_OK) {
        goto fail;
    }

    if (voice_id_text != NULL) {
        EntityID *voice_id = NULL;
        if (entity_id_create((const char *)voice_id_text, &voice_id) != ENTITY_ID_OK) {
            goto fail;
        }
        if (voice_reference_create(voice_id, &voice) != VOICE_REFERENCE_OK) {
            entity_id_destroy(voice_id);
            goto fail;
        }
    }

    if (provenance_state == PROVENANCE_REFERENCE_PRESENT) {
        EntityID *provenance_id = NULL;
        if (entity_id_create((const char *)provenance_id_text, &provenance_id) != ENTITY_ID_OK) {
            goto fail;
        }
        if (provenance_reference_create_present(provenance_id, &provenance) != PROVENANCE_REFERENCE_OK) {
            entity_id_destroy(provenance_id);
            goto fail;
        }
    } else {
        if (provenance_reference_create_absent(&provenance) != PROVENANCE_REFERENCE_OK) {
            goto fail;
        }
    }

    if (note_event_create(id, anchor, duration, pitch, NULL, voice, provenance, &event) != NOTE_EVENT_OK) {
        goto fail;
    }

    *out_event = event;
    return PERSISTENCE_NOTE_EVENTS_OK;

fail:
    if (id != NULL) {
        entity_id_destroy(id);
    }
    if (anchor != NULL) {
        temporal_anchor_destroy(anchor);
    }
    if (duration != NULL) {
        duration_destroy(duration);
    }
    if (pitch != NULL) {
        pitch_identity_destroy(pitch);
    }
    if (voice != NULL) {
        voice_reference_destroy(voice);
    }
    if (provenance != NULL) {
        provenance_reference_destroy(provenance);
    }
    return PERSISTENCE_NOTE_EVENTS_ERR_DB;
}

PersistenceNoteEventsResult persistence_load_note_events(PersistenceHandle *handle, NoteEvent ***out_events,
                                                          size_t *out_count) {
    if (handle == NULL || out_events == NULL || out_count == NULL) {
        return PERSISTENCE_NOTE_EVENTS_ERR_NULL_ARG;
    }

    sqlite3 *db = persistence_db(handle);
    const char *sql = "SELECT id, anchor_numerator, anchor_denominator, "
                       "duration_numerator, duration_denominator, pitch_token, pitch_token_size, "
                       "voice_reference_id, provenance_state, provenance_id "
                       "FROM note_events ORDER BY sequence_order ASC";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        return PERSISTENCE_NOTE_EVENTS_ERR_DB;
    }

    NoteEvent **events = NULL;
    size_t count = 0;
    size_t capacity = 0;
    PersistenceNoteEventsResult result = PERSISTENCE_NOTE_EVENTS_OK;

    int rc;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count == capacity) {
            capacity = capacity == 0 ? 8 : capacity * 2;
            NoteEvent **resized = realloc(events, capacity * sizeof(NoteEvent *));
            if (resized == NULL) {
                result = PERSISTENCE_NOTE_EVENTS_ERR_ALLOC;
                break;
            }
            events = resized;
        }

        NoteEvent *event = NULL;
        result = load_one(stmt, &event);
        if (result != PERSISTENCE_NOTE_EVENTS_OK) {
            break;
        }
        events[count++] = event;
    }

    if (rc != SQLITE_DONE && result == PERSISTENCE_NOTE_EVENTS_OK) {
        result = PERSISTENCE_NOTE_EVENTS_ERR_DB;
    }

    sqlite3_finalize(stmt);

    if (result != PERSISTENCE_NOTE_EVENTS_OK) {
        for (size_t i = 0; i < count; i++) {
            note_event_destroy(events[i]);
        }
        free(events);
        return result;
    }

    *out_events = events;
    *out_count = count;
    return PERSISTENCE_NOTE_EVENTS_OK;
}
