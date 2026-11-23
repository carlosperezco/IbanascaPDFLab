# Núcleo sin interfaz. Biblioteca de objetos: así los módulos que se registran
# solos (variables estáticas) no los descarta el enlazador.
file(GLOB IBZ_MODULOS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/*.cpp)

add_library(ibanascapdfcore OBJECT
    src/claves.cpp
    src/documento.cpp
    src/idioma.cpp
    src/modulo.cpp
    src/operaciones.cpp
    src/rangos.cpp
    src/rutas.cpp
    src/texto.cpp
    ${IBZ_MODULOS})

target_include_directories(ibanascapdfcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(ibanascapdfcore
    PUBLIC PDFium::PDFium PkgConfig::QPDF
    PRIVATE ibz_opciones)
if(WIN32)
    target_link_libraries(ibanascapdfcore PUBLIC shell32)
endif()
