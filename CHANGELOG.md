# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/)
and the project follows [Semantic Versioning](https://semver.org/).

## [0.0.4] - 2026-11-23

### Added
- Pages › Add Text (Ctrl+T): click on the page, then write the text (several
  lines), font (Helvetica, Times, Courier, regular or bold), size and color.
  Standard PDF fonts with WinAnsi encoding: no font file is embedded. The text is
  appended to the page content with QPDF and stays horizontal on rotated pages.
- Parameter dialogs: multi-line text and color controls; the last font, size,
  color and options chosen are remembered.

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
