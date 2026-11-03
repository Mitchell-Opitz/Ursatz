#include <stdlib.h>
#include <string.h>

#include <ursatz_composer/composer_api.h>
#include <ursatz_composer/variant_registry.h>

#include "api_internal.h"

ComposerStatus composer_api_variant_operations(ComposerResponse *out) {
    memset(out, 0, sizeof *out);
    out->json = variant_registry_to_json();
    return out->json ? COMPOSER_OK : api_fail(&out->error, COMPOSER_ERR_ALLOC, "", "out of memory");
}
