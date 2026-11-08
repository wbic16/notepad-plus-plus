// PhextDocument.h — the phext model that lives BESIDE Scintilla
// (SUBSPACE-DESIGN §3.2). One per open .phext file, owned by its Buffer.
//
// It holds the backing bytes exactly as loaded (BOM recorded as a flag and
// stripped from the working bytes, restored on save) and a sparse offset
// index: one entry per scroll SLOT encountered by a single delimiter walk,
// populated or empty. Nothing here is sized by coordinate extents — cost is
// proportional to file bytes on load and to populated scrolls thereafter.
//
// The index preserves every slot, including empty ones and the trailing
// empty slot a final break creates, because splice write-back (Projection)
// must reproduce bytes outside an edited region exactly.

#pragma once

#include "phext_core.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace phext {

struct ScrollEntry {
    Coordinate coord;          // coordinate of this scroll slot
    size_t offset = 0;         // offset of the scroll's first text byte
    size_t length = 0;         // text length in bytes (0 = empty slot)
    uint8_t precedingBreak = 0; // break byte before this scroll; 0 for the first slot
    // Offset of precedingBreak in the backing bytes; == offset when
    // precedingBreak == 0, else offset - 1.
    size_t breakOffset() const { return precedingBreak ? offset - 1 : offset; }
    size_t end() const { return offset + length; }
    bool populated() const { return length > 0; }
};

class PhextDocument {
public:
    // Parse raw file bytes (single delimiter walk, O(bytes)). A UTF-8 BOM is
    // stripped and remembered. Never fails on content: any byte stream is a
    // phext (a break-free stream is one scroll at 1.1.1/1.1.1/1.1.1).
    void load(std::string_view bytes);
    // Replace the backing bytes after a successful splice; re-indexes.
    void reload(std::string_view bytes) { load(bytes); }

    const std::string& bytes() const { return _bytes; }
    bool hasBom() const { return _hasBom; }
    // Full on-disk byte stream (working bytes + BOM if it was present).
    std::string fileBytes() const { return _hasBom ? std::string("\xEF\xBB\xBF") + _bytes : _bytes; }

    const std::vector<ScrollEntry>& entries() const { return _entries; }
    size_t populatedCount() const { return _populated; }

    // True when the file ends with a structural break byte, i.e. the last
    // index entry is an empty trailing slot created by that break.
    bool endsWithBreak() const {
        return !_entries.empty() && _entries.size() > 1 && !_entries.back().populated()
            && _entries.back().precedingBreak != 0 && _entries.back().end() == _bytes.size();
    }
    uint8_t trailingBreak() const { return endsWithBreak() ? _entries.back().precedingBreak : 0; }

    // Index of the entry at `coord`, or -1. (Linear scan is fine at editor
    // scale; entries are in ascending coordinate order, so this is a
    // binary-searchable array if a caller ever needs it hotter.)
    int find(const Coordinate& coord) const;
    // Text of the scroll at `coord`, or "" when absent/empty.
    std::string fetch(const Coordinate& coord) const;
    std::string_view textOf(const ScrollEntry& e) const {
        return std::string_view(_bytes).substr(e.offset, e.length);
    }

    // Canonical serialization of an ordered (coord, text) member list,
    // starting from the coordinate in effect at the segment start
    // (libphext-rs append_scroll / dephokenize semantics: minimal break
    // sequences, empty members emit breaks only to reach later members).
    // Members must be in strictly ascending coordinate order, all >= start.
    // Returns false (out untouched) when the order precondition is violated.
    struct Member { Coordinate coord; std::string text; };
    static bool serializeSegment(const std::vector<Member>& members,
                                 const Coordinate& start, std::string& out);

private:
    std::string _bytes;                  // working bytes, BOM stripped
    bool _hasBom = false;
    std::vector<ScrollEntry> _entries;   // document order == coordinate order
    size_t _populated = 0;
};

} // namespace phext
