// Projection.h — one subspace projected as one ordinary editable document
// (SUBSPACE-DESIGN §2.1, §3.3). Editor-agnostic: the projection is plain
// text plus a line/offset map; the Notepad++ wiring puts this text into a
// Scintilla document and reads it back on save.
//
// Projected text, per member scroll, in document order:
//     [separator line] "\n"
//     [scroll text, verbatim]
//     "\n"                      <- joiner newline (projection syntax, not body)
// Because exactly one joiner newline always follows a body, segmentation of
// an edited projection is unambiguous: a member's text is its body lines
// joined with '\n'. The projection contains ZERO structural phext bytes.
//
// A separator line is generated, structural, and editable-as-structure:
//     § 1.1.1/1.1.2/3.4.5 — Section break ──────────────
// Grammar: "§ " + coordinate + " — " + (DimensionName " break" | "Start")
//          + insignificant fill. Editing the coordinate is a move; deleting
// the line is a merge; a line that starts with "§ " but no longer parses is
// CORRUPTION — parse fails loudly and save is blocked, never guessed.

#pragma once

#include "PhextDocument.h"
#include "SubspaceSpec.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace phext {

struct ProjLineInfo {
    int memberIndex = -1;   // index into Projection::memberEntries()
    bool separator = false;
    int breakDim = -1;      // separator only: dimension of the preceding break; -1 = Start
    size_t srcOffset = 0;   // backing-file offset this line starts at
                            // (separator: its break's offset; body: text offset)
};

struct ProjLocated {
    Coordinate coord;          // coordinate of the scroll at this offset
    bool separator = false;    // offset is inside a separator line
    int breakDim = -1;         // separator only
    size_t offsetInScroll = 0; // body only: byte offset inside the scroll
    int memberIndex = -1;
};

class Projection {
public:
    void build(const PhextDocument& doc, const SubspaceSpec& spec);

    const SubspaceSpec& spec() const { return _spec; }
    const std::string& text() const { return _text; }
    const std::vector<ProjLineInfo>& lines() const { return _lines; }
    // Indices into PhextDocument::entries() for each projected member.
    const std::vector<int>& memberEntries() const { return _memberEntries; }
    size_t memberCount() const { return _memberEntries.size(); }

    // Bidirectional offset map (§2.1 line map).
    ProjLocated locate(size_t projOffset) const;
    // Projection offset of (member, offset-in-scroll); body start when 0.
    size_t projOffsetFor(size_t memberIndex, size_t offsetInScroll) const;

    // --- Separator lines ---
    static std::string separatorLine(const Coordinate& coord, int breakDim);
    // Parse a separator line. breakDimOut: dimension, or -1 for "Start".
    static bool parseSeparatorLine(std::string_view line, Coordinate& coordOut, int& breakDimOut);
    static bool looksLikeSeparator(std::string_view line); // starts with "§ "

    // --- Parse an edited projection back into members ---
    struct ParsedMember {
        Coordinate coord;
        std::string text;
        int firstLine = 0; // 0-based projection line of its separator
    };
    struct ParseResult {
        bool ok = false;
        std::string error;   // set when !ok
        int errorLine = -1;  // 0-based line that failed, when !ok
        std::vector<ParsedMember> members;
    };
    static ParseResult parseEdited(std::string_view edited);

    // --- Splice edited members back into the full document (§3.3) ---
    struct SpliceResult {
        bool ok = false;
        std::string error;
        std::string bytes;    // new backing bytes (working bytes, no BOM)
        bool unchanged = false; // parsed members identical to the originals
        bool reordered = false; // members were sorted into coordinate order
    };
    // `doc` must be the document this projection was built from.
    // forceCanonical skips the no-edit fast path (tests use it to prove the
    // canonical splice itself preserves the coordinate->text mapping).
    SpliceResult splice(const PhextDocument& doc,
                        const std::vector<ParsedMember>& parsed,
                        bool forceCanonical = false) const;

private:
    SubspaceSpec _spec;
    std::string _text;
    std::vector<ProjLineInfo> _lines;
    std::vector<size_t> _lineStart;      // projection offset of each line
    std::vector<int> _memberEntries;
    std::vector<Coordinate> _memberCoords; // coordinate per member, at build time
    std::vector<size_t> _memberBodyStart; // projection offset of each member's body
};

} // namespace phext
