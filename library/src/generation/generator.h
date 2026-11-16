#ifndef MUSIC_GENERATION_GENERATOR_H
#define MUSIC_GENERATION_GENERATOR_H

#include <stdbool.h>

/*
 * Generator (Registry #26): the declared metadata of a generator, the descriptor a fill operator carries. A Generator
 * declares, not merely implements (TDD Sec160): what it takes and gives, what configuration it accepts, what
 * Constraint and Preference vocabulary it honors, whether the same input, seed and configuration always give the same
 * output, and its version. Nothing is invoked through this struct: the function-pointer interface it once had is gone,
 * because nothing calls a generator through it.
 *   - supported_inputs/supported_outputs: opaque type-tag strings.
 *   - configuration_description: what configuration the generator accepts, "none" when it takes none.
 *   - constraints_description: what Constraint/Preference vocabulary it honors.
 *   - deterministic: whether repeated calls with the same input, random_seed and configuration MUST produce identical
 *     output.
 *   - version: a required, non-empty generator_version string.
 */

typedef struct {
    const char *name;
    const char *supported_inputs;
    const char *supported_outputs;
    const char *configuration_description;
    const char *constraints_description;
    bool deterministic;
    const char *version;
} Generator;

#endif
