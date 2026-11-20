#include <assert.h>
#include <stdio.h>

#include <common/status.h>

/* ursatz_status_name and its inverse ursatz_status_parse: every word parses back to its status, anything else does not. */
int main(void) {
    for (int status = URSATZ_OK; status <= URSATZ_ERR_INTERNAL; status++) {
        UrsatzStatus parsed = URSATZ_ERR_INTERNAL;
        const char *name = ursatz_status_name((UrsatzStatus)status);
        assert(ursatz_status_parse(name, &parsed) && (int)parsed == status);
    }
    UrsatzStatus parsed = URSATZ_ERR_CONFLICT;
    assert(!ursatz_status_parse("bogus", &parsed) && parsed == URSATZ_ERR_CONFLICT);
    assert(!ursatz_status_parse("", &parsed) && !ursatz_status_parse("OK", &parsed) && !ursatz_status_parse(NULL, &parsed));
    assert(ursatz_status_parse("not-found", NULL));
    puts("ok");
    return 0;
}
