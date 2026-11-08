# PATCHES.md — divergence record for the phext fork of Notepad++

Fork-patch discipline: this fork is upstream Notepad++ **plus** the auditable
patch set below. Every file that differs from upstream is listed here with
the reason. If it isn't in this file, it isn't a fork change.

- **Upstream base:** `d87f8bd8db9baebbfd38b580eb22c7eb05cd2b7e`
  ("Refactoring", just after the v8.9.9 tag) on upstream `master`.
- **Fork branch:** `emise/phext-subspace`.
- **Design:** `SUBSPACE-DESIGN.md` in the task workspace
  (`~/workspace/notepadpp-phext/`); the feature is described in `PHEXT.md`.
- **Nothing under `scintilla/` or `lexilla/` is modified.** The 2023 patch's
  Scintilla `Document` surgery is discarded by design: the phext model lives
  beside Scintilla, never inside it.

## New files — the phext core (`PowerEditor/src/phext/`)

Editor-agnostic C++17, single canonical copy (the 2023 patch carried two
diverging copies of its headers; that does not recur). Unit-tested
standalone with g++ — see `tests/`.

| File | Why |
|---|---|
| `phext/phext_core.h` | Coordinate (9 unbounded components), break bytes, dead-reckoning break application, strict coordinate parse/format, and canonical `append_scroll` break emission. Semantics mirror libphext-rs v0.5.0; this header is the documented swap-in point for the libphext-cpp parity port. |
| `phext/PhextDocument.h` / `.cpp` | The model beside Scintilla: backing bytes (BOM as a flag), one-pass delimiter walk, sparse offset index with one entry per scroll slot (populated or empty). Cost follows populated scrolls, never coordinate extents. |
| `phext/SubspaceSpec.h` / `.cpp` | Subspace specification: per-dimension Fixed/Range/Free, canonical text form, interval vs strided classification, and the six presets (P0 Whole File, P1 Whole Volume, P2 One Chapter, P3 3D Slice, P4 Scroll Line, P5 Same Chapter in Every Book). |
| `phext/Projection.h` / `.cpp` | One subspace as one editable document: generated separator lines (editable-as-structure), the bidirectional projection-offset ↔ (coordinate, offset-in-scroll) map, edited-projection parsing (corrupted separators are an error, never a guess), and splice write-back — bytes outside the edited region copied verbatim; no-edit save is byte-identical. |
| `phext/PhextBufferGlue.h` | The thin Windows-side seam: extension-only `.phext` detection, per-buffer state (document + spec + projection), default-spec choice, status-bar coordinate lookup, and the save entry point. |
| `phext/tests/test_phext.cpp` | Standalone acceptance tests (g++, no Windows deps): round-trips on real corpora, split/merge/delete, corruption blocking, moves, strided scatter, sparsity, offset map, perf timings. |

## New files — CI

| File | Why |
|---|---|
| `.github/workflows/phext-build.yml` | Fork-only CI for this branch: compiles and runs the phext core tests under MSVC (`cl /std:c++17`) and builds `notepadPlus.sln` Release\|x64, uploading the msbuild log on failure. Upstream's `CI_build.yml` is unmodified and also runs; this workflow gives the phext wiring its own attributable signal. Triggers on pushes to `emise/phext-subspace` and manually. |

## Modified files — the wiring (all Windows-only)

| File | Change | Why |
|---|---|---|
| `PowerEditor/src/ScintillaComponent/Buffer.h` | Includes the phext glue; `Buffer` gains a `std::unique_ptr<phext::PhextBufferState>` (null for ordinary buffers) plus `isPhext()`/`phextState()`; `FileManager` gains `loadPhextFileData()` and a pending-state slot used during load/reload. | A phext buffer must carry its model next to its Scintilla document; plugins cannot own load/save, so this is core. `isPhext()` is a real per-buffer fact — the 2023 patch's always-true `isPhextEnabled()` must not recur. |
| `PowerEditor/src/ScintillaComponent/Buffer.cpp` | (1) `loadFileData` dispatches `.phext` (extension only — **no content sniffing**) to `loadPhextFileData`, and refuses `.phext.xz` with an explanation instead of mangling it through the text path. (2) `loadPhextFileData` reads raw bytes, builds the model, and fills the Scintilla document with projection text — encoding detection and EOL conversion are bypassed entirely. (3) `loadFile`/`reloadBuffer` adopt the pending state into the `Buffer`. (4) `setLoadedBufferEncodingAndEol` early-returns for phext buffers (display values are inert). (5) `saveBuffer` splices the edited projection back through the model and writes the assembled bytes directly; a blocked splice writes nothing and reports. "Save a Copy" keeps upstream behaviour. | Load/save ownership is the core patch's reason to exist: phext break bytes (0x01, 0x17–0x1F) are destroyed by codepage round-trips and EOL rewrites. |
| `PowerEditor/src/menuCmdID.h` | New command block `IDM_PHEXT = IDM + 9500`, ten commands (+1…+10). | Clear of every upstream block (EXECUTE ends at IDM+9001); no existing ID changes value. |
| `PowerEditor/src/resource.h` | Adds `MENUINDEX_PHEXT 15` only. | The Phext menu is appended at the end of the menu bar so **no existing `MENUINDEX_*` shifts** (HELP stays 12, LIST stays 14). |
| `PowerEditor/src/Notepad_plus.rc` | Adds the `&Phext` popup menu (Open/Change Subspace, Go to Coordinate, Split/Insert/Delete Scroll, Repair Separators, View Raw Bytes, Navigator, File Info) as the last popup of `IDR_M30_MENU`. | The 2023 patch built a Phext menu in code but never added it to the resource, so it never appeared. A feature must be findable from the main UI. |
| `PowerEditor/src/Notepad_plus.cpp` | (1) `checkDocState` enables the ten Phext commands iff the current buffer `isPhext()`. (2) `updateStatusBar` prefixes the caret section with the coordinate of the scroll under the caret (from the projection offset map), phext buffers only. | Menu honesty (greyed, not hidden, for ordinary files) and the primary "where am I?" channel (design §2.6). |
| `PowerEditor/src/NppCommands.cpp` | Command handlers: `IDM_PHEXT_FILE_INFO` (live: spec, populated/slot counts, bytes) and `IDM_PHEXT_REPAIR_SEPARATORS` (live: validates the current projection, reports and jumps to a corrupted separator) are functional; the dialog-driven commands (Open/Change Subspace, Go to Coordinate, Split, Insert, Delete, View Raw, Navigator) are guarded stubs that say so in the UI — TODO(PHEXT-PHASE2). | No fake functionality: what works works, what isn't built says it isn't. |
| `PowerEditor/visual.net/notepadPlus.vcxproj` | Registers the three core `.cpp` files and five phext headers. | So the MSVC build compiles the core and glue with the app. |
| `PowerEditor/src/CMakeLists.txt` | Adds the three phext core `.cpp` files to the explicit source list, next to `Buffer.cpp`. | Upstream's CMake build (the `build_windows_cmake` CI job) lists sources explicitly; without this the CMake link failed with 7 unresolved phext symbols (LNK2001) even though the vcxproj build was green — found on the first full `CI_build` run of this branch, 2026-11-08. |

**MSVC correction (2026-11-08, first `phext-build` Actions run):** the phase-1
draft had pasted the save-splice block into `backupCurrentBuffer` (where
`isCopy`/`fullpath` are not in scope — MSVC C2065 ×2) and `saveBuffer` had no
phext branch at all. Corrected: the block now lives in `saveBuffer` (with the
upstream tail's post-save bookkeeping mirrored — dirty flag, save point,
backup-file cleanup, hidden/system attribute restore; the language-from-content
probe is skipped for phext buffers), and `backupCurrentBuffer` gained its own
branch — splice first, write the assembled bytes to the backup file in place
of the convertor write, stay silent on a blocked splice. Known limitation of
the save branch: it does not take upstream's UAC-elevation path — a target
that cannot be opened for writing fails closed with `SaveOpenFailed` rather
than writing projection text through the convertor.

## Deliberately NOT carried over from the 2023 patch

- Scintilla `Document`/`CellBuffer` per-scroll tree surgery (whole approach
  discarded; the model is beside Scintilla now).
- Hand-added Scintilla messages 6000–6030 inside autogenerated headers.
- Content-sniffing phext detection (false-positives on binary files).
- Per-scroll navigation commands and the "Enter inserts a dimension break"
  hijack; navigation is scrolling inside one projection, the Navigator,
  and Go to Coordinate.
- The duplicated `phext.h`/`phext_string.h` copies in two trees.

## Rebase notes

The recurring upstream rebase touches only the hook sites in `Buffer.cpp`
(load dispatch, save dispatch), `Buffer.h`, `Notepad_plus.cpp` (two small
blocks), `NppCommands.cpp` (one case block), the `.rc` menu tail, and the
two ID headers — everything else is additive new files.
