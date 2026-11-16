#include "support/test_composer.h"

#include <stdlib.h>

#include "support/test_support.h"

PlanResult test_read_plan_file(const char *path, PiecePlan **out, PlanJsonError *error) {
    char *text = test_read_file(path, NULL);
    PlanResult result = plan_from_json(text, out, error);
    free(text);
    return result;
}

MotiveResult test_load_motive_file(const char *path, MotiveFormat format, const MotiveOptions *options, Motive *out,
                                   MotiveError *error) {
    size_t size = 0;
    char *data = test_read_file(path, &size);
    MotiveResult result = motive_load(data, size, format, options, out, error);
    free(data);
    return result;
}
