#ifndef URSATZ_FRAMEWORKS_STANDARD_FORM_CADENTIAL_BASS_RULES_H
#define URSATZ_FRAMEWORKS_STANDARD_FORM_CADENTIAL_BASS_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "rules/constraint.h"
#include "rules/rule.h"

/*
 * Cadential bass rules for standard-form-v1. A cadential ending harmonizes a particular bass
 * pattern, the core line mi-fa-sol-do (scale degrees 3 4 5 1):
 *
 *   CORE_LINE        the core bass line is degrees 3, 4, 5, 1
 *   HARMONIZATION    mi takes I6, fa takes ii6 or IV, sol takes V or V7 (often elaborated
 *                    with a cadential six-four), do takes I
 *   EMBELLISHMENT    the line may be embellished by tonicizing the dominant: fi (raised
 *                    fa) before sol, making the line mi-fa-fi-sol-do
 *   CLEAREST         the clearest cadential endings are four measures long with one bass
 *                    note per measure
 *
 * A bass note is a scale degree and an alteration, because this framework does no pitch
 * arithmetic: fi is degree 4 with alteration +1. Only a raise of one semitone is modeled.
 */

typedef enum {
    STANDARD_FORM_CADENTIAL_BASS_RULE_CORE_LINE = 0,
    STANDARD_FORM_CADENTIAL_BASS_RULE_HARMONIZATION,
    STANDARD_FORM_CADENTIAL_BASS_RULE_EMBELLISHMENT,
    STANDARD_FORM_CADENTIAL_BASS_RULE_CLEAREST,
    STANDARD_FORM_CADENTIAL_BASS_RULE_COUNT
} StandardFormCadentialBassRule;

const Rule *standard_form_cadential_bass_rule(StandardFormCadentialBassRule which);
const Constraint *standard_form_cadential_bass_constraint(StandardFormCadentialBassRule which);

typedef struct {
    int degree;     /* scale degree of the bass note, 1 to 7 */
    int alteration; /* 0 for the diatonic note, +1 for the note raised a semitone */
} StandardFormBassNote;

/* CORE_LINE: how many notes the core line has (four). */
size_t standard_form_cadential_core_length(void);

/* CORE_LINE: the scale degree at a core-line position (3, 4, 5, 1); 0 past the end. */
int standard_form_cadential_core_degree(size_t index);

/* CORE_LINE and EMBELLISHMENT: true when the bass is the core line, or the core line with fi
   added before sol. Every other degree, alteration, or length is false. */
bool standard_form_bass_is_cadential(const StandardFormBassNote *notes, size_t count);

/* EMBELLISHMENT: true when the bass is the core line with fi (degree 4 raised) before sol. */
bool standard_form_bass_has_fi_embellishment(const StandardFormBassNote *notes, size_t count);

typedef enum {
    STANDARD_FORM_CADENTIAL_CHORD_I6 = 0,
    STANDARD_FORM_CADENTIAL_CHORD_II6,
    STANDARD_FORM_CADENTIAL_CHORD_IV,
    STANDARD_FORM_CADENTIAL_CHORD_V,
    STANDARD_FORM_CADENTIAL_CHORD_V7,
    STANDARD_FORM_CADENTIAL_CHORD_I,
    STANDARD_FORM_CADENTIAL_CHORD_COUNT
} StandardFormCadentialChord;

/* HARMONIZATION: true when the chord may harmonize the core-line note at index: I6 for mi,
   ii6 or IV for fa, V or V7 for sol, I for do. The note fi is not characterized, so this takes
   core positions only. */
bool standard_form_cadential_chord_allowed(size_t core_index, StandardFormCadentialChord chord);

/* HARMONIZATION: true for the sol position, where the dominant is often elaborated with a
   cadential six-four. */
bool standard_form_cadential_six_four_elaborates(size_t core_index);

/* CLEAREST: true for four measures with one bass note per measure. Other lengths and shapes are
   possible cadential endings, just less clear. */
bool standard_form_cadential_is_clearest(size_t measures, size_t bass_note_count);

#endif
