#include "theory/counterpoint/species_helpers.h"

#include "model/pitch/pitch_arithmetic.h"
#include "model/pitch/midi_pitch.h"

bool standard_counterpoint_species_is_tonic(const PitchSpelling *note, const PitchSpelling *tonic) {
    return pitch_spelling_letter(note) == pitch_spelling_letter(tonic) &&
           pitch_spelling_accidental(note) == pitch_spelling_accidental(tonic);
}

bool standard_counterpoint_species_is_sol(const PitchSpelling *note, const PitchSpelling *tonic) {
    int letters = (((pitch_spelling_step_number(note) - pitch_spelling_step_number(tonic)) % 7) + 7) % 7;
    int semitones =
        pitch_class_of_midi(pitch_spelling_midi(note) - pitch_spelling_midi(tonic));
    return letters == 4 && semitones == 7;
}

int standard_counterpoint_species_perfect_class(const StandardTonalityInterval *interval) {
    StandardTonalityIntervalCategory category;
    if (!standard_counterpoint_classify_interval(interval, &category) ||
        category != STANDARD_COUNTERPOINT_PERFECT_CONSONANCE) {
        return 0;
    }
    return interval->simple_size == 5 ? 5 : 8;
}

bool standard_counterpoint_species_check_leaps(const PitchSpelling *const *notes, size_t count,
                                               StandardCounterpointSpeciesLeapException exception, const void *context) {
    for (size_t j = 1; j < count; j++) {
        StandardTonalityInterval interval;
        if (!standard_tonality_interval_between(notes[j - 1], notes[j], &interval)) {
            return false;
        }
        if (interval.size > 8 || interval.quality == STANDARD_TONALITY_INTERVAL_QUALITY_AUGMENTED ||
            interval.quality == STANDARD_TONALITY_INTERVAL_QUALITY_DIMINISHED || interval.simple_size == 7) {
            return false;
        }
        if (interval.simple_size == 6) {
            bool ascending_minor = interval.quality == STANDARD_TONALITY_INTERVAL_QUALITY_MINOR &&
                                   pitch_spelling_direction(notes[j - 1], notes[j]) > 0;
            if (!ascending_minor && (exception == NULL || !exception(context, j))) {
                return false;
            }
        }
    }
    return true;
}
