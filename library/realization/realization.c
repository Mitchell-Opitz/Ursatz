#include "realization/realization.h"

#include <string.h>

bool realization_id_valid(const char *id) {
    if (id == NULL) {
        return false;
    }
    size_t length = strlen(id);
    if (length == 0 || length > REALIZATION_ID_MAX - 1) {
        return false;
    }
    bool letter = (id[0] >= 'A' && id[0] <= 'Z') || (id[0] >= 'a' && id[0] <= 'z');
    if (!letter) {
        return false;
    }
    for (size_t i = 1; i < length; i++) {
        char c = id[i];
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}
