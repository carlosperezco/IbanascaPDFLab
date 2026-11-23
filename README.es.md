# Ibanasca PDF Lab

*[Read in English](README.md)*

Editor ligero de PDF, gratuito y de código abierto, para Windows y Linux, escrito
en C++20 con GTK 3, PDFium y QPDF. Forma parte de [DonatellaTech](https://donatellatech.com).

## Funciones

- **Ver:** desplazamiento continuo, zoom, ajustar al ancho y a la página, miniaturas,
  ir a una página, PDF protegidos con contraseña, arrastrar y soltar.
- **Páginas:** rotar, mover, eliminar, duplicar, insertar páginas en blanco y
  reordenarlas arrastrando las miniaturas.
- **Archivo:** nuevo PDF con una página en blanco del tamaño elegido (A4, Carta, Oficio, A5, A3).
- **Documento:** fusionar PDF; separar por página o por rangos, o extraer páginas a otro PDF.
- **Escribir:** añadir texto (Helvetica, Times o Courier; tamaño y color) e insertar imágenes PNG.
- **Firmar:** firma digital PAdES-B-B con certificado `.p12`/`.pfx` (recuadro visible con
  nombre y fecha, imagen PNG de la firma manuscrita o invisible) y firma visual con una
  imagen PNG.
- Deshacer y rehacer; el archivo original nunca se sobrescribe («Guardar como»).
- Interfaz en español, inglés e italiano.

## Descarga

El instalador para Windows (`IbanascaPDFLab-<versión>-win64.exe`) se publica en cada
versión (Release) del repositorio, junto con el código fuente de las bibliotecas
incluidas. En Linux se compila desde el código fuente como se explica a continuación.

## Compilar

PDFium no está empaquetado en las distribuciones ni en MSYS2; el script
`tools/obtener_pdfium.sh` descarga los binarios precompilados de
[bblanchon/pdfium-binaries](https://github.com/bblanchon/pdfium-binaries) en
`third_party/pdfium` (con `PDFIUM_VERSION=<número>` se fija una versión).

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libqpdf-dev zlib1g-dev libgnutls28-dev curl
./tools/obtener_pdfium.sh
cmake -S . -B build && cmake --build build -j
./build/bin/ibanascapdf
```

Fedora: `sudo dnf install gcc-c++ cmake pkgconf gtk3-devel qpdf-devel zlib-devel gnutls-devel curl`.

### Windows (MSYS2, terminal UCRT64)

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,gtk3,qpdf,gnutls,nsis} curl tar
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

## Sobre el nombre

El nombre rinde homenaje a **Ibanasca**, madre del agua y diosa protectora de los
nevados en la cultura pijao del Tolima, Colombia. Según la tradición, fue quemada
durante la conquista y purificada por el dios del fuego. El proyecto no pretende
representar a la deidad ni hablar en nombre del pueblo pijao.

## Donaciones

Ibanasca PDF Lab es gratuito y seguirá siéndolo. Si le gusta y quiere agradecerlo,
puede hacer una donación voluntaria en
[buymeacoffee.com/CarlosNPerezM](https://buymeacoffee.com/CarlosNPerezM).

Una donación es solo un agradecimiento por el programa: no da derecho a soporte,
servicios, prioridad, funciones ni suscripción de ningún tipo.

## Licencia

MIT — Copyright (c) 2026 Carlos N. Perez-Montenegro (DonatellaTech).
Librerías de terceros y sus licencias: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Créditos

Autor: Carlos N. Perez-Montenegro — DonatellaTech — contacto@donatellatech.com
