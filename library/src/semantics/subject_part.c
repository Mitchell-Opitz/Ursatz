#include "semantics/subject_part.h"

#include <stddef.h>

static const char *const NAMES[SUBJECT_PART_COUNT] = {"kopfmotiv", "fortspinnung", "cadence"};

bool subject_part_valid(SubjectPart part) {
    return part >= 0 && part < SUBJECT_PART_COUNT;
}

const char *subject_part_name(SubjectPart part) {
    return subject_part_valid(part) ? NAMES[part] : NULL;
}
