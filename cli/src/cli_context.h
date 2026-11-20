#ifndef URSATZ_COMPOSER_CLI_CONTEXT_H
#define URSATZ_COMPOSER_CLI_CONTEXT_H

#include <ursatz_composer/composer_api.h>

/*
 * The call context every command of this process hands the Composer API (composer_api.h): one knowledge snapshot of the
 * embedded entries, built when first asked for and released when the process exits. When the snapshot cannot be built
 * the reason is printed once and the context holds none, so every entry refuses with "a knowledge snapshot is required".
 */
const ComposerContext *cli_context(void);

#endif
