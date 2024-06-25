# Frontend tests are plain Node scripts; register each one when node exists.
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
  file(GLOB URSATZ_GUI_FRONTEND_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.test.js)
  foreach(test_file IN LISTS URSATZ_GUI_FRONTEND_TESTS)
    get_filename_component(test_stem ${test_file} NAME_WE)
    # NAME_WE cuts at the first dot, so "analysis-view.test.js" gives "analysis-view".
    add_test(NAME frontend_${test_stem} COMMAND ${NODE_EXECUTABLE} ${test_file})
  endforeach()
else()
  message(STATUS "node not found: frontend tests are not registered with CTest")
endif()
