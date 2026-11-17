ursatz_test(style_model LIBS ursatz_theory)

ursatz_test(style_model_build LIBS ursatz_theory)

ursatz_test(style_model_blend LIBS ursatz_theory)

# Reads theory/style/ to show StyleModel has no path to a Rule, so it runs from this directory.
ursatz_test(style_model_not_a_rule LIBS ursatz_theory ursatz_test_support WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
