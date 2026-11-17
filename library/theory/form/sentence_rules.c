#include "theory/form/sentence_rules.h"

bool standard_form_subphrase_roles_in_order(SubphraseRole first, SubphraseRole second) {
    return first == SUBPHRASE_ROLE_PRESENTATION && second == SUBPHRASE_ROLE_CONTINUATION;
}

bool standard_form_presentation_length_usual(size_t measures) {
    return measures == 4;
}

bool standard_form_basic_idea_length_valid(size_t measures) {
    return measures == 1 || measures == 2 || measures == 4;
}

bool standard_form_continuation_length_usual(size_t presentation, size_t continuation) {
    return presentation > 0 && continuation >= presentation;
}

bool standard_form_continuation_balanced(size_t presentation, size_t continuation) {
    return presentation > 0 && presentation == continuation;
}

bool standard_form_is_fragmentation(size_t established_unit_size, size_t unit_size) {
    return unit_size > 0 && unit_size < established_unit_size;
}

bool standard_form_idea_reads_as_repetition(bool same_contour) {
    return same_contour;
}

bool standard_form_phrase_is_sentential(bool has_presentation, bool has_continuation) {
    return has_presentation && has_continuation;
}

bool standard_form_sentence_is_archetypal(size_t total_measures, size_t presentation_measures,
                                          size_t continuation_measures, bool has_fragmentation) {
    return (total_measures == 4 || total_measures == 8) &&
           presentation_measures + continuation_measures == total_measures &&
           standard_form_continuation_balanced(presentation_measures, continuation_measures) && has_fragmentation;
}
