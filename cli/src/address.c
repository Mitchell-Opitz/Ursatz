#include "address.h"

#include <stdlib.h>
#include <string.h>

static const struct {
    const char *name;
    CliAddressKind kind;
} KINDS[] = {{"item", CLI_ADDRESS_ITEM},       {"piece", CLI_ADDRESS_PIECE}, {"project", CLI_ADDRESS_PROJECT}, {"subject", CLI_ADDRESS_SUBJECT},
             {"collection", CLI_ADDRESS_COLLECTION}, {"style", CLI_ADDRESS_STYLE}, {"links", CLI_ADDRESS_LINKS},  {"job", CLI_ADDRESS_JOB}};

/* The length of the kind word at the start of text when it is followed by ':', else 0. */
static size_t kind_length(const char *text, CliAddressKind *kind) {
    for (size_t i = 0; text && i < sizeof(KINDS) / sizeof(KINDS[0]); i++) {
        size_t length = strlen(KINDS[i].name);
        if (strncmp(text, KINDS[i].name, length) == 0 && text[length] == ':') {
            if (kind) {
                *kind = KINDS[i].kind;
            }
            return length;
        }
    }
    return 0;
}

bool cli_is_address(const char *text) {
    return kind_length(text, NULL) > 0;
}

bool cli_address_parse(const char *text, CliAddress *out) {
    CliAddressKind kind;
    size_t length = kind_length(text, &kind);
    if (length == 0) {
        return false;
    }
    const char *id = text + length + 1;
    const char *at = strstr(id, "@v");
    size_t id_length = at ? (size_t)(at - id) : strlen(id);
    if (id_length == 0 || id_length >= sizeof(out->id) || strchr(id, '/') != NULL) {
        return false;
    }
    long version = 0;
    if (at) {
        char *end = NULL;
        version = strtol(at + 2, &end, 10);
        if (kind != CLI_ADDRESS_ITEM || end == at + 2 || *end != '\0' || version < 1) {
            return false;
        }
    }
    out->kind = kind;
    memcpy(out->id, id, id_length);
    out->id[id_length] = '\0';
    out->version = version;
    return true;
}
