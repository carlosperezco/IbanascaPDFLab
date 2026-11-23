# Ibanasca PDF Lab

*[Read in English](README.md)*

Editor ligero de PDF, gratuito y de código abierto, para Windows y Linux, escrito
en C++20 con GTK 3, PDFium y QPDF. Forma parte de [DonatellaTech](https://donatellatech.com).

> **Estado: en desarrollo (0.0.5).** Ya están el visor, las operaciones de páginas, fusionar, separar, texto y firma visual;
> el resto de funciones de edición llegan en las siguientes 0.0.x.

## Funciones previstas para la 1.0.0

- Firmar: firma criptográfica (PAdES-B-B con certificado `.p12`/`.pfx`) y firma
  visual / manuscrita (PNG)
- Fusionar PDF
- Separar por página o por rangos, y extraer páginas o rangos a otro PDF
- Escribir texto en un PDF
- Rotar, reordenar y eliminar páginas

Disponible en la 0.0.5: firma visual (PNG); añadir texto (fuentes estándar, tamaño, color); fusionar PDF; separar por página o por rangos, o extraer
páginas a otro PDF; visor continuo (ajustar al ancho, a la página, zoom) con
miniaturas; rotar, mover, eliminar y reordenar páginas (también arrastrando las
miniaturas), con deshacer; PDF protegidos, arrastrar y soltar, «Guardar como»,
interfaz en español, inglés e italiano.

## Compilar

PDFium no está empaquetado en las distribuciones ni en MSYS2; el script
`tools/obtener_pdfium.sh` descarga los binarios precompilados de
[bblanchon/pdfium-binaries](https://github.com/bblanchon/pdfium-binaries) en
`third_party/pdfium` (con `PDFIUM_VERSION=<número>` se fija una versión).

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libqpdf-dev zlib1g-dev curl
./tools/obtener_pdfium.sh
cmake -S . -B build && cmake --build build -j
./build/bin/ibanascapdf
```

Fedora: `sudo dnf install gcc-c++ cmake pkgconf gtk3-devel qpdf-devel zlib-devel curl`.

### Windows (MSYS2, terminal UCRT64)

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,gtk3,qpdf,nsis} curl tar
./tools/obtener_pdfium.sh
cmake -S . -B build -G Ninja && cmake --build build
./build/bin/ibanascapdf.exe
```

Paquete (`dist/`) e instalador (`instalador/IbanascaPDFLab-<versión>-win64.exe`):

```bash
./empaquetar_windows.sh --instalador
```

## Uso

```
ibanascapdf [opciones] [archivo.pdf]
  -h, --help      Muestra la ayuda
  -v, --version   Muestra la versión
  --list          Lista los módulos disponibles
```

El archivo original nunca se sobrescribe: los cambios se guardan siempre en un
archivo nuevo. Los ajustes del usuario se guardan en
`%APPDATA%\DonatellaTech\IbanascaPDFLab\` (Windows) o
`~/.config/donatellatech/ibanascapdf/` (Linux).

## Arquitectura

```
app/        punto de entrada, versión, recursos de Windows
core/       libibanascapdfcore — lógica de PDF, sin interfaz (usable desde la línea de órdenes)
  include/ibanascapdfcore/   API pública
  src/modules/               un archivo por operación; se registran solos
gui/        libibanascapdfgui — interfaz GTK 3, sin lógica de PDF
config/     app.conf (título, icono, logo, mensaje inicial, idioma por defecto)
lang/       es.lang, en.lang, it.lang (clave = valor, mismas claves en los tres)
assets/     icono (SVG, PNG, ICO) y logo
tools/      scripts auxiliares
```

El documento vive en memoria; cada operación produce un PDF nuevo que PDFium
vuelve a cargar desde memoria (sin archivos temporales), lo que además da una
pila de deshacer sencilla.

### Añadir un módulo

Cree `core/src/modules/<nombre>.cpp` con una clase derivada de `ibz::Modulo` y
termínelo con `IBZ_REGISTRAR_MODULO(Clase);`. CMake recoge el archivo, y la entrada
de menú (`modulo.<id>.nombre` en los archivos de idioma) y el diálogo de parámetros
se crean solos. Ver `core/include/ibanascapdfcore/modulo.h`.

## Versiones

Versionado semántico; la versión se define solo en `project(... VERSION x.y.z)`
de `CMakeLists.txt`. Ver [CHANGELOG.md](CHANGELOG.md).

## Licencia

MIT — Copyright (c) 2026 Carlos N. Perez-Montenegro (DonatellaTech).
Librerías de terceros y sus licencias: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Créditos

Autor: Carlos N. Perez-Montenegro — DonatellaTech — contacto@donatellatech.com
