#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <ursatz_composer/theme_shape.h>

#include "support/test_knowledge.h"
#include "ursatz_test.h"

/*
 * The theme shape table (theme_shape.c) pinned to the theme type entries (R-708): for each of the 3 x 3 pairings, the entry the
 * snapshot finds by the library names of the halves holds, in order, the pairing's rows of the table: name, slot, half, units and the
 * formal function of the row's role. The idea the entry marks `final` is the one at theme_shape_last_place, and the one it marks
 * `first` is the antecedent's contrasting idea. The table is read here, not generated from the entries: its readers hold no snapshot.
 */

static const char *function_code(GrowIdeaRole role) {
    switch (role) {
    case GROW_ROLE_PRESENTATION:
        return "basic-idea";
    case GROW_ROLE_CONTRAST:
        return "contrasting-idea";
    case GROW_ROLE_CONTINUATION:
        return "fragment";
    case GROW_ROLE_CADENTIAL:
        return "cadential-idea";
    default:
        return NULL;
    }
}

static bool in_pairing(const ThemeShapeIdea *row, const SectionPairing *pairing) {
    return row->half == (row->in_beginning ? (int)pairing->beginning : (int)pairing->ending);
}

int main(void) {
    for (int b = SECTION_BEGINNING_PRESENTATION; b <= SECTION_BEGINNING_COMPOUND_BASIC_IDEA; b++) {
        for (int e = SECTION_ENDING_CONTINUATION; e <= SECTION_ENDING_CADENTIAL; e++) {
            SectionPairing pairing = {.beginning = (SectionBeginning)b, .ending = (SectionEnding)e};
            const char *beginning = theme_shape_beginning(pairing.beginning)->library_name;
            const char *ending = theme_shape_ending(pairing.ending)->library_name;
            const ThemeType *type = knowledge_snapshot_theme_type_of(test_knowledge(), beginning, ending);
            assert(type != NULL);

            size_t at = 0;
            GrowPlace last = theme_shape_last_place(&pairing);
            size_t finals = 0;
            size_t firsts = 0;
            for (size_t i = 0; i < theme_shape_idea_count(); i++) {
                const ThemeShapeIdea *row = theme_shape_idea_at(i);
                if (!in_pairing(row, &pairing)) {
                    continue;
                }
                assert(at < type->idea_count);
                const ThemeTypeIdea *idea = &type->ideas[at++];
                assert(strcmp(idea->name, row->name) == 0);
                assert(strcmp(idea->slot, theme_shape_slot(row->slot)->name) == 0);
                assert(idea->in_beginning == row->in_beginning);
                assert((size_t)idea->units == row->units);
                assert(strcmp(idea->function->code, function_code(row->role)) == 0);
                if (idea->cadence == THEME_IDEA_CADENCE_FINAL) {
                    finals++;
                    assert(row->place == (unsigned)last);
                }
                if (idea->cadence == THEME_IDEA_CADENCE_FIRST) {
                    firsts++;
                    assert(pairing.beginning == SECTION_BEGINNING_ANTECEDENT && row->idea == GROW_IDEA_CONTRASTING);
                }
            }
            assert(at == type->idea_count);
            assert(finals == 1);
            assert(firsts == (pairing.beginning == SECTION_BEGINNING_ANTECEDENT ? 1u : 0u));
        }
    }
    return 0;
}
