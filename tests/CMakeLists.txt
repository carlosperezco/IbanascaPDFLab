add_executable(prueba_core prueba_core.cpp)
target_link_libraries(prueba_core PRIVATE ibanascapdfcore ibz_opciones)
set_target_properties(prueba_core PROPERTIES BUILD_RPATH "$ORIGIN")
add_test(NAME prueba_core COMMAND prueba_core)
