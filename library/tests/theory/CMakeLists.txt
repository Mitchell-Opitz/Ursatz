ursatz_test(framework LIBS ursatz)
ursatz_test(style_model LIBS ursatz)
ursatz_test(style_model_build LIBS ursatz)
ursatz_test(style_model_blend LIBS ursatz)

# Reads src/theory/ to show StyleModel has no path to a Rule, so it runs from this directory.
ursatz_test(style_model_not_a_rule LIBS ursatz WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
