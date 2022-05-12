#include "semantics/schema_function.h"

#include <stddef.h>

static const char *const NAMES[SCHEMA_FUNCTION_COUNT] = {"opening-gambit", "continuation", "pre-cadential",
                                                         "cadence", "post-cadential"};

bool schema_function_valid(SchemaFunction function) {
    return function >= 0 && function < SCHEMA_FUNCTION_COUNT;
}

const char *schema_function_name(SchemaFunction function) {
    return schema_function_valid(function) ? NAMES[function] : NULL;
}
