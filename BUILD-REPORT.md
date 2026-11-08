# BUILD REPORT — Notepad++ phext subspace support, Phase 1

- **Task coordinate:** 38.6.8/8.7.12/6.6.12
- **Date:** 2026-11-08
- **Repo:** `~/workspace/notepadpp-phext/build/` — fresh clone of upstream
  `notepad-plus-plus/notepad-plus-plus`, **not pushed** (coordinator verifies
  and pushes).
- **Branch:** `emise/phext-subspace`
- **Base:** upstream `d87f8bd8db9baebbfd38b580eb22c7eb05cd2b7e` (v8.9.9 + 2 commits)
- **HEAD:** `69a0d04587bd447414c2cfbd3cd22db891deb850`
  - `29a33f4` — editor-agnostic subspace core + tests
  - `69a0d04` — Notepad++ wiring + PATCHES.md + PHEXT.md
- The old fork master (`549d7a0bed`, 2023 patch) is untouched. Nothing of
  its code was carried over — only its intent, per the review. No Scintilla
  or Lexilla file is modified (also upstream `AGENTS.md` house rule).

## What was built

### 1. Editor-agnostic core — `PowerEditor/src/phext/` (TESTED)

Single canonical copy, C++17, no Windows dependencies:

- **`phext_core.h`** — `Coordinate` (9 × uint32, unbounded), break bytes,
  dead-reckoning `applyBreak`, strict coordinate parse/format (canonical
  and URL forms), and canonical break emission matching libphext-rs
  v0.5.0's `append_scroll`. Written against the RS spec directly; this
  header is the documented swap-in point for the parallel libphext-cpp
  parity port.
- **`PhextDocument`** — backing bytes exactly as loaded (UTF-8 BOM kept as
  a flag, restored on save) + a sparse offset index from one delimiter
  walk: one entry per scroll *slot* (empty slots included, so splices can
  be exact), populated count tracked separately. Cost is O(file bytes) to
  load and O(populated) thereafter — never O(coordinate extent).
- **`SubspaceSpec`** — per-dimension Fixed/Range/Free, canonical text
  form (`1.1.1/1.1.1-3/*.*.*`), interval/strided classification
  (interval ⇔ no Fixed dimension below a non-Fixed one), and all six
  presets from design §1.2 (P0–P5).
- **`Projection`** — one subspace as one editable text document:
  generated separator lines (`§ <coord> — <Dim> break ───…`, plus `Start`
  for the file's first scroll), the bidirectional offset map
  (projection offset ↔ (coordinate, offset-in-scroll)), edited-projection
  parsing, and **splice write-back**:
  - exactly one joiner newline after each body makes segmentation
    unambiguous (a member's text is its body lines joined with `\n`);
  - corrupted separator (any line starting `§ ` that fails the grammar)
    → parse error, save blocked, nothing written, never a guess;
  - coordinate edit in a separator = move (validated against the spec);
    separator deletion = merge; missing member = delete; extra member =
    split/insert — all fall out of segmentation + canonical serialization;
  - interval splice copies prefix/suffix bytes verbatim (the preceding
    break is re-emitted canonically — verified to be the identical byte
    when unchanged) and re-serializes only the region;
  - no-edit fast path returns the backing bytes verbatim;
  - strided splice (phase 1): text edits with an unchanged coordinate
    set scatter per member with every break byte verbatim; structural
    edits in a strided view are refused with an explicit error;
  - trailing-break preservation via the fast path and suffix copying.

### 2. Notepad++ wiring — Windows-only, code-complete, NOT MSVC-compiled

Enumerated per file in `PATCHES.md` (8 modified files, +284/−3 lines).
Summary: extension-only `.phext` detection in `FileManager::loadFileData`
(no content sniffing; `.phext.xz` refused with a message), byte-faithful
`loadPhextFileData` filling the Scintilla document with projection text
(encoding/EOL paths bypassed), `saveBuffer` phext branch that parses the
projection, splices, and writes assembled bytes directly (Save-a-Copy
keeps upstream behaviour), `Buffer::_phextState` + `isPhext()`,
`IDM_PHEXT` command block at IDM+9500, the `&Phext` popup appended last
in `Notepad_plus.rc` with `MENUINDEX_PHEXT 15` in `resource.h` (**no
existing menu index shifts**), `checkDocState` enabling Phext commands
iff the current buffer is phext (the 2023 always-true bug does not
recur), status-bar coordinate prefix in `updateStatusBar` driven by the
projection map (phext buffers only), functional File Info and Repair
Separators handlers, guarded stubs for the dialog commands
(TODO PHEXT-PHASE2), and vcxproj registration of the core sources.

## Test results

Standalone runner: `PowerEditor/src/phext/tests/run_tests.sh`
(g++ -std=c++17 -O2, no Windows deps).

**201 checks passed, 0 failed** (synthetic suite + all four corpora).

Coverage: spec parse/format/presets/classification; byte-identity
round-trip (fast path AND forced-canonical splice, the latter also
proving the coordinate→text mapping is preserved); zero structural bytes
in projection text; region-confined edit (prefix/suffix bytes identical);
split (with sibling renumbering), merge (both line-merge and byte-exact
concat forms), delete; separator corruption blocked with error line;
move within subspace; move out of subspace blocked; strided text scatter
with non-member bytes untouched + strided structural edit refused;
offset map both directions; sparse far coordinate
(100.200.300/400.500.600/700.800.900); trailing break; empty file;
insert into an empty subspace.

Corpus timings (this VM, single run, -O2):

| Corpus | Bytes | Slots | Populated | Load+index | Project (P0) | Parse + canonical splice |
|---|---|---|---|---|---|
| CYOA (`choose-your-own-adventure.phext`) | 4,602,370 | 82,708 | 830 | 26 ms | 45 ms | 75 ms |
| agi.phext (`phext-notepad-perf/agi-repo/`) | 1,471,101 | 354,942 | 7,281 | 24 ms | 221 ms | 303 ms |
| Incipit (`incipit.phext`) | 1,072,922 | 7,545 | 197 | 6 ms | 5 ms | 17 ms |
| emise.phext | 360,046 | 130 | 21 | 0 ms | 0 ms | 1 ms |

All four corpora round-trip **byte-identical even through the canonical
splice** (they are already in canonical form). agi.phext's Whole-File
projection is the worst case (7,281 members); realistic subspace views
are far smaller.

The glue header (`PhextBufferGlue.h`) was also compiled and exercised
standalone on Linux (extension checks, default Whole-Volume spec choice,
unmodified save == original bytes): passed.

## Files touched

**Core-testable (compiled + tested here):**
`PowerEditor/src/phext/{phext_core.h, PhextDocument.h/.cpp,
SubspaceSpec.h/.cpp, Projection.h/.cpp, PhextBufferGlue.h,
tests/test_phext.cpp, tests/run_tests.sh}`

**Windows-only (written against v8.9.9 sources, NOT compiled):**
`PowerEditor/src/ScintillaComponent/Buffer.h`,
`PowerEditor/src/ScintillaComponent/Buffer.cpp`,
`PowerEditor/src/Notepad_plus.cpp`, `PowerEditor/src/NppCommands.cpp`,
`PowerEditor/src/Notepad_plus.rc`, `PowerEditor/src/menuCmdID.h`,
`PowerEditor/src/resource.h`, `PowerEditor/visual.net/notepadPlus.vcxproj`

**Docs:** `PATCHES.md`, `PHEXT.md` (repo root).

## What remains for a Windows/MSVC session

1. **Compile and fix** whatever MSVC flags in the wiring (expected risk
   areas: `Buffer.cpp` phext save block's Win32 calls, `SCI_GETTEXT`
   handler code in `NppCommands.cpp`, the `\u00B7` literal in
   `updateStatusBar`). The core itself is portable C++17 and pre-tested.
2. **Smoke-test the data path**: open CYOA, edit one scroll, save, verify
   SHA-256 behaviour matches the core tests (untouched save = identical;
   edited save = region-confined).
3. **Phase 2 UI**: Open Subspace dialog (spec grid + populated-value
   dropdowns), Navigator dockable panel, Go to Coordinate, structural
   commands' Scintilla-side edits (split/insert/delete as single undo
   units), View Raw Bytes, re-projection of the Scintilla document after
   a reordering save, per-file spec persistence in `session.xml`.
4. **Known phase-1 limitations** (all documented in PHEXT.md/PATCHES.md):
   strided structural edits refused; one projection per buffer (default
   spec only until the dialog lands); sealed `.phext.xz` refused;
   body lines beginning with `§ ` are reserved (they parse as separators);
   backup-on-save for phext buffers uses the direct-write path, not
   upstream's backup machinery.

## Notes for the coordinator

- Do not push the old `upstream/` blobless clone state — it has a broken
  index (staged deletions); `build/` is a fresh, verified clone at the
  exact base commit and supersedes it.
- Clone depth is 60 (shallow) to keep the fetch reliable; unshallow
  before any history-sensitive operation.
