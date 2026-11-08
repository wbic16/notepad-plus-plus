// phext_core.h — phext coordinate + break primitives (editor-agnostic).
//
// Semantics mirror libphext-rs v0.5.0 (src/phext.rs), read directly:
//  - 9 dimensions, highest->lowest: Library, Shelf, Series, Collection,
//    Volume, Book, Chapter, Section, Scroll.
//  - Canonical text form: LB.SF.SR/CN.VM.BK/CH.SN.SC, 1-indexed, components
//    unbounded (the 999 cap was removed in libphext 0.4.0).
//  - A break increments its dimension and resets every lower dimension to 1.
//  - Coordinates are positions derived by dead reckoning, not identities.
//
// SWAP-IN POINT: when libphext-cpp reaches parity with libphext-rs v0.5.0,
// this header is the single place to delegate to it. Everything above this
// layer (PhextDocument, SubspaceSpec, Projection) is written against the
// semantics in this file, not against any particular library surface.
// Single canonical copy: this header lives only under PowerEditor/src/phext/.
// (The 2023 patch carried two diverging copies; that must not recur.)

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace phext {

// Dimension indices, highest (0) -> lowest (8).
enum Dim : int {
    DimLibrary = 0,
    DimShelf = 1,
    DimSeries = 2,
    DimCollection = 3,
    DimVolume = 4,
    DimBook = 5,
    DimChapter = 6,
    DimSection = 7,
    DimScroll = 8,
    DimCount = 9
};

// Structural break bytes (libphext-rs *_BREAK_U8 constants).
inline constexpr uint8_t kLibraryBreak    = 0x01;
inline constexpr uint8_t kShelfBreak      = 0x1F;
inline constexpr uint8_t kSeriesBreak     = 0x1E;
inline constexpr uint8_t kCollectionBreak = 0x1D;
inline constexpr uint8_t kVolumeBreak     = 0x1C;
inline constexpr uint8_t kBookBreak       = 0x1A;
inline constexpr uint8_t kChapterBreak    = 0x19;
inline constexpr uint8_t kSectionBreak    = 0x18;
inline constexpr uint8_t kScrollBreak     = 0x17;
inline constexpr uint8_t kLineBreak       = 0x0A; // inside a scroll, never structural

inline constexpr std::array<uint8_t, DimCount> kBreakByte = {
    kLibraryBreak, kShelfBreak, kSeriesBreak, kCollectionBreak, kVolumeBreak,
    kBookBreak, kChapterBreak, kSectionBreak, kScrollBreak
};

inline constexpr std::array<const char*, DimCount> kDimName = {
    "Library", "Shelf", "Series", "Collection", "Volume",
    "Book", "Chapter", "Section", "Scroll"
};

inline bool isDimensionBreak(uint8_t b) {
    for (uint8_t br : kBreakByte) if (b == br) return true;
    return false;
}

// Dimension whose counter a break byte advances, or -1 if not a break.
inline int dimOfBreak(uint8_t b) {
    for (int d = 0; d < DimCount; ++d) if (kBreakByte[d] == b) return d;
    return -1;
}

struct Coordinate {
    std::array<uint32_t, DimCount> v{1, 1, 1, 1, 1, 1, 1, 1, 1};

    uint32_t operator[](int d) const { return v[static_cast<size_t>(d)]; }
    uint32_t& operator[](int d) { return v[static_cast<size_t>(d)]; }

    bool valid() const {
        for (uint32_t c : v) if (c < 1) return false;
        return true;
    }
    // Canonical "LB.SF.SR/CN.VM.BK/CH.SN.SC".
    std::string toString() const {
        std::string s;
        for (int i = 0; i < DimCount; ++i) {
            if (i) s += (i % 3 == 0) ? '/' : '.';
            s += std::to_string(v[static_cast<size_t>(i)]);
        }
        return s;
    }
    // URL form "LB.SF.SR;CN.VM.BK;CH.SN.SC" (libphext to_urlencoded).
    std::string toUrlEncoded() const {
        std::string s;
        for (int i = 0; i < DimCount; ++i) {
            if (i) s += (i % 3 == 0) ? ';' : '.';
            s += std::to_string(v[static_cast<size_t>(i)]);
        }
        return s;
    }
};

// Lexicographic order == canonical coordinate order == document order.
// (Matches the derived Ord on libphext-rs Coordinate: z, then y, then x.)
inline int compare(const Coordinate& a, const Coordinate& b) {
    for (int i = 0; i < DimCount; ++i) {
        if (a.v[static_cast<size_t>(i)] != b.v[static_cast<size_t>(i)])
            return a.v[static_cast<size_t>(i)] < b.v[static_cast<size_t>(i)] ? -1 : 1;
    }
    return 0;
}
inline bool operator==(const Coordinate& a, const Coordinate& b) { return compare(a, b) == 0; }
inline bool operator!=(const Coordinate& a, const Coordinate& b) { return compare(a, b) != 0; }
inline bool operator<(const Coordinate& a, const Coordinate& b) { return compare(a, b) < 0; }
inline bool operator<=(const Coordinate& a, const Coordinate& b) { return compare(a, b) <= 0; }
inline bool operator>(const Coordinate& a, const Coordinate& b) { return compare(a, b) > 0; }
inline bool operator>=(const Coordinate& a, const Coordinate& b) { return compare(a, b) >= 0; }

// Advance a coordinate across one break of dimension d:
// increment d, reset every lower dimension to 1. (libphext-rs *_break().)
inline void applyBreak(Coordinate& c, int d) {
    c.v[static_cast<size_t>(d)] += 1;
    for (int j = d + 1; j < DimCount; ++j) c.v[static_cast<size_t>(j)] = 1;
}
inline void applyBreakByte(Coordinate& c, uint8_t b) {
    int d = dimOfBreak(b);
    if (d >= 0) applyBreak(c, d);
}

// Strict parse of "LB.SF.SR/CN.VM.BK/CH.SN.SC" (';' accepted as the macro
// separator, as in libphext's URL form). Every component must be >= 1.
// Returns false on any malformed input; never guesses.
inline bool parseCoordinate(std::string_view s, Coordinate& out) {
    Coordinate r;
    size_t pos = 0;
    for (int i = 0; i < DimCount; ++i) {
        if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') return false;
        uint64_t val = 0;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
            val = val * 10 + static_cast<uint64_t>(s[pos] - '0');
            if (val > 0xFFFFFFFFull) return false;
            ++pos;
        }
        if (val < 1) return false;
        r.v[static_cast<size_t>(i)] = static_cast<uint32_t>(val);
        if (i + 1 < DimCount) {
            char want = (i % 3 == 2) ? '/' : '.';
            if (pos >= s.size()) return false;
            if (s[pos] != want && !(want == '/' && s[pos] == ';')) return false;
            ++pos;
        }
    }
    if (pos != s.size()) return false;
    out = r;
    return true;
}

// Canonical serialization core (libphext-rs append_scroll): emit the minimal
// break sequence that advances `cur` to `target`, leaving cur == target.
// Precondition: target >= cur in coordinate order, and target is reachable
// from cur by breaks alone (always true when target >= cur: the highest
// differing dimension is larger in target, breaks raise it and reset the
// lower dimensions, which are then raised in turn). Returns false without
// emitting if target < cur.
inline bool emitBreaksTo(std::string& out, Coordinate& cur, const Coordinate& target) {
    if (target < cur) return false;
    while (cur < target) {
        for (int d = 0; d < DimCount; ++d) {
            if (cur[d] < target[d]) {
                out += static_cast<char>(kBreakByte[static_cast<size_t>(d)]);
                applyBreak(cur, d);
                break; // restart scan from the highest dimension, as append_scroll does
            }
        }
    }
    return cur == target;
}

} // namespace phext
