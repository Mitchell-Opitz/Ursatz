#ifndef WORKSHOP_TEST_THEME_BODY_H
#define WORKSHOP_TEST_THEME_BODY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* The body of a theme version in format 2 (R-650) for the tests: a sentence of 8 bars in natural minor whose basic slot names the
 * motif version a and whose cadential slot names the motif version b (the same item: one motive), both version 1. seed < 0 gives
 * the body no seed. */
static inline void theme_body_text(char *out, size_t size, const char *a, const char *b, int seed) {
    char motives[512];
    bool same = strcmp(a, b) == 0;
    if (same) {
        snprintf(motives, sizeof(motives), "[{\"id\":\"M1\",\"$item\":{\"item_id\":\"%s\",\"version\":1}}]", a);
    } else {
        snprintf(motives, sizeof(motives),
                 "[{\"id\":\"M1\",\"$item\":{\"item_id\":\"%s\",\"version\":1}},{\"id\":\"M2\",\"$item\":{\"item_id\":\"%s\",\"version\":1}}]", a, b);
    }
    char tail[32] = "";
    if (seed >= 0) {
        snprintf(tail, sizeof(tail), ",\"seed\":%d", seed);
    }
    snprintf(out, size,
             "{\"uses\":[],\"request\":{\"schema\":\"ursatz-theme-request\",\"schema_version\":2,\"key\":\"C\",\"into\":\"sentence\","
             "\"material\":\"omt.material.natural-minor\",\"meter\":\"4/4\",\"bars\":8,\"cadence\":\"PAC\",\"motives\":%s,"
             "\"slots\":{\"basic\":\"M1\",\"cadential\":\"%s\",\"contrasting\":null,\"continuation\":null},\"ideas\":{},\"chords\":[],"
             "\"accompaniment\":null,\"voicing\":null,\"tempo\":null}%s}",
             motives, same ? "M1" : "M2", tail);
}

#endif
