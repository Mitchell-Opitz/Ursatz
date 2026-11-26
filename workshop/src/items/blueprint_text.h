#ifndef URSATZ_WORKSHOP_ITEMS_BLUEPRINT_TEXT_H
#define URSATZ_WORKSHOP_ITEMS_BLUEPRINT_TEXT_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

/*
 * A blueprint in Ursatz Text (R-747): the theme request of R-743 with a `blueprint`, one line a bar. The document is
 *
 *   ursatz-text 1 blueprint
 *   key C major
 *   meter 4/4
 *   into sentence
 *   bars 8
 *   cadence PAC
 *   tempo 92
 *   accompaniment arpeggiated
 *   climax 6 2 A5
 *   1: chords I ; rhythm x:h x:e x:e x:q ; anchor 0 D5 ; event 0 appoggiatura
 *   2: chords V7 ; melody G5:q. F5:e D5:h ; bass G2:w ; inner r:w ; inner r:w
 *
 * Header lines come first, each at most once, in any order: `key <tonic> [<material>]`, `meter <n/m>`, `into <word>`, `bars <n>`,
 * `cadence <word>`, `first_cadence <word>`, `development <word>`, `tempo <qpm>`, `accompaniment <pattern> [voicing <id>]`, `climax <bar>
 * <beats> <pitch>` (the bar from 1) and `basic <degrees> [pickup <beats>]` (the motive of the basic slot, as `motives` and `slots.basic`).
 * Then one line a bar, `<bar from 1>: ` and fields split by `;`: `chords`, `melody`, `rhythm`, `job`, `bass` (each at most once, the rest of
 * the field is the text of the member), `anchor <beats> <pitch>`, `event <beats> <type>` and `inner <notes>` (each repeatable). A bar with no
 * field is an open bar. Comment lines (first character `#`) and blank lines are ignored.
 *
 * blueprint_text_read makes the request JSON (schema ursatz-theme-request, version 2); it only reads the grammar: whether the request is
 * a theme the Composer takes is the Composer's to say. A failure is false with `error` holding "line <n>: <why>" (or "<why>" for the
 * document as a whole): an unknown header or field, a header given twice, a header after the first bar, a bar out of order, a line
 * before the header, a value that is not what the field takes.
 *
 * blueprint_text_write is the other way: false with the reason when the request has no `blueprint`, or holds a member or a value the
 * text has no place for (a style, a motive with segments, a ';' inside a note list).
 */

/* The request JSON of `text` (a document), a new tree. */
bool blueprint_text_read(const char *text, cJSON **request, char *error, size_t size);

/* The document of a theme request with a blueprint, a new string. */
bool blueprint_text_write(const cJSON *request, char **text, char *error, size_t size);

/* Whether `text` is a blueprint document: its first line, comment and blank lines apart, is `ursatz-text 1 blueprint`. */
bool blueprint_text_is(const char *text);

#endif
