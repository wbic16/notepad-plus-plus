// PhextFont.h — Willbot Mono for phext buffers (fork patch, 2026-11-17).
//
// Will's direction: phext buffers render in Willbot Mono, the monospace
// cut of his handwriting font. The font ships with the fork at
// PowerEditor/src/phext/fonts/WillbotMono.otf; at runtime it is loaded
// as a PRIVATE font resource from beside the executable
// (<exe dir>\phext\fonts\ or <exe dir>\fonts\), so no system install is
// needed. A system-installed copy (%WINDIR%\Fonts\WillbotMono.otf) is
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
inline const char* willbotMonoFaceA() { return "Willbot Mono"; }

// Load the bundled font once per process. Returns true when the
// "Willbot Mono" face should be available to this process afterwards.
inline bool ensureWillbotMonoLoaded() {
    static int state = -1; // -1 = not tried, 0 = unavailable, 1 = available
    if (state >= 0) return state == 1;
    state = 0;

    wchar_t modulePath[MAX_PATH] = {};
    if (::GetModuleFileNameW(nullptr, modulePath, MAX_PATH)) {
        std::wstring dir(modulePath);
        const size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) {
            dir.resize(slash + 1);
            const wchar_t* rels[] = {L"phext\\fonts\\WillbotMono.otf",
                                     L"fonts\\WillbotMono.otf"};
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
            const std::wstring full = std::wstring(winDir) + L"\\Fonts\\WillbotMono.otf";
            if (::GetFileAttributesW(full.c_str()) != INVALID_FILE_ATTRIBUTES)
                state = 1;
        }
    }
    return state == 1;
}

} // namespace phext
