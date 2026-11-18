#ifndef URSATZ_COMPOSER_THEME_SHAPE_H
#define URSATZ_COMPOSER_THEME_SHAPE_H

#include <stdbool.h>
#include <stddef.h>

#include <ursatz_composer/constraint.h>

/*
 * What a theme of each beginning and ending is made of (R-416): its ideas, the slot each takes its motive from, the
 * role each has, the place each holds and how long each is. One table, read by grow, the fidelity check, the motives
 * stage and the API vocabulary, so a theme's shape is not written again in each of them.
 */

/*
 * The motives a theme is built from, by the function of the ideas they give (R-143, R-166): the basic motive begins
 * the theme; the others are optional and give the ideas that contrast, continue and end it. A slot with no motive of
 * its own is made from the basic motive, as every idea is without slots.
 *
 *   slot          ideas
 *   basic         basic idea, repetition, consequent basic idea
 *   contrasting   contrasting idea, consequent contrasting idea
 *   continuation  fragment 1, fragment 2
 *   cadential     cadential (after a continuation), cadential 1, cadential 2
 */
typedef enum {
    GROW_SLOT_BASIC,
    GROW_SLOT_CONTRASTING,
    GROW_SLOT_CONTINUATION,
    GROW_SLOT_CADENTIAL,
    GROW_SLOT_COUNT
} GrowSlot;

/*
 * The ideas of a theme by name, as the plan names them (R-206). Which ones a theme has depends on its pairing: the
 * basic idea always; the repetition for a presentation, the contrasting idea for an antecedent or a compound basic
 * idea; fragment 1, fragment 2 and the cadential idea for a continuation, the consequent basic idea and the
 * consequent contrasting idea for a consequent, cadential 1 and cadential 2 for a cadential ending.
 */
typedef enum {
    GROW_IDEA_BASIC,
    GROW_IDEA_REPETITION,
    GROW_IDEA_CONTRASTING,
    GROW_IDEA_CONSEQUENT_BASIC,
    GROW_IDEA_CONSEQUENT_CONTRASTING,
    GROW_IDEA_FRAGMENT_1,
    GROW_IDEA_FRAGMENT_2,
    GROW_IDEA_CADENTIAL,
    GROW_IDEA_CADENTIAL_1,
    GROW_IDEA_CADENTIAL_2,
    GROW_IDEA_COUNT
} GrowIdea;

/* The ideas are the section description's (plan/section.h): the same values in the same order. */
_Static_assert((int)GROW_IDEA_BASIC == (int)SECTION_IDEA_BASIC, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_REPETITION == (int)SECTION_IDEA_REPETITION, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_CONTRASTING == (int)SECTION_IDEA_CONTRASTING, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_CONSEQUENT_BASIC == (int)SECTION_IDEA_CONSEQUENT_BASIC, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_CONSEQUENT_CONTRASTING == (int)SECTION_IDEA_CONSEQUENT_CONTRASTING, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_FRAGMENT_1 == (int)SECTION_IDEA_FRAGMENT_1, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_FRAGMENT_2 == (int)SECTION_IDEA_FRAGMENT_2, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_CADENTIAL == (int)SECTION_IDEA_CADENTIAL, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_CADENTIAL_1 == (int)SECTION_IDEA_CADENTIAL_1, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_CADENTIAL_2 == (int)SECTION_IDEA_CADENTIAL_2, "GrowIdea follows SectionIdea");
_Static_assert((int)GROW_IDEA_COUNT == SECTION_CONSTRUCTION_IDEAS, "GrowIdea follows SectionIdea");

/* The role an idea has in a theme by its name (R-355, grow_idea_role). */
typedef enum {
    GROW_ROLE_NONE,
    GROW_ROLE_PRESENTATION,
    GROW_ROLE_CONTINUATION,
    GROW_ROLE_CADENTIAL,
    GROW_ROLE_CONTRAST
} GrowIdeaRole;

/*
 * One beginning (presentation, antecedent, compound basic idea) or one ending (continuation, consequent, cadential).
 * `value` is the SectionBeginning or SectionEnding. `plan_name` is the word the plan's theme-pairing constraint
 * uses; `library_name` is the name standard-form's phrase_beginning_from_name and phrase_ending_from_name take.
 */
typedef struct {
    bool beginning;
    int value;
    const char *plan_name;
    const char *library_name;
} ThemeShapeHalf;

/*
 * One idea of one half. `half` is the SectionBeginning (in_beginning) or SectionEnding value it belongs to;
 * `place` is the GrowPlace index the idea holds in a theme of that half (grow_internal.h); `first` is whether the
 * idea is the first of its slot; `units` is its length in eighths of the theme.
 */
typedef struct {
    GrowIdea idea;
    const char *name;
    bool in_beginning;
    int half;
    unsigned place;
    GrowSlot slot;
    bool first;
    GrowIdeaRole role;
    size_t units;
} ThemeShapeIdea;

/* One slot: its name ("basic"), the function of its ideas ("beginning") and the plan node a motive of it is named ("s1"; NULL for basic). */
typedef struct {
    GrowSlot slot;
    const char *name;
    const char *function;
    const char *node_name;
} ThemeShapeSlot;

/* The halves: the three beginnings, then the three endings, in enum order. */
const ThemeShapeHalf *theme_shape_beginning(SectionBeginning beginning);
const ThemeShapeHalf *theme_shape_ending(SectionEnding ending);
/* The half of that kind with that plan name; NULL for an unknown name or NULL. */
const ThemeShapeHalf *theme_shape_half_find(bool beginning, const char *plan_name);

/* The idea rows, one per idea of each half (13 rows for 10 ideas: the basic idea is in every beginning). NULL past the end. */
size_t theme_shape_idea_count(void);
const ThemeShapeIdea *theme_shape_idea_at(size_t index);
/* The row of the idea under the pairing's beginning or ending; NULL when the pairing has none. */
const ThemeShapeIdea *theme_shape_idea_in(const SectionPairing *pairing, GrowIdea idea);
/* The row at that place under the pairing: places BASIC and REPETITION come from the beginning, the others from the ending. */
const ThemeShapeIdea *theme_shape_place_in(const SectionPairing *pairing, unsigned place);
/* The row of the idea with that name in any half (the first); NULL for an unknown name or NULL. */
const ThemeShapeIdea *theme_shape_idea_find(const char *name);
/* The slot of the idea (the first row it has); GROW_SLOT_BASIC for an invalid idea. */
GrowSlot theme_shape_idea_slot(GrowIdea idea);
/* "basic idea", "repetition", ... in GrowIdea order; "unknown" for an invalid idea. */
const char *theme_shape_idea_name(GrowIdea idea);

/* The slot row; NULL for an invalid slot. */
const ThemeShapeSlot *theme_shape_slot(GrowSlot slot);

#endif
