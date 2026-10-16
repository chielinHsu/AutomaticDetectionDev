// use case GenerateObjectsFromSample 的端到端測試（假 back end）。
//
// 「標準答案」來自 makeFakeGrid（測試資料產生器），不是來自 use case 自己的輸出：
// 假 back end 的搜尋器與測試的標準答案用同一份產生器，所以世界是什麼樣子是已知的；被測的是 use case 怎麼用 port 把它找出來。
#include <QtTest>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

#include "CoreTestSupport.h"
#include "domain/image_buffer.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "application/usecases/ProposalView.h"
#include "domain/lattice/lattice_accumulator.h"
#include "infrastructure/perf/PerfProbe.h"

using namespace aoi;
using namespace aoi::test;
using namespace std::chrono_literals;

namespace {

const TestImage& image800x600() {
    static const TestImage image(800, 600);
    return image;
}

// 樣本 40×30，中心 (400, 300)；pitch 1.5 倍 → 60×45：9 欄 × 5 列的格子（halfCols 4、halfRows 2）都落在 800×600 內。
const Rect kSample{380.0, 285.0, 40.0, 30.0};

fake::FakeSpec gridSpec() {
    fake::FakeSpec spec;
    spec.halfCols = 4;
    spec.halfRows = 2;
    return spec;
}

struct Fixture {
    explicit Fixture(fake::FakeSpec s) : spec(std::move(s)), builder(spec), searcher(spec), useCase(builder, searcher) {}
    fake::FakeSpec spec;
    CountingBuilder builder;
    CountingSearcher searcher;
    GenerateObjectsFromSample useCase;

    Result<SampleProposal> run(Rect sample = kSample, const TestImage& image = image800x600(),
                               const CancelToken& cancel = {}, IProgressSink* progress = nullptr) {
        return useCase.run(makeRequest(image, sample), cancel, progress);
    }
};

// 標準答案：依 spec 排出的格子，以及「種子」（Strong＋樣本）的索引範圍。
struct Oracle {
    std::vector<fake::FakeCell> cells;
    int minI = 0, maxI = 0, minJ = 0, maxJ = 0;
    bool insideSeedBox(const fake::FakeCell& c) const { return c.i >= minI && c.i <= maxI && c.j >= minJ && c.j <= maxJ; }
};

Oracle makeOracle(const fake::FakeSpec& spec, Rect sample) {
    Oracle o;
    o.cells = fake::makeFakeGrid(spec, sample, 800, 600);
    bool first = true;
    for (const fake::FakeCell& c : o.cells) {
        if (c.kind != fake::FakeCellKind::Strong) continue;
        if (first) {
            o.minI = o.maxI = c.i;
            o.minJ = o.maxJ = c.j;
            first = false;
        }
        o.minI = std::min(o.minI, c.i);
        o.maxI = std::max(o.maxI, c.i);
        o.minJ = std::min(o.minJ, c.j);
        o.maxJ = std::max(o.maxJ, c.j);
    }
    return o;
}

bool hasCandidateAt(const std::vector<Candidate>& candidates, Pose2D pose, CandidateSource source, double tolerance = 1e-6) {
    return std::any_of(candidates.begin(), candidates.end(), [&](const Candidate& c) {
        return c.source == source && distance(c.pose.position(), pose.position()) <= tolerance;
    });
}

struct ProgressLog final : IProgressSink {
    void report(const ProgressEvent& e) noexcept override { events.push_back(e); }
    std::vector<ProgressEvent> events;
};

// S05：腳本化的 back end。種子搜尋回傳 seeds；視窗搜尋回傳 world 裡中心落在任一視窗內的。
// 用來造假 back end 排不出來的情境：重複的匹配、兩個陣列、散佈的點、位置有雜訊的種子。
struct ScriptedTemplate final : ITemplate {
    explicit ScriptedTemplate(Size2D s) : size_(s) {}
    Size2D size() const noexcept override { return size_; }
    std::string_view backendId() const noexcept override { return "scripted"; }
    Size2D size_;
};

struct ScriptedBuilder final : ITemplateBuilder {
    explicit ScriptedBuilder(Size2D s) : size(s) {}
    std::string_view backendId() const noexcept override { return "scripted"; }
    Result<TemplatePtr> build(const ImageView&, const Rect&, const SearchParams&, const CancelToken&) override {
        return TemplatePtr(std::make_shared<const ScriptedTemplate>(size));
    }
    Size2D size;
};

struct ScriptedSearcher final : IPatternSearcher {
    std::string_view backendId() const noexcept override { return "scripted"; }
    Result<std::vector<Match>> find(const ImageView&, const ITemplate&, const SearchRegion& region, const SearchParams& params,
                                    const CancelToken&) override {
        std::vector<Match> out;
        if (region.kind == SearchRegion::Kind::WholeImage) {
            for (const Match& m : seeds) {
                if (m.score >= params.minScore) out.push_back(m);
            }
        } else {
            ++windowCalls;
            for (const Match& m : world) {
                if (m.score < params.minScore) continue;
                for (const Rect& w : region.windows) {
                    if (w.contains(m.pose.position())) {
                        out.push_back(m);
                        break;
                    }
                }
            }
        }
        std::sort(out.begin(), out.end(), [](const Match& a, const Match& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.pose.y != b.pose.y ? a.pose.y < b.pose.y : a.pose.x < b.pose.x;
        });
        return out;
    }
    std::vector<Match> seeds;
    std::vector<Match> world;
    int windowCalls = 0;
};

Point2D at(Point2D origin, double pitchX, double pitchY, int col, int row) { return {origin.x + col * pitchX, origin.y + row * pitchY}; }

Rect sampleAround(Point2D center) { return rectFromCenter(center, {40.0, 30.0}); }

}  // namespace

class TstGenerateObjects : public QObject {
    Q_OBJECT

private slots:
    // ---- 完整流程 ----

    void aCleanGridIsFoundCompletely() {
        Fixture f(gridSpec());
        const Result<SampleProposal> r = f.run();
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        const SampleProposal& p = r.value();

        QVERIFY(!p.sampleProblem.has_value());
        QCOMPARE(p.candidates.size(), std::size_t{45});  // 9 × 5
        QCOMPARE(countSource(p.candidates, CandidateSource::Sample), std::size_t{1});
        QCOMPARE(countSource(p.candidates, CandidateSource::Seed), std::size_t{44});
        QCOMPARE(countSource(p.candidates, CandidateSource::Verified), std::size_t{0});
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), std::size_t{0});
        QVERIFY(p.candidates.front().source == CandidateSource::Sample);
        QVERIFY(approxEqual(p.partSize.w, 40.0) && approxEqual(p.partSize.h, 30.0));
        QVERIFY(p.grid.found && p.grid.hasV);
        QCOMPARE(p.grid.holes, std::size_t{0});

        const Oracle oracle = makeOracle(f.spec, kSample);
        for (const fake::FakeCell& cell : oracle.cells) {
            const CandidateSource expected = (cell.i == 0 && cell.j == 0) ? CandidateSource::Sample : CandidateSource::Seed;
            QVERIFY2(hasCandidateAt(p.candidates, cell.pose, expected), "every cell must come back at its exact position");
        }
    }

    void classifyTurnsCandidatesIntoStatesByThreshold() {
        Fixture f(gridSpec());
        const SampleProposal p = f.run().value();
        const ProposalView v = classify(p, 0.80);
        QCOMPARE(v.objects.size(), std::size_t{45});
        QCOMPARE(v.found, std::size_t{44});
        QCOMPARE(v.rejected, std::size_t{0});
        QVERIFY(!v.noMatch.has_value());
        QVERIFY(v.objects.front().state == ObjectState::Sample);
        for (std::size_t i = 0; i < v.objects.size(); ++i) {
            QCOMPARE(v.objects[i].id, static_cast<int>(i) + 1);
            QVERIFY(approxEqual(v.objects[i].rect.width, 40.0) && approxEqual(v.objects[i].rect.height, 30.0));
            QVERIFY(v.objects[i].origin == ObjectOrigin::Auto);
        }
    }

    // 分數低的零件：種子搜尋（下限 0.70）找不到，視窗驗證（下限 0.30）要找回來。
    void weakPartsAreRecoveredByWindowVerification() {
        fake::FakeSpec spec = gridSpec();
        spec.weakFraction = 0.25;
        spec.seed = 7;
        const Oracle oracle = makeOracle(spec, kSample);
        std::size_t expectedVerified = 0;
        for (const fake::FakeCell& c : oracle.cells) {
            if (c.kind == fake::FakeCellKind::Weak && oracle.insideSeedBox(c)) ++expectedVerified;
        }
        QVERIFY2(expectedVerified >= 3, "the fixture itself must contain weak parts, or the test proves nothing");

        Fixture f(spec);
        const SampleProposal p = f.run().value();
        QCOMPARE(countSource(p.candidates, CandidateSource::Verified), expectedVerified);
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), std::size_t{0});
        QVERIFY2(f.searcher.windowCalls >= 1, "weak parts can only be found by the windowed search");
        for (const fake::FakeCell& c : oracle.cells) {
            if (c.kind == fake::FakeCellKind::Weak && oracle.insideSeedBox(c)) {
                QVERIFY(hasCandidateAt(p.candidates, c.pose, CandidateSource::Verified));
            }
        }
        for (const Candidate& c : p.candidates) {
            if (c.source == CandidateSource::Verified) QVERIFY(c.score >= 0.30 && c.score < 0.70);
        }
    }

    // 預算 U3 的前提：門檻變動只要重新過濾，不必重新搜尋。同一份提案、不同門檻，不呼叫任何 port。
    void changingTheThresholdNeverSearchesAgain() {
        fake::FakeSpec spec = gridSpec();
        spec.weakFraction = 0.25;
        spec.seed = 7;
        Fixture f(spec);
        const SampleProposal p = f.run().value();
        const int searchesBefore = f.searcher.calls;
        const int buildsBefore = f.builder.calls;

        const ProposalView strict = classify(p, 0.95);
        const ProposalView normal = classify(p, 0.80);
        const ProposalView lax = classify(p, 0.35);
        QCOMPARE(f.searcher.calls.load(), searchesBefore);
        QCOMPARE(f.builder.calls.load(), buildsBefore);

        QVERIFY(strict.found <= normal.found && normal.found <= lax.found);
        QVERIFY(lax.found > normal.found);  // 低分的零件在鬆的門檻下變成 Found
        QCOMPARE(strict.found + strict.rejected, normal.found + normal.rejected);  // 只是換狀態，候選總數不變
        QCOMPARE(normal.found + normal.rejected, lax.found + lax.rejected);
    }

    void missingPartsBecomeInferredCandidatesWithoutScores() {
        fake::FakeSpec spec = gridSpec();
        spec.missingFraction = 0.2;
        spec.seed = 3;
        const Oracle oracle = makeOracle(spec, kSample);
        std::size_t expectedPredicted = 0;
        for (const fake::FakeCell& c : oracle.cells) {
            if (c.kind == fake::FakeCellKind::Missing && oracle.insideSeedBox(c)) ++expectedPredicted;
        }
        QVERIFY2(expectedPredicted >= 2, "the fixture itself must contain missing parts");

        Fixture f(spec);
        const SampleProposal p = f.run().value();
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), expectedPredicted);
        QCOMPARE(p.grid.holes, expectedPredicted);
        for (const fake::FakeCell& c : oracle.cells) {
            if (c.kind == fake::FakeCellKind::Missing && oracle.insideSeedBox(c)) {
                QVERIFY(hasCandidateAt(p.candidates, c.pose, CandidateSource::Predicted));
            }
        }

        const ProposalView v = classify(p, 0.80);
        QCOMPARE(v.inferred, expectedPredicted);
        for (const InspectionObject& o : v.objects) {
            if (o.state == ObjectState::Inferred) QVERIFY2(!o.score.has_value(), "an inferred part has no score, and the core must not invent one");
        }
    }

    void aRotatedNoisyGridWithMixedPartsMatchesTheOracle() {
        fake::FakeSpec spec = gridSpec();
        spec.gridAngleDeg = 3.0;
        spec.jitterPx = 0.8;
        spec.weakFraction = 0.12;
        spec.missingFraction = 0.12;
        spec.seed = 11;
        const Oracle oracle = makeOracle(spec, kSample);
        std::size_t strong = 0, weakIn = 0, missingIn = 0;
        for (const fake::FakeCell& c : oracle.cells) {
            if (c.kind == fake::FakeCellKind::Strong) ++strong;
            if (c.kind == fake::FakeCellKind::Weak && oracle.insideSeedBox(c)) ++weakIn;
            if (c.kind == fake::FakeCellKind::Missing && oracle.insideSeedBox(c)) ++missingIn;
        }
        Fixture f(spec);
        const SampleProposal p = f.run().value();
        QCOMPARE(countSource(p.candidates, CandidateSource::Sample) + countSource(p.candidates, CandidateSource::Seed), strong);
        QCOMPARE(countSource(p.candidates, CandidateSource::Verified), weakIn);
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), missingIn);
        QVERIFY(p.grid.rmsPx < 1.5);
        for (const Candidate& c : p.candidates) QVERIFY(approxEqual(c.pose.angleDeg, 3.0) || c.source == CandidateSource::Predicted);
    }

    // 一排（一維）：另一個方向的格子都在影像外。
    void aSingleRowIsHandledAsOneDimensional() {
        fake::FakeSpec spec;
        spec.halfCols = 5;
        spec.pitchFactorY = 50.0;  // 其他列都在影像外
        spec.missingFraction = 0.2;
        spec.seed = 5;
        const Oracle oracle = makeOracle(spec, kSample);
        QCOMPARE(oracle.cells.size(), std::size_t{11});
        std::size_t missingIn = 0;
        for (const fake::FakeCell& c : oracle.cells) {
            if (c.kind == fake::FakeCellKind::Missing && oracle.insideSeedBox(c)) ++missingIn;
        }
        Fixture f(spec);
        const SampleProposal p = f.run().value();
        QVERIFY(p.grid.found);
        QVERIFY(!p.grid.hasV);
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), missingIn);
    }

    void resultsAreReproducible() {
        fake::FakeSpec spec = gridSpec();
        spec.jitterPx = 0.7;
        spec.weakFraction = 0.2;
        spec.missingFraction = 0.1;
        spec.seed = 99;
        Fixture a(spec);
        Fixture b(spec);
        const SampleProposal pa = a.run().value();
        const SampleProposal pb = b.run().value();
        QCOMPARE(pa.candidates.size(), pb.candidates.size());
        for (std::size_t i = 0; i < pa.candidates.size(); ++i) {
            QVERIFY(pa.candidates[i].source == pb.candidates[i].source);
            QCOMPARE(pa.candidates[i].pose.x, pb.candidates[i].pose.x);  // 完全相同，不是「差不多」
            QCOMPARE(pa.candidates[i].pose.y, pb.candidates[i].pose.y);
            QCOMPARE(pa.candidates[i].score, pb.candidates[i].score);
        }
        spec.seed = 100;
        Fixture c(spec);
        const SampleProposal pc = c.run().value();
        bool differs = pc.candidates.size() != pa.candidates.size();
        for (std::size_t i = 0; !differs && i < pa.candidates.size(); ++i) differs = pc.candidates[i].pose.x != pa.candidates[i].pose.x;
        QVERIFY2(differs, "a different seed must give a different world");
    }

    // ---- 沒有結果：原因要對，而且要在搜尋之前就停下的就不要搜尋 ----

    void onlyTheSampleMeansOnlyOne() {
        fake::FakeSpec spec;
        spec.pitchFactorX = 50.0;  // 其他格子都在影像外
        spec.pitchFactorY = 50.0;
        Fixture f(spec);
        const SampleProposal p = f.run().value();
        QCOMPARE(p.candidates.size(), std::size_t{1});
        const ProposalView v = classify(p, 0.80);
        QVERIFY(v.noMatch.has_value());
        QVERIFY(v.noMatch->reason == NoMatchReason::OnlyOne);
        QCOMPARE(v.found, std::size_t{0});
        QCOMPARE(f.searcher.windowCalls.load(), 0);  // 沒有晶格就不做視窗驗證
    }

    // 「降低門檻到 {suggested}」的按鈕要真的有用：建議值必須讓最高分的候選過關。
    void aSuggestedThresholdMakesTheBestCandidatePass() {
        Fixture f(gridSpec());
        const SampleProposal p = f.run().value();
        const ProposalView tooHigh = classify(p, 0.999);  // 分數最高 0.99
        QVERIFY(tooHigh.noMatch.has_value());
        QVERIFY(tooHigh.noMatch->reason == NoMatchReason::ThresholdTooHigh);
        QCOMPARE(tooHigh.found, std::size_t{0});
        QCOMPARE(tooHigh.rejected, std::size_t{44});
        const double highest = tooHigh.noMatch->highestScore;
        const double suggested = tooHigh.noMatch->suggestedThreshold;
        QVERIFY(highest > 0.90 && highest <= 0.99);
        QVERIFY(suggested <= highest);
        QVERIFY(suggested >= 0.30);

        const ProposalView retry = classify(p, suggested);
        QVERIFY(retry.found >= 1);
        QVERIFY(!retry.noMatch.has_value());
    }

    void sampleProblemsStopBeforeAnySearch_data() {
        QTest::addColumn<QRectF>("sample");
        QTest::addColumn<bool>("flatImage");
        QTest::addColumn<int>("reason");
        QTest::newRow("too small") << QRectF(100, 100, 5, 5) << false << static_cast<int>(NoMatchReason::SampleTooSmall);
        QTest::newRow("thin sliver") << QRectF(100, 100, 200, 4) << false << static_cast<int>(NoMatchReason::SampleTooSmall);
        QTest::newRow("empty rect") << QRectF(100, 100, 0, 0) << false << static_cast<int>(NoMatchReason::SampleTooSmall);
        QTest::newRow("too large") << QRectF(50, 50, 700, 500) << false << static_cast<int>(NoMatchReason::SampleTooLarge);
        QTest::newRow("outside the image") << QRectF(2000, 2000, 40, 30) << false << static_cast<int>(NoMatchReason::SampleOffPart);
        QTest::newRow("flat area") << QRectF(380, 285, 40, 30) << true << static_cast<int>(NoMatchReason::LowContrast);
    }
    void sampleProblemsStopBeforeAnySearch() {
        QFETCH(QRectF, sample);
        QFETCH(bool, flatImage);
        QFETCH(int, reason);
        const TestImage flat(800, 600, true);
        Fixture f(gridSpec());
        const Rect rect{sample.x(), sample.y(), sample.width(), sample.height()};
        const Result<SampleProposal> r = f.run(rect, flatImage ? flat : image800x600());
        QVERIFY2(r.ok(), "a bad sample is a normal outcome with a reason, not an error");
        QVERIFY(r.value().sampleProblem.has_value());
        QCOMPARE(static_cast<int>(r.value().sampleProblem->reason), reason);
        QVERIFY(r.value().candidates.empty());
        QCOMPARE(f.searcher.calls.load(), 0);
        QCOMPARE(f.builder.calls.load(), 0);

        const ProposalView v = classify(r.value(), 0.80);  // classify 直接帶出這個原因
        QVERIFY(v.noMatch.has_value());
        QCOMPARE(static_cast<int>(v.noMatch->reason), reason);
    }

    // S06 新增 Bgr24、Gray16：對比檢查要用對的通道與單位。棋盤只在一個通道上有起伏，
    // 起伏的大小選在門檻（標準差 3）兩側：讀錯通道或沒把 16 bit 換到 0..255，結果就會翻過來。
    void theContrastCheckReadsEveryPixelFormat_data() {
        QTest::addColumn<int>("format");
        QTest::addColumn<int>("channel");    // 有起伏的 byte（Gray16 是整個像素）
        QTest::addColumn<int>("amplitude");  // 棋盤兩格的差的一半（該通道的單位）
        QTest::addColumn<bool>("lowContrast");
        QTest::addColumn<int>("bits");  // Gray16 的有效位元數（0 ＝ 16）
        // 亮度權重：R 77、G 150、B 29（/256）。棋盤的標準差 ＝ 權重 × 起伏。
        QTest::newRow("Gray8 ±2 is flat") << int(PixelFormat::Gray8) << 0 << 2 << true << 0;
        QTest::newRow("Gray8 ±10 has texture") << int(PixelFormat::Gray8) << 0 << 10 << false << 0;
        QTest::newRow("Gray16 ±2x257 is flat") << int(PixelFormat::Gray16) << 0 << 2 * 257 << true << 0;
        QTest::newRow("Gray16 ±10x257 has texture") << int(PixelFormat::Gray16) << 0 << 10 * 257 << false << 0;
        // 12 bit（值只到 4095）：±10×16 在 8 bit 的單位是 ±10；當成 16 bit 會被低估成 ±0.6
        QTest::newRow("12 bit ±2x16 is flat") << int(PixelFormat::Gray16) << 0 << 2 * 16 << true << 12;
        QTest::newRow("12 bit ±10x16 has texture") << int(PixelFormat::Gray16) << 0 << 10 * 16 << false << 12;
        QTest::newRow("12 bit data read as 16 bit is flat") << int(PixelFormat::Gray16) << 0 << 10 * 16 << true << 16;
        QTest::newRow("Bgr24 blue ±20 is flat") << int(PixelFormat::Bgr24) << 0 << 20 << true << 0;     // 29/256×20 ≈ 2.3
        QTest::newRow("Bgr24 red ±20 has texture") << int(PixelFormat::Bgr24) << 2 << 20 << false << 0; // 77/256×20 ≈ 6.0
        QTest::newRow("Rgb24 red ±20 has texture") << int(PixelFormat::Rgb24) << 0 << 20 << false << 0;
        QTest::newRow("Rgb24 blue ±20 is flat") << int(PixelFormat::Rgb24) << 2 << 20 << true << 0;
        QTest::newRow("Bgrx32 blue ±20 is flat") << int(PixelFormat::Bgrx32) << 0 << 20 << true << 0;
        QTest::newRow("Bgrx32 red ±20 has texture") << int(PixelFormat::Bgrx32) << 2 << 20 << false << 0;
        QTest::newRow("Bgrx32 x byte is ignored") << int(PixelFormat::Bgrx32) << 3 << 100 << true << 0;
    }
    void theContrastCheckReadsEveryPixelFormat() {
        QFETCH(int, format);
        QFETCH(int, channel);
        QFETCH(int, amplitude);
        QFETCH(bool, lowContrast);
        QFETCH(int, bits);
        const PixelFormat f = static_cast<PixelFormat>(format);
        ImageBuffer buffer(800, 600, f);
        QVERIFY(buffer.valid());
        const int bpp = bytesPerPixel(f);
        for (int y = 0; y < buffer.height(); ++y) {
            std::uint8_t* row = buffer.row(y);
            for (int x = 0; x < buffer.width(); ++x) {
                const int sign = ((x / 4 + y / 4) & 1) ? 1 : -1;
                std::uint8_t* p = row + static_cast<std::ptrdiff_t>(x) * bpp;
                if (f == PixelFormat::Gray16) {
                    const int middle = (bits == 12 || (bits == 16 && amplitude < 257)) ? 2048 : 32768;  // 12 bit 的資料在 0..4095 的中間
                    const auto v = static_cast<std::uint16_t>(middle + sign * amplitude);
                    std::memcpy(p, &v, sizeof v);
                } else {
                    for (int c = 0; c < bpp; ++c) p[c] = 128;
                    p[channel] = static_cast<std::uint8_t>(128 + sign * amplitude);
                }
            }
        }
        Fixture fx(gridSpec());
        SampleRequest request;
        request.image = buffer.view();
        request.image.significantBits = bits;
        request.sample = kSample;
        const Result<SampleProposal> r = fx.useCase.run(request, {}, nullptr);
        QVERIFY(r.ok());
        const bool stopped = r.value().sampleProblem.has_value() &&
                             r.value().sampleProblem->reason == NoMatchReason::LowContrast;
        QCOMPARE(stopped, lowContrast);
        QCOMPARE(fx.searcher.calls.load() == 0, lowContrast);
    }

    // 範本建立器才看得出來的樣本問題（框到不只一個元件、沒框到元件）：建了範本、但不搜尋。
    void problemsReportedByTheTemplateStopBeforeTheSearch_data() {
        QTest::addColumn<int>("reason");
        QTest::newRow("covers many") << static_cast<int>(NoMatchReason::SampleCoversMany);
        QTest::newRow("off part") << static_cast<int>(NoMatchReason::SampleOffPart);
    }
    void problemsReportedByTheTemplateStopBeforeTheSearch() {
        QFETCH(int, reason);
        fake::FakeSpec spec = gridSpec();
        spec.forcedSampleProblem = static_cast<NoMatchReason>(reason);
        Fixture f(spec);
        const Result<SampleProposal> r = f.run();
        QVERIFY(r.ok());
        QVERIFY(r.value().sampleProblem.has_value());
        QCOMPARE(static_cast<int>(r.value().sampleProblem->reason), reason);
        QCOMPARE(f.builder.calls.load(), 1);
        QCOMPARE(f.searcher.calls.load(), 0);
    }

    // ---- 錯誤與取消 ----

    void invalidInputsAreErrorsNotNoMatch() {
        Fixture f(gridSpec());
        SampleRequest request = makeRequest(image800x600(), kSample);
        request.image = ImageView{};
        const Result<SampleProposal> noImage = f.useCase.run(request, CancelToken());
        QVERIFY(!noImage.ok());
        QVERIFY(noImage.error().code == ErrorCode::InvalidArgument);

        request = makeRequest(image800x600(), {std::nan(""), 0.0, 10.0, 10.0});
        const Result<SampleProposal> nan = f.useCase.run(request, CancelToken());
        QVERIFY(!nan.ok());
        QVERIFY(nan.error().code == ErrorCode::InvalidArgument);
        QCOMPARE(f.searcher.calls.load(), 0);
    }

    void aCancelledTokenStopsBeforeAnyWork() {
        Fixture f(gridSpec());
        CancelSource source;
        source.cancel();
        const Result<SampleProposal> r = f.run(kSample, image800x600(), source.token());
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::Cancelled);
        QCOMPARE(f.builder.calls.load(), 0);
        QCOMPARE(f.searcher.calls.load(), 0);
    }

    // 搜尋進行中取消：假搜尋器每 1 ms 檢查一次旗標，所以 use case 應該在遠小於「整個搜尋時間」的時間內回來。
    void cancellingDuringTheSearchReturnsPromptly() {
        fake::FakeSpec spec = gridSpec();
        spec.latencyMs = 2000.0;  // 假裝一次搜尋要 2 秒
        Fixture f(spec);
        CancelSource source;
        std::thread canceller([&] {
            std::this_thread::sleep_for(40ms);
            source.cancel();
        });
        const auto start = std::chrono::steady_clock::now();
        const Result<SampleProposal> r = f.run(kSample, image800x600(), source.token());
        const auto elapsedMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        canceller.join();
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::Cancelled);
        QVERIFY2(elapsedMs < 1000.0, qPrintable(QStringLiteral("returned after %1 ms").arg(elapsedMs)));
    }

    // 範本與搜尋器不是同一個 back end 建的：use case 不猜，把搜尋器的錯誤原樣回報。
    void aTemplateFromAnotherBackendIsRefused() {
        struct ForeignTemplate final : ITemplate {
            Size2D size() const noexcept override { return {40.0, 30.0}; }
            std::string_view backendId() const noexcept override { return "someone-else"; }
        };
        struct ForeignBuilder final : ITemplateBuilder {
            std::string_view backendId() const noexcept override { return "someone-else"; }
            Result<TemplatePtr> build(const ImageView&, const Rect&, const SearchParams&, const CancelToken&) override {
                return TemplatePtr(std::make_shared<const ForeignTemplate>());
            }
        } builder;
        fake::FakeSearcher searcher(gridSpec());
        GenerateObjectsFromSample useCase(builder, searcher);
        const Result<SampleProposal> r = useCase.run(makeRequest(image800x600(), kSample), CancelToken());
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::BackendMismatch);
    }

    void aBuilderThatReturnsNothingIsAnInternalError() {
        struct NullBuilder final : ITemplateBuilder {
            std::string_view backendId() const noexcept override { return "null"; }
            Result<TemplatePtr> build(const ImageView&, const Rect&, const SearchParams&, const CancelToken&) override {
                return TemplatePtr();
            }
        } builder;
        fake::FakeSearcher searcher(gridSpec());
        GenerateObjectsFromSample useCase(builder, searcher);
        const Result<SampleProposal> r = useCase.run(makeRequest(image800x600(), kSample), CancelToken());
        QVERIFY(!r.ok());
        QVERIFY(r.error().code == ErrorCode::Internal);
    }

    // ---- 進度、量測 ----

    void progressIsReportedStageByStage() {
        fake::FakeSpec spec = gridSpec();
        spec.missingFraction = 0.2;  // 有缺格才會有視窗驗證
        spec.seed = 3;
        Fixture f(spec);
        ProgressLog log;
        QVERIFY(f.run(kSample, image800x600(), CancelToken(), &log).ok());
        QCOMPARE(log.events.size(), std::size_t{4});
        const Stage expected[] = {Stage::Template, Stage::Seed, Stage::Fit, Stage::Verify};
        for (std::size_t i = 0; i < 4; ++i) {
            QVERIFY(log.events[i].stage == expected[i]);
            QCOMPARE(log.events[i].fraction, 1.0);
        }
    }

    void theProbeRecordsEveryStageWhenEnabled() {
        fake::FakeSpec spec = gridSpec();
        spec.missingFraction = 0.2;
        spec.seed = 3;
        CountingBuilder builder(spec);
        CountingSearcher searcher(spec);
        infra::PerfProbe probe(1024, true);
        GenerateObjectsFromSample useCase(builder, searcher, &probe);
        QVERIFY(useCase.run(makeRequest(image800x600(), kSample), CancelToken()).ok());

        std::vector<std::string> names;
        for (const auto& r : probe.snapshot()) names.push_back(r.name);
        for (const char* expected : {"uc.sample.template", "uc.sample.seed", "uc.sample.fit", "uc.sample.verify", "uc.sample.total"}) {
            QVERIFY2(std::find(names.begin(), names.end(), expected) != names.end(), expected);
        }
        QCOMPARE(names.back(), std::string("uc.sample.total"));  // 最外層的計時最後結束
    }

    void aDisabledProbeRecordsNothing() {
        CountingBuilder builder(gridSpec());
        CountingSearcher searcher(gridSpec());
        infra::PerfProbe probe(1024, false);
        GenerateObjectsFromSample useCase(builder, searcher, &probe);
        QVERIFY(useCase.run(makeRequest(image800x600(), kSample), CancelToken()).ok());
        QCOMPARE(probe.recorded(), std::size_t{0});
    }

    void timingsAreFilledAndConsistent() {
        Fixture f(gridSpec());
        const StageTimings t = f.run().value().timings;
        QVERIFY(t.totalMs > 0.0);
        QVERIFY(t.templateMs >= 0.0 && t.seedMs >= 0.0 && t.fitMs >= 0.0 && t.verifyMs >= 0.0);
        QVERIFY(t.totalMs + 1e-6 >= t.templateMs + t.seedMs + t.fitMs + t.verifyMs);
    }

    // ---- S05：晶格（domain/lattice）接進 use case ----

    // 同一個元件被找到兩次（差 2 像素、分數低一點）：NMS 只留分數高的，候選數量和沒有重複時一樣。
    void duplicateSeedMatchesAreMergedIntoOne() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        const Point2D origin{100.0, 100.0};
        for (int c = 0; c < 6; ++c) {
            for (int r = 0; r < 4; ++r) {
                const Point2D p = at(origin, 60.0, 45.0, c, r);
                searcher.seeds.push_back({{p.x, p.y, 0.0}, 0.92});
                searcher.seeds.push_back({{p.x + 2.0, p.y - 1.0, 0.0}, 0.86});  // 重複
            }
        }
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(origin)), CancelToken()).value();
        QCOMPARE(p.candidates.size(), std::size_t{24});
        QCOMPARE(countSource(p.candidates, CandidateSource::Seed), std::size_t{23});
        for (const Candidate& c : p.candidates) QVERIFY2(c.source == CandidateSource::Sample || c.score == 0.92, "the better of each pair survives");
        QVERIFY(p.grid.found && p.grid.hasV);
        QCOMPARE(p.grid.holes, std::size_t{0});
        QCOMPARE(p.grid.lattices, std::size_t{1});
        QCOMPARE(p.grid.coverage, 1.0);
        QVERIFY(p.grid.confidence > 0.9 && !p.grid.suggestCheck);
    }

    // 兩個間距不同的陣列：兩組晶格，各自的缺件都補上，位置是精確的。
    void twoArraysWithDifferentPitchesAreBothFilled() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        const Point2D a{100.0, 100.0};
        const Point2D b{130.0, 380.0};
        for (int c = 0; c < 8; ++c) {
            for (int r = 0; r < 3; ++r) {
                if (c == 3 && r == 1) continue;  // 陣列 A 缺一個
                const Point2D p = at(a, 60.0, 45.0, c, r);
                searcher.seeds.push_back({{p.x, p.y, 0.0}, 0.9});
            }
        }
        for (int c = 0; c < 6; ++c) {
            for (int r = 0; r < 3; ++r) {
                if (c == 2 && r == 1) continue;  // 陣列 B 缺一個
                const Point2D p = at(b, 52.0, 40.0, c, r);
                searcher.seeds.push_back({{p.x, p.y, 0.0}, 0.9});
            }
        }
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(a)), CancelToken()).value();
        QCOMPARE(p.grid.lattices, std::size_t{2});
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), std::size_t{2});
        QVERIFY(hasCandidateAt(p.candidates, Pose2D{at(a, 60.0, 45.0, 3, 1).x, at(a, 60.0, 45.0, 3, 1).y, 0.0}, CandidateSource::Predicted));
        QVERIFY(hasCandidateAt(p.candidates, Pose2D{at(b, 52.0, 40.0, 2, 1).x, at(b, 52.0, 40.0, 2, 1).y, 0.0}, CandidateSource::Predicted));
        QCOMPARE(searcher.windowCalls, 1);  // 兩組晶格的缺格一起驗證：一次批次
    }

    // 散佈的元件（不成陣列）：沒有晶格，不補漏，也不做視窗驗證；結果就是找到的（NMS 之後）。
    void scatteredMatchesAreNotALattice() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        // 固定的散佈位置（黃金角螺線：沒有任何三點等距共線）。
        const Point2D center{400.0, 300.0};
        for (int k = 0; k < 30; ++k) {
            const double r = 22.0 * std::sqrt(static_cast<double>(k) + 1.0);
            const double t = k * 2.399963229728653;
            searcher.seeds.push_back({{center.x + r * std::cos(t), center.y + r * std::sin(t), 0.0}, 0.9});
        }
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(searcher.seeds.front().pose.position())), CancelToken()).value();
        QVERIFY2(!p.grid.found, qPrintable(QStringLiteral("lattice u=(%1,%2) v=(%3,%4) hasV=%5 holes=%6 confidence=%7")
                                               .arg(p.grid.u.x).arg(p.grid.u.y).arg(p.grid.v.x).arg(p.grid.v.y)
                                               .arg(p.grid.hasV).arg(p.grid.holes).arg(p.grid.confidence)));
        QVERIFY(p.grid.noLattice != NoLatticeReason::None);
        QCOMPARE(p.grid.holes, std::size_t{0});
        QCOMPARE(searcher.windowCalls, 0);
        QCOMPARE(p.candidates.size(), std::size_t{30});
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), std::size_t{0});
    }

    // 驗證找到的元件加進晶格重新擬合（O(1)／個），沒找到的缺格放在修正後的位置：
    // 預期值 ＝ 「種子 ＋ 驗證找到的」一起做最小平方法；和只用種子的預測不同（種子有雜訊）。
    void verifiedPartsCorrectThePredictedPositions() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        const Point2D origin{100.0, 100.0};
        const std::vector<std::pair<int, int>> weak{{2, 1}, {4, 0}, {5, 2}};
        const std::pair<int, int> missing{6, 1};
        // 視窗裡找到的東西離預測位置 15 px（超過晶格的容差 11.25 px）：仍是 Verified 候選，但不拿來修正晶格。
        const std::pair<int, int> farWeak{1, 2};
        aoi::LatticeAccumulator seedsOnly(origin, {0, 0});
        aoi::LatticeAccumulator all(origin, {0, 0});
        quint32 state = 4242;  // 固定的雜訊（各平台一致）
        auto noise = [&] {
            state = state * 1664525u + 1013904223u;
            return (static_cast<double>(state >> 8) / 16777216.0 - 0.5) * 3.0;  // ±1.5 px
        };
        for (int c = 0; c < 7; ++c) {
            for (int r = 0; r < 3; ++r) {
                const Point2D p = at(origin, 60.0, 45.0, c, r);
                if (std::make_pair(c, r) == missing) continue;
                if (std::make_pair(c, r) == farWeak) {
                    searcher.world.push_back({{p.x + 15.0, p.y, 0.0}, 0.5});
                    continue;
                }
                if (std::find(weak.begin(), weak.end(), std::make_pair(c, r)) != weak.end()) {
                    searcher.world.push_back({{p.x, p.y, 0.0}, 0.5});  // 只有視窗搜尋找得到，位置精確
                    all.add({c, r}, p);
                    continue;
                }
                const Point2D q = (c == 0 && r == 0) ? p : Point2D{p.x + noise(), p.y + noise()};
                searcher.seeds.push_back({{q.x, q.y, 0.0}, 0.9});
                seedsOnly.add({c, r}, q);
                all.add({c, r}, q);
            }
        }
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(origin)), CancelToken()).value();
        QCOMPARE(countSource(p.candidates, CandidateSource::Verified), weak.size() + 1);
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), std::size_t{1});
        const Point2D expected = all.solve()->at(missing.first, missing.second);
        const Point2D seedOnly = seedsOnly.solve()->at(missing.first, missing.second);
        QVERIFY2(distance(expected, seedOnly) > 1e-3, "the fixture must make the refit visible");
        for (const Candidate& c : p.candidates) {
            if (c.source != CandidateSource::Predicted) continue;
            QVERIFY2(distance(c.pose.position(), expected) < 1e-9, "the predicted part sits where seeds and verified parts agree");
        }
    }

    // ADR-026：晶格的成員（樣本、種子的內點、視窗驗證找到且在容差內的）拿到最終基底上的格點當晶格位姿，角度是樣本的；
    // pose 不改；離預測位置太遠的 Verified 不是成員，只記下離格點多遠；Predicted 的晶格位姿就是它的位置。
    void membersGetTheirLatticeNodeAndStraysOnlyTheirDeviation() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        const Point2D origin{100.0, 100.0};
        const std::vector<std::pair<int, int>> weak{{2, 1}, {4, 0}, {5, 2}};
        const std::pair<int, int> missing{6, 1};
        const std::pair<int, int> farWeak{1, 2};
        aoi::LatticeAccumulator all(origin, {0, 0});
        std::vector<Match> matched;  // 腳本給搜尋器的每個匹配（pose 必須原樣回來）
        quint32 state = 4242;
        auto noise = [&] {
            state = state * 1664525u + 1013904223u;
            return (static_cast<double>(state >> 8) / 16777216.0 - 0.5) * 3.0;  // ±1.5 px
        };
        for (int c = 0; c < 7; ++c) {
            for (int r = 0; r < 3; ++r) {
                const Point2D p = at(origin, 60.0, 45.0, c, r);
                if (std::make_pair(c, r) == missing) continue;
                if (std::make_pair(c, r) == farWeak) {
                    searcher.world.push_back({{p.x + 15.0, p.y, 0.0}, 0.5});
                    continue;
                }
                if (std::find(weak.begin(), weak.end(), std::make_pair(c, r)) != weak.end()) {
                    searcher.world.push_back({{p.x, p.y, 0.0}, 0.5});
                    all.add({c, r}, p);
                    continue;
                }
                const Point2D q = (c == 0 && r == 0) ? p : Point2D{p.x + noise(), p.y + noise()};
                searcher.seeds.push_back({{q.x, q.y, 0.0}, 0.9});
                all.add({c, r}, q);
            }
        }
        matched = searcher.seeds;
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(origin)), CancelToken()).value();
        const aoi::LatticeBasis basis = *all.solve();
        std::size_t members = 0;
        for (const Candidate& c : p.candidates) {
            // 它應該在哪一格：從它的位置反推（雜訊 ±1.5 px 遠小於半格）。
            const int col = static_cast<int>(std::lround((c.pose.x - origin.x) / 60.0));
            const int row = static_cast<int>(std::lround((c.pose.y - origin.y) / 45.0));
            const Point2D node = basis.at(col, row);
            if (c.source == CandidateSource::Predicted) {
                QVERIFY(c.onLattice());
                QVERIFY(distance(c.latticePose->position(), c.pose.position()) < 1e-12);
                QCOMPARE(*c.latticeDeviationPx, 0.0);
                continue;
            }
            if (c.source == CandidateSource::Verified && distance(c.pose.position(), node) > 10.0) {
                // 離格點 15 px 的那一個：不是成員，只有偏差。
                QVERIFY(!c.onLattice());
                QVERIFY(c.latticeDeviationPx.has_value());
                QVERIFY2(std::abs(*c.latticeDeviationPx - 15.0) < 0.5, qPrintable(QString::number(*c.latticeDeviationPx)));
                continue;
            }
            ++members;
            QVERIFY2(c.onLattice(), qPrintable(QStringLiteral("(%1, %2) %3").arg(col).arg(row).arg(static_cast<int>(c.source))));
            QVERIFY2(distance(c.latticePose->position(), node) < 1e-6,
                     qPrintable(QStringLiteral("node (%1, %2) vs (%3, %4)").arg(node.x).arg(node.y).arg(c.latticePose->x).arg(c.latticePose->y)));
            QCOMPARE(c.latticePose->angleDeg, p.candidates.front().pose.angleDeg);  // 角度是樣本的
            QVERIFY(std::abs(*c.latticeDeviationPx - distance(c.pose.position(), node)) < 1e-9);
        }
        QCOMPARE(members, std::size_t{7 * 3 - 2});  // 全部減掉缺格與離太遠的那一個
        // pose 原樣：每個腳本給的種子都在候選裡、位置完全相同（契約測試與 A/B 看的是它）。
        for (const Match& m : matched) QVERIFY(hasCandidateAt(p.candidates, m.pose, CandidateSource::Seed, 0.0) || sampleAround(origin).contains(m.pose.position()));
        QVERIFY(p.candidates.front().onLattice());  // 樣本自己是成員
    }

    // 一個種子離它的格點 12.5 px：不是內點，所以沒有晶格位姿、位置保留匹配的，只記下離格點 12.5 px；其他的都在格點上。
    void aSeedOffTheLatticeKeepsItsMatchedPoseAndReportsTheDeviation() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        const Point2D origin{100.0, 100.0};
        for (int c = 0; c < 6; ++c) {
            for (int r = 0; r < 4; ++r) {
                Point2D p = at(origin, 60.0, 45.0, c, r);
                if (c == 3 && r == 2) p.x += 12.5;
                searcher.seeds.push_back({{p.x, p.y, 0.0}, 0.9});
            }
        }
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(origin)), CancelToken()).value();
        QVERIFY(p.grid.found);
        std::size_t strays = 0;
        for (const Candidate& c : p.candidates) {
            if (std::abs(c.pose.x - (origin.x + 3 * 60.0 + 12.5)) < 1e-9 && std::abs(c.pose.y - (origin.y + 2 * 45.0)) < 1e-9) {
                ++strays;
                QVERIFY(!c.onLattice());
                QVERIFY(c.latticeDeviationPx.has_value());
                QVERIFY2(std::abs(*c.latticeDeviationPx - 12.5) < 1e-6, qPrintable(QString::number(*c.latticeDeviationPx)));
                continue;
            }
            QVERIFY(c.onLattice());
            QVERIFY(distance(c.latticePose->position(), c.pose.position()) < 1e-6);  // 點本來就在格點上
            QVERIFY(*c.latticeDeviationPx < 1e-6);
        }
        QCOMPARE(strays, std::size_t{1});
    }

    // 沒有晶格：沒有任何候選有晶格位姿或偏差（沒有陣列可以吸附）。
    void withoutALatticeNoCandidateHasALatticePose() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        const Point2D center{400.0, 300.0};
        for (int k = 0; k < 30; ++k) {
            const double r = 22.0 * std::sqrt(static_cast<double>(k) + 1.0);
            const double t = k * 2.399963229728653;
            searcher.seeds.push_back({{center.x + r * std::cos(t), center.y + r * std::sin(t), 0.0}, 0.9});
        }
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(searcher.seeds.front().pose.position())), CancelToken()).value();
        QVERIFY(!p.grid.found);
        for (const Candidate& c : p.candidates) {
            QVERIFY(!c.onLattice());
            QVERIFY(!c.latticeDeviationPx.has_value());
        }
    }

    // 一排（一維晶格）：垂直方向的雜訊被吸掉——成員的晶格位姿都在同一條線上、間距一致。
    void aSingleRowSnapsOntoTheLine() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        const Point2D origin{100.0, 300.0};
        quint32 state = 99;
        auto noise = [&] {
            state = state * 1664525u + 1013904223u;
            return (static_cast<double>(state >> 8) / 16777216.0 - 0.5) * 2.0;  // ±1 px
        };
        for (int c = 0; c < 8; ++c) {
            const Point2D p = at(origin, 60.0, 0.0, c, 0);
            const Point2D q = c == 0 ? p : Point2D{p.x + noise(), p.y + noise()};
            searcher.seeds.push_back({{q.x, q.y, 0.0}, 0.9});
        }
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(origin)), CancelToken()).value();
        QVERIFY(p.grid.found);
        QVERIFY(!p.grid.hasV);
        std::vector<Point2D> nodes;
        for (const Candidate& c : p.candidates) {
            QVERIFY(c.onLattice());
            nodes.push_back(c.latticePose->position());
        }
        QCOMPARE(nodes.size(), std::size_t{8});
        std::sort(nodes.begin(), nodes.end(), [](const Point2D& a, const Point2D& b) { return a.x < b.x; });
        for (std::size_t k = 1; k < nodes.size(); ++k) {
            // 同一條線（直線的方向由最小平方法決定，相鄰兩個格點的向量都相同）
            QVERIFY(std::abs((nodes[k].x - nodes[k - 1].x) - (nodes[1].x - nodes[0].x)) < 1e-9);
            QVERIFY(std::abs((nodes[k].y - nodes[k - 1].y) - (nodes[1].y - nodes[0].y)) < 1e-9);
        }
    }

    // 一個種子離它的格點 12.5 px（超過容差 11.25 px，所以不是內點），那一格因此是「缺格」；但那裡已經有這個種子
    // （主體框重疊 IoU 0.52 > 0.5）：不補，否則同一個元件會多一個推算的框。
    void aHoleUnderAnOffLatticeSeedIsNotPredicted() {
        ScriptedBuilder builder({40.0, 30.0});
        ScriptedSearcher searcher;
        const Point2D origin{100.0, 100.0};
        for (int c = 0; c < 6; ++c) {
            for (int r = 0; r < 4; ++r) {
                Point2D p = at(origin, 60.0, 45.0, c, r);
                if (c == 3 && r == 2) p.x += 12.5;
                searcher.seeds.push_back({{p.x, p.y, 0.0}, 0.9});
            }
        }
        GenerateObjectsFromSample useCase(builder, searcher);
        const SampleProposal p = useCase.run(makeRequest(image800x600(), sampleAround(origin)), CancelToken()).value();
        QVERIFY(p.grid.found);
        QCOMPARE(p.grid.holes, std::size_t{0});
        QCOMPARE(p.candidates.size(), std::size_t{24});
        QCOMPARE(countSource(p.candidates, CandidateSource::Predicted), std::size_t{0});
        QCOMPARE(searcher.windowCalls, 0);
    }

    // 驗證與推算的候選由近到遠（離樣本）；超過 maxPredictions 時留最近的。
    void holesAreOrderedNearToFarAndTheCapKeepsTheNearest() {
        fake::FakeSpec spec;
        spec.halfCols = 4;
        spec.halfRows = 2;
        spec.missingFraction = 0.25;
        spec.seed = 3;
        Fixture f(spec);
        const SampleProposal all = f.run().value();
        const Point2D anchor = all.candidates.front().pose.position();
        std::vector<double> distances;
        for (const Candidate& c : all.candidates) {
            if (c.source == CandidateSource::Predicted || c.source == CandidateSource::Verified) distances.push_back(distance(c.pose.position(), anchor));
        }
        QVERIFY2(distances.size() >= 3, "the fixture itself must contain holes");
        QVERIFY(std::is_sorted(distances.begin(), distances.end()));

        Fixture capped(spec);
        SampleRequest request = makeRequest(image800x600(), kSample);
        request.options.maxPredictions = 2;
        const SampleProposal two = capped.useCase.run(request, CancelToken()).value();
        QCOMPARE(two.grid.holes, std::size_t{2});
        std::vector<double> kept;
        for (const Candidate& c : two.candidates) {
            if (c.source == CandidateSource::Predicted || c.source == CandidateSource::Verified) kept.push_back(distance(c.pose.position(), anchor));
        }
        QCOMPARE(kept.size(), std::size_t{2});
        QVERIFY(std::abs(kept[0] - distances[0]) < 1e-9 && std::abs(kept[1] - distances[1]) < 1e-9);
    }

    // 規模：5 000 個零件的端到端，驗證數量正確（這個 use case 自己的成本在 perf_core 量）。
    void aLargeBoardIsHandled() {
        const TestImage big(2592, 1944);
        fake::FakeSpec spec;
        spec.pitchFactorX = 1.25;
        spec.pitchFactorY = 1.25;
        spec.weakFraction = 0.05;
        spec.missingFraction = 0.03;
        spec.seed = 21;
        const Rect sample{1280.0, 960.0, 24.0, 16.0};
        const std::vector<fake::FakeCell> cells = fake::makeFakeGrid(spec, sample, 2592, 1944);
        QVERIFY(cells.size() > 5000);
        CountingBuilder builder(spec);
        CountingSearcher searcher(spec);
        GenerateObjectsFromSample useCase(builder, searcher);
        const Result<SampleProposal> r = useCase.run(makeRequest(big, sample), CancelToken());
        QVERIFY(r.ok());
        std::size_t strong = 0;
        for (const fake::FakeCell& c : cells) strong += c.kind == fake::FakeCellKind::Strong ? 1u : 0u;
        const SampleProposal& p = r.value();
        QCOMPARE(countSource(p.candidates, CandidateSource::Sample) + countSource(p.candidates, CandidateSource::Seed), strong);
        QVERIFY(countSource(p.candidates, CandidateSource::Verified) > 0);
        QVERIFY(countSource(p.candidates, CandidateSource::Predicted) > 0);
    }
};

QTEST_APPLESS_MAIN(TstGenerateObjects)
#include "tst_generate_objects.moc"
