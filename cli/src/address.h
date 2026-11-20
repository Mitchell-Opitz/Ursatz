#ifndef URSATZ_CLI_ADDRESS_H
#define URSATZ_CLI_ADDRESS_H

#include <stdbool.h>

/*
 * The addresses of the things in a data folder (R-658): <kind>:<id>, and item:<id>@v<n> for one version of an item. A subcommand that takes a path or an
 * address decides by cli_is_address (a known kind followed by ":"; a path that merely contains a colon is a path).
 */
typedef enum {
    CLI_ADDRESS_ITEM,
    CLI_ADDRESS_PIECE,
    CLI_ADDRESS_PROJECT,
    CLI_ADDRESS_SUBJECT,
    CLI_ADDRESS_COLLECTION,
    CLI_ADDRESS_STYLE,
    CLI_ADDRESS_LINKS,
    CLI_ADDRESS_JOB
} CliAddressKind;

typedef struct {
    CliAddressKind kind;
    char id[256];
    long version; /* 0 when absent: only an item address has one */
} CliAddress;

/* True and *out filled for an address; false for anything else (an unknown kind, an empty id, a version on a kind that has none, a version that is not
 * a whole number from 1). */
bool cli_address_parse(const char *text, CliAddress *out);

/* A known kind followed by ":": the text is meant as an address (cli_address_parse says whether it is a good one). */
bool cli_is_address(const char *text);

#endif
