# Phext subspace editing in Notepad++

This fork adds native phext support to Notepad++ as **free-form subspace
views**: open a `.phext` file and you get one ordinary, editable document
per subspace — a whole volume, one chapter, a 3-D slice — instead of one
scroll per tab. You read and edit the region as flowing text; the editor
keeps track of coordinates for you.

## How to find it

- **Open any `.phext` file.** It opens as a subspace projection (Whole
  Volume when the file has a single populated volume, otherwise Whole
  File) — never as raw control bytes.
- **The `Phext` menu** is on the menu bar (far right). It is enabled only
  when the current buffer is a phext buffer, greyed otherwise:
  Open Subspace…, Change Subspace…, Go to Coordinate…, Split Scroll,
  Insert Scroll After, Delete Scroll, Repair Separators, View Raw Bytes,
  Navigator, File Info. *(Phase 1: File Info and Repair Separators work;
  the dialog commands announce themselves as phase 2 — see Status.)*
- **The status bar** shows the coordinate of the scroll under the caret
  (`1.1.1/1.1.2/3.4.5 · Ln …, Col …`) for phext buffers.

## How it works

- Scrolls inside the projection are separated by generated **separator
  lines**: `§ 1.1.1/1.1.2/3.4.5 — Section break ──────────────`. They are
  real text with a grammar: edit the coordinate to **move** the scroll,
  delete the line to **merge** with the previous scroll, add one to
  **split**. Fill characters after the break name are insignificant.
- A separator line that no longer parses **blocks the save** and is
  reported — the editor never guesses at structure, and a blocked save
  writes nothing. *Phext ▸ Repair Separators* validates and jumps to the
  offending line.
- **Saving splices**: bytes outside the edited subspace are copied
  verbatim; inside it, the region is re-serialized canonically (minimal
  break sequences, per libphext). Opening a phext and saving it untouched
  is byte-identical — verified on multi-megabyte real files.
- Inserting/deleting a scroll renumbers its following siblings up to the
  next higher break (phext coordinates are positions, not identities).
  Empty scrolls are not stored unless later scrolls follow them.
- Strided subspaces (e.g. "chapter 2 of every book") support text edits in
  phase 1; structural edits there are refused with an explanation.

## Status (honest)

- **Core: built and tested.** The editor-agnostic model
  (`PowerEditor/src/phext/`: PhextDocument, SubspaceSpec, Projection)
  builds with any C++17 compiler and passes its standalone test suite
  (`PowerEditor/src/phext/tests/test_phext.cpp`): byte-identity round-trips
  on CYOA (4.6 MB), Incipit (1.1 MB), agi.phext (1.5 MB, ~7,300 populated
  scrolls), split/merge/delete/move, corruption blocking, strided scatter,
  and sparse-coordinate cases.
- **Windows wiring: code-complete, NOT yet compiled with MSVC.** This
  phase was built on Linux; there is no MSVC/Windows SDK in that
  environment. The load/save hooks, menu, and status bar are written
  against the v8.9.9 sources but must be compiled and smoke-tested in a
  Windows session before this fork is usable as an editor.
- **Not yet built:** the Open Subspace dialog and Navigator panel, Go to
  Coordinate, the structural menu commands' document edits, View Raw
  Bytes, per-file spec persistence in the session file, and sealed
  (`.phext.xz`) files (refused with an explanation for now). The core
  operations behind all of these exist and are tested; the remaining work
  is Win32 surface.
- The C++ phext library situation: the core's semantics mirror
  libphext-rs v0.5.0 and were written fresh for this feature;
  `phext_core.h` is the single swap-in point when libphext-cpp reaches
  parity.

See `PATCHES.md` for the exact, per-file divergence from upstream.
