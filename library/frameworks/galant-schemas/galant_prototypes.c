#include "galant_prototypes.h"

#include <stddef.h>

#define N GALANT_ALTERATION_NATURAL
#define E GALANT_ALTERATION_EITHER
#define STRONG BEAT_STRENGTH_STRONG
#define WEAK BEAT_STRENGTH_WEAK

static const GalantPrototype PROTOTYPES[GALANT_SCHEMA_COUNT] = {
    {GALANT_SCHEMA_MEYER,
     "meyer",
     SCHEMA_FUNCTION_OPENING_GAMBIT,
     4,
     {{WEAK, {1, N}, {1, N}, NULL, "I", NULL},
      {STRONG, {7, N}, {2, N}, NULL, "V", NULL},
      {WEAK, {4, N}, {7, N}, NULL, "V", NULL},
      {STRONG, {3, N}, {1, N}, NULL, "I", NULL}},
     false,
     false},
    {GALANT_SCHEMA_PRINNER,
     "prinner",
     SCHEMA_FUNCTION_CONTINUATION,
     4,
     {{STRONG, {6, E}, {4, N}, "5", "IV", NULL},
      {WEAK, {5, N}, {3, E}, "6", "I", NULL},
      {STRONG, {4, N}, {2, N}, "7-6", "vii", NULL},
      {WEAK, {3, E}, {1, N}, "5", "I", NULL}},
     true,
     true},
    {GALANT_SCHEMA_MODULATING_PRINNER,
     "modulating-prinner",
     SCHEMA_FUNCTION_CONTINUATION,
     4,
     {{STRONG, {3, N}, {1, N}, "5", "I", "IV"},
      {WEAK, {2, N}, {7, N}, "6", "V", "I"},
      {STRONG, {1, N}, {6, N}, "7-#6", "vii/V", "vii"},
      {WEAK, {7, N}, {5, N}, "5", "V", "I"}},
     true,
     true},
};

bool galant_schema_valid(GalantSchemaId id) {
    return id >= 0 && id < GALANT_SCHEMA_COUNT;
}

const char *galant_schema_name(GalantSchemaId id) {
    return galant_schema_valid(id) ? PROTOTYPES[id].name : NULL;
}

const GalantPrototype *galant_prototype(GalantSchemaId id) {
    return galant_schema_valid(id) ? &PROTOTYPES[id] : NULL;
}
