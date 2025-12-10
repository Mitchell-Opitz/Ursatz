#ifndef MUSIC_GENERATION_GENERATOR_REGISTRY_H
#define MUSIC_GENERATION_GENERATOR_REGISTRY_H

#include <stddef.h>

#include "generation/generator.h"

/*
 * GeneratorRegistry: the set of Generators a consumer app has available,
 * found by name. Generator (generation/generator.h) is a function-pointer
 * interface a concrete generator module implements and exposes as a value;
 * this is where those values are collected so something can run one by name
 * (generation/generator_dispatch.h).
 *
 * The registry BORROWS the Generator values registered with it and never
 * copies or owns them: each Generator, and every string it points at, must
 * outlive the registry (or at least every call that uses it). Destroying the
 * registry destroys only the registry.
 *
 * Registration validates the Generator, so a lookup always returns one a
 * dispatch can call: name and version non-empty, generate non-NULL, and the
 * four descriptor strings (supported_inputs, supported_outputs,
 * configuration_description, constraints_description) non-NULL. A name is
 * unique: registering a second Generator under a name already present is
 * GENERATOR_REGISTRY_ERR_DUPLICATE_NAME and changes nothing, even if it is the
 * same Generator again. Names are compared exactly (case-sensitive). There is
 * no unregister. Registration order is kept.
 *
 * Not thread-safe: a registry is built, then read; do not register while
 * another thread uses it.
 */

typedef struct GeneratorRegistry GeneratorRegistry;

typedef enum {
    GENERATOR_REGISTRY_OK = 0,
    GENERATOR_REGISTRY_ERR_NULL_ARGUMENT,
    GENERATOR_REGISTRY_ERR_INVALID_GENERATOR, /* see the validation rules above */
    GENERATOR_REGISTRY_ERR_DUPLICATE_NAME,
    GENERATOR_REGISTRY_ERR_ALLOC
} GeneratorRegistryResult;

/* A new, empty registry the caller owns. *out is untouched on any other result. */
GeneratorRegistryResult generator_registry_create(GeneratorRegistry **out);

/* Safe on NULL. Does not touch the registered Generators. */
void generator_registry_destroy(GeneratorRegistry *registry);

/*
 * Adds a borrowed Generator. GENERATOR_REGISTRY_ERR_NULL_ARGUMENT for a NULL
 * registry or generator; INVALID_GENERATOR if it fails validation;
 * DUPLICATE_NAME if its name is already registered. The registry is unchanged
 * on any result other than OK.
 */
GeneratorRegistryResult generator_registry_register(GeneratorRegistry *registry, const Generator *generator);

/* The Generator registered under name, borrowed; NULL if there is none (or either argument is NULL). */
const Generator *generator_registry_lookup(const GeneratorRegistry *registry, const char *name);

/* How many Generators are registered (0 for NULL), and the one at index in registration order (NULL if out of range). */
size_t generator_registry_count(const GeneratorRegistry *registry);
const Generator *generator_registry_at(const GeneratorRegistry *registry, size_t index);

#endif
