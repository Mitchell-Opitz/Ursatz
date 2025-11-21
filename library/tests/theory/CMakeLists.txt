add_ursatz_test(framework)
add_ursatz_test(heuristic)
add_ursatz_test(tendency)
add_ursatz_test(statistical_model)
add_ursatz_test(style_model)
add_ursatz_architecture_test(theory)
add_ursatz_test(style_model_build)
add_ursatz_test(style_model_blend)

# Reads src/theory/ to show StyleModel and Tendency have no path to a Rule, so it runs from this directory
# like the architecture test.
add_executable(test_style_model_not_a_rule test_style_model_not_a_rule.c)
target_link_libraries(test_style_model_not_a_rule PRIVATE ursatz)
add_test(NAME style_model_not_a_rule COMMAND test_style_model_not_a_rule
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
