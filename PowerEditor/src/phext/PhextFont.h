// PhextFont.h — Emise for phext buffers (fork patch, 2026-11-17;
// face swapped from Willbot Mono to Emise 2026-11-19).
//
// Will's direction: phext buffers render in Emise — the successor to
// Willbot Mono, in which Will's handwriting (Willbot) is redrawn as a
// designed monospace (a Will–Emise collaboration, shipped 2026-11-19
// as the default face of edit.phext.io, Scrollspace, and Phext
// Notepad; same 520-unit advance as Willbot Mono, so nothing shifts).
// WillbotMono.otf remains in the tree for rollback but is no longer
// loaded. The font ships with the fork at
// PowerEditor/src/phext/fonts/Emise.ttf; at runtime it is loaded
// as a PRIVATE font resource from beside the executable
// (<exe dir>\phext\fonts\ or <exe dir>\fonts\), so no system install is
// needed. A system-installed copy (%WINDIR%\Fonts\Emise.ttf) is
// also accepted. If neither is present, loading fails soft and the view
// keeps the user's configured font — a missing font must never break
// the editor.
//
// Header-only and Windows-only, like PhextBufferGlue.h: its only
// consumer is Notepad_plus.cpp, which applies the face to the current
// Scintilla view when the user switches into a phext buffer.

#pragma once

#include <windows.h>

#include <string>

namespace phext {

// Scintilla takes the face name as a narrow string; the name is ASCII.
inline const char* emiseFaceA() { return "Emise"; }

// Load the bundled font once per process. Returns true when the
// "Emise" face should be available to this process afterwards.
inline bool ensureEmiseLoaded() {
    static int state = -1; // -1 = not tried, 0 = unavailable, 1 = available
    if (state >= 0) return state == 1;
    state = 0;

    wchar_t modulePath[MAX_PATH] = {};
    if (::GetModuleFileNameW(nullptr, modulePath, MAX_PATH)) {
        std::wstring dir(modulePath);
        const size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            dir.resize(slash + 1);
            const wchar_t* rels[] = {L"phext\\fonts\\Emise.ttf",
                                     L"fonts\\Emise.ttf"};
            for (const wchar_t* rel : rels) {
                const std::wstring full = dir + rel;
                if (::GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES
                    && ::AddFontResourceExW(full.c_str(), FR_PRIVATE, nullptr) > 0) {
                    state = 1;
                    break;
                }
            }
        }
    }

    if (state == 0) { // system-installed copy?
        wchar_t winDir[MAX_PATH] = {};
        if (::GetWindowsDirectoryW(winDir, MAX_PATH)) {
            const std::wstring full = std::wstring(winDir) + L"\\Fonts\\Emise.ttf";
            if (::GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES)
                state = 1;
        }
    }
    return state == 1;
}

} // namespace phext
