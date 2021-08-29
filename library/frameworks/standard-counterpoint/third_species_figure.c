#include "third_species_figure.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"

#define SLOTS STANDARD_COUNTERPOINT_THIRD_SPECIES_SLOTS_PER_BAR

typedef StandardCounterpointThirdSpeciesExercise Exercise;

static const char *const NAMES[STANDARD_COUNTERPOINT_FIGURE_COUNT] = {
    "passing-tone",       "double-passing-tone", "neighbor-tone",
    "double-neighbor",    "nota-cambiata-down",  "nota-cambiata-up"};

const char *standard_counterpoint_dissonance_figure_name(StandardCounterpointDissonanceFigure figure) {
    return figure >= 0 && figure < STANDARD_COUNTERPOINT_FIGURE_COUNT ? NAMES[figure] : NULL;
}

NonChordTone standard_counterpoint_dissonance_figure_non_chord_tone(StandardCounterpointDissonanceFigure figure) {
    switch (figure) {
        case STANDARD_COUNTERPOINT_FIGURE_PASSING_TONE:
        case STANDARD_COUNTERPOINT_FIGURE_DOUBLE_PASSING_TONE:
            return NON_CHORD_TONE_PASSING;
        case STANDARD_COUNTERPOINT_FIGURE_NEIGHBOR_TONE:
            return NON_CHORD_TONE_NEIGHBOR;
        case STANDARD_COUNTERPOINT_FIGURE_DOUBLE_NEIGHBOR:
            return NON_CHORD_TONE_DOUBLE_NEIGHBOR;
        case STANDARD_COUNTERPOINT_FIGURE_NOTA_CAMBIATA_DOWN:
        case STANDARD_COUNTERPOINT_FIGURE_NOTA_CAMBIATA_UP:
            return NON_CHORD_TONE_CHANGING_TONE;
        default:
            return NON_CHORD_TONE_COUNT;
    }
}

/* Slot-level views of the counterpoint. A slot index is 4 * bar + beat. */

static size_t slot_count(const Exercise *exercise) {
    return exercise->counterpoint->count;
}

static const PitchSpelling *pitch_at(const Exercise *exercise, size_t slot) {
    const StandardCounterpointSlot *entry = &exercise->counterpoint->slots[slot];
    return entry->kind == STANDARD_COUNTERPOINT_SLOT_REST ? NULL : entry->pitch;
}

static bool is_attack(const Exercise *exercise, size_t slot) {
    return exercise->counterpoint->slots[slot].kind == STANDARD_COUNTERPOINT_SLOT_ATTACK;
}

static bool dissonant_at(const Exercise *exercise, size_t slot, bool *dissonant) {
    const PitchSpelling *pitch = pitch_at(exercise, slot);
    StandardCounterpointInterval interval;
    StandardCounterpointIntervalCategory category;
    if (pitch == NULL || !standard_counterpoint_interval_between(exercise->cantus->notes[slot / SLOTS], pitch, &interval) ||
        !standard_counterpoint_classify_interval(&interval, &category)) {
        return false;
    }
    *dissonant = category == STANDARD_COUNTERPOINT_DISSONANCE;
    return true;
}

static bool consonant_at(const Exercise *exercise, size_t slot) {
    bool dissonant;
    return dissonant_at(exercise, slot, &dissonant) && !dissonant;
}

static bool sounds_dissonant_at(const Exercise *exercise, size_t slot) {
    bool dissonant;
    return dissonant_at(exercise, slot, &dissonant) && dissonant;
}

/* A step (a major or minor second) or a leap of a third (a major or minor
   third) from one slot's pitch to another's. */
static bool melodic_size_is(const PitchSpelling *a, const PitchSpelling *b, int size) {
    StandardCounterpointInterval interval;
    return a != NULL && b != NULL && standard_counterpoint_interval_between(a, b, &interval) &&
           interval.size == size && standard_counterpoint_interval_melodic_consonant(&interval);
}

static int direction_between(const Exercise *exercise, size_t from, size_t to) {
    return standard_counterpoint_direction(pitch_at(exercise, from), pitch_at(exercise, to));
}

static bool step_between(const Exercise *exercise, size_t from, size_t to) {
    return melodic_size_is(pitch_at(exercise, from), pitch_at(exercise, to), 2);
}

static bool third_between(const Exercise *exercise, size_t from, size_t to) {
    return melodic_size_is(pitch_at(exercise, from), pitch_at(exercise, to), 3);
}

static bool same_pitch_at(const Exercise *exercise, size_t a, size_t b) {
    return standard_counterpoint_same_pitch(pitch_at(exercise, a), pitch_at(exercise, b));
}

static bool passing_tone(const Exercise *exercise, size_t slot) {
    if (slot == 0 || slot + 1 >= slot_count(exercise)) {
        return false;
    }
    return consonant_at(exercise, slot - 1) && consonant_at(exercise, slot + 1) &&
           step_between(exercise, slot - 1, slot) && step_between(exercise, slot, slot + 1) &&
           direction_between(exercise, slot - 1, slot) == direction_between(exercise, slot, slot + 1) &&
           direction_between(exercise, slot, slot + 1) != 0;
}

/* Two dissonant notes at `slot` and `slot + 1`, neither on a downbeat,
   between two consonant notes, all in one direction by step. */
static bool double_passing_pair(const Exercise *exercise, size_t slot) {
    if (slot == 0 || slot + 2 >= slot_count(exercise) || slot % SLOTS == 0 || (slot + 1) % SLOTS == 0 ||
        !is_attack(exercise, slot) || !is_attack(exercise, slot + 1)) {
        return false;
    }
    if (!sounds_dissonant_at(exercise, slot) || !sounds_dissonant_at(exercise, slot + 1)) {
        return false;
    }
    int direction = direction_between(exercise, slot - 1, slot);
    return consonant_at(exercise, slot - 1) && consonant_at(exercise, slot + 2) &&
           step_between(exercise, slot - 1, slot) && step_between(exercise, slot, slot + 1) &&
           step_between(exercise, slot + 1, slot + 2) && direction != 0 &&
           direction_between(exercise, slot, slot + 1) == direction &&
           direction_between(exercise, slot + 1, slot + 2) == direction;
}

static bool double_passing_tone(const Exercise *exercise, size_t slot) {
    return double_passing_pair(exercise, slot) || (slot > 0 && double_passing_pair(exercise, slot - 1));
}

static bool neighbor_tone(const Exercise *exercise, size_t slot) {
    if (slot == 0 || slot + 1 >= slot_count(exercise)) {
        return false;
    }
    int out = direction_between(exercise, slot - 1, slot);
    int back = direction_between(exercise, slot, slot + 1);
    return consonant_at(exercise, slot - 1) && consonant_at(exercise, slot + 1) &&
           same_pitch_at(exercise, slot - 1, slot + 1) && step_between(exercise, slot - 1, slot) &&
           step_between(exercise, slot, slot + 1) && out != 0 && back != 0 && out != back;
}

/* The double neighbor over bar `bar`, whose slots are 4 * bar to 4 * bar + 3
   and whose next downbeat is slot 4 * bar + 4. */
static bool double_neighbor_bar(const Exercise *exercise, size_t bar) {
    size_t first = SLOTS * bar;
    size_t next = first + SLOTS;
    if (next >= slot_count(exercise) || !is_attack(exercise, first) || !is_attack(exercise, first + 1) ||
        !is_attack(exercise, first + 2) || !is_attack(exercise, first + 3)) {
        return false;
    }
    if (!consonant_at(exercise, first) || !same_pitch_at(exercise, first, first + 3) ||
        !sounds_dissonant_at(exercise, first + 1) || !sounds_dissonant_at(exercise, first + 2)) {
        return false;
    }
    int upper = direction_between(exercise, first, first + 1);
    int lower = direction_between(exercise, first, first + 2);
    int into = direction_between(exercise, first + 2, first + 3);
    int across = direction_between(exercise, first + 3, next);
    return step_between(exercise, first, first + 1) && step_between(exercise, first, first + 2) && upper != 0 &&
           upper == -lower && third_between(exercise, first + 1, first + 2) && into != 0 && into == across &&
           step_between(exercise, first + 3, next);
}

/* The nota cambiata over bar `bar`: 0 for none, 1 for the form that fills in a
   step down, 2 for the form that fills in a step up. */
static int nota_cambiata_bar(const Exercise *exercise, size_t bar) {
    size_t first = SLOTS * bar;
    size_t next = first + SLOTS;
    if (next >= slot_count(exercise)) {
        return 0;
    }
    for (size_t i = first; i <= next; i++) {
        if (!is_attack(exercise, i)) {
            return 0;
        }
    }
    if (!consonant_at(exercise, first) || !sounds_dissonant_at(exercise, first + 1) ||
        !consonant_at(exercise, first + 2) || !consonant_at(exercise, next) || !same_pitch_at(exercise, first + 1, next)) {
        return 0;
    }
    int step_away = direction_between(exercise, first, first + 1);
    if (step_away == 0 || !step_between(exercise, first, first + 1) || !third_between(exercise, first + 1, first + 2) ||
        direction_between(exercise, first + 1, first + 2) != step_away ||
        !step_between(exercise, first + 2, first + 3) || direction_between(exercise, first + 2, first + 3) != -step_away ||
        !step_between(exercise, first + 3, next) || direction_between(exercise, first + 3, next) != -step_away) {
        return 0;
    }
    return step_away < 0 ? 1 : 2;
}

bool standard_counterpoint_third_species_beat_is_dissonant(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                           size_t bar, size_t beat) {
    if (!standard_counterpoint_third_species_exercise_valid(exercise) || bar >= exercise->cantus->count || beat >= SLOTS) {
        return false;
    }
    return sounds_dissonant_at(exercise, SLOTS * bar + beat);
}

bool standard_counterpoint_third_species_dissonance_figure(const StandardCounterpointThirdSpeciesExercise *exercise,
                                                           size_t bar, size_t beat,
                                                           StandardCounterpointDissonanceFigure *figure) {
    if (figure == NULL || !standard_counterpoint_third_species_exercise_valid(exercise) ||
        bar >= exercise->cantus->count || beat == 0 || beat >= SLOTS) {
        return false;
    }
    size_t slot = SLOTS * bar + beat;
    if (!is_attack(exercise, slot) || !sounds_dissonant_at(exercise, slot)) {
        return false;
    }

    if (beat == 1) {
        int cambiata = nota_cambiata_bar(exercise, bar);
        if (cambiata != 0) {
            *figure = cambiata == 1 ? STANDARD_COUNTERPOINT_FIGURE_NOTA_CAMBIATA_DOWN
                                    : STANDARD_COUNTERPOINT_FIGURE_NOTA_CAMBIATA_UP;
            return true;
        }
    }
    if ((beat == 1 || beat == 2) && double_neighbor_bar(exercise, bar)) {
        *figure = STANDARD_COUNTERPOINT_FIGURE_DOUBLE_NEIGHBOR;
        return true;
    }
    if (double_passing_tone(exercise, slot)) {
        *figure = STANDARD_COUNTERPOINT_FIGURE_DOUBLE_PASSING_TONE;
        return true;
    }
    if (passing_tone(exercise, slot)) {
        *figure = STANDARD_COUNTERPOINT_FIGURE_PASSING_TONE;
        return true;
    }
    if (neighbor_tone(exercise, slot)) {
        *figure = STANDARD_COUNTERPOINT_FIGURE_NEIGHBOR_TONE;
        return true;
    }
    return false;
}
