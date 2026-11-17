# No source left here: the rules moved to library/theory (B3 deletes this target).
add_library(standard_markings INTERFACE)
target_link_libraries(standard_markings INTERFACE ursatz_theory)
