#ifndef URSATZ_COMPOSER_VARIANT_INTERNAL_H
#define URSATZ_COMPOSER_VARIANT_INTERNAL_H

#include <ursatz_composer/variant_registry.h>

/* Fills the record's kind, kept, changed and recognizability from the source and a variant of it. */
void variant_record_difference(const Motive *source, const Motive *result, VariantRecord *record);

/* Writes the record's derivation from its steps, kept and changed. */
void variant_record_derivation(VariantRecord *record);

#endif
