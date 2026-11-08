// SubspaceSpec.h — a phext subspace specification (SUBSPACE-DESIGN §1).
//
// A spec assigns each of the 9 dimensions exactly one of Fixed(v),
// Range(lo,hi) inclusive, or Free (*). A scroll is a member iff every
// dimension satisfies its entry. The projection is the member scrolls in
// document order — the only order in which splice-back is well-defined.
//
// Canonical text form reuses coordinate syntax with '*' and 'lo-hi'
// extensions, e.g. "1.1.1/1.2.1/2.*.*" or "1.1.1/1.1.1-3/*.*.*".

#pragma once

#include "phext_core.h"

#include <array>
#include <string>
#include <string_view>

namespace phext {

enum class DimKind { Fixed, Range, Free };

struct DimSpec {
    DimKind kind = DimKind::Free;
    uint32_t lo = 1; // Fixed: the value. Range: inclusive low (>= 1).
    uint32_t hi = 1; // Range: inclusive high. Fixed/Free: mirrors lo / unused.

    static DimSpec fixed(uint32_t v) { DimSpec d; d.kind = DimKind::Fixed; d.lo = d.hi = v; return d; }
    static DimSpec range(uint32_t lo_, uint32_t hi_) { DimSpec d; d.kind = DimKind::Range; d.lo = lo_; d.hi = hi_; return d; }
    static DimSpec free() { return DimSpec{}; }

    bool matches(uint32_t component) const {
        switch (kind) {
            case DimKind::Fixed: return component == lo;
            case DimKind::Range: return component >= lo && component <= hi;
            case DimKind::Free:  return true;
        }
        return false;
    }
    bool isFixed() const { return kind == DimKind::Fixed; }
};

struct SubspaceSpec {
    std::array<DimSpec, DimCount> dims{}; // default: all Free (Whole File, P0)

    bool matches(const Coordinate& c) const {
        for (int d = 0; d < DimCount; ++d)
            if (!dims[static_cast<size_t>(d)].matches(c[d])) return false;
        return true;
    }

    // Interval iff no Fixed dimension sits below a non-Fixed one — i.e. the
    // non-Fixed dimensions form a suffix. Interval members occupy one
    // contiguous byte range of the file; anything else is strided (members
    // interleaved with non-members). See SUBSPACE-DESIGN §1.1.
    bool isInterval() const {
        bool seenNonFixed = false;
        for (int d = 0; d < DimCount; ++d) {
            if (!dims[static_cast<size_t>(d)].isFixed()) seenNonFixed = true;
            else if (seenNonFixed) return false;
        }
        return true;
    }

    std::string toString() const {
        std::string s;
        for (int i = 0; i < DimCount; ++i) {
            if (i) s += (i % 3 == 0) ? '/' : '.';
            const DimSpec& d = dims[static_cast<size_t>(i)];
            switch (d.kind) {
                case DimKind::Fixed: s += std::to_string(d.lo); break;
                case DimKind::Range: s += std::to_string(d.lo) + "-" + std::to_string(d.hi); break;
                case DimKind::Free:  s += '*'; break;
            }
        }
        return s;
    }

    // Parse the canonical text form. Returns false on malformed input.
    static bool parse(std::string_view s, SubspaceSpec& out);

    // --- Presets (SUBSPACE-DESIGN §1.2). `anchor` is a populated scroll's
    // coordinate (typically the scroll under the caret / Navigator node). ---
    static SubspaceSpec wholeFile();                                    // P0: *.*.*/*.*.*/*.*.*
    static SubspaceSpec wholeVolume(const Coordinate& anchor);          // P1: fix through Volume, rest Free
    static SubspaceSpec slice3D(const Coordinate& anchor);              // P3: fix Z+Y arms, X arm Free
    static SubspaceSpec oneChapter(const Coordinate& anchor);           // P2: fix through Chapter, Section+Scroll Free
    static SubspaceSpec scrollLine(const Coordinate& anchor);           // P4: fix through Section, Scroll Free
    static SubspaceSpec sameChapterEveryBook(const Coordinate& anchor); // P5 (strided): Book Free, Chapter fixed

private:
    static SubspaceSpec fixedThrough(const Coordinate& anchor, int lastFixedDim);
};

} // namespace phext
