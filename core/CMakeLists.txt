# Núcleo sin interfaz, compartido por el escritorio (GTK) y Android.
#
# Biblioteca de objetos: así los módulos que se registran solos (variables
# estáticas) no los descarta el enlazador.
#
# Quien lo incluye con add_subdirectory() debe definir antes estos destinos:
#   ibz_pdfium, ibz_qpdf, ibz_zlib   y, si IBZ_FIRMA_GNUTLS=ON, ibz_gnutls
# (el escritorio los define en el CMakeLists.txt principal; Android, en el suyo).
cmake_minimum_required(VERSION 3.16)

option(IBZ_FIRMA_GNUTLS "Firma con certificados .p12/.pfx mediante GnuTLS (escritorio)" ON)

if(NOT TARGET ibz_opciones)
    add_library(ibz_opciones INTERFACE)
    target_compile_options(ibz_opciones INTERFACE -Wall -Wextra -Wpedantic)
endif()

file(GLOB IBZ_MODULOS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/modules/*.cpp)

add_library(ibanascapdfcore OBJECT
    src/claves.cpp
    src/der.cpp
    src/dibujo.cpp
    src/documento.cpp
    src/firma.cpp
    src/firma_cms.cpp
    src/idioma.cpp
    src/imagen.cpp
    src/modulo.cpp
    src/operaciones.cpp
    src/rangos.cpp
    src/rutas.cpp
    src/sha256.cpp
    src/texto.cpp
    ${IBZ_MODULOS})

target_include_directories(ibanascapdfcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ibanascapdfcore PUBLIC cxx_std_20)
target_link_libraries(ibanascapdfcore
    PUBLIC ibz_pdfium ibz_qpdf ibz_zlib
    PRIVATE ibz_opciones)

if(IBZ_FIRMA_GNUTLS)
    target_sources(ibanascapdfcore PRIVATE src/firmante_gnutls.cpp)
    target_link_libraries(ibanascapdfcore PUBLIC ibz_gnutls)
endif()

if(WIN32)
    target_link_libraries(ibanascapdfcore PUBLIC shell32)
endif()
