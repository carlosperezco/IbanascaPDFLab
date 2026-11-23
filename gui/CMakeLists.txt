# Interfaz GTK 3 (sin lógica de PDF).
add_library(ibanascapdfgui OBJECT
    src/dialogos.cpp
    src/miniaturas.cpp
    src/util.cpp
    src/ventana.cpp
    src/visor.cpp)

target_include_directories(ibanascapdfgui
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(ibanascapdfgui
    PUBLIC ibanascapdfcore PkgConfig::GTK3
    PRIVATE ibz_opciones ibz_version)
