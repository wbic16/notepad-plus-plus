// PhextDocument.cpp — see PhextDocument.h.
//
// The delimiter walk matches libphext-rs v0.5.0 phokenize / the workspace
// dead-reckoning parser (~/workspace/phext/parse_phext.py): walk the bytes
// once; a structural break closes the current slot and advances the walking
// coordinate (increment that dimension, reset all lower dimensions to 1).

#include "PhextDocument.h"

namespace phext {

void PhextDocument::load(std::string_view bytes) {
    _hasBom = bytes.size() >= 3
        && static_cast<uint8_t>(bytes[0]) == 0xEF
        && static_cast<uint8_t>(bytes[1]) == 0xBB
        && static_cast<uint8_t>(bytes[2]) == 0xBF;
    if (_hasBom) bytes.remove_prefix(3);
    _bytes.assign(bytes);
    _entries.clear();
    _populated = 0;

    Coordinate coord; // 1.1.1/1.1.1/1.1.1
    size_t segStart = 0;
    uint8_t preceding = 0;
    const size_t n = _bytes.size();
    for (size_t i = 0; i < n; ++i) {
        uint8_t b = static_cast<uint8_t>(_bytes[i]);
        int d = dimOfBreak(b);
        if (d < 0) continue;
        ScrollEntry e;
        e.coord = coord;
        e.offset = segStart;
        e.length = i - segStart;
        e.precedingBreak = preceding;
        if (e.populated()) ++_populated;
        _entries.push_back(e);
        applyBreak(coord, d);
        segStart = i + 1;
        preceding = b;
    }
    ScrollEntry last;
    last.coord = coord;
    last.offset = segStart;
    last.length = n - segStart;
    last.precedingBreak = preceding;
    if (last.populated()) ++_populated;
    _entries.push_back(last);
}

int PhextDocument::find(const Coordinate& coord) const {
    // Entries are stored in ascending coordinate order.
    size_t lo = 0, hi = _entries.size();
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = compare(_entries[mid].coord, coord);
        if (c == 0) return static_cast<int>(mid);
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return -1;
}

std::string PhextDocument::fetch(const Coordinate& coord) const {
    int i = find(coord);
    if (i < 0) return std::string();
    return std::string(textOf(_entries[static_cast<size_t>(i)]));
}

bool PhextDocument::serializeSegment(const std::vector<Member>& members,
                                     const Coordinate& start, std::string& out) {
    std::string result;
    Coordinate cur = start;
    for (size_t i = 0; i < members.size(); ++i) {
        if (i && !(members[i - 1].coord < members[i].coord)) return false;
        if (!emitBreaksTo(result, cur, members[i].coord)) return false;
        result += members[i].text;
    }
    out = result;
    return true;
}

} // namespace phext
