#ifndef URSATZ_TEXT_TEXT_H
#define URSATZ_TEXT_TEXT_H

#include <stdbool.h>

/*
 * Ursatz Text (R-662): a compact text form of the JSON a layer's codec writes, and the views printed from the layers. A document
 * starts with the line "ursatz-text 1 <kind>". The grammar and the API are in library/text/README.md.
 */

#define URSATZ_TEXT_GRAMMAR_VERSION 1

typedef enum {
    TEXT_KIND_JSON,
    TEXT_KIND_PLAN,
    TEXT_KIND_READING,
    TEXT_KIND_REALIZATION,
    TEXT_KIND_KNOWLEDGE,
    TEXT_KIND_KNOWLEDGE_ENTRY,
    TEXT_KIND_COMMANDS,
    TEXT_KIND_VIEW,
    TEXT_KIND_BLUEPRINT
} TextKind;

/* Filled on failure: the 1-based line of the text, 0 for the document as a whole. */
typedef struct {
    int line;
    char message[256];
} TextError;

/* "json", "plan", "reading", "realization", "knowledge", "knowledge-entry", "commands", "view" or "blueprint". The kinds `commands` and
   `blueprint` have line grammars of their own, read by the workshop (R-665, R-747): text_to_json reads one only as the tree. */
const char *text_kind_name(TextKind kind);

/* The kind named `name`; false (kind untouched) for an unknown name. */
bool text_kind_find(const char *name, TextKind *kind);

#endif
