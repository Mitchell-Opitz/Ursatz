#include "transform/transformation.h"

#include <stdlib.h>
#include <string.h>

#include "core/string_copy.h"

struct Transformation {
    EntityID *id;
    char *input_type;
    Value *parameters;
    char *output_type;
    char *preserved_dimensions;
    char *modified_dimensions;
    char *mapping_behavior;
    char *validation;
    bool deterministic;
    ProvenanceReference *provenance;
};

TransformationResult transformation_create(EntityID *id, const char *input_type, Value *parameters,
                                            const char *output_type, const char *preserved_dimensions,
                                            const char *modified_dimensions, const char *mapping_behavior,
                                            const char *validation, bool deterministic,
                                            ProvenanceReference *provenance, Transformation **out) {
    if (id == NULL) {
        return TRANSFORMATION_ERR_NULL_ID;
    }
    if (input_type == NULL || input_type[0] == '\0') {
        return TRANSFORMATION_ERR_NULL_INPUT_TYPE;
    }
    if (output_type == NULL || output_type[0] == '\0') {
        return TRANSFORMATION_ERR_NULL_OUTPUT_TYPE;
    }
    if (preserved_dimensions == NULL || preserved_dimensions[0] == '\0') {
        return TRANSFORMATION_ERR_NULL_PRESERVED_DIMENSIONS;
    }
    if (modified_dimensions == NULL || modified_dimensions[0] == '\0') {
        return TRANSFORMATION_ERR_NULL_MODIFIED_DIMENSIONS;
    }
    if (mapping_behavior == NULL || mapping_behavior[0] == '\0') {
        return TRANSFORMATION_ERR_NULL_MAPPING_BEHAVIOR;
    }
    if (validation == NULL || validation[0] == '\0') {
        return TRANSFORMATION_ERR_NULL_VALIDATION;
    }
    if (provenance == NULL) {
        return TRANSFORMATION_ERR_NULL_PROVENANCE;
    }

    Transformation *transformation = (Transformation *)malloc(sizeof(Transformation));
    if (transformation == NULL) {
        return TRANSFORMATION_ERR_ALLOC;
    }

    transformation->input_type = ursatz_string_copy(input_type);
    if (transformation->input_type == NULL) {
        free(transformation);
        return TRANSFORMATION_ERR_ALLOC;
    }

    transformation->output_type = ursatz_string_copy(output_type);
    if (transformation->output_type == NULL) {
        free(transformation->input_type);
        free(transformation);
        return TRANSFORMATION_ERR_ALLOC;
    }

    transformation->preserved_dimensions = ursatz_string_copy(preserved_dimensions);
    if (transformation->preserved_dimensions == NULL) {
        free(transformation->output_type);
        free(transformation->input_type);
        free(transformation);
        return TRANSFORMATION_ERR_ALLOC;
    }

    transformation->modified_dimensions = ursatz_string_copy(modified_dimensions);
    if (transformation->modified_dimensions == NULL) {
        free(transformation->preserved_dimensions);
        free(transformation->output_type);
        free(transformation->input_type);
        free(transformation);
        return TRANSFORMATION_ERR_ALLOC;
    }

    transformation->mapping_behavior = ursatz_string_copy(mapping_behavior);
    if (transformation->mapping_behavior == NULL) {
        free(transformation->modified_dimensions);
        free(transformation->preserved_dimensions);
        free(transformation->output_type);
        free(transformation->input_type);
        free(transformation);
        return TRANSFORMATION_ERR_ALLOC;
    }

    transformation->validation = ursatz_string_copy(validation);
    if (transformation->validation == NULL) {
        free(transformation->mapping_behavior);
        free(transformation->modified_dimensions);
        free(transformation->preserved_dimensions);
        free(transformation->output_type);
        free(transformation->input_type);
        free(transformation);
        return TRANSFORMATION_ERR_ALLOC;
    }

    transformation->id = id;
    transformation->parameters = parameters;
    transformation->deterministic = deterministic;
    transformation->provenance = provenance;

    *out = transformation;
    return TRANSFORMATION_OK;
}

void transformation_destroy(Transformation *transformation) {
    if (transformation == NULL) {
        return;
    }
    entity_id_destroy(transformation->id);
    free(transformation->input_type);
    value_destroy(transformation->parameters);
    free(transformation->output_type);
    free(transformation->preserved_dimensions);
    free(transformation->modified_dimensions);
    free(transformation->mapping_behavior);
    free(transformation->validation);
    provenance_reference_destroy(transformation->provenance);
    free(transformation);
}

const EntityID *transformation_id(const Transformation *transformation) {
    return transformation->id;
}

const char *transformation_input_type(const Transformation *transformation) {
    return transformation->input_type;
}

const Value *transformation_parameters(const Transformation *transformation) {
    return transformation->parameters;
}

const char *transformation_output_type(const Transformation *transformation) {
    return transformation->output_type;
}

const char *transformation_preserved_dimensions(const Transformation *transformation) {
    return transformation->preserved_dimensions;
}

const char *transformation_modified_dimensions(const Transformation *transformation) {
    return transformation->modified_dimensions;
}

const char *transformation_mapping_behavior(const Transformation *transformation) {
    return transformation->mapping_behavior;
}

const char *transformation_validation(const Transformation *transformation) {
    return transformation->validation;
}

bool transformation_deterministic(const Transformation *transformation) {
    return transformation->deterministic;
}

const ProvenanceReference *transformation_provenance(const Transformation *transformation) {
    return transformation->provenance;
}

bool transformation_identity_equals(const Transformation *a, const Transformation *b) {
    if (a == NULL || b == NULL) {
        return false;
    }
    return entity_id_equals(a->id, b->id);
}