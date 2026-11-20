#ifndef URSATZ_TEXT_TEXT_TREE_H
#define URSATZ_TEXT_TEXT_TREE_H

#include <stdbool.h>
#include <stddef.h>

#include <cJSON.h>

#include "text/text.h"

/*
 * The tree form of Ursatz Text (R-662, grammar 1): any JSON object as indented lines, and back. The rules, with one example each, are
 * in library/text/README.md. A document is the header line "ursatz-text 1 <kind>" and the members of the object at indent 0, one line
 * each, two spaces a level. Printing then parsing gives a tree cJSON_Compare finds equal, with the member order kept.
 */

/* The document: the header, then the members of `object`. NULL when `object` is not an object. Caller frees. */
char *text_tree_print(const cJSON *object, TextKind kind);

/*
 * One scalar token: null, true, false, a number (the text cJSON_PrintUnformatted gives it), n/d for {"num", "den"} (integers, den above
 * 0), a..b for {"start", "end"} of two such, a bare or a quoted string; or an inline array of scalars written [a b c]. NULL for anything
 * else (an object, an array that holds a non-scalar). The views and the command sheet print values with it. Caller frees.
 */
char *text_scalar_print(const cJSON *value);

typedef struct {
    int indent; /* spaces */
    int number; /* the 1-based line of the text */
    char *text; /* the line without its indent and trailing space */
} TextLine;

typedef struct {
    TextLine *lines;
    size_t count;
} TextLines;

/* Splits `text` into lines, dropping blank lines and comment lines (the first non-space character is '#'). A tab or an odd indent is
   refused: the line and "indent with two spaces". Free with text_lines_free (also after a failure). */
bool text_lines_split(const char *text, TextLines *out, TextError *error);
void text_lines_free(TextLines *lines);

/* The units of one line (`number` is its line, for errors) as an array: a scalar, a rational or a span object, an empty object or an
   inline array of units. An unterminated string or [ is refused with the line. NULL on failure. Caller deletes. */
cJSON *text_units_parse(const char *line, int number, TextError *error);

/* The block of lines at `indent` spaces from *pos: an object when its lines are members, an array when they start with "- ". Advances
   *pos past the block; no line at that indent gives an empty object. NULL with the line in `error` on failure. Caller deletes. */
cJSON *text_block_parse(const TextLines *lines, size_t *pos, int indent, TextError *error);

/* Parses a document: the header, then the block at indent 0 as an object. *kind is the header's kind. NULL with the line in `error` on
   failure. Caller deletes. */
cJSON *text_tree_parse(const char *text, TextKind *kind, TextError *error);

#endif
