// test_phext.cpp — standalone acceptance tests for the editor-agnostic
// phext subspace core (PhextDocument / SubspaceSpec / Projection).
//
// Build & run (no Windows dependencies):
//   g++ -std=c++17 -O2 -I.. test_phext.cpp ../PhextDocument.cpp
//       ../SubspaceSpec.cpp ../Projection.cpp -o test_phext
//   ./test_phext [corpus.phext ...]
//
// Covers the editor-agnostic subset of SUBSPACE-DESIGN §7: byte-identity
// round-trips on real corpora, region-confined edits, split/merge/delete,
// separator-corruption blocking, coordinate moves, strided text scatter,
// sparsity, the offset map, and sparse-performance timings.

#include "PhextDocument.h"
#include "Projection.h"
#include "SubspaceSpec.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace phext;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond) do { if (cond) { ++g_pass; } else { ++g_fail; \
    std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_MSG(cond, msg) do { if (cond) { ++g_pass; } else { ++g_fail; \
    std::printf("FAIL %s:%d: %s (%s)\n", __FILE__, __LINE__, #cond, msg); } } while (0)

static Coordinate C(const char* s) { Coordinate c; if (!parseCoordinate(s, c)) { std::printf("bad coord %s\n", s); ++g_fail; } return c; }
static std::string readFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss; ss << f.rdbuf(); return ss.str();
}
static std::string makeDoc(std::vector<PhextDocument::Member> members) {
    std::string out;
    if (!PhextDocument::serializeSegment(members, Coordinate{}, out)) { std::printf("makeDoc failed\n"); ++g_fail; }
    return out;
}
// Re-parse bytes into a coord->text map (non-empty + empty slots, minus a
// trailing empty slot) for mapping-equality comparisons.
static std::vector<PhextDocument::Member> mappingOf(const std::string& bytes) {
    PhextDocument d; d.load(bytes);
    std::vector<PhextDocument::Member> m;
    for (size_t i = 0; i < d.entries().size(); ++i) {
        if (d.endsWithBreak() && i + 1 == d.entries().size()) break;
        m.push_back({d.entries()[i].coord, std::string(d.textOf(d.entries()[i]))});
    }
    return m;
}
static bool mappingEqual(const std::vector<PhextDocument::Member>& a,
                         const std::vector<PhextDocument::Member>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].coord != b[i].coord || a[i].text != b[i].text) return false;
    return true;
}

static std::string synthetic() {
    return makeDoc({
        {C("1.1.1/1.1.1/1.1.1"), "alpha one\nline two"},
        {C("1.1.1/1.1.1/1.1.2"), "alpha two"},
        {C("1.1.1/1.1.1/1.2.1"), "beta one"},
        {C("1.1.1/1.1.1/2.1.1"), "chapter two opens"},
        {C("1.1.1/1.1.1/2.1.2"), "chapter two second"},
        {C("1.1.1/1.1.2/1.1.1"), "book two text"},
        {C("1.1.1/1.2.1/1.1.1"), "volume two text"},
    });
}

static void testSpec() {
    SubspaceSpec s;
    CHECK(SubspaceSpec::parse("1.1.1/1.2.1/2.*.*", s));
    CHECK(s.toString() == "1.1.1/1.2.1/2.*.*");
    CHECK(s.isInterval());
    CHECK(s.matches(C("1.1.1/1.2.1/2.7.9")));
    CHECK(!s.matches(C("1.1.1/1.2.2/2.7.9")));
    CHECK(SubspaceSpec::parse("1.1.1/1.1.1-3/*.*.*", s));
    CHECK(s.matches(C("1.1.1/1.1.2/9.9.9")));
    CHECK(!s.matches(C("1.1.1/1.1.4/9.9.9")));
    CHECK(!SubspaceSpec::parse("1.1.1/1.1.0/*.*.*", s));
    CHECK(!SubspaceSpec::parse("1.1/1.1.1/1.1.1", s));
    CHECK(!SubspaceSpec::parse("3-1.1.1/1.1.1/1.1.1", s));
    Coordinate a = C("1.1.1/1.1.2/3.4.5");
    CHECK(SubspaceSpec::wholeFile().isInterval());
    CHECK(SubspaceSpec::wholeVolume(a).toString() == "1.1.1/1.1.*/*.*.*");
    CHECK(SubspaceSpec::slice3D(a).toString() == "1.1.1/1.1.2/*.*.*");
    CHECK(SubspaceSpec::oneChapter(a).toString() == "1.1.1/1.1.2/3.*.*");
    CHECK(SubspaceSpec::scrollLine(a).toString() == "1.1.1/1.1.2/3.4.*");
    SubspaceSpec p5 = SubspaceSpec::sameChapterEveryBook(a);
    CHECK(p5.toString() == "1.1.1/1.1.*/3.*.*");
    CHECK(!p5.isInterval()); // strided
}

static void testRoundTripBytes(const std::string& bytes, const char* label) {
    PhextDocument doc; doc.load(bytes);
    Projection proj; proj.build(doc, SubspaceSpec::wholeFile());
    // Projection must contain zero structural bytes (§7 test 6 invariant).
    bool clean = true;
    for (char ch : proj.text()) if (isDimensionBreak(static_cast<uint8_t>(ch))) clean = false;
    CHECK_MSG(clean, label);
    auto parsed = Projection::parseEdited(proj.text());
    CHECK_MSG(parsed.ok, label);
    auto sp = proj.splice(doc, parsed.members);
    CHECK_MSG(sp.ok, label);
    CHECK_MSG(sp.bytes == doc.bytes(), label); // byte identity, untouched
    // Forced-canonical splice must preserve the coordinate->text mapping.
    auto spc = proj.splice(doc, parsed.members, true);
    CHECK_MSG(spc.ok, label);
    CHECK_MSG(mappingEqual(mappingOf(spc.bytes), mappingOf(doc.bytes())), label);
}

static void testSynthetic() {
    std::string bytes = synthetic();
    PhextDocument doc; doc.load(bytes);
    CHECK(doc.populatedCount() == 7);
    CHECK(doc.fetch(C("1.1.1/1.1.1/2.1.2")) == "chapter two second");
    CHECK(doc.fetch(C("9.9.9/9.9.9/9.9.9")) == "");
    testRoundTripBytes(bytes, "synthetic");
}

static void testRegionConfinedEdit() {
    std::string bytes = synthetic();
    PhextDocument doc; doc.load(bytes);
    Coordinate anchor = C("1.1.1/1.1.1/2.1.1");
    Projection proj; proj.build(doc, SubspaceSpec::oneChapter(anchor)); // chapter 2 of book 1
    CHECK(proj.memberCount() == 2);
    auto parsed = Projection::parseEdited(proj.text());
    CHECK(parsed.ok);
    parsed.members[0].text = "chapter two opens EDITED";
    auto sp = proj.splice(doc, parsed.members);
    CHECK(sp.ok);
    // Bytes before the region and after it are identical (§7 test 2).
    int firstIdx = proj.memberEntries().front();
    int lastIdx = proj.memberEntries().back();
    size_t regionStart = doc.entries()[static_cast<size_t>(firstIdx)].breakOffset();
    size_t regionEnd = doc.entries()[static_cast<size_t>(lastIdx)].end();
    CHECK(sp.bytes.compare(0, regionStart, bytes, 0, regionStart) == 0);
    CHECK(sp.bytes.substr(sp.bytes.size() - (bytes.size() - regionEnd)) == bytes.substr(regionEnd));
    PhextDocument after; after.load(sp.bytes);
    CHECK(after.fetch(C("1.1.1/1.1.1/2.1.1")) == "chapter two opens EDITED");
    CHECK(after.fetch(C("1.1.1/1.1.2/1.1.1")) == "book two text");
}

static std::string replaceFirst(std::string s, const std::string& from, const std::string& to) {
    size_t p = s.find(from); if (p != std::string::npos) s.replace(p, from.size(), to); return s;
}

static void testSplitMergeDelete() {
    std::string bytes = synthetic();
    PhextDocument doc; doc.load(bytes);
    Projection proj; proj.build(doc, SubspaceSpec::scrollLine(C("1.1.1/1.1.1/1.1.1")));
    CHECK(proj.memberCount() == 2);

    // Split: insert a new separator (coordinate .../1.1.2) inside member 1's
    // body — equivalent to the Split command's document edit.
    std::string edited = proj.text();
    std::string sep = Projection::separatorLine(C("1.1.1/1.1.1/1.1.2"), DimScroll);
    edited = replaceFirst(edited, "alpha one\nline two\n",
        "alpha one\n" + sep + "\ninserted middle\n");
    // Renumber the old second scroll's separator to 1.1.3.
    std::string oldSep = Projection::separatorLine(C("1.1.1/1.1.1/1.1.2"), DimScroll);
    std::string newSep = Projection::separatorLine(C("1.1.1/1.1.1/1.1.3"), DimScroll);
    // The FIRST occurrence of oldSep is the one we inserted; find the second.
    size_t first = edited.find(oldSep);
    size_t second = edited.find(oldSep, first + 1);
    CHECK(second != std::string::npos);
    if (second != std::string::npos) edited.replace(second, oldSep.size(), newSep);
    auto parsed = Projection::parseEdited(edited);
    CHECK(parsed.ok);
    CHECK(parsed.members.size() == 3);
    auto sp = proj.splice(doc, parsed.members);
    CHECK_MSG(sp.ok, sp.error.c_str());
    PhextDocument after; after.load(sp.bytes);
    CHECK(after.fetch(C("1.1.1/1.1.1/1.1.1")) == "alpha one");
    CHECK(after.fetch(C("1.1.1/1.1.1/1.1.2")) == "inserted middle");
    CHECK(after.fetch(C("1.1.1/1.1.1/1.1.3")) == "alpha two");

    // Merge: delete the separator between the two members of a fresh projection.
    Projection proj2; proj2.build(doc, SubspaceSpec::scrollLine(C("1.1.1/1.1.1/1.1.1")));
    std::string merged = proj2.text();
    std::string sep2 = Projection::separatorLine(C("1.1.1/1.1.1/1.1.2"), DimScroll) + "\n";
    merged = replaceFirst(merged, sep2, "");
    auto pm = Projection::parseEdited(merged);
    CHECK(pm.ok);
    CHECK(pm.members.size() == 1);
    auto sm = proj2.splice(doc, pm.members);
    CHECK_MSG(sm.ok, sm.error.c_str());
    PhextDocument afterM; afterM.load(sm.bytes);
    // The splice inserts nothing: the bodies remain on the separate lines
    // the user left them on, so the merged scroll keeps that line break.
    CHECK(afterM.fetch(C("1.1.1/1.1.1/1.1.1")) == "alpha one\nline two\nalpha two");
    // Byte-exact concatenation (design §2.4) is the same gesture with the
    // joiner newline also deleted: lower body joins upper's last line.
    std::string mergedExact = proj2.text();
    mergedExact = replaceFirst(mergedExact, "\n" + sep2, ""); // sep2 includes its '\n'
    auto pme = Projection::parseEdited(mergedExact);
    CHECK(pme.ok && pme.members.size() == 1);
    auto sme = proj2.splice(doc, pme.members);
    CHECK_MSG(sme.ok, sme.error.c_str());
    PhextDocument afterME; afterME.load(sme.bytes);
    CHECK(afterME.fetch(C("1.1.1/1.1.1/1.1.1")) == "alpha one\nline twoalpha two");

    // Delete: drop the second member entirely (separator + body + joiner).
    Projection proj3; proj3.build(doc, SubspaceSpec::scrollLine(C("1.1.1/1.1.1/1.1.1")));
    std::string del = proj3.text();
    size_t pos = del.find(Projection::separatorLine(C("1.1.1/1.1.1/1.1.2"), DimScroll));
    CHECK(pos != std::string::npos);
    if (pos != std::string::npos) del.erase(pos);
    auto pd = Projection::parseEdited(del);
    CHECK(pd.ok && pd.members.size() == 1);
    auto sd = proj3.splice(doc, pd.members);
    CHECK_MSG(sd.ok, sd.error.c_str());
    PhextDocument afterD; afterD.load(sd.bytes);
    CHECK(afterD.fetch(C("1.1.1/1.1.1/1.1.1")) == "alpha one\nline two");
    CHECK(afterD.fetch(C("1.1.1/1.1.1/1.2.1")) == "beta one"); // next section intact
}

static void testCorruptionAndMoves() {
    std::string bytes = synthetic();
    PhextDocument doc; doc.load(bytes);
    Projection proj; proj.build(doc, SubspaceSpec::scrollLine(C("1.1.1/1.1.1/1.1.1")));

    // Corrupted separator: save is blocked, never guessed.
    std::string bad = replaceFirst(proj.text(),
        Projection::separatorLine(C("1.1.1/1.1.1/1.1.2"), DimScroll),
        "\xC2\xA7 not a coordinate at all");
    auto pb = Projection::parseEdited(bad);
    CHECK(!pb.ok);
    CHECK(pb.errorLine >= 0);

    // Move within the subspace via separator edit (1.1.2 -> 1.1.5).
    std::string moved = replaceFirst(proj.text(),
        Projection::separatorLine(C("1.1.1/1.1.1/1.1.2"), DimScroll),
        Projection::separatorLine(C("1.1.1/1.1.1/1.1.5"), DimScroll));
    auto pmv = Projection::parseEdited(moved);
    CHECK(pmv.ok);
    auto smv = proj.splice(doc, pmv.members);
    CHECK_MSG(smv.ok, smv.error.c_str());
    PhextDocument after; after.load(smv.bytes);
    CHECK(after.fetch(C("1.1.1/1.1.1/1.1.5")) == "alpha two");
    CHECK(after.fetch(C("1.1.1/1.1.1/1.1.2")) == "");

    // Move OUT of the subspace is blocked.
    std::string out = replaceFirst(proj.text(),
        Projection::separatorLine(C("1.1.1/1.1.1/1.1.2"), DimScroll),
        Projection::separatorLine(C("1.1.1/1.1.1/9.9.9"), DimScroll));
    auto po = Projection::parseEdited(out);
    CHECK(po.ok);
    auto so = proj.splice(doc, po.members);
    CHECK(!so.ok);
}

static void testStrided() {
    // Two books, each with chapters 1 and 2 -> P5 picks chapter 2 of both.
    std::string bytes = makeDoc({
        {C("1.1.1/1.1.1/1.1.1"), "b1 c1"},
        {C("1.1.1/1.1.1/2.1.1"), "b1 c2"},
        {C("1.1.1/1.1.2/1.1.1"), "b2 c1"},
        {C("1.1.1/1.1.2/2.1.1"), "b2 c2"},
    });
    PhextDocument doc; doc.load(bytes);
    Projection proj; proj.build(doc, SubspaceSpec::sameChapterEveryBook(C("1.1.1/1.1.1/2.1.1")));
    CHECK(proj.memberCount() == 2);
    auto parsed = Projection::parseEdited(proj.text());
    CHECK(parsed.ok);
    parsed.members[1].text = "b2 c2 EDITED";
    auto sp = proj.splice(doc, parsed.members);
    CHECK_MSG(sp.ok, sp.error.c_str());
    PhextDocument after; after.load(sp.bytes);
    CHECK(after.fetch(C("1.1.1/1.1.2/2.1.1")) == "b2 c2 EDITED");
    CHECK(after.fetch(C("1.1.1/1.1.2/1.1.1")) == "b2 c1"); // non-member untouched
    // Structural edit in strided view is refused.
    parsed.members.pop_back();
    auto sp2 = proj.splice(doc, parsed.members);
    CHECK(!sp2.ok);
}

static void testOffsetMapAndSparse() {
    std::string bytes = synthetic();
    PhextDocument doc; doc.load(bytes);
    Projection proj; proj.build(doc, SubspaceSpec::wholeFile());
    size_t off = proj.projOffsetFor(1, 3); // member 1 = "alpha two", +3
    ProjLocated loc = proj.locate(off);
    CHECK(!loc.separator);
    CHECK(loc.coord == C("1.1.1/1.1.1/1.1.2"));
    CHECK(loc.offsetInScroll == 3);
    ProjLocated sepLoc = proj.locate(proj.projOffsetFor(1, 0) - 2);
    CHECK(sepLoc.separator);
    CHECK(sepLoc.coord == C("1.1.1/1.1.1/1.1.2"));

    // Sparsity (§7 test 5): one far-away scroll; cost follows populated count.
    std::string far = makeDoc({
        {C("1.1.1/1.1.1/1.1.1"), "home"},
        {C("100.200.300/400.500.600/700.800.900"), "far away"},
    });
    PhextDocument fdoc; fdoc.load(far);
    CHECK(fdoc.populatedCount() == 2);
    CHECK(fdoc.fetch(C("100.200.300/400.500.600/700.800.900")) == "far away");
    testRoundTripBytes(far, "sparse-far");
}

static void testTrailingBreakAndEmpty() {
    std::string bytes = synthetic() + "\x17"; // trailing scroll break
    PhextDocument doc; doc.load(bytes);
    CHECK(doc.endsWithBreak());
    testRoundTripBytes(bytes, "trailing-break");
    std::string empty;
    PhextDocument edoc; edoc.load(empty);
    CHECK(edoc.populatedCount() == 0);
    testRoundTripBytes(empty, "empty");
    // Insert into an empty subspace of an existing file.
    PhextDocument doc2; doc2.load(synthetic());
    SubspaceSpec spec; CHECK(SubspaceSpec::parse("1.1.1/1.1.1/5.*.*", spec));
    Projection proj; proj.build(doc2, spec);
    CHECK(proj.memberCount() == 0);
    std::vector<Projection::ParsedMember> nm = {{C("1.1.1/1.1.1/5.1.1"), "brand new chapter", 0}};
    auto sp = proj.splice(doc2, nm);
    CHECK_MSG(sp.ok, sp.error.c_str());
    PhextDocument after; after.load(sp.bytes);
    CHECK(after.fetch(C("1.1.1/1.1.1/5.1.1")) == "brand new chapter");
    CHECK(after.fetch(C("1.1.1/1.1.2/1.1.1")) == "book two text");
}

static void perfCorpus(const char* path) {
    std::string bytes = readFile(path);
    if (bytes.empty()) { std::printf("perf: %s not readable, skipped\n", path); return; }
    using clk = std::chrono::steady_clock;
    auto t0 = clk::now();
    PhextDocument doc; doc.load(bytes);
    auto t1 = clk::now();
    Projection proj; proj.build(doc, SubspaceSpec::wholeFile());
    auto t2 = clk::now();
    auto parsed = Projection::parseEdited(proj.text());
    auto sp = proj.splice(doc, parsed.members, true);
    auto t3 = clk::now();
    auto ms = [](auto a, auto b) { return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count(); };
    std::printf("perf %s: %zu bytes, %zu entries, %zu populated | load+index %lld ms, "
                "project %lld ms, parse+canonical-splice %lld ms, roundtrip-bytes-equal=%d mapping-equal=%d\n",
        path, bytes.size(), doc.entries().size(), doc.populatedCount(),
        (long long)ms(t0, t1), (long long)ms(t1, t2), (long long)ms(t2, t3),
        (int)(sp.ok && sp.bytes == doc.bytes()),
        (int)(sp.ok && mappingEqual(mappingOf(sp.bytes), mappingOf(doc.bytes()))));
    CHECK(parsed.ok);
    CHECK(sp.ok);
    CHECK(mappingEqual(mappingOf(sp.bytes), mappingOf(doc.bytes())));
}

int main(int argc, char** argv) {
    testSpec();
    testSynthetic();
    testRegionConfinedEdit();
    testSplitMergeDelete();
    testCorruptionAndMoves();
    testStrided();
    testOffsetMapAndSparse();
    testTrailingBreakAndEmpty();
    for (int i = 1; i < argc; ++i) {
        std::string bytes = readFile(argv[i]);
        if (bytes.empty()) { std::printf("corpus %s not readable, skipped\n", argv[i]); continue; }
        testRoundTripBytes(bytes, argv[i]);
        perfCorpus(argv[i]);
    }
    std::printf("test_phext: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
