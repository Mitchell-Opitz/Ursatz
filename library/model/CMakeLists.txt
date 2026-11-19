# The model group (spec/target-architecture.md 7.2): the kernel's value types, values only.
# Sources include headers by their path from library/ (model/<area>/<file>.h).
add_library(ursatz_model
    identity/entity_id.c
    identity/schema_version.c
    core/entity.c
    core/string_copy.c
    core/once.c
    core/content_hash.c
    core/rational.c
    core/value.c
    core/occurrence.c
    core/voice_reference.c
    core/provenance_reference.c
    time/duration.c
    time/musical_time.c
    time/time_span.c
    time/temporal_anchor.c
    time/metronome_marking.c
    time/note_value.c
    time/time_signature.c
    time/meter_map.c
    time/metrical_position.c
    time/tempo_map.c
    time/tuplet.c
    time/meter_arithmetic.c
    time/score_time.c
    pitch/pitch_identity.c
    pitch/pitch_spelling.c
    pitch/pitch_arithmetic.c
    pitch/tuning_system.c
    pitch/pitch_numeric_value.c
    pitch/key_signature.c
    pitch/midi_pitch.c
    pitch/pitch_material.c
    pitch/region.c
    events/note_event.c
    events/dynamics.c
    events/tempo_marking.c
    containers/id_array.c
    containers/section.c
    structure/structure.c
    structure/motif.c
    structure/motif_occurrence.c
    structure/cadence.c
    structure/phrase_occurrence.c
)
ursatz_public_headers(ursatz_model
    BASE ${PROJECT_SOURCE_DIR}
    SOURCE_DIRS ${PROJECT_SOURCE_DIR}
    HEADERS
        model/containers/section.h
        model/core/entity.h
        model/core/occurrence.h
        model/core/once.h
        model/core/provenance_reference.h
        model/core/content_hash.h
        model/core/rational.h
        model/core/string_copy.h
        model/core/value.h
        model/core/voice_reference.h
        model/events/dynamics.h
        model/events/note_event.h
        model/events/tempo_marking.h
        model/identity/entity_id.h
        model/identity/schema_version.h
        model/pitch/chord_type.h
        model/pitch/key_signature.h
        model/pitch/midi_pitch.h
        model/pitch/pitch_arithmetic.h
        model/pitch/pitch_identity.h
        model/pitch/pitch_material.h
        model/pitch/pitch_numeric_value.h
        model/pitch/region.h
        model/pitch/pitch_spelling.h
        model/pitch/tuning_system.h
        model/structure/cadence.h
        model/structure/motif.h
        model/structure/motif_occurrence.h
        model/structure/phrase_occurrence.h
        model/structure/structure.h
        model/time/duration.h
        model/time/meter_arithmetic.h
        model/time/meter_map.h
        model/time/metrical_position.h
        model/time/metronome_marking.h
        model/time/musical_time.h
        model/time/note_value.h
        model/time/score_time.h
        model/time/tempo_map.h
        model/time/temporal_anchor.h
        model/time/time_signature.h
        model/time/time_span.h
        model/time/tuplet.h
)
# lround, floor and friends
target_link_libraries(ursatz_model PUBLIC m)
# core/once.c (thread-safe lazy initialization)
find_package(Threads REQUIRED)
target_link_libraries(ursatz_model PUBLIC Threads::Threads)

if(URSATZ_TESTS)
    add_subdirectory(tests)
endif()
