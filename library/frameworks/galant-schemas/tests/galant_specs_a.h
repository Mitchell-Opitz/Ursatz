#ifndef URSATZ_GALANT_SCHEMAS_TESTS_GALANT_SPECS_A_H
#define URSATZ_GALANT_SCHEMAS_TESTS_GALANT_SPECS_A_H

#include "galant_prototype_spec.h"

/* The opening gambits and the continuation schemas, as the chapters give them. */

static const PrototypeSpec SPECS_A[] = {
    {GALANT_SCHEMA_MEYER, "meyer", SCHEMA_FUNCTION_OPENING_GAMBIT, "WSWS",
     {DN(1), DN(7), DN(4), DN(3)}, {DN(1), DN(2), DN(7), DN(1)}, {NONE},
     {"5", "6/4/3", "6/5", "5"}, {"I", "V", "V", "I"}, false, false},
    {GALANT_SCHEMA_ROMANESCA, "romanesca", SCHEMA_FUNCTION_OPENING_GAMBIT, "SWSW",
     {DN(1), DN(5), DN(1), DN(1)}, {DN(1), DN(7), DN(6), DN(3)}, {NONE},
     {"5", "6", "5", "6"}, {"I", "V", "vi", "I"}, true, false},
    {GALANT_SCHEMA_DO_RE_MI, "do-re-mi", SCHEMA_FUNCTION_OPENING_GAMBIT, "SWS",
     {DN(1), DN(2), DN(3)}, {DN(1), DN(7), DN(1)}, {NONE},
     {"5", "6", "5"}, {"I", "V", "I"}, true, false},
    {GALANT_SCHEMA_DO_RE_MI_FOUR_STAGE, "do-re-mi-four-stage", SCHEMA_FUNCTION_OPENING_GAMBIT, "SWWS",
     {DN(1), DN(2), DN(2), DN(3)}, {DN(1), DN(7), DN(7), DN(1)}, {NONE},
     {"5", "6", "6", "5"}, {"I", "V", "V", "I"}, true, false},
    {GALANT_SCHEMA_SOL_FA_MI, "sol-fa-mi", SCHEMA_FUNCTION_OPENING_GAMBIT, "WSWS",
     {DN(5), DN(4), DN(4), DN(3)}, {DN(1), DN(2), DN(7), DN(1)}, {NONE},
     {"5", "5", "6/5", "5"}, {"I", "ii", "V", "I"}, true, false},
    {GALANT_SCHEMA_APRILE, "aprile", SCHEMA_FUNCTION_OPENING_GAMBIT, "SWSW",
     {DN(1), DN(7), DN(2), DN(1)}, {DN(1), DN(2), DN(7), DN(1)}, {NONE},
     {"5", "6/4/3", "6/5", "5"}, {"I", "V", "V", "I"}, false, false},
    {GALANT_SCHEMA_JUPITER, "jupiter", SCHEMA_FUNCTION_OPENING_GAMBIT, "SWSW",
     {DN(1), DN(2), DN(4), DN(3)}, {DN(1), DN(7), DN(5), DN(1)}, {NONE},
     {"5", "6", "5", "5"}, {"I", "V", "V", "I"}, false, false},
    {GALANT_SCHEMA_PASTORELLA, "pastorella", SCHEMA_FUNCTION_OPENING_GAMBIT, "SWSW",
     {DN(3), DN(2), DN(4), DN(3)}, {DN(1), DN(5), DN(5), DN(1)}, {NONE},
     {"5", "5", "7", "5"}, {"I", "V", "V", "I"}, false, false},
    {GALANT_SCHEMA_PRINNER, "prinner", SCHEMA_FUNCTION_CONTINUATION, "SWSW",
     {DE(6), DN(5), DN(4), DE(3)}, {DN(4), DE(3), DN(2), DN(1)}, {NONE},
     {"5", "6", "7-6", "5"}, {"IV", "I", "vii", "I"}, true, true},
    {GALANT_SCHEMA_MODULATING_PRINNER, "modulating-prinner", SCHEMA_FUNCTION_CONTINUATION, "SWSW",
     {DN(3), DN(2), DN(1), DN(7)}, {DN(1), DN(7), DN(6), DN(5)}, {NONE},
     {"5", "6", "7-#6", "5"}, {"I", "V", "vii/V", "V"}, true, true},
    {GALANT_SCHEMA_PRINNER_WITH_DOMINANT, "prinner-with-dominant", SCHEMA_FUNCTION_CONTINUATION, "SWSWS",
     {DE(6), DN(5), DN(4), DN(4), DE(3)}, {DN(4), DE(3), DN(2), DN(5), DN(1)}, {NONE},
     {"5", "6", "7-6", "7", "5"}, {"IV", "I", "vii", "V", "I"}, true, false},
    {GALANT_SCHEMA_MODULATING_PRINNER_WITH_DOMINANT, "modulating-prinner-with-dominant",
     SCHEMA_FUNCTION_CONTINUATION, "SWSWS",
     {DN(3), DN(2), DN(1), DN(1), DN(7)}, {DN(1), DN(7), DN(6), DN(2), DN(5)}, {NONE},
     {"5", "6", "7-#6", "7/#", "5"}, {"I", "V", "vii/V", "V/V", "V"}, true, false},
    {GALANT_SCHEMA_FONTE, "fonte", SCHEMA_FUNCTION_CONTINUATION, "WSWS",
     {DN(5), DN(4), DN(4), DN(3)}, {DR(1), DN(2), DN(7), DN(1)}, {NONE},
     {"6/5", "5", "6/5", "5"}, {"V/ii", "ii", "V", "I"}, true, false},
    {GALANT_SCHEMA_MONTE, "monte", SCHEMA_FUNCTION_CONTINUATION, "WSWS",
     {DN(1), DN(6), DN(2), DN(7)}, {DN(3), DN(4), DR(4), DN(5)}, {DL(7), NONE, DN(1), NONE},
     {"6/5", "5", "6/5", "5"}, {"V/IV", "V", "V/V", "V"}, true, false},
    {GALANT_SCHEMA_PONTE, "ponte", SCHEMA_FUNCTION_CONTINUATION, "SWS",
     {DN(5), DN(7), DN(2)}, {DN(5), DN(5), DN(5)}, {NONE},
     {"5", "7", "7"}, {"V", "V", "V"}, true, false},
};

#define SPECS_A_COUNT (sizeof(SPECS_A) / sizeof(SPECS_A[0]))

#endif
