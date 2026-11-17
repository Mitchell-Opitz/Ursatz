
ursatz_test(sonority LIBS ursatz_analysis)

ursatz_test(sonority_segmentation LIBS ursatz_analysis)

ursatz_test(rhythmic_pattern LIBS ursatz_analysis)

ursatz_test(contour LIBS ursatz_analysis)

ursatz_test(tone_line LIBS ursatz_analysis VALGRIND)

ursatz_test(analysis_note LIBS ursatz_analysis VALGRIND)

ursatz_test(analysis_key LIBS ursatz_analysis VALGRIND)
