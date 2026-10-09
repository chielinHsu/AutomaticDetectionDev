// 契約測試（S09）：use case（GenerateObjectsFromSample，application 層）接上組裝根註冊的每一個會讀像素的 back end，
// 在合成板子上從「框一個樣本」跑到候選清單，和標準答案比：找到的（T6a 召回率）、位置誤差（T6b）、缺件被推算出來。
// 同一個 use case、同一組板子、同一組容差（ContractTolerances.h）：證明 use case 不必改就能換 back end（開放封閉），
// 而且每個 back end 換上去之後的行為都在容差內（Liskov）。S07 的 tst_opencv_end_to_end 與 S08 的 tst_halcon_end_to_end 併進這裡。
// 假 back end 不看影像，不在這裡（它的端到端在 tests/core/tst_generate_objects）。
#include <QtTest>
#include <algorithm>
#include <cmath>
#include <limits>

#include "BackendTestSupport.h"
#include "ContractSupport.h"
#include "QtTestPrinters.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "domain/image_depth.h"
#include "synthetic_board.h"

Q_DECLARE_METATYPE(aoi::test::SyntheticBoardSpec)

using namespace aoi;
using namespace aoi::contract;
using namespace aoi::test;

namespace {

struct Outcome {
    std::size_t present = 0;
    std::size_t foundPresent = 0;     // 有元件的格子，被有分數的候選（Sample、Seed、Verified）找到
    std::size_t aboveThreshold = 0;   // 其中分數 ≥ 0.70（預設門檻）的
    std::size_t falsePositives = 0;   // 有分數的候選，附近沒有元件
    std::size_t missing = 0;          // 缺件的格子
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
// 固定的偏移（最多半個像素）；所有元件都差同一個偏移，所以 T6b 量的是扣掉這個偏移之後的誤差。
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

Result<SampleProposal> run(const SearchEngine& engine, const SyntheticBoard& board, const ImageView& image) {
    SampleRequest request;
    request.image = image;
    request.sample = sampleBoxAround(board, middlePresentIndex(board));
    return engine.generate->run(request, CancelToken());
}

}  // namespace

class TstContractEndToEnd : public QObject {
    Q_OBJECT

private slots:
    // T6 的 8 種情況（S07 的 perf_opencv 與 S08 的 perf_halcon 同一組）× 每個會讀像素的 back end。
    void syntheticBoards_data() {
        QTest::addColumn<QString>("backend");
        QTest::addColumn<SyntheticBoardSpec>("spec");
        const SyntheticBoardSpec s = baseSpec();
        std::vector<std::pair<const char*, SyntheticBoardSpec>> boards;
        boards.emplace_back("clean", s);
        SyntheticBoardSpec missing = s;
        missing.missingFraction = 0.10;
        boards.emplace_back("10% missing", missing);
        SyntheticBoardSpec noisy = s;
        noisy.noiseAmplitude = 12;
        noisy.blurRadius = 1;
        boards.emplace_back("noise 12, blur 1", noisy);
        SyntheticBoardSpec glare = s;
        glare.glareFraction = 0.10;
        boards.emplace_back("10% glare", glare);
        SyntheticBoardSpec tilted = s;
        tilted.tiltDeg = 3.0;
        boards.emplace_back("tilt 3 deg", tilted);
        SyntheticBoardSpec jitter = s;
        jitter.positionJitterPx = 2.0;
        boards.emplace_back("jitter 2 px", jitter);
        SyntheticBoardSpec deep = s;
        deep.format = PixelFormat::Gray16;
        deep.grayBits = 12;
        boards.emplace_back("12 bit", deep);
        SyntheticBoardSpec color = s;
        color.format = PixelFormat::Bgrx32;
        boards.emplace_back("Bgrx32", color);
        for (const std::string& id : backendIds(Rows::Vision)) {
            for (const auto& [name, spec] : boards) QTest::addRow("%s %s", id.c_str(), name) << QString::fromStdString(id) << spec;
        }
    }

    // T6a（召回率 ≥ 99%：合成板子上要 100%）與 T6b（位置誤差 ≤ 0.5 px）的端到端版本；缺件要被推算出來（Inferred），不能被當成找到。
    void syntheticBoards() {
        CONTRACT_BACKEND(be);
        QFETCH(SyntheticBoardSpec, spec);
        const EndToEndTolerances& tol = beProfile.endToEnd;
        const SyntheticBoard board = makeSyntheticBoard(spec);
        QVERIFY(board.image.valid());
        const Result<SampleProposal> r = run(be, board, board.image);
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        const SampleProposal& p = r.value();
        QVERIFY(!p.sampleProblem);
        const Outcome o = evaluate(board, p);
        qInfo("%s", qPrintable(QStringLiteral("%1: %2 | lattice %3, holes %4 | template %5 seed %6 fit %7 verify %8 ms")
                                   .arg(QString::fromStdString(be.backend.id), o.describe()).arg(p.grid.found).arg(p.grid.holes)
                                   .arg(p.timings.templateMs, 0, 'f', 1).arg(p.timings.seedMs, 0, 'f', 1)
                                   .arg(p.timings.fitMs, 0, 'f', 1).arg(p.timings.verifyMs, 0, 'f', 1)));
        QVERIFY2(o.foundPresent == o.present, qPrintable(o.describe()));
        QVERIFY2(o.falsePositives == 0, qPrintable(o.describe()));
        QVERIFY2(o.maxErrorPx <= tol.positionMaxPx, qPrintable(o.describe()));
        QVERIFY2(o.missingInferred == o.missing, qPrintable(o.describe()));
        QVERIFY(p.grid.found);
        // 樣本的角度是 0（範本就是它），其他元件和它同一個方向（板子整個傾斜時也是）。強雜訊時 HALCON 的上限改用框的角落位移
        // （ContractTolerances.h 的 strongNoiseCornerShiftPx，ADR-004）。
        const double halfDiagonal = 0.5 * std::hypot(p.partSize.w, p.partSize.h);
        const bool strongNoise = spec.noiseAmplitude >= 12 && tol.strongNoiseCornerShiftPx > 0.0;
        const double angleLimit = strongNoise ? radiansToDegrees(tol.strongNoiseCornerShiftPx / halfDiagonal) : tol.angleMaxDeg;
        double worstAngle = 0.0;
        for (const Candidate& c : p.candidates) worstAngle = std::max(worstAngle, std::abs(c.pose.angleDeg));
        qInfo("  worst |angle| %.3f deg (limit %.3f)", worstAngle, angleLimit);
        QVERIFY2(worstAngle < angleLimit, qPrintable(QString::number(worstAngle)));
    }

    // 有效位元數的覆寫（S07）：很暗、對比很低的 12 bit 板子。看最大值會推測成 10 bit，亮度被放大 4 倍，
    // use case 的對比檢查因此放行；覆寫成真正的 12 bit 之後，判定「對比太低」（每個 back end 都用 view 的位元數）。
    void aDarkTwelveBitBoardNeedsTheBitOverride_data() { addBackendRows(Rows::Vision); }
    void aDarkTwelveBitBoardNeedsTheBitOverride() {
        CONTRACT_BACKEND(be);
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
        const Result<SampleProposal> wrong = run(be, board, guessed);
        QVERIFY(wrong.ok());
        QVERIFY2(!wrong.value().sampleProblem, "with the guessed 10 bits the contrast looks 4 times higher than it is");
        ImageView told = board.image;
        told.significantBits = 12;
        const Result<SampleProposal> right = run(be, board, told);
        QVERIFY(right.ok());
        QVERIFY(right.value().sampleProblem.has_value());
        QVERIFY(right.value().sampleProblem->reason == NoMatchReason::LowContrast);
    }

    // 暗、但對比夠的 12 bit 板子：覆寫成 12 bit 之後照樣全部找到（搜尋器轉 8 bit 時用覆寫的位元數）。
    void aDarkTwelveBitBoardWithEnoughContrastIsFound_data() { addBackendRows(Rows::Vision); }
    void aDarkTwelveBitBoardWithEnoughContrastIsFound() {
        CONTRACT_BACKEND(be);
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
        const Result<SampleProposal> r = run(be, board, told);
        QVERIFY(r.ok());
        QVERIFY(!r.value().sampleProblem);
        const Outcome o = evaluate(board, r.value());
        QVERIFY2(o.foundPresent == o.present && o.falsePositives == 0 && o.maxErrorPx <= beProfile.endToEnd.positionMaxPx,
                 qPrintable(o.describe()));
    }
};

QTEST_GUILESS_MAIN(TstContractEndToEnd)
#include "tst_contract_end_to_end.moc"
