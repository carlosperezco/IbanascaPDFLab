set(IBZ_FUENTES main.cpp)
if(WIN32)
    set(IBZ_ICO "${PROJECT_SOURCE_DIR}/assets/ibanascapdf.ico")
    configure_file(ibanascapdf.rc.in ${CMAKE_BINARY_DIR}/generado/ibanascapdf.rc @ONLY)
    list(APPEND IBZ_FUENTES ${CMAKE_BINARY_DIR}/generado/ibanascapdf.rc)
endif()

# WIN32: aplicación gráfica en Windows (sin consola); se ignora en Linux.
add_executable(ibanascapdf WIN32 ${IBZ_FUENTES})
target_link_libraries(ibanascapdf PRIVATE ibanascapdfgui ibanascapdfcore ibz_version ibz_opciones)
set_target_properties(ibanascapdf PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}/ibanascapdf")
add_dependencies(ibanascapdf ibz_datos)

# PDFium junto al ejecutable compilado.
add_custom_command(TARGET ibanascapdf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PDFIUM_RUNTIME}" "$<TARGET_FILE_DIR:ibanascapdf>"
    COMMENT "Copiando PDFium junto al ejecutable")
