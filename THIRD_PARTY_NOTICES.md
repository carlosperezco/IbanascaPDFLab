# Third-party notices

Ibanasca PDF Lab is distributed under the MIT license (see `LICENSE`). It uses
the following libraries, linked dynamically and without modification.

| Library | Used for | License | Source code |
|---|---|---|---|
| GTK 3 (with GLib, GIO, Pango, Cairo, GdkPixbuf, HarfBuzz…) | User interface, drawing, loading images | LGPL-2.1-or-later (GTK, GLib, Pango, GdkPixbuf); LGPL-2.1 or MPL-1.1 (Cairo); MIT (HarfBuzz) | https://gitlab.gnome.org/GNOME/gtk |
| QPDF | Rotating, reordering, deleting, merging and splitting pages; preparing the digital signature | Apache-2.0 (depends on zlib, libjpeg-turbo and a crypto library: OpenSSL or GnuTLS, depending on the build) | https://github.com/qpdf/qpdf |
| zlib | Reading PNG images for the visual signature (already a QPDF dependency) | Zlib | https://zlib.net |
| PDFium (prebuilt by bblanchon/pdfium-binaries) | Rendering and navigating PDF pages | BSD-3-Clause and Apache-2.0; bundled components (FreeType, libjpeg-turbo, OpenJPEG, Little CMS, zlib, ICU…) under their own licenses | https://pdfium.googlesource.com/pdfium/ · https://github.com/bblanchon/pdfium-binaries |

The Windows installer also contains the runtime DLLs these libraries depend on
(MSYS2 UCRT64 packages). `empaquetar_windows.sh` copies the license files of
every included package to `licenses/` and writes `licenses/LISTA.txt`
(package, version, license), which is reviewed for each release.

The icon (`assets/ibanascapdf.svg` and derived PNG/ICO files) was designed for
this project and is distributed under the same MIT license.
