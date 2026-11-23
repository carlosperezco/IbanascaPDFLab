# Ibanasca PDF Lab — editor ligero de PDF. Parte de DonatellaTech.
# La versión del programa se define SOLO aquí (project ... VERSION).
cmake_minimum_required(VERSION 3.16)

project(ibanascapdf
    VERSION 0.0.5
    DESCRIPTION "Ibanasca PDF Lab"
    HOMEPAGE_URL "https://donatellatech.com"
    LANGUAGES C CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)  # QPDF 12 exige C++20 en sus cabeceras
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Tipo de compilación" FORCE)
endif()

include(GNUInstallDirs)

# Disposición de la compilación igual a la instalada:
#   build/bin/ibanascapdf[.exe]  y  build/share/ibanascapdf/{config,lang,assets}
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(IBZ_DATOS_COMPILACION ${CMAKE_BINARY_DIR}/share/ibanascapdf)

# Opciones comunes: sin advertencias con -Wall -Wextra -Wpedantic.
add_library(ibz_opciones INTERFACE)
target_compile_options(ibz_opciones INTERFACE -Wall -Wextra -Wpedantic)
if(WIN32)
    target_compile_definitions(ibz_opciones INTERFACE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# ------------------------------------------------------------------ dependencias
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0>=3.22)
pkg_check_modules(QPDF REQUIRED IMPORTED_TARGET libqpdf>=11)
find_package(ZLIB REQUIRED) # ya es dependencia de QPDF; se usa para leer PNG
include(${PROJECT_SOURCE_DIR}/cmake/PDFium.cmake)

# ------------------------------------------------------------------ versión
configure_file(app/version.h.in ${CMAKE_BINARY_DIR}/generado/version.h @ONLY)
add_library(ibz_version INTERFACE)
target_include_directories(ibz_version INTERFACE ${CMAKE_BINARY_DIR}/generado)

# ------------------------------------------------------------------ componentes
add_subdirectory(core)
add_subdirectory(gui)
add_subdirectory(app)

option(IBZ_PRUEBAS "Compilar las pruebas del núcleo" ON)
if(IBZ_PRUEBAS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Recursos junto al ejecutable compilado (se actualizan en cada compilación).
add_custom_target(ibz_datos ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${IBZ_DATOS_COMPILACION}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/config ${IBZ_DATOS_COMPILACION}/config
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/lang ${IBZ_DATOS_COMPILACION}/lang
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_SOURCE_DIR}/assets ${IBZ_DATOS_COMPILACION}/assets
    COMMENT "Copiando recursos")

# ------------------------------------------------------------------ instalación (Linux)
install(TARGETS ibanascapdf RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY config lang assets DESTINATION ${CMAKE_INSTALL_DATADIR}/ibanascapdf)
if(UNIX AND NOT APPLE)
    install(FILES ${PDFIUM_RUNTIME} DESTINATION ${CMAKE_INSTALL_LIBDIR}/ibanascapdf)
    install(FILES assets/ibanascapdf.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/ibanascapdf.svg DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
endif()

# ------------------------------------------------------------------ instalador (Windows, NSIS vía CPack)
# empaquetar_windows.sh prepara dist/ (ejecutable, DLL, recursos, licencias) y
# CPack empaqueta esa carpeta tal cual.
set(CPACK_PACKAGE_NAME "IbanascaPDFLab")
set(CPACK_PACKAGE_VENDOR "DonatellaTech")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Ibanasca PDF Lab - editor ligero de PDF")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_INSTALL_DIRECTORY "DonatellaTech\\\\Ibanasca PDF Lab")
set(CPACK_PACKAGE_EXECUTABLES "ibanascapdf;Ibanasca PDF Lab")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
if(WIN32)
    set(CPACK_GENERATOR NSIS)
    set(CPACK_PACKAGE_FILE_NAME "IbanascaPDFLab-${PROJECT_VERSION}-win64")
    set(CPACK_INSTALL_CMAKE_PROJECTS "")
    set(CPACK_INSTALLED_DIRECTORIES "${PROJECT_SOURCE_DIR}/dist;.")
    set(CPACK_NSIS_PACKAGE_NAME "Ibanasca PDF Lab")
    set(CPACK_NSIS_DISPLAY_NAME "Ibanasca PDF Lab ${PROJECT_VERSION}")
    set(CPACK_NSIS_MUI_ICON "${PROJECT_SOURCE_DIR}/assets/ibanascapdf.ico")
    set(CPACK_NSIS_MUI_UNIICON "${PROJECT_SOURCE_DIR}/assets/ibanascapdf.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\ibanascapdf.exe")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY "bin")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://donatellatech.com")
    set(CPACK_NSIS_CONTACT "contacto@donatellatech.com")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    # La página de opciones de CPack es la que ofrece el acceso directo opcional
    # en el escritorio; por defecto NO se añade nada al PATH.
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_CREATE_DESKTOP_LINKS ibanascapdf)
else()
    set(CPACK_GENERATOR TGZ)
endif()
include(CPack)
