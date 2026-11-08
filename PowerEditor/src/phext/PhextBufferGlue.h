// PhextBufferGlue.h — the thin seam between the editor-agnostic phext core
// (PhextDocument / SubspaceSpec / Projection) and Notepad++'s Buffer /
// FileManager. WINDOWS-ONLY consumers: Buffer.h includes this header; the
// core headers it pulls in are platform-free and unit-tested standalone.
//
// Detection is by EXTENSION ONLY (.phext, case-insensitive). The 2023 patch
// content-sniffed the first load block for break bytes and false-positived
// on arbitrary binary files; SUBSPACE-DESIGN §3.1 makes sniffing advisory
// at most, and phase 1 does not sniff at all. A ".phext.xz" file is a
// sealed phext: phase 1 refuses it with an explanation rather than guessing
// (unsealing is libphext-cpp parity work, §3.5 item 7).

#pragma once

#include "PhextDocument.h"
#include "Projection.h"
#include "SubspaceSpec.h"

#include <string>

namespace phext {

inline bool hasSuffixNoCase(const wchar_t* path, const wchar_t* suffix) {
    if (!path || !suffix) return false;
    size_t n = 0; while (path[n]) ++n;
    size_t m = 0; while (suffix[m]) ++m;
    if (n < m) return false;
    for (size_t i = 0; i < m; ++i) {
        wchar_t a = path[n - m + i], b = suffix[i];
        if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a - L'A' + L'a');
        if (b >= L'A' && b <= L'Z') b = static_cast<wchar_t>(b - L'A' + L'a');
        if (a != b) return false;
    }
    return true;
}

inline bool isPhextFileName(const wchar_t* path) {
    return hasSuffixNoCase(path, L".phext");
}
inline bool isSealedPhextFileName(const wchar_t* path) {
    return hasSuffixNoCase(path, L".phext.xz");
}

// Per-buffer phext state, owned by Buffer (unique_ptr). Holds the model
// beside Scintilla: the Scintilla document shows `projection().text()`;
// saves parse that text back and splice it into `document()`.
class PhextBufferState {
public:
    // Load raw file bytes and build the default projection: last-used spec
    // is a session concern (phase 2); phase 1 uses Whole Volume when the
    // file has exactly one populated (collection, volume) pair, else
    // Whole File (SUBSPACE-DESIGN §1.3 / Q1 default).
    void loadFromBytes(const std::string& fileBytes) {
        _doc.load(fileBytes);
        _spec = defaultSpecFor(_doc);
        rebuildProjection();
    }

    static SubspaceSpec defaultSpecFor(const PhextDocument& doc) {
        bool seen = false;
        Coordinate first;
        for (const ScrollEntry& e : doc.entries()) {
            if (!e.populated()) continue;
            if (!seen) { first = e.coord; seen = true; continue; }
            if (e.coord[DimLibrary] != first[DimLibrary] || e.coord[DimShelf] != first[DimShelf]
                || e.coord[DimSeries] != first[DimSeries]
                || e.coord[DimCollection] != first[DimCollection]
                || e.coord[DimVolume] != first[DimVolume])
                return SubspaceSpec::wholeFile();
        }
        return seen ? SubspaceSpec::wholeVolume(first) : SubspaceSpec::wholeFile();
    }

    void rebuildProjection() { _proj.build(_doc, _spec); }

    const PhextDocument& document() const { return _doc; }
    const Projection& projection() const { return _proj; }
    const SubspaceSpec& spec() const { return _spec; }
    void setSpec(const SubspaceSpec& spec) { _spec = spec; rebuildProjection(); }

    // Coordinate of the scroll under a projection offset, for the status
    // bar. Returns false when the offset is not inside a member.
    bool coordinateAt(size_t projOffset, Coordinate& out, bool& onSeparator) const {
        ProjLocated loc = _proj.locate(projOffset);
        if (loc.memberIndex < 0) return false;
        out = loc.coord;
        onSeparator = loc.separator;
        return true;
    }

    // Save path: parse the edited projection text and splice it into the
    // document. On success outFileBytes is the exact byte stream to write
    // (BOM restored iff present at load) and the model is updated. On
    // failure error describes the block; nothing was written anywhere.
    bool saveFromProjectionText(const std::string& editedText,
                                std::string& outFileBytes, std::string& error) {
        Projection::ParseResult parsed = Projection::parseEdited(editedText);
        if (!parsed.ok) { error = parsed.error; return false; }
        Projection::SpliceResult sp = _proj.splice(_doc, parsed.members);
        if (!sp.ok) { error = sp.error; return false; }
        _doc.reload(sp.bytes);
        rebuildProjection();
        outFileBytes = _doc.fileBytes();
        return true;
    }

private:
    PhextDocument _doc;
    SubspaceSpec _spec;
    Projection _proj;
};

} // namespace phext
