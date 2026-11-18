ursatz_test(style_model LIBS ursatz_theory)

ursatz_test(style_model_build LIBS ursatz_theory)

ursatz_test(style_model_blend LIBS ursatz_theory)

# Reads theory/style/ to show StyleModel has no path to a Rule, so it runs from this directory.
ursatz_test(style_model_not_a_rule LIBS ursatz_theory ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

ursatz_test(theory_style_labels SOURCES test_style_labels.c LIBS ursatz_theory ursatz_test_support)

ursatz_test(theory_style_profile SOURCES test_style_profile.c LIBS ursatz_theory ursatz_test_support)

ursatz_test(theory_style_compare SOURCES test_style_compare.c LIBS ursatz_theory ursatz_test_support)

ursatz_test(theory_style_model_file SOURCES test_style_model_file.c LIBS ursatz_theory ursatz_test_support
    DEFINITIONS URSATZ_STYLE_MODEL_FILE_WORK_DIR="${CMAKE_CURRENT_BINARY_DIR}/style_model_file_work"
    PROPERTIES RESOURCE_LOCK style_model_file_work)
