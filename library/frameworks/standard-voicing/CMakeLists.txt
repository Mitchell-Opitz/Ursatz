# No source left here: the rules moved to library/theory (B3 deletes this target).
add_library(standard_voicing INTERFACE)
target_link_libraries(standard_voicing INTERFACE ursatz_theory)
