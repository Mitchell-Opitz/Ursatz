ursatz_test_includes()

# The JSON files the tree form is tried on: the repository's knowledge, test fixtures, schemas and forms (relative to the source root,
# without the build tree), as one absolute path a line in text_tree_files.txt (written at configure).
set(_text_tree_patterns
    "^knowledge/.*\\.json$"
    "^[^/]+/tests/.*\\.json$"
    "^tests/.*\\.json$"
    "^[^/]+/schema/.*\\.json$"
    "^library/[^/]+/schema/.*\\.json$"
    "^composer/forms/[^/]+\\.json$"
    "^gui/backend/tests/.*\\.json$")
set(_text_tree_files "")
foreach(_dir knowledge tests analyzer composer gui library workshop cli common)
    file(GLOB_RECURSE _found RELATIVE "${CMAKE_SOURCE_DIR}" CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/${_dir}/*.json")
    foreach(_file IN LISTS _found)
        foreach(_pattern IN LISTS _text_tree_patterns)
            if(_file MATCHES "${_pattern}" AND NOT _file MATCHES "node_modules")
                list(APPEND _text_tree_files "${CMAKE_SOURCE_DIR}/${_file}")
                break()
            endif()
        endforeach()
    endforeach()
endforeach()
list(REMOVE_DUPLICATES _text_tree_files)
list(SORT _text_tree_files)
list(JOIN _text_tree_files "\n" _text_tree_list)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/text_tree_files.txt" "${_text_tree_list}\n")

# The tree form: every JSON object of the list prints and parses back equal, with the member order kept; the hand cases and every error.
ursatz_test(text_tree LIBS ursatz_text ursatz_test_support VALGRIND
    DEFINITIONS TEXT_TREE_FILES="${CMAKE_CURRENT_BINARY_DIR}/text_tree_files.txt")
