// SubspaceSpec.cpp — see SubspaceSpec.h.

#include "SubspaceSpec.h"

namespace phext {

static bool parseUnsigned(std::string_view s, size_t& pos, uint32_t& out) {
    if (pos >= s.size() || s[pos] < '0' || s[pos] > '9') return false;
    uint64_t val = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') {
        val = val * 10 + static_cast<uint64_t>(s[pos] - '0');
        if (val > 0xFFFFFFFFull) return false;
        ++pos;
    }
    if (val < 1) return false;
    out = static_cast<uint32_t>(val);
    return true;
}

bool SubspaceSpec::parse(std::string_view s, SubspaceSpec& out) {
    SubspaceSpec r;
    size_t pos = 0;
    for (int i = 0; i < DimCount; ++i) {
        DimSpec d;
        if (pos < s.size() && s[pos] == '*') {
            d = DimSpec::free();
            ++pos;
        } else {
            uint32_t lo = 0;
            if (!parseUnsigned(s, pos, lo)) return false;
            if (pos < s.size() && s[pos] == '-') {
                ++pos;
                uint32_t hi = 0;
                if (!parseUnsigned(s, pos, hi)) return false;
                if (hi < lo) return false;
                d = DimSpec::range(lo, hi);
            } else {
                d = DimSpec::fixed(lo);
            }
        }
        r.dims[static_cast<size_t>(i)] = d;
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

SubspaceSpec SubspaceSpec::fixedThrough(const Coordinate& anchor, int lastFixedDim) {
    SubspaceSpec s;
    for (int d = 0; d < DimCount; ++d)
        s.dims[static_cast<size_t>(d)] = (d <= lastFixedDim) ? DimSpec::fixed(anchor[d]) : DimSpec::free();
    return s;
}

SubspaceSpec SubspaceSpec::wholeFile() { return SubspaceSpec{}; }
SubspaceSpec SubspaceSpec::wholeVolume(const Coordinate& a) { return fixedThrough(a, DimVolume); }
SubspaceSpec SubspaceSpec::slice3D(const Coordinate& a) { return fixedThrough(a, DimBook); }
SubspaceSpec SubspaceSpec::oneChapter(const Coordinate& a) { return fixedThrough(a, DimChapter); }
SubspaceSpec SubspaceSpec::scrollLine(const Coordinate& a) { return fixedThrough(a, DimSection); }

SubspaceSpec SubspaceSpec::sameChapterEveryBook(const Coordinate& a) {
    // Fix Z arm + Collection + Volume and Chapter/Section-of... per design P5:
    // "1.1.1 / 1.1.* / 2.*.*" — Book Free, Chapter fixed, Section+Scroll Free.
    SubspaceSpec s = fixedThrough(a, DimVolume);
    s.dims[DimBook] = DimSpec::free();
    s.dims[DimChapter] = DimSpec::fixed(a[DimChapter]);
    s.dims[DimSection] = DimSpec::free();
    s.dims[DimScroll] = DimSpec::free();
    return s;
}

} // namespace phext
