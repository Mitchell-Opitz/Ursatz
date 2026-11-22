#include <assert.h>
#include <stdio.h>

#include "chain/chain_result.h"
#include "ursatz_test_knowledge.h"

/*
 * The key helpers the chain's result shares (chain_result.h): the tonic of a key signature's sharps and flats, and whether a key is
 * signed in flats (a minor key judged by its relative major).
 */

static void test_key_signature_major_tonic(void) {
    assert(chain_key_signature_major_tonic(0) == 0);   /* C */
    assert(chain_key_signature_major_tonic(1) == 7);   /* G */
    assert(chain_key_signature_major_tonic(-1) == 5);  /* F */
    assert(chain_key_signature_major_tonic(2) == 2);   /* D */
    assert(chain_key_signature_major_tonic(7) == 1);   /* C-sharp */
    assert(chain_key_signature_major_tonic(-7) == 11); /* C-flat */
    assert(chain_key_signature_major_tonic(8) == -1);
    assert(chain_key_signature_major_tonic(-8) == -1);
}

static void test_tonic_prefers_flats(void) {
    const CommonPracticeMaterials *materials = test_materials();
    assert(chain_tonic_prefers_flats(3, materials->major));  /* E-flat major */
    assert(chain_tonic_prefers_flats(5, materials->major));  /* F major */
    assert(!chain_tonic_prefers_flats(0, materials->major)); /* C major */
    assert(!chain_tonic_prefers_flats(7, materials->major)); /* G major */
    assert(!chain_tonic_prefers_flats(6, materials->major)); /* F-sharp major: a tie with G-flat */
    assert(chain_tonic_prefers_flats(5, materials->natural_minor)); /* F minor, four flats */
    assert(!chain_tonic_prefers_flats(9, materials->natural_minor)); /* A minor */
    assert(!chain_tonic_prefers_flats(0, NULL));
}

int main(void) {
    test_key_signature_major_tonic();
    test_tonic_prefers_flats();
    printf("test_result_spelling: ok\n");
    return 0;
}
