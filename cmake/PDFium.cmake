# Localiza PDFium precompilado (https://github.com/bblanchon/pdfium-binaries).
# Por defecto en third_party/pdfium (lo descarga tools/obtener_pdfium.sh).
#
# Define:
#   PDFium::PDFium   biblioteca importada
#   PDFIUM_RUNTIME   archivo que hay que copiar junto al ejecutable (.dll / .so)

set(PDFIUM_ROOT "${PROJECT_SOURCE_DIR}/third_party/pdfium" CACHE PATH "Carpeta de PDFium precompilado")

find_path(PDFIUM_INCLUDE_DIR fpdfview.h PATHS "${PDFIUM_ROOT}/include" NO_DEFAULT_PATH)

if(WIN32)
    find_file(PDFIUM_RUNTIME pdfium.dll PATHS "${PDFIUM_ROOT}/bin" "${PDFIUM_ROOT}/lib" NO_DEFAULT_PATH)
    # MinGW enlaza directamente contra la DLL (la API de PDFium es C); si existe
    # una biblioteca de importación se usa esa.
    find_file(PDFIUM_IMPLIB NAMES libpdfium.dll.a pdfium.dll.lib PATHS "${PDFIUM_ROOT}/lib" NO_DEFAULT_PATH)
    if(NOT PDFIUM_IMPLIB)
        set(PDFIUM_IMPLIB "${PDFIUM_RUNTIME}")
    endif()
else()
    find_library(PDFIUM_RUNTIME NAMES pdfium PATHS "${PDFIUM_ROOT}/lib" NO_DEFAULT_PATH)
endif()

if(NOT PDFIUM_INCLUDE_DIR OR NOT PDFIUM_RUNTIME)
    message(FATAL_ERROR
        "No se encontró PDFium en ${PDFIUM_ROOT}.\n"
        "Ejecute: ./tools/obtener_pdfium.sh   (o indique -DPDFIUM_ROOT=<carpeta>)")
endif()

add_library(PDFium::PDFium SHARED IMPORTED)
set_target_properties(PDFium::PDFium PROPERTIES
    IMPORTED_LOCATION "${PDFIUM_RUNTIME}"
    INTERFACE_INCLUDE_DIRECTORIES "${PDFIUM_INCLUDE_DIR}")
if(WIN32)
    set_target_properties(PDFium::PDFium PROPERTIES IMPORTED_IMPLIB "${PDFIUM_IMPLIB}")
endif()

if(EXISTS "${PDFIUM_ROOT}/VERSION")
    file(STRINGS "${PDFIUM_ROOT}/VERSION" _pdfium_version LIMIT_COUNT 4)
    message(STATUS "PDFium: ${PDFIUM_ROOT} (${_pdfium_version})")
else()
    message(STATUS "PDFium: ${PDFIUM_ROOT}")
endif()
