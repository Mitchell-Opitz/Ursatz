#include "idea_label.h"

#include <stddef.h>

static const char *const NAMES[IDEA_LABEL_COUNT] = {"basic-idea", "contrasting-idea", "fragment", "unit"};

bool idea_label_valid(IdeaLabel value) {
    return value >= 0 && value < IDEA_LABEL_COUNT;
}

const char *idea_label_name(IdeaLabel value) {
    return idea_label_valid(value) ? NAMES[value] : NULL;
}

