# Frontend tests are plain Node scripts; register each one when node exists (UrsatzTools.cmake looks for it).
if(URSATZ_NODE)
  file(GLOB URSATZ_GUI_FRONTEND_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.test.js)
  foreach(test_file IN LISTS URSATZ_GUI_FRONTEND_TESTS)
    get_filename_component(test_stem ${test_file} NAME_WE)
    # NAME_WE cuts at the first dot, so "analysis-view.test.js" gives "analysis-view".
    ursatz_test(frontend_${test_stem} COMMAND ${URSATZ_NODE} ${test_file})
  endforeach()
endif()
