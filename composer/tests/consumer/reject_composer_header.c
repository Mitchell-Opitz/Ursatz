/* Must not compile: a consumer sees composer_api.h and no other Composer header. */
#include <ursatz_composer/motive.h>

int main(void) { return 0; }
