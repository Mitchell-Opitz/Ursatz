ursatz_test(motif_window_matching LIBS ursatz_analysis)

ursatz_test(motive_sketch LIBS ursatz_analysis)

ursatz_test(motive_sketch_value LIBS ursatz_analysis VALGRIND)

ursatz_test(motive_relation LIBS ursatz_analysis)

ursatz_test(motive_relation_params LIBS ursatz_analysis VALGRIND)

ursatz_test(motive_tone_roles LIBS ursatz_analysis VALGRIND)

# 130 s measured (serial, 10 s rule); runs in the nightly.
ursatz_test_executable(test_motive_variant_recognition LIBS ursatz_analysis)

ursatz_test(motive_variant_recognition TARGET test_motive_variant_recognition LABELS slow)

ursatz_test(motive_variant_recognition_chains LIBS ursatz_analysis)

# Every frame relation in a chain of two with every other case, both orders (about 2 minutes); CI runs ctest -LE slow.
ursatz_test(motive_variant_recognition_frame_chains TARGET test_motive_variant_recognition ARGS frame-chains
    LABELS slow)

ursatz_test(motive_link LIBS ursatz_analysis VALGRIND)

ursatz_test(motive_link_groups LIBS ursatz_analysis VALGRIND)
