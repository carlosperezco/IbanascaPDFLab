# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and the project follows [Semantic Versioning](https://semver.org/).

## [1.1.1] - 2026-11-25

### Fixed
- The core converts decimal numbers itself, with the same result on every platform and
  system language. The Android C++ library lacks `std::from_chars` for `double`, so the
  core did not compile there.

## [1.1.0] - 2026-11-25

### Added
- Signature drawn by hand with the mouse (or the finger on Android): in Sign › Visual
  Signature and as a new appearance of the digital signature. It is written to the
  PDF as smoothed vector paths (no image, no extra library).
- Dialog fields that only apply to one option are shown only when that option is chosen.
- ECDSA certificates for the digital signature (in addition to RSA).

### Changed
- The core builds the CMS signature itself (SHA-256, PAdES-B-B attributes); GnuTLS is
  now only used on the desktop to read `.p12`/`.pfx` files and sign with the key.
- `core/` can be reused by other front ends: it expects the targets `ibz_pdfium`,
  `ibz_qpdf`, `ibz_zlib` (and `ibz_gnutls`), and the data and settings folders can be
  set by the platform. This is what the Android version uses.
- Clearer messages when a password-protected PDF cannot be merged or signed.

## [1.0.0] - 2026-11-23

First public release.

### Added
- PDF viewer (PDFium): continuous scrolling, zoom, fit width / fit page, thumbnails,
  go to page, password-protected PDFs, drag and drop.
- Pages (QPDF): rotate, move, delete, duplicate, insert blank pages, reorder by
  dragging thumbnails.
- File › New: new PDF with a blank page (A4, Letter, Legal, A5 or A3; portrait or landscape).
- Document: merge PDFs; split by page or by ranges, or extract pages to another PDF.
- Add text with the standard PDF fonts; insert PNG images.
- Digital signature PAdES-B-B with a `.p12`/`.pfx` certificate (GnuTLS), visible
  (name and date, or PNG image) or invisible; visual signature with a PNG image.
- Undo / redo; the original file is never overwritten.
- Interface in English, Spanish and Italian; command line `--version`, `--help`, `--list`.
- Windows installer (NSIS) with the licenses and the source code of the bundled
  libraries.
- Green icon with a turtle in the background (DonatellaTech colours).
- Own About dialog: all its texts follow the language chosen in the Language menu,
  also when it is changed while the program is open.

### Known limitations
- One digital signature per document; no timestamp.
- Standard PDF fonts only (Western European characters); existing text cannot be edited.

## [0.0.7] - 2026-11-23

### Added
- Pages › Insert Blank Page: before or after the current page or at the end; same
  size as the current page, A4 or Letter.
- Pages › Duplicate Pages (Ctrl+D): each copy goes right after its original; copies
  share the content (the file barely grows); annotations are not copied.
- About dialog and README: origin of the name Ibanasca.

### Changed
- Language is now a menu of its own in the main menu bar (it was inside View).
- `empaquetar_windows.sh`: writes `licenses/FUENTES.txt` (source code address of every
  bundled package) and `licenses/AVISO_GPL.txt` (GPL-only packages); new option
  `--fuentes` downloads the GPL/LGPL/MPL source archives to `instalador/fuentes/`
  to publish them with each release.
- THIRD_PARTY_NOTICES: source code of the bundled libraries and the GPL-2.0 jbigkit
  component (pulled in by GTK through GdkPixbuf → libtiff).

## [0.0.6] - 2026-11-23

### Added
- Sign › Digital Signature with Certificate: PAdES-B-B signature
  (`ETSI.CAdES.detached`, SHA-256, `signing-certificate-v2`, no `signing-time`)
  with a `.p12`/`.pfx` certificate, made with GnuTLS (already a QPDF dependency)
  and QPDF, without PoDoFo or OpenSSL. Appearance: box with name and date, PNG
  image of the handwritten signature, or invisible. Visible signatures are placed
  with a click after the dialog and stay upright on rotated pages.
- Checks: wrong certificate password, expired or not yet valid certificate,
  already signed document, password-protected document.
- Test certificate (`tests/datos/firma_prueba.p12`, fictitious) and signing tests.

### Known limitations
- One signature per document (QPDF rewrites the whole file); no timestamp.

## [0.0.5] - 2026-11-23

### Added
- Sign › Visual Signature (PNG image): click on the page, choose the PNG, the width
  in millimetres (the height keeps the proportion) and whether to remove the white
  background (useful for scanned or photographed signatures). Works on rotated
  pages and can be undone.
- Pages › Insert Image (PNG) (Ctrl+I): inserts the whole PNG as it is (keeps its own
  transparency, no background removal), with the width in millimetres.
- Own PNG reader (grey, grey+alpha, RGB, RGBA and palette; 1 to 16 bits;
  transparency) using zlib, already a QPDF dependency: no new libraries.

## [0.0.4] - 2026-11-23

### Added
- Pages › Add Text (Ctrl+T): click on the page, then write the text (several
  lines), font (Helvetica, Times, Courier, regular or bold), size and color.
  Standard PDF fonts with WinAnsi encoding: no font file is embedded. The text is
  appended to the page content with QPDF and stays horizontal on rotated pages.
- Parameter dialogs: multi-line text control and a color list with swatches
  inside the dialog (no separate color window, which could stay hidden behind
  the dialog on Windows); the last font, size, color and options are remembered.

## [0.0.3] - 2026-11-23

### Added
- Document › Merge with Other PDFs: ordered list of files, inserted at the end,
  at the beginning or after the current page (undoable).
- Document › Split or Extract Pages: one file per page, one file per range, or all
  the chosen pages in a single PDF. Existing files are never overwritten.
- Parameter dialogs: ordered PDF list (add, remove, move up/down) and folder field.
- Modules can return a message for the user and declare that they do not modify
  the document.

## [0.0.2] - 2026-11-23

### Added
- Page operations with QPDF (undoable): rotate right / left (Ctrl+R / Ctrl+L),
  rotate a range of pages, move pages to another position, delete pages (Delete).
- Reorder pages by dragging the thumbnails.
- View › Fit Page (Ctrl+2).
- Redo (Ctrl+Y, Ctrl+Shift+Z).
- Page number field in the status bar to jump to a page (View › Go to Page, Ctrl+G).
- The page operations also appear in the Edit menu (the Pages menu stays).
- Modules can declare a keyboard shortcut; parameter dialogs start on the visible page.

### Changed
- C++20 (required by the QPDF 12 headers); paths are converted to UTF-8 without
  the deprecated `u8path()`.
- `empaquetar_windows.sh` finds the DLLs recursively with `objdump` instead of `ldd`.
- Files dropped on the window are opened outside the drag-and-drop signal.
- `tools/obtener_pdfium.sh` retries with the Windows `curl.exe` when the MSYS2
  `curl` cannot verify the certificate, and accepts a manually downloaded archive
  (`PDFIUM_ARCHIVO=...`).

## [0.0.1] - 2026-11-23

### Added
- Project structure: `core` (no GUI), `gui` (GTK 3), `app`, self-registering modules.
- PDF viewer based on PDFium: continuous scrolling, zoom (fit width, actual size,
  Ctrl + wheel), HiDPI rendering, page thumbnails rendered in idle time.
- Open PDFs from the menu, the command line or by drag and drop; password-protected PDFs.
- Undo stack and "Save as" (the original file is never overwritten).
- Interface in Spanish, English and Italian, switchable at run time.
- Command line: `--version`, `--help`, `--list`.
- Packaging script for Windows (`empaquetar_windows.sh`) and NSIS installer through CPack.
