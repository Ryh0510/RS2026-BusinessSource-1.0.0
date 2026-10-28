# Qt 5.15 windeployqt can deploy qpdf without its dynamically loaded QtPdf DLL.
# Complete this existing plugin's runtime closure without a public QtPdf link.
if(NOT DEFINED QT_BIN_DIR OR NOT DEFINED OUTPUT_DIR OR NOT DEFINED CONFIG)
    message(FATAL_ERROR "Qt plugin runtime deployment requires QT_BIN_DIR, OUTPUT_DIR and CONFIG")
endif()

set(_qt_suffix "")
if(CONFIG STREQUAL "Debug")
    set(_qt_suffix "d")
endif()

if(EXISTS "${OUTPUT_DIR}/imageformats/qpdf${_qt_suffix}.dll")
    set(_pdf_runtime "${QT_BIN_DIR}/Qt5Pdf${_qt_suffix}.dll")
    if(NOT EXISTS "${_pdf_runtime}")
        message(FATAL_ERROR "Deployed qpdf plugin requires missing runtime: ${_pdf_runtime}")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_pdf_runtime}" "${OUTPUT_DIR}/Qt5Pdf${_qt_suffix}.dll"
        RESULT_VARIABLE _copy_result
    )
    if(NOT _copy_result EQUAL 0)
        message(FATAL_ERROR "Failed to deploy Qt PDF plugin runtime: ${_copy_result}")
    endif()
endif()
