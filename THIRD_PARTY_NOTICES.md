# Third-party notices

Ibanasca PDF Lab is distributed under the MIT license (see `LICENSE`). It uses
the following libraries, linked dynamically and without modification.

| Library | Used for | License | Source code |
|---|---|---|---|
| GTK 3 (with GLib, GIO, Pango, Cairo, GdkPixbuf, HarfBuzz…) | User interface, drawing, loading images | LGPL-2.1-or-later (GTK, GLib, Pango, GdkPixbuf); LGPL-2.1 or MPL-1.1 (Cairo); MIT (HarfBuzz) | https://gitlab.gnome.org/GNOME/gtk |
| QPDF | Rotating, reordering, deleting, merging and splitting pages; preparing the digital signature | Apache-2.0 (depends on zlib, libjpeg-turbo and a crypto library: OpenSSL or GnuTLS, depending on the build) | https://github.com/qpdf/qpdf |
| GnuTLS (with Nettle, GMP, libtasn1, p11-kit…) | Digital signature: reading .p12/.pfx certificates and creating the CMS signature (already a QPDF dependency) | LGPL-2.1-or-later (GnuTLS); LGPL-3.0-or-later or GPL-2.0-or-later (Nettle, GMP); others in `licenses/` | https://gnutls.org |
| zlib | Reading PNG images for the visual signature (already a QPDF dependency) | Zlib | https://zlib.net |
| PDFium (prebuilt by bblanchon/pdfium-binaries) | Rendering and navigating PDF pages | BSD-3-Clause and Apache-2.0; bundled components (FreeType, libjpeg-turbo, OpenJPEG, Little CMS, zlib, ICU…) under their own licenses | https://pdfium.googlesource.com/pdfium/ · https://github.com/bblanchon/pdfium-binaries |

The Windows installer also contains the runtime DLLs these libraries depend on
(MSYS2 UCRT64 packages). `empaquetar_windows.sh` copies the license files of
every included package to `licenses/` and writes `licenses/LISTA.txt`
(package, version, license), which is reviewed for each release.

## Source code of the bundled libraries

Ibanasca PDF Lab's own code is published under the MIT license. The libraries it
uses keep their own licenses. For the libraries under the GPL, LGPL or MPL, their
complete source code is offered to everyone who receives the installer:

- `licenses/FUENTES.txt` (in the installed program) lists, for every bundled MSYS2
  package, the address of its exact source code at `https://mirror.msys2.org/mingw/sources/`.
- Each release on GitHub also attaches those source archives next to the installer,
  so they remain available even if the mirror changes.
- PDFium: https://pdfium.googlesource.com/pdfium/ (version in `licenses/LISTA.txt`).

**GPL component.** GTK reads images through GdkPixbuf, which in MSYS2 includes TIFF
support through libtiff; libtiff in turn uses **jbigkit (GPL-2.0)**. Ibanasca PDF Lab
does not use TIFF images, but that library is part of the GTK runtime included in
the installer, so the installer as a whole is distributed respecting the GPL-2.0:
its source code is published together with each release, and Ibanasca PDF Lab's own
source code (MIT, compatible with the GPL) is public.

The libraries are linked dynamically (separate DLL files) and are not modified, so
any of them can be replaced by the user with another compatible version.

 and derived PNG/ICO files) was designed for
this project and is distributed under the same MIT license.
