// 假 back end 遵守 port 契約（IPatternSearcher.h、ITemplateBuilder.h 裡寫的那份）。
// S09 會把同一組斷言泛化成「對每個 back end 跑」的契約測試；這裡先對假 back end 跑，
// 因為 use case 的測試要靠它，它自己錯了，其他測試就都沒有意義。
#include <QtTest>
#include <algorithm>
#include <thread>

#include "CoreTestSupport.h"

using namespace aoi;
using namespace aoi::test;

namespace {

const Rect kSample{380.0, 285.0, 40.0, 30.0};

const TestImage& image() {
    static const TestImage img(800, 600);
    return img;
}

fake::FakeSpec spec() {
    fake::FakeSpec s;
    s.halfCols = 4;
    s.halfRows = 2;
    s.weakFraction = 0.25;
    s.seed = 7;
    return s;
}

TemplatePtr buildTemplate(const fake::FakeSpec& s) {
    fake::FakeTemplateBuilder builder(s);
    Result<TemplatePtr> t = builder.build(image().view(), kSample, SearchParams{}, CancelToken());
    return t.ok() ? t.value() : TemplatePtr();
}

Result<std::vector<Match>> find(const fake::FakeSpec& s, const SearchRegion& region, SearchParams params,
                                const CancelToken& cancel = {}) {
    const TemplatePtr t = buildTemplate(s);
    fake::FakeSearcher searcher(s);
    return searcher.find(image().view(), *t, region, params, cancel);
}

SearchParams everything() {
    SearchParams p;
    p.minScore = 0.0;
    p.angleMinDeg = -180.0;
    p.angleMaxDeg = 180.0;
    return p;
}

}  // namespace

class TstFakeBackend : public QObject {
    Q_OBJECT

private slots:
    void theBuilderMakesATemplateTheSizeOfTheSample() {
        fake::FakeTemplateBuilder builder(spec());
        QVERIFY(builder.backendId() == "fake");
        const Result<TemplatePtr> t = builder.build(image().view(), kSample, SearchParams{}, CancelToken());
        QVERIFY(t.ok());
        QVERIFY(approxEqual(t.value()->size().w, 40.0) && approxEqual(t.value()->size().h, 30.0));
        QVERIFY(t.value()->backendId() == "fake");
        QVERIFY(!t.value()->sampleProblem().has_value());
    }

    void theBuilderClampsASampleThatSticksOutOfTheImage() {
        fake::FakeTemplateBuilder builder(spec());
        const Result<TemplatePtr> t = builder.build(image().view(), {780.0, 580.0, 40.0, 40.0}, SearchParams{}, CancelToken());
        QVERIFY(t.ok());
        QVERIFY(approxEqual(t.value()->size().w, 20.0) && approxEqual(t.value()->size().h, 20.0));
    }

    void theBuilderRefusesBadInput() {
        fake::FakeTemplateBuilder builder(spec());
        const Result<TemplatePtr> noImage = builder.build(ImageView{}, kSample, SearchParams{}, CancelToken());
        QVERIFY(!noImage.ok() && noImage.error().code == ErrorCode::InvalidArgument);
        const Result<TemplatePtr> outside = builder.build(image().view(), {5000.0, 5000.0, 10.0, 10.0}, SearchParams{}, CancelToken());
        QVERIFY(!outside.ok() && outside.error().code == ErrorCode::InvalidArgument);
        CancelSource source;
        source.cancel();
        const Result<TemplatePtr> cancelled = builder.build(image().view(), kSample, SearchParams{}, source.token());
        QVERIFY(!cancelled.ok() && cancelled.error().code == ErrorCode::Cancelled);
    }

    void aWholeImageSearchReturnsEveryPartSortedByScore() {
        const fake::FakeSpec s = spec();
        const Result<std::vector<Match>> r = find(s, SearchRegion::wholeImage(), everything());
        QVERIFY(r.ok());
        std::size_t present = 0;
        for (const fake::FakeCell& c : fake::makeFakeGrid(s, kSample, 800, 600)) present += c.kind != fake::FakeCellKind::Missing ? 1u : 0u;
        QCOMPARE(r.value().size(), present);
        for (std::size_t i = 1; i < r.value().size(); ++i) {
            const Match& a = r.value()[i - 1];
            const Match& b = r.value()[i];
            QVERIFY2(a.score > b.score || (a.score == b.score && (a.pose.y < b.pose.y || (a.pose.y == b.pose.y && a.pose.x <= b.pose.x))),
                     "matches must be sorted by score, then y, then x");
        }
        for (const Match& m : r.value()) QVERIFY(m.score >= 0.0 && m.score <= 1.0);
    }

    void minScoreIsAHardFloor() {
        const fake::FakeSpec s = spec();
        SearchParams p = everything();
        p.minScore = 0.80;
        const Result<std::vector<Match>> r = find(s, SearchRegion::wholeImage(), p);
        QVERIFY(r.ok());
        QVERIFY(!r.value().empty());
        for (const Match& m : r.value()) QVERIFY(m.score >= 0.80);
        std::size_t expected = 0;
        for (const fake::FakeCell& c : fake::makeFakeGrid(s, kSample, 800, 600)) expected += (c.kind != fake::FakeCellKind::Missing && c.score >= 0.80) ? 1u : 0u;
        QCOMPARE(r.value().size(), expected);
    }

    // 契約：視窗搜尋只掃視窗 —— 匹配的中心都在某個視窗內，視窗外的零件一個都不能出現。
    void aWindowSearchReturnsOnlyPartsInsideTheWindows() {
        const fake::FakeSpec s = spec();
        const std::vector<Rect> windows{{150.0, 200.0, 130.0, 100.0}, {500.0, 250.0, 100.0, 90.0}};
        const Result<std::vector<Match>> r = find(s, SearchRegion::inWindows(windows), everything());
        QVERIFY(r.ok());
        QVERIFY(!r.value().empty());
        for (const Match& m : r.value()) {
            const bool inside = std::any_of(windows.begin(), windows.end(), [&](const Rect& w) { return w.contains(m.pose.position()); });
            QVERIFY2(inside, "a match outside every window breaks the contract");
        }
        std::size_t expected = 0;
        for (const fake::FakeCell& c : fake::makeFakeGrid(s, kSample, 800, 600)) {
            if (c.kind == fake::FakeCellKind::Missing) continue;
            expected += std::any_of(windows.begin(), windows.end(), [&](const Rect& w) { return w.contains(c.pose.position()); }) ? 1u : 0u;
        }
        QCOMPARE(r.value().size(), expected);
    }

    void aSearchWithNoWindowsFindsNothingAndThatIsNotAnError() {
        const Result<std::vector<Match>> r = find(spec(), SearchRegion::inWindows({}), everything());
        QVERIFY(r.ok());
        QVERIFY(r.value().empty());
    }

    void theAngleRangeFiltersParts() {
        fake::FakeSpec s = spec();
        s.gridAngleDeg = 20.0;
        SearchParams narrow = everything();
        narrow.angleMinDeg = -5.0;
        narrow.angleMaxDeg = 5.0;
        const Result<std::vector<Match>> none = find(s, SearchRegion::wholeImage(), narrow);
        QVERIFY(none.ok());
        QVERIFY2(none.value().empty(), "parts at 20 degrees are outside a -5..5 range");
        SearchParams wide = everything();
        wide.angleMinDeg = 10.0;
        wide.angleMaxDeg = 30.0;
        const Result<std::vector<Match>> all = find(s, SearchRegion::wholeImage(), wide);
        QVERIFY(all.ok());
        QVERIFY(!all.value().empty());
        for (const Match& m : all.value()) QVERIFY(approxEqual(m.pose.angleDeg, 20.0));
    }

    void maxMatchesKeepsTheBestOnes() {
        SearchParams p = everything();
        p.maxMatches = 5;
        const Result<std::vector<Match>> r = find(spec(), SearchRegion::wholeImage(), p);
        QVERIFY(r.ok());
        QCOMPARE(r.value().size(), std::size_t{5});
        const Result<std::vector<Match>> all = find(spec(), SearchRegion::wholeImage(), everything());
        for (std::size_t i = 0; i < 5; ++i) QCOMPARE(r.value()[i].score, all.value()[i].score);
    }

    void aTemplateFromAnotherBackendIsRefused() {
        struct Foreign final : ITemplate {
            Size2D size() const noexcept override { return {1.0, 1.0}; }
            std::string_view backendId() const noexcept override { return "fake"; }  // 連名字都冒用：型別檢查要擋下
        } foreign;
        fake::FakeSearcher searcher(spec());
        const Result<std::vector<Match>> r = searcher.find(image().view(), foreign, SearchRegion::wholeImage(), everything(), CancelToken());
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::BackendMismatch);
    }

    void badSearchInputIsAnErrorNotAnException() {
        const TemplatePtr t = buildTemplate(spec());
        fake::FakeSearcher searcher(spec());
        const Result<std::vector<Match>> noImage = searcher.find(ImageView{}, *t, SearchRegion::wholeImage(), everything(), CancelToken());
        QVERIFY(!noImage.ok() && noImage.error().code == ErrorCode::InvalidArgument);
        SearchParams bad = everything();
        bad.minScore = 1.5;
        const Result<std::vector<Match>> tooHigh = searcher.find(image().view(), *t, SearchRegion::wholeImage(), bad, CancelToken());
        QVERIFY(!tooHigh.ok() && tooHigh.error().code == ErrorCode::InvalidArgument);
        bad.minScore = std::nan("");
        const Result<std::vector<Match>> nan = searcher.find(image().view(), *t, SearchRegion::wholeImage(), bad, CancelToken());
        QVERIFY(!nan.ok() && nan.error().code == ErrorCode::InvalidArgument);
    }

    void aRaisedCancelFlagStopsTheSearch() {
        CancelSource source;
        source.cancel();
        const Result<std::vector<Match>> r = find(spec(), SearchRegion::wholeImage(), everything(), source.token());
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::Cancelled);
    }

    // 契約：可以被多個 worker 同時呼叫。四個執行緒共用同一個搜尋器與範本，結果必須完全相同。
    void severalThreadsCanSearchWithTheSameObjects() {
        const TemplatePtr t = buildTemplate(spec());
        fake::FakeSearcher searcher(spec());
        const Result<std::vector<Match>> reference = searcher.find(image().view(), *t, SearchRegion::wholeImage(), everything(), CancelToken());
        QVERIFY(reference.ok());
        std::atomic<int> same{0};
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back([&] {
                for (int k = 0; k < 50; ++k) {
                    const Result<std::vector<Match>> r = searcher.find(image().view(), *t, SearchRegion::wholeImage(), everything(), CancelToken());
                    if (r.ok() && r.value().size() == reference.value().size() &&
                        std::equal(r.value().begin(), r.value().end(), reference.value().begin(),
                                   [](const Match& a, const Match& b) { return a.score == b.score && a.pose.x == b.pose.x && a.pose.y == b.pose.y; })) {
                        ++same;
                    }
                }
            });
        }
        for (std::thread& th : threads) th.join();
        QCOMPARE(same.load(), 200);
    }

    void theWorldDoesNotDependOnWhoAsks() {
        const fake::FakeSpec s = spec();
        const auto a = fake::makeFakeGrid(s, kSample, 800, 600);
        const auto b = fake::makeFakeGrid(s, kSample, 800, 600);
        QCOMPARE(a.size(), b.size());
        for (std::size_t i = 0; i < a.size(); ++i) {
            QVERIFY(a[i].kind == b[i].kind);
            QCOMPARE(a[i].score, b[i].score);
            QCOMPARE(a[i].pose.x, b[i].pose.x);
        }
        const auto offCenter = fake::makeFakeGrid(s, {100.0, 100.0, 40.0, 30.0}, 800, 600);
        QVERIFY2(offCenter.size() != a.size() || offCenter.front().pose.x != a.front().pose.x, "the grid follows the sample");
    }

    void theSampleCellIsAlwaysAStrongPartWithNoNoise() {
        fake::FakeSpec s = spec();
        s.missingFraction = 0.5;
        s.jitterPx = 3.0;
        for (std::uint64_t seed = 1; seed <= 20; ++seed) {
            s.seed = seed;
            const auto grid = fake::makeFakeGrid(s, kSample, 800, 600);
            const auto it = std::find_if(grid.begin(), grid.end(), [](const fake::FakeCell& c) { return c.i == 0 && c.j == 0; });
            QVERIFY(it != grid.end());
            QVERIFY(it->kind == fake::FakeCellKind::Strong);
            QVERIFY(approxEqual(it->pose.x, 400.0) && approxEqual(it->pose.y, 300.0));
        }
    }
};

QTEST_APPLESS_MAIN(TstFakeBackend)
#include "tst_fake_backend.moc"
