#include "fifth_species_figure.h"

#include <stddef.h>

#include "consonance_rules.h"
#include "counterpoint_interval.h"
#include "counterpoint_pitch.h"

#define SLOTS STANDARD_COUNTERPOINT_FIFTH_SPECIES_SLOTS_PER_BAR
#define MAX_NOTES STANDARD_COUNTERPOINT_FIFTH_SPECIES_MAX_NOTES

typedef StandardCounterpointFifthSpeciesExercise Exercise;
typedef StandardCounterpointFifthSpeciesNote Note;

static const char *const NAMES[STANDARD_COUNTERPOINT_FIFTH_FIGURE_COUNT] = {
    "passing-tone", "double-passing-tone", "neighbor-tone",    "double-neighbor", "nota-cambiata-down",
    "nota-cambiata-up", "suspension",       "anticipation",     "anticipation-turn"};

const char *standard_counterpoint_fifth_species_figure_name(StandardCounterpointFifthSpeciesFigure figure) {
    return figure >= 0 && figure < STANDARD_COUNTERPOINT_FIFTH_FIGURE_COUNT ? NAMES[figure] : NULL;
}

NonChordTone standard_counterpoint_fifth_species_figure_non_chord_tone(StandardCounterpointFifthSpeciesFigure figure) {
    switch (figure) {
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_PASSING_TONE:
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_DOUBLE_PASSING_TONE:
            return NON_CHORD_TONE_PASSING;
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_NEIGHBOR_TONE:
            return NON_CHORD_TONE_NEIGHBOR;
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_DOUBLE_NEIGHBOR:
            return NON_CHORD_TONE_DOUBLE_NEIGHBOR;
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_NOTA_CAMBIATA_DOWN:
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_NOTA_CAMBIATA_UP:
            return NON_CHORD_TONE_CHANGING_TONE;
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_SUSPENSION:
            return NON_CHORD_TONE_SUSPENSION;
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_ANTICIPATION:
        case STANDARD_COUNTERPOINT_FIFTH_FIGURE_ANTICIPATION_TURN:
            return NON_CHORD_TONE_ANTICIPATION;
        default:
            return NON_CHORD_TONE_COUNT;
    }
}

typedef struct {
    const Exercise *exercise;
    Note notes[MAX_NOTES];
    size_t count;
} Context;

static bool load(const Exercise *exercise, Context *context) {
    context->exercise = exercise;
    return standard_counterpoint_fifth_species_notes(exercise, context->notes, MAX_NOTES, &context->count);
}

/* The consonance category of a pitch sounding in the bar of `slot`. */
static bool category_at(const Context *context, const PitchSpelling *pitch, size_t slot,
                        StandardCounterpointIntervalCategory *category) {
    const PitchSpelling *cantus = standard_counterpoint_fifth_species_cantus(context->exercise, slot / SLOTS);
    StandardCounterpointInterval interval;
    return cantus != NULL && pitch != NULL && standard_counterpoint_interval_between(cantus, pitch, &interval) &&
           standard_counterpoint_classify_interval(&interval, category);
}

static bool dissonant_at(const Context *context, const PitchSpelling *pitch, size_t slot) {
    StandardCounterpointIntervalCategory category;
    return category_at(context, pitch, slot, &category) && category == STANDARD_COUNTERPOINT_DISSONANCE;
}

static bool consonant_at(const Context *context, const PitchSpelling *pitch, size_t slot) {
    StandardCounterpointIntervalCategory category;
    return category_at(context, pitch, slot, &category) && category != STANDARD_COUNTERPOINT_DISSONANCE;
}

/* A note is consonant at its attack, or consonant just before the next note
   begins (the last slot it sounds in before slot `slot`). */
static bool note_consonant_at_attack(const Context *context, size_t n) {
    return consonant_at(context, context->notes[n].pitch, context->notes[n].slot);
}

static bool note_dissonant_at_attack(const Context *context, size_t n) {
    return dissonant_at(context, context->notes[n].pitch, context->notes[n].slot);
}

/* The note before note n sounds consonant in the slot just before n begins. */
static bool before_consonant(const Context *context, size_t n) {
    return n > 0 && consonant_at(context, context->notes[n - 1].pitch, context->notes[n].slot - 1);
}

static bool crosses_bar(const Note *note) {
    return note->slot / SLOTS != (note->slot + note->duration - 1) / SLOTS;
}

/* Direction (1 up, -1 down) when the two pitches are a consonant melodic step
   apart; 0 otherwise. */
static int step_between(const PitchSpelling *from, const PitchSpelling *to) {
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(from, to, &interval) || !standard_counterpoint_interval_is_step(&interval) ||
        !standard_counterpoint_interval_melodic_consonant(&interval)) {
        return 0;
    }
    return standard_counterpoint_direction(from, to);
}

/* Direction when the two pitches are a consonant melodic leap of a third
   apart; 0 otherwise. */
static int third_between(const PitchSpelling *from, const PitchSpelling *to) {
    StandardCounterpointInterval interval;
    if (!standard_counterpoint_interval_between(from, to, &interval) || interval.size != 3 ||
        !standard_counterpoint_interval_melodic_consonant(&interval)) {
        return 0;
    }
    return standard_counterpoint_direction(from, to);
}

static const PitchSpelling *pitch_of(const Context *context, size_t n) {
    return context->notes[n].pitch;
}

static bool off_downbeat_dissonant_attack(const Context *context, size_t n) {
    return context->notes[n].slot % SLOTS != 0 && !crosses_bar(&context->notes[n]) && note_dissonant_at_attack(context, n);
}

static bool passing_tone(const Context *context, size_t n) {
    if (n == 0 || n + 1 >= context->count || !off_downbeat_dissonant_attack(context, n) || !before_consonant(context, n) ||
        !note_consonant_at_attack(context, n + 1)) {
        return false;
    }
    int in = step_between(pitch_of(context, n - 1), pitch_of(context, n));
    int out = step_between(pitch_of(context, n), pitch_of(context, n + 1));
    return in != 0 && in == out;
}

/* Two dissonant notes in a row, n and n + 1. */
static bool double_passing_from(const Context *context, size_t n) {
    if (n == 0 || n + 2 >= context->count || !off_downbeat_dissonant_attack(context, n) ||
        !off_downbeat_dissonant_attack(context, n + 1) || !before_consonant(context, n) ||
        !note_consonant_at_attack(context, n + 2)) {
        return false;
    }
    int first = step_between(pitch_of(context, n - 1), pitch_of(context, n));
    int second = step_between(pitch_of(context, n), pitch_of(context, n + 1));
    int third = step_between(pitch_of(context, n + 1), pitch_of(context, n + 2));
    return first != 0 && first == second && second == third;
}

static bool double_passing_tone(const Context *context, size_t n) {
    return double_passing_from(context, n) || (n > 0 && double_passing_from(context, n - 1));
}

static bool neighbor_tone(const Context *context, size_t n) {
    if (n == 0 || n + 1 >= context->count || !off_downbeat_dissonant_attack(context, n) || !before_consonant(context, n) ||
        !note_consonant_at_attack(context, n + 1) ||
        !standard_counterpoint_same_pitch(pitch_of(context, n - 1), pitch_of(context, n + 1))) {
        return false;
    }
    int in = step_between(pitch_of(context, n - 1), pitch_of(context, n));
    int out = step_between(pitch_of(context, n), pitch_of(context, n + 1));
    return in != 0 && in == -out;
}

/* The double neighbor whose first dissonance is note b. */
static bool double_neighbor_from(const Context *context, size_t b) {
    if (b == 0 || b + 2 >= context->count || !off_downbeat_dissonant_attack(context, b) ||
        !off_downbeat_dissonant_attack(context, b + 1) || !before_consonant(context, b) ||
        !note_consonant_at_attack(context, b + 2) ||
        !standard_counterpoint_same_pitch(pitch_of(context, b - 1), pitch_of(context, b + 2))) {
        return false;
    }
    int first = step_between(pitch_of(context, b - 1), pitch_of(context, b));
    int leap = third_between(pitch_of(context, b), pitch_of(context, b + 1));
    int last = step_between(pitch_of(context, b + 1), pitch_of(context, b + 2));
    return first != 0 && leap == -first && last == first;
}

static bool double_neighbor(const Context *context, size_t n) {
    return double_neighbor_from(context, n) || (n > 0 && double_neighbor_from(context, n - 1));
}

/* The nota cambiata whose dissonance is note n: `way` is -1 for the figure
   that fills in a step down, 1 for its inversion. */
static bool nota_cambiata(const Context *context, size_t n, int way) {
    if (n == 0 || n + 3 >= context->count || !off_downbeat_dissonant_attack(context, n) || !before_consonant(context, n) ||
        !note_consonant_at_attack(context, n + 1) || !note_consonant_at_attack(context, n + 3) ||
        !standard_counterpoint_same_pitch(pitch_of(context, n), pitch_of(context, n + 3))) {
        return false;
    }
    return step_between(pitch_of(context, n - 1), pitch_of(context, n)) == way &&
           third_between(pitch_of(context, n), pitch_of(context, n + 1)) == way &&
           step_between(pitch_of(context, n + 1), pitch_of(context, n + 2)) == -way &&
           step_between(pitch_of(context, n + 2), pitch_of(context, n + 3)) == -way;
}

/* An anticipation: note n, reached by step down, whose pitch comes again as
   a consonance on the next downbeat. */
static bool anticipation_core(const Context *context, size_t n) {
    if (n == 0 || n + 1 >= context->count || !note_dissonant_at_attack(context, n) ||
        context->notes[n + 1].slot != (context->notes[n].slot / SLOTS + 1) * SLOTS ||
        !standard_counterpoint_same_pitch(pitch_of(context, n), pitch_of(context, n + 1)) ||
        !note_consonant_at_attack(context, n + 1)) {
        return false;
    }
    return step_between(pitch_of(context, n - 1), pitch_of(context, n)) < 0;
}

static bool anticipation(const Context *context, size_t n) {
    return context->notes[n].slot % SLOTS == 6 && context->notes[n].duration == 2 && before_consonant(context, n) &&
           anticipation_core(context, n);
}

static bool anticipation_turn(const Context *context, size_t n) {
    return n > 0 && context->notes[n].slot % SLOTS == 7 && context->notes[n].duration == 1 &&
           context->notes[n - 1].duration == 1 && note_consonant_at_attack(context, n - 1) && anticipation_core(context, n);
}

static bool sounds_dissonant(const Context *context, size_t slot) {
    return dissonant_at(context, standard_counterpoint_fifth_species_pitch(context->exercise, slot), slot);
}

bool standard_counterpoint_fifth_species_slot_is_dissonant(const Exercise *exercise, size_t slot) {
    Context context;
    return load(exercise, &context) && sounds_dissonant(&context, slot);
}

/* The index of the note sounding in the slot, or count when there is none. */
static size_t note_at(const Context *context, size_t slot) {
    for (size_t n = 0; n < context->count; n++) {
        if (context->notes[n].slot <= slot && slot < context->notes[n].slot + context->notes[n].duration) {
            return n;
        }
    }
    return context->count;
}

static bool suspension_type_of(const Context *context, size_t n, size_t slot, StandardCounterpointSuspensionType *type) {
    if (n + 1 >= context->count || context->notes[n].slot / SLOTS + 1 != slot / SLOTS || slot % SLOTS != 0 ||
        !note_consonant_at_attack(context, n) || !sounds_dissonant(context, slot) ||
        !note_consonant_at_attack(context, n + 1) ||
        step_between(pitch_of(context, n), pitch_of(context, n + 1)) >= 0) {
        return false;
    }
    const PitchSpelling *cantus = standard_counterpoint_fifth_species_cantus(context->exercise, slot / SLOTS);
    const PitchSpelling *held = pitch_of(context, n);
    StandardCounterpointInterval suspension, resolved;
    if (!standard_counterpoint_interval_between(cantus, held, &suspension) ||
        !standard_counterpoint_interval_between(cantus, pitch_of(context, n + 1), &resolved)) {
        return false;
    }
    int from = suspension.simple_size;
    int to = resolved.simple_size;
    if (standard_counterpoint_direction(cantus, held) > 0) {
        if (from == 7 && to == 6) {
            *type = STANDARD_COUNTERPOINT_SUSPENSION_SEVEN_SIX;
        } else if (from == 4 && suspension.quality == STANDARD_COUNTERPOINT_QUALITY_PERFECT && to == 3) {
            *type = STANDARD_COUNTERPOINT_SUSPENSION_FOUR_THREE;
        } else if (from == 2 && (to == 1 || to == 8)) {
            *type = STANDARD_COUNTERPOINT_SUSPENSION_NINE_EIGHT;
        } else {
            return false;
        }
        return true;
    }
    if (from == 2 && to == 3) {
        *type = STANDARD_COUNTERPOINT_SUSPENSION_TWO_THREE;
        return true;
    }
    return false;
}

bool standard_counterpoint_fifth_species_suspension_type(const Exercise *exercise, size_t slot,
                                                         StandardCounterpointSuspensionType *type) {
    Context context;
    if (type == NULL || !load(exercise, &context) || !standard_counterpoint_fifth_species_slot_is_tie(exercise, slot)) {
        return false;
    }
    size_t n = note_at(&context, slot);
    return n < context.count && suspension_type_of(&context, n, slot, type);
}

bool standard_counterpoint_fifth_species_dissonance_figure(const Exercise *exercise, size_t slot,
                                                           StandardCounterpointFifthSpeciesFigure *figure) {
    Context context;
    if (figure == NULL || !load(exercise, &context) || !sounds_dissonant(&context, slot)) {
        return false;
    }
    size_t n = note_at(&context, slot);
    if (n >= context.count) {
        return false;
    }
    if (context.notes[n].slot != slot && slot % SLOTS == 0) {
        StandardCounterpointSuspensionType type;
        if (suspension_type_of(&context, n, slot, &type)) {
            *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_SUSPENSION;
            return true;
        }
        return false;
    }
    if (context.notes[n].slot % SLOTS == 0) {
        return false;
    }
    if (nota_cambiata(&context, n, -1)) {
        *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_NOTA_CAMBIATA_DOWN;
    } else if (nota_cambiata(&context, n, 1)) {
        *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_NOTA_CAMBIATA_UP;
    } else if (double_neighbor(&context, n)) {
        *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_DOUBLE_NEIGHBOR;
    } else if (double_passing_tone(&context, n)) {
        *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_DOUBLE_PASSING_TONE;
    } else if (passing_tone(&context, n)) {
        *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_PASSING_TONE;
    } else if (neighbor_tone(&context, n)) {
        *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_NEIGHBOR_TONE;
    } else if (anticipation(&context, n)) {
        *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_ANTICIPATION;
    } else if (anticipation_turn(&context, n)) {
        *figure = STANDARD_COUNTERPOINT_FIFTH_FIGURE_ANTICIPATION_TURN;
    } else {
        return false;
    }
    return true;
}
