# Ibanasca PDF Lab

*[Leer en español](README.es.md)*

A lightweight, free and open-source PDF editor for Windows and Linux, written in
C++20 with GTK 3, PDFium and QPDF. Part of [DonatellaTech](https://donatellatech.com).

## Features

- **View:** continuous scrolling, zoom, fit width and fit page, thumbnails, go to page,
  password-protected PDFs, drag and drop.
- **Pages:** rotate, move, delete, duplicate, insert blank pages and reorder them by
  dragging the thumbnails.
- **File:** new PDF with a blank page of the chosen size (A4, Letter, Legal, A5, A3).
- **Document:** merge PDFs; split by page or by ranges, or extract pages to another PDF.
- **Write:** add text (Helvetica, Times or Courier; size and colour) and insert PNG images.
- **Sign:** digital signature PAdES-B-B with a `.p12`/`.pfx` certificate (visible box
  with name and date, PNG image of the handwritten signature, or invisible) and visual
  signature with a PNG image.
- Undo and redo; the original file is never overwritten ("Save as").
- Interface in English, Spanish and Italian.

## Download

The Windows installer (`IbanascaPDFLab-<version>-win64.exe`) is attached to each
release of the repository, together with the source code of the bundled libraries.
On Linux, build it from source as explained below.

## Build

PDFium is not packaged by the distributions or MSYS2; the script
`tools/obtener_pdfium.sh` downloads the prebuilt binaries from
[bblanchon/pdfium-binaries](https://github.com/bblanchon/pdfium-binaries) into
`third_party/pdfium` (set `PDFIUM_VERSION=<number>` to pin a version).

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev libqpdf-dev zlib1g-dev libgnutls28-dev curl
./tools/obtener_pdfium.sh
cmake -S . -B build && cmake --build build -j
./build/bin/ibanascapdf
```

Fedora: `sudo dnf install gcc-c++ cmake pkgconf gtk3-devel qpdf-devel zlib-devel gnutls-devel curl`.

### Windows (MSYS2, UCRT64 terminal)

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,gtk3,qpdf,gnutls,nsis} curl tar
./tools/obtener_pdfium.sh
cmake -S . -B build -G Ninja && cmake --build build
./build/bin/ibanascapdf.exe
```

Package (`dist/`) and installer (`instalador/IbanascaPDFLab-<version>-win64.exe`):

```bash
./empaquetar_windows.sh --instalador
```

## Usage

```
ibanascapdf [options] [file.pdf]
  -h, --help      Show the help
  -v, --version   Show the version
  --list          List the available modules
```

The original file is never overwritten: changes are always saved to a new file.
User settings are stored in `%APPDATA%\DonatellaTech\IbanascaPDFLab\` (Windows)
or `~/.config/donatellatech/ibanascapdf/` (Linux).

## Architecture

```
app/        entry point, version, Windows resources
core/       libibanascapdfcore — PDF logic, no GUI (usable from the command line)
  include/ibanascapdfcore/   public API
  src/modules/               one file per operation; they register themselves
gui/        libibanascapdfgui — GTK 3 interface, no PDF logic
config/     app.conf (title, icon, logo, welcome message, default language)
lang/       es.lang, en.lang, it.lang (key = value, same keys in all three)
assets/     icon (SVG, PNG, ICO) and logo
tools/      helper scripts
```

The document lives in memory; each operation produces a new PDF that PDFium
reloads from memory (no temporary files), which also gives a simple undo stack.

### Adding a module

Create `core/src/modules/<name>.cpp` with a class derived from `ibz::Modulo` and
end it with `IBZ_REGISTRAR_MODULO(Class);`. CMake picks the file up and the menu
entry (`modulo.<id>.nombre` in the language files) and parameter dialog are built
automatically. See `core/include/ibanascapdfcore/modulo.h`.

## Versions

Semantic versioning; the version is defined only in `project(... VERSION x.y.z)`
in `CMakeLists.txt`. See [CHANGELOG.md](CHANGELOG.md).

## About the name

The name honours **Ibanasca**, mother of water and protective goddess of the
snow-capped mountains in the Pijao culture of Tolima, Colombia. According to
tradition, she was burned during the conquest and purified by the god of fire.
The project does not claim to represent the deity or to speak for the Pijao people.

## Donations

Ibanasca PDF Lab is free and will stay free. If you like it and want to say thanks,
you can make a voluntary donation at
[buymeacoffee.com/CarlosNPerezM](https://buymeacoffee.com/CarlosNPerezM).

A donation is only a thank-you for the software: it does not buy support, services,
priority, features or a subscription of any kind.

## License

MIT — Copyright (c) 2026 Carlos N. Perez-Montenegro (DonatellaTech).
Third-party libraries and their licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Credits

Author: Carlos N. Perez-Montenegro — DonatellaTech — contacto@donatellatech.com
