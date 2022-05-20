#ifndef URSATZ_GALANT_SCHEMAS_TESTS_GALANT_SPECS_B_H
#define URSATZ_GALANT_SCHEMAS_TESTS_GALANT_SPECS_B_H

#include "galant_prototype_spec.h"

/* The pre-cadential, cadence, and post-cadential schemas, as the chapter gives
   them. */

static const PrototypeSpec SPECS_B[] = {
    {GALANT_SCHEMA_FENAROLI, "fenaroli", SCHEMA_FUNCTION_PRE_CADENTIAL, "SWSW",
     {DN(4), DN(3), DN(7), DN(1)}, {DN(7), DN(1), DN(2), DN(3)}, {NONE},
     {"6/5", "5", "6/5", "6"}, {"V", "I", "V", "I"}, true, false},
    {GALANT_SCHEMA_INDUGIO, "indugio", SCHEMA_FUNCTION_PRE_CADENTIAL, "SWSWS",
     {DN(2), DN(4), DN(6), DN(1), DN(7)}, {DN(4), DN(4), DN(4), DR(4), DN(5)}, {NONE},
     {"6/5", "6/5", "6/5", "6/5", "5"}, {"IV", "IV", "IV", "V/V", "V"}, true, false},
    {GALANT_SCHEMA_DECEPTIVE_CADENCE, "deceptive-cadence", SCHEMA_FUNCTION_PRE_CADENTIAL, "WSWS",
     {DN(1), DN(2), DN(2), DN(1)}, {DN(3), DN(4), DN(5), DN(6)}, {NONE},
     {"6", "6/5", "5", "5"}, {"I", "ii", "V", "vi"}, true, false},
    {GALANT_SCHEMA_EVADED_CADENCE, "evaded-cadence", SCHEMA_FUNCTION_PRE_CADENTIAL, "WSWS",
     {DN(1), DN(2), DN(2), DN(1)}, {DN(3), DN(4), DN(5), DN(3)}, {NONE},
     {"6", "6/5", "5", "6"}, {"I", "ii", "V", "I"}, true, false},
    {GALANT_SCHEMA_PASSO_INDIETRO, "passo-indietro", SCHEMA_FUNCTION_PRE_CADENTIAL, "SW",
     {DN(7), DN(1)}, {DN(4), DN(3)}, {NONE},
     {"6/4/2", "6"}, {"V", "I"}, true, false},
    {GALANT_SCHEMA_COMMA, "comma", SCHEMA_FUNCTION_PRE_CADENTIAL, "WS",
     {DN(4), DN(3)}, {DN(7), DN(1)}, {NONE},
     {"6/5", "5"}, {"V", "I"}, true, false},
    {GALANT_SCHEMA_CONVERGING_CADENCE, "converging-cadence", SCHEMA_FUNCTION_PRE_CADENTIAL, "WSWS",
     {DN(3), DN(2), DN(1), DN(7)}, {DN(3), DN(4), DR(4), DN(5)}, {NONE},
     {"6", "6/5", "6/5", "5"}, {"I", "ii", "V/V", "V"}, true, false},
    {GALANT_SCHEMA_CADENZA_SEMPLICE, "cadenza-semplice", SCHEMA_FUNCTION_CADENCE, "WSWS",
     {DN(1), DN(2), DN(2), DN(1)}, {DN(3), DN(4), DN(5), DN(1)}, {NONE},
     {"6", "6/5", "5", "5"}, {"I", "ii", "V", "I"}, true, false},
    {GALANT_SCHEMA_CADENZA_COMPOSTA, "cadenza-composta", SCHEMA_FUNCTION_CADENCE, "WSSWS",
     {DN(1), DN(2), DN(3), DN(2), DN(1)}, {DN(3), DN(4), DN(5), DN(5), DN(1)}, {NONE},
     {"6", "6/5", "6/4", "7", "5"}, {"I", "ii", "Cad.", "V", "I"}, true, false},
    {GALANT_SCHEMA_CADENZA_DOPPIA, "cadenza-doppia", SCHEMA_FUNCTION_CADENCE, "SWSWS",
     {DN(4), DN(3), DN(2), DN(2), DN(1)}, {DN(5), DN(5), DN(5), DN(5), DN(1)}, {NONE},
     {"5", "6/4", "4", "3", "5"}, {"V", "V", "V", "V", "I"}, true, false},
    {GALANT_SCHEMA_QUIESCENZA, "quiescenza", SCHEMA_FUNCTION_POST_CADENTIAL, "WSWS",
     {DL(7), DN(6), DN(7), DN(1)}, {DN(1), DN(1), DN(1), DN(1)}, {NONE},
     {"b7", "6/4", "7/4/2", "5"}, {"V/IV", "V", "V", "I"}, true, false},
};

#define SPECS_B_COUNT (sizeof(SPECS_B) / sizeof(SPECS_B[0]))

#endif
