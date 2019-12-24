add_library(ursatz_analyzer_pipeline
    pipeline/midi_pipeline.c
    pipeline/segmentation.c
    pipeline/pitch_class_util.c
    pipeline/key_pipeline.c
    pipeline/chord_pipeline.c
    pipeline/analysis_report.c
    pipeline/analysis_pipeline.c
    pipeline/markdown_report.c
    pipeline/report_writer.c
)
target_include_directories(ursatz_analyzer_pipeline PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ursatz_analyzer_pipeline PUBLIC ursatz common_practice_minimal)

add_executable(ursatz-analyzer main.c)
target_link_libraries(ursatz-analyzer PRIVATE ursatz_analyzer_pipeline)
