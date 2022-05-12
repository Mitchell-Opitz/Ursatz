#include "semantics/schema_sense.h"

#include <stddef.h>

static const char *const NAMES[SCHEMA_SENSE_COUNT] = {"prototype", "exemplar", "theory"};

bool schema_sense_valid(SchemaSense sense) {
    return sense >= 0 && sense < SCHEMA_SENSE_COUNT;
}

const char *schema_sense_name(SchemaSense sense) {
    return schema_sense_valid(sense) ? NAMES[sense] : NULL;
}
