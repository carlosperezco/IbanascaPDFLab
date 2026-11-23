# Módulos

Cada archivo `.cpp` de esta carpeta es una operación (rotar, fusionar, firmar…).
CMake los recoge solos (`file(GLOB CONFIGURE_DEPENDS ...)`) y cada uno se
registra con `IBZ_REGISTRAR_MODULO(Clase);`. Ver `include/ibanascapdfcore/modulo.h`.

Para cada módulo hay que añadir en `lang/es.lang`, `en.lang` e `it.lang`:

```
modulo.<id>.nombre = _Nombre en el menú…
```

y una clave por cada parámetro que declare.
