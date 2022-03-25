#include "semantics/ostinato_type.h"

#include <stddef.h>

static const char *const NAMES[OSTINATO_TYPE_COUNT] = {"ostinato", "ground-bass", "chaconne", "passacaglia"};

bool ostinato_type_valid(OstinatoType type) {
    return type >= 0 && type < OSTINATO_TYPE_COUNT;
}

const char *ostinato_type_name(OstinatoType type) {
    return ostinato_type_valid(type) ? NAMES[type] : NULL;
}

bool ostinato_type_is_ground_bass(OstinatoType type) {
    return type == OSTINATO_TYPE_GROUND_BASS || ostinato_type_is_triple_meter_form(type);
}

bool ostinato_type_is_triple_meter_form(OstinatoType type) {
    return type == OSTINATO_TYPE_CHACONNE || type == OSTINATO_TYPE_PASSACAGLIA;
}
