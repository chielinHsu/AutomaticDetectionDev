// 端到端（S07）：use case（GenerateObjectsFromSample，application 層，一個字都沒改）接上 OpenCV back end，
// 在合成板子上從「框一個樣本」跑到候選清單，和標準答案比：找到的（T6a 召回率）、位置誤差（T6b）、缺件被推算出來。
// 用的是組裝根註冊的那一個 back end（app::makeBackendRegistry 的 "opencv"），證明 use case 不必改就能換 back end。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <limits>

#include "BackendTestSupport.h"
#include "QtTestPrinters.h"
#include "app/CompositionRoot.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "application/usecases/ProposalView.h"
#include "domain/image_depth.h"
#include "synthetic_board.h"

Q_DECLARE_METATYPE(aoi::test::SyntheticBoardSpec)

using namespace aoi;
using namespace aoi::test;

namespace {

struct Outcome {
    std::size_t present = 0;
    std::size_t foundPresent = 0;   // 有元件的格子，被有分數的候選（Sample、Seed、Verified）找到
    std::size_t aboveThreshold = 0; // 其中分數 ≥ 0.70（預設門檻）的
    std::size_t falsePositives = 0; // 有分數的候選，附近沒有元件
    std::size_t missing = 0;        // 缺件的格子
    std::size_t missingInferred = 0;  // 缺件的格子上有 Predicted（Inferred）
    double meanErrorPx = 0.0;
    double maxErrorPx = 0.0;
    QString describe() const {
        return QStringLiteral("present %1 found %2 (>=0.70: %3) false %4 | missing %5 inferred %6 | error mean %7 max %8 px")
            .arg(present).arg(foundPresent).arg(aboveThreshold).arg(falsePositives).arg(missing).arg(missingInferred)
            .arg(meanErrorPx, 0, 'f', 3).arg(maxErrorPx, 0, 'f', 3);
    }
};

// 每個匹配的位置是「樣本框的中心」：使用者框的那個框定義了參考點。樣本框四捨五入到整數像素，和元件真正的中心差一個
// 固定的偏移（最多半個像素）；所有元件都差同一個偏移（同一個方向），所以 T6b 量的是扣掉這個偏移之後的誤差。
Point2D sampleOffset(const SyntheticBoard& board, const SampleProposal& proposal) {
    const Candidate& sample = proposal.candidates.front();
    const SyntheticPart* nearest = nullptr;
    double best = std::numeric_limits<double>::max();
    for (const SyntheticPart& p : board.parts) {
        const double d = distance(sample.pose.position(), p.pose.position());
        if (p.present && d < best) {
            best = d;
            nearest = &p;
        }
    }
    if (nearest == nullptr) return {};
    return {sample.pose.x - nearest->pose.x, sample.pose.y - nearest->pose.y};
}

Outcome evaluate(const SyntheticBoard& board, const SampleProposal& proposal, double tolerance = 3.0) {
    Outcome o;
    double sum = 0.0;
    const Point2D offset = proposal.candidates.empty() ? Point2D{} : sampleOffset(board, proposal);
    for (const SyntheticPart& p : board.parts) {
        const Point2D expected{p.pose.x + offset.x, p.pose.y + offset.y};
        const Candidate* nearest = nullptr;
        double best = std::numeric_limits<double>::max();
        for (const Candidate& c : proposal.candidates) {
            const double d = distance(c.pose.position(), expected);
            if (d < best) {
                best = d;
                nearest = &c;
            }
        }
        const bool near = nearest != nullptr && best <= tolerance;
        if (p.present) {
            ++o.present;
            if (near && nearest->hasScore()) {
                ++o.foundPresent;
                if (nearest->source == CandidateSource::Sample || nearest->score >= 0.70) ++o.aboveThreshold;
                sum += best;
                o.maxErrorPx = std::max(o.maxErrorPx, best);
            }
        } else {
            ++o.missing;
            if (near && nearest->source == CandidateSource::Predicted) ++o.missingInferred;
        }
    }
    for (const Candidate& c : proposal.candidates) {
        if (!c.hasScore()) continue;
        const bool nearPart = std::any_of(board.parts.begin(), board.parts.end(), [&](const SyntheticPart& p) {
            return p.present && distance(c.pose.position(), {p.pose.x + offset.x, p.pose.y + offset.y}) <= tolerance;
        });
        if (!nearPart) ++o.falsePositives;
    }
    o.meanErrorPx = o.foundPresent == 0 ? 0.0 : sum / static_cast<double>(o.foundPresent);
    return o;
}

SyntheticBoardSpec baseSpec() {
    SyntheticBoardSpec s;
    s.width = 1280;
    s.height = 960;
    s.rows = 12;
    s.cols = 14;
    s.seed = 11;
    return s;
}

}  // namespace

class TstOpenCvEndToEnd : public QObject {
    Q_OBJECT

private:
    app::CoreServices core_;

    Result<SampleProposal> run(const SyntheticBoard& board, const ImageView& image) {
        SampleRequest request;
        request.image = image;
        request.sample = sampleBoxAround(board, middlePresentIndex(board));
        return core_.generateObjects->run(request, CancelToken());
    }

private slots:
    void initTestCase() {
        const BackendRegistry registry = app::makeBackendRegistry();
        bool listed = false;
        for (const BackendInfo& info : registry.list()) {
            if (info.id == "opencv") listed = info.available;
        }
        QVERIFY2(listed, "an AOI_WITH_OPENCV=ON build registers the OpenCV back end as available");
        Result<app::CoreServices> core = app::composeCore(registry, "opencv", false, app::ExecutorKind::StdThreads);
        QVERIFY2(core.ok(), core.ok() ? "" : core.error().message.c_str());
        core_ = std::move(core.value());
        QCOMPARE(core_.backend.id, std::string("opencv"));
    }

    void syntheticBoards_data() {
        QTest::addColumn<SyntheticBoardSpec>("spec");
        SyntheticBoardSpec s = baseSpec();
        QTest::newRow("clean") << s;
        SyntheticBoardSpec missing = s;
        missing.missingFraction = 0.10;
        QTest::newRow("10% missing") << missing;
        SyntheticBoardSpec noisy = s;
        noisy.noiseAmplitude = 12;
        noisy.blurRadius = 1;
        QTest::newRow("noise 12, blur 1") << noisy;
        SyntheticBoardSpec glare = s;
        glare.glareFraction = 0.10;
        QTest::newRow("10% glare") << glare;
        SyntheticBoardSpec tilted = s;
        tilted.tiltDeg = 3.0;
        QTest::newRow("tilt 3 deg") << tilted;
        SyntheticBoardSpec jitter = s;
        jitter.positionJitterPx = 2.0;
        QTest::newRow("jitter 2 px") << jitter;
        SyntheticBoardSpec deep = s;
        deep.format = PixelFormat::Gray16;
        deep.grayBits = 12;
        QTest::newRow("12 bit") << deep;
        SyntheticBoardSpec color = s;
        color.format = PixelFormat::Bgrx32;
        QTest::newRow("Bgrx32") << color;
    }

    // T6a（召回率 ≥ 99%）與 T6b（位置誤差 ≤ 0.5 px）的端到端版本；缺件要被推算出來（Inferred），不能被當成找到。
    void syntheticBoards() {
        QFETCH(SyntheticBoardSpec, spec);
        const SyntheticBoard board = makeSyntheticBoard(spec);
        QVERIFY(board.image.valid());
        const Result<SampleProposal> r = run(board, board.image);
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        const SampleProposal& p = r.value();
        QVERIFY(!p.sampleProblem);
        const Outcome o = evaluate(board, p);
        qInfo("%s", qPrintable(QStringLiteral("%1 | lattice %2, holes %3 | template %4 seed %5 fit %6 verify %7 ms")
                                   .arg(o.describe()).arg(p.grid.found).arg(p.grid.holes)
                                   .arg(p.timings.templateMs, 0, 'f', 1).arg(p.timings.seedMs, 0, 'f', 1)
                                   .arg(p.timings.fitMs, 0, 'f', 1).arg(p.timings.verifyMs, 0, 'f', 1)));
        QVERIFY2(o.foundPresent == o.present, qPrintable(o.describe()));
        QVERIFY2(o.falsePositives == 0, qPrintable(o.describe()));
        QVERIFY2(o.maxErrorPx <= 0.5, qPrintable(o.describe()));
        QVERIFY2(o.missingInferred == o.missing, qPrintable(o.describe()));
        QVERIFY(p.grid.found);
        // 樣本的角度是 0（範本就是它），其他元件和它同一個方向（板子整個傾斜時也是）。
        for (const Candidate& c : p.candidates) QVERIFY2(std::abs(c.pose.angleDeg) < 0.6, qPrintable(QString::number(c.pose.angleDeg)));
    }

    // 有效位元數的覆寫（S07）：很暗、對比很低的 12 bit 板子。看最大值會推測成 10 bit，亮度被放大 4 倍，
    // use case 的對比檢查因此放行；覆寫成真正的 12 bit 之後，判定「對比太低」（換算是對的）。
    void aDarkTwelveBitBoardNeedsTheBitOverride() {
        SyntheticBoardSpec s = baseSpec();
        s.format = PixelFormat::Gray16;
        s.grayBits = 12;
        s.backgroundLevel = 20;
        s.bodyLevel = 22;
        s.padLevel = 24;
        const SyntheticBoard board = makeSyntheticBoard(s);
        ImageView guessed = board.image;
        guessed.significantBits = detectSignificantBits(board.image);
        QCOMPARE(guessed.significantBits, 10);
        const Result<SampleProposal> wrong = run(board, guessed);
        QVERIFY(wrong.ok());
        QVERIFY2(!wrong.value().sampleProblem, "with the guessed 10 bits the contrast looks 4 times higher than it is");

        ImageView told = board.image;
        told.significantBits = 12;
        const Result<SampleProposal> right = run(board, told);
        QVERIFY(right.ok());
        QVERIFY(right.value().sampleProblem.has_value());
        QVERIFY(right.value().sampleProblem->reason == NoMatchReason::LowContrast);
    }

    // 暗、但對比夠的 12 bit 板子：覆寫成 12 bit 之後照樣全部找到（搜尋器轉 8 bit 時用覆寫的位元數）。
    void aDarkTwelveBitBoardWithEnoughContrastIsFoundWithTheOverride() {
        SyntheticBoardSpec s = baseSpec();
        s.format = PixelFormat::Gray16;
        s.grayBits = 12;
        s.backgroundLevel = 10;
        s.bodyLevel = 32;
        s.padLevel = 60;
        const SyntheticBoard board = makeSyntheticBoard(s);
        QCOMPARE(detectSignificantBits(board.image), 10);
        ImageView told = board.image;
        told.significantBits = 12;
        const Result<SampleProposal> r = run(board, told);
        QVERIFY(r.ok());
        QVERIFY(!r.value().sampleProblem);
        const Outcome o = evaluate(board, r.value());
        QVERIFY2(o.foundPresent == o.present && o.falsePositives == 0 && o.maxErrorPx <= 0.5, qPrintable(o.describe()));
    }
};


QTEST_GUILESS_MAIN(TstOpenCvEndToEnd)
#include "tst_opencv_end_to_end.moc"
