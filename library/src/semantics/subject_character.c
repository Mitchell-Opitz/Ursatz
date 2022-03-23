#include "semantics/subject_character.h"

#include <stddef.h>

static const char *const NAMES[SUBJECT_CHARACTER_COUNT] = {"toccata", "ricercar"};

bool subject_character_valid(SubjectCharacter character) {
    return character >= 0 && character < SUBJECT_CHARACTER_COUNT;
}

const char *subject_character_name(SubjectCharacter character) {
    return subject_character_valid(character) ? NAMES[character] : NULL;
}
