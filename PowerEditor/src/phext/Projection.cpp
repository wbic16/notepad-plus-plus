// Projection.cpp — see Projection.h and SUBSPACE-DESIGN §2/§3.3.

#include "Projection.h"

#include <algorithm>

namespace phext {

namespace {
const std::string kSepPrefix = "\xC2\xA7 ";        // "§ "
const std::string kSepDash = " \xE2\x80\x94 ";     // " — "
const std::string kFillUnit = "\xE2\x94\x80";      // "─"

// Split into lines (without '\n'); a trailing '\n' does not produce an
// extra empty line entry here — callers track it separately.
struct LineSpan { size_t start; size_t len; };
std::vector<LineSpan> splitLines(std::string_view s) {
    std::vector<LineSpan> out;
    size_t pos = 0;
    while (pos < s.size()) {
        size_t nl = s.find('\n', pos);
        if (nl == std::string_view::npos) { out.push_back({pos, s.size() - pos}); break; }
        out.push_back({pos, nl - pos});
        pos = nl + 1;
    }
    if (s.empty()) out.push_back({0, 0});
    return out;
}
} // namespace

std::string Projection::separatorLine(const Coordinate& coord, int breakDim) {
    std::string s = kSepPrefix + coord.toString() + kSepDash;
    if (breakDim < 0) s += "Start";
    else { s += kDimName[static_cast<size_t>(breakDim)]; s += " break"; }
    s += ' ';
    for (int i = 0; i < 14; ++i) s += kFillUnit;
    return s;
}

bool Projection::looksLikeSeparator(std::string_view line) {
    return line.size() >= kSepPrefix.size()
        && line.compare(0, kSepPrefix.size(), kSepPrefix) == 0;
}

bool Projection::parseSeparatorLine(std::string_view line, Coordinate& coordOut, int& breakDimOut) {
    if (!looksLikeSeparator(line)) return false;
    size_t dash = line.find(kSepDash, kSepPrefix.size());
    if (dash == std::string_view::npos) return false;
    if (!parseCoordinate(line.substr(kSepPrefix.size(), dash - kSepPrefix.size()), coordOut))
        return false;
    std::string_view rest = line.substr(dash + kSepDash.size());
    // Strip insignificant fill: trailing '─' units, '-' and spaces.
    while (!rest.empty()) {
        if (rest.size() >= 3 && rest.compare(rest.size() - 3, 3, kFillUnit) == 0) { rest.remove_suffix(3); continue; }
        char c = rest.back();
        if (c == ' ' || c == '-') { rest.remove_suffix(1); continue; }
        break;
    }
    if (rest == "Start") { breakDimOut = -1; return true; }
    for (int d = 0; d < DimCount; ++d) {
        std::string label = std::string(kDimName[static_cast<size_t>(d)]) + " break";
        if (rest == label) { breakDimOut = d; return true; }
    }
    return false;
}

void Projection::build(const PhextDocument& doc, const SubspaceSpec& spec) {
    _spec = spec;
    _text.clear();
    _lines.clear();
    _lineStart.clear();
    _memberEntries.clear();
    _memberCoords.clear();
    _memberBodyStart.clear();

    const std::vector<ScrollEntry>& entries = doc.entries();
    for (size_t i = 0; i < entries.size(); ++i) {
        const ScrollEntry& e = entries[i];
        if (!spec.matches(e.coord)) continue;
        // The trailing empty slot a final break creates is not a member:
        // its existence is the file's trailing break, preserved by splice.
        if (doc.endsWithBreak() && i + 1 == entries.size()) continue;

        int memberIndex = static_cast<int>(_memberEntries.size());
        _memberEntries.push_back(static_cast<int>(i));
        _memberCoords.push_back(e.coord);

        int breakDim = e.precedingBreak ? dimOfBreak(e.precedingBreak) : -1;
        std::string sep = separatorLine(e.coord, breakDim);
        ProjLineInfo sli;
        sli.memberIndex = memberIndex;
        sli.separator = true;
        sli.breakDim = breakDim;
        sli.srcOffset = e.breakOffset();
        _lineStart.push_back(_text.size());
        _lines.push_back(sli);
        _text += sep;
        _text += '\n';

        _memberBodyStart.push_back(_text.size());
        std::string_view body = doc.textOf(e);
        // Body lines: the lines of (body + joiner '\n') — one line per
        // '\n'-separated segment of the body, so line count = NL(body)+1.
        {
            size_t off = 0;
            for (;;) {
                ProjLineInfo bli;
                bli.memberIndex = memberIndex;
                bli.separator = false;
                bli.srcOffset = e.offset + off;
                _lineStart.push_back(_memberBodyStart.back() + off);
                _lines.push_back(bli);
                size_t nl = body.find('\n', off);
                if (nl == std::string_view::npos) break;
                off = nl + 1;
            }
        }
        _text += body;
        _text += '\n'; // joiner newline
    }
}

ProjLocated Projection::locate(size_t projOffset) const {
    ProjLocated r;
    if (_lines.empty()) return r;
    if (projOffset > _text.size()) projOffset = _text.size();
    // Last line whose start <= projOffset.
    size_t lo = 0, hi = _lineStart.size();
    while (lo + 1 < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (_lineStart[mid] <= projOffset) lo = mid; else hi = mid;
    }
    const ProjLineInfo& li = _lines[lo];
    r.memberIndex = li.memberIndex;
    r.separator = li.separator;
    r.breakDim = li.breakDim;
    if (li.memberIndex >= 0)
        r.coord = _memberCoords[static_cast<size_t>(li.memberIndex)];
    if (!li.separator && li.memberIndex >= 0) {
        // Body lines carry absolute backing offsets; the member's scroll
        // starts at its first body line's srcOffset.
        size_t firstBodyLine = lo;
        while (firstBodyLine > 0 && !_lines[firstBodyLine - 1].separator
               && _lines[firstBodyLine - 1].memberIndex == li.memberIndex)
            --firstBodyLine;
        r.offsetInScroll = (li.srcOffset - _lines[firstBodyLine].srcOffset)
            + (projOffset - _lineStart[lo]);
    }
    return r;
}

size_t Projection::projOffsetFor(size_t memberIndex, size_t offsetInScroll) const {
    if (memberIndex >= _memberBodyStart.size()) return _text.size();
    return _memberBodyStart[memberIndex] + offsetInScroll;
}

Projection::ParseResult Projection::parseEdited(std::string_view edited) {
    ParseResult r;
    std::vector<LineSpan> spans = splitLines(edited);
    bool haveMember = false;
    std::vector<std::string> bodyLines;
    auto closeMember = [&]() {
        if (!haveMember) return;
        std::string text;
        for (size_t i = 0; i < bodyLines.size(); ++i) {
            if (i) text += '\n';
            text += bodyLines[i];
        }
        r.members.back().text = text;
        bodyLines.clear();
    };
    for (size_t li = 0; li < spans.size(); ++li) {
        std::string_view line = edited.substr(spans[li].start, spans[li].len);
        // Tolerate a CR left by CRLF display settings on separator lines only.
        if (looksLikeSeparator(line)) {
            Coordinate c;
            int bd = -2;
            std::string_view probe = line;
            if (!probe.empty() && probe.back() == '\r') probe.remove_suffix(1);
            if (!parseSeparatorLine(probe, c, bd)) {
                r.ok = false;
                r.error = "corrupted separator line (save blocked; use Phext > Repair Separators)";
                r.errorLine = static_cast<int>(li);
                r.members.clear();
                return r;
            }
            closeMember();
            ParsedMember m;
            m.coord = c;
            m.firstLine = static_cast<int>(li);
            r.members.push_back(m);
            haveMember = true;
        } else {
            if (!haveMember) {
                if (line.empty() && spans.size() == 1) break; // empty projection
                r.ok = false;
                r.error = "text before the first separator line";
                r.errorLine = static_cast<int>(li);
                r.members.clear();
                return r;
            }
            bodyLines.push_back(std::string(line));
        }
    }
    closeMember();
    r.ok = true;
    return r;
}

Projection::SpliceResult Projection::splice(const PhextDocument& doc,
                                            const std::vector<ParsedMember>& parsedIn,
                                            bool forceCanonical) const {
    SpliceResult r;
    std::vector<ParsedMember> parsed = parsedIn;

    // Validation (§3.3 step 2): spec membership, valid coordinates, no
    // duplicates. Out-of-order moves are normalized by sorting.
    for (const ParsedMember& m : parsed) {
        if (!m.coord.valid()) { r.error = "invalid coordinate " + m.coord.toString(); return r; }
        if (!_spec.matches(m.coord)) {
            r.error = "coordinate " + m.coord.toString() + " is outside this subspace (" + _spec.toString() + ")";
            return r;
        }
    }
    std::stable_sort(parsed.begin(), parsed.end(),
        [](const ParsedMember& a, const ParsedMember& b) { return a.coord < b.coord; });
    for (size_t i = 1; i < parsed.size(); ++i) {
        if (parsed[i].coord == parsed[i - 1].coord) {
            r.error = "duplicate coordinate " + parsed[i].coord.toString();
            return r;
        }
        if (parsed[i].firstLine < parsed[i - 1].firstLine) r.reordered = true;
    }

    const std::vector<ScrollEntry>& entries = doc.entries();

    // No-edit fast path: forced save of an untouched projection is the
    // backing bytes verbatim (the round-trip guarantee, §3.3).
    if (!forceCanonical && parsed.size() == _memberEntries.size()) {
        bool same = true;
        for (size_t i = 0; i < parsed.size(); ++i) {
            const ScrollEntry& e = entries[static_cast<size_t>(_memberEntries[i])];
            if (parsed[i].coord != e.coord || parsed[i].text != doc.textOf(e)) { same = false; break; }
        }
        if (same) { r.ok = true; r.unchanged = true; r.bytes = doc.bytes(); return r; }
    }

    if (_spec.isInterval()) {
        std::vector<PhextDocument::Member> members;
        for (const ParsedMember& m : parsed) members.push_back({m.coord, m.text});

        std::string prefix, suffix, segment;
        if (!_memberEntries.empty()) {
            int firstIdx = _memberEntries.front();
            int lastIdx = _memberEntries.back();
            const ScrollEntry& first = entries[static_cast<size_t>(firstIdx)];
            const ScrollEntry& last = entries[static_cast<size_t>(lastIdx)];
            Coordinate state = (firstIdx > 0) ? entries[static_cast<size_t>(firstIdx - 1)].coord : Coordinate{};
            if (!members.empty() && members.front().coord < state) {
                r.error = "first scroll moved before the subspace region";
                return r;
            }
            if (!PhextDocument::serializeSegment(members, state, segment)) {
                r.error = "members are not reachable in coordinate order from the region start";
                return r;
            }
            prefix = doc.bytes().substr(0, first.breakOffset());
            suffix = doc.bytes().substr(last.end());
        } else if (!members.empty()) {
            // Empty subspace: insert at the first entry past the new members,
            // or append at EOF. State = coordinate in effect at that point.
            size_t k = entries.size();
            for (size_t i = 0; i < entries.size(); ++i)
                if (members.front().coord < entries[i].coord) { k = i; break; }
            if (k == entries.size()) {
                Coordinate state = entries.back().coord;
                // A trailing empty slot means the trailing break already
                // advanced the state to its coordinate; serializing from the
                // last entry's coordinate is correct in both cases.
                if (!PhextDocument::serializeSegment(members, state, segment)) {
                    r.error = "members are not reachable in coordinate order at end of file";
                    return r;
                }
                prefix = doc.bytes();
            } else {
                Coordinate state = (k > 0) ? entries[k - 1].coord : Coordinate{};
                if (!PhextDocument::serializeSegment(members, state, segment)) {
                    r.error = "members are not reachable in coordinate order at insertion point";
                    return r;
                }
                prefix = doc.bytes().substr(0, entries[k].breakOffset());
                suffix = doc.bytes().substr(entries[k].breakOffset());
            }
        }
        r.bytes = prefix + segment + suffix;
        r.ok = true;
        return r;
    }

    // Strided: phase 1 supports text edits with an unchanged coordinate set
    // (scatter per member, every break byte verbatim). Structural edits are
    // refused loudly rather than guessed.
    if (parsed.size() != _memberEntries.size()) {
        r.error = "structural edits (split/merge/delete/move) are not supported in a strided subspace in phase 1";
        return r;
    }
    for (size_t i = 0; i < parsed.size(); ++i) {
        const ScrollEntry& e = entries[static_cast<size_t>(_memberEntries[i])];
        if (parsed[i].coord != e.coord) {
            r.error = "structural edits (split/merge/delete/move) are not supported in a strided subspace in phase 1";
            return r;
        }
    }
    {
        std::string out;
        size_t memberCursor = 0;
        for (size_t i = 0; i < entries.size(); ++i) {
            const ScrollEntry& e = entries[i];
            size_t spanStart = (i == 0) ? 0 : e.breakOffset();
            bool isMember = memberCursor < _memberEntries.size()
                && _memberEntries[memberCursor] == static_cast<int>(i);
            if (isMember) {
                if (e.precedingBreak) out += static_cast<char>(e.precedingBreak);
                out += parsed[memberCursor].text;
                ++memberCursor;
            } else {
                out += doc.bytes().substr(spanStart, e.end() - spanStart);
            }
        }
        r.bytes = out;
        r.ok = true;
        return r;
    }
}

} // namespace phext
