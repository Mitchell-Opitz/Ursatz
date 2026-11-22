/* Two global functions for check_exports_self_test.cmake: the export check must accept a file that lists both, refuse one that
   leaves one out and refuse one that lists a name the library does not define. */
int exports_self_test_kept(void) {
    return 1;
}

int exports_self_test_extra(void) {
    return 2;
}
