// 真實影像、幾個樣本（R07）：使用者回報「樣本越多，匹配越差」。這個量測在使用者的影像上用 1、2、3 個樣本各搜尋一次
// （走產品的 searchBoardWithSamples：每個樣本一條 GenerateObjectsFromSample，再依位置合併），倒出每一種情況的
// 候選來源（Seed／Verified／Predicted）、分數分布、合併數、門檻 0.69 與 0.87 的分類、不在格點上的數量，
// 以及每個樣本自己的晶格；另外畫一張縮小的疊圖（<建置目錄>/multi-<檔名>-<back end>-<n>.png）給人看。
// 先量再改（S11 規則）：改驗證下限與合併規則之前的 before、之後的 after 都用這個工具。
//
// 環境變數：AOI_REAL_IMAGE_DIR 指到影像的資料夾才執行（沒有就 QSKIP）；AOI_MULTI_FILE 檔名（預設 0_0_HA.tif）；
// AOI_MULTI_MATCH 第一個樣本的比對區："x,y,w,h[,edge]" 矩形或 "mask:cx,cy[,grow][,edge]" 自動擷取的遮罩（見 matchFor；沒設 ＝ 整框、灰階）；
// AOI_MULTI_SAMPLE="x,y,w,h" 第一個樣本（預設 AdvancePin 的 0_0_HA.tif 一根端子：1621,328,100,100，和使用者畫的一樣是
// 100×100 的方框）。第二、第三個樣本不用手挑：從第一個樣本的種子裡取分數 ≥ 0.90、離已有樣本最遠的（使用者畫的是同一種
// 零件的幾個實例）。每個註冊而且可用的 back end 一列；HALCON 不能用時那一列跳過並說原因。
// 讀 TIFF 要 OpenCV 的影像來源，所以只在 AOI_WITH_OPENCV=ON 時建置（AiInspTool/tests/tests.pro）。
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QtTest>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <span>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "QtTestPrinters.h"
#include "app/CompositionRoot.h"
#include "application/usecases/GenerateObjectsFromSample.h"
#include "application/usecases/ProposalView.h"
#include "application/usecases/SearchBoardWithSamples.h"
#include "domain/image_depth.h"
#include "domain/feature_extract.h"
#include "domain/lattice/lattice_estimator.h"
#include "domain/lattice/lattice_predictor.h"
#include "infrastructure/backend_opencv/OpenCvImageSource.h"
#include "infrastructure/backend_opencv/OpenCvImages.h"

using namespace aoi;

namespace {

Rect firstSample() {
    const QString text = qEnvironmentVariable("AOI_MULTI_SAMPLE");
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() == 4) return {parts[0].toDouble(), parts[1].toDouble(), parts[2].toDouble(), parts[3].toDouble()};
    return {1621.0, 328.0, 100.0, 100.0};
}

Rect sampleAround(Point2D center, const Rect& like) {
    return {std::round(center.x - like.w / 2.0), std::round(center.y - like.h / 2.0), like.w, like.h};
}

// 比對區（ADR-028）：AOI_MULTI_MATCH 設定第一個樣本的比對區（影像像素、絕對座標），其他樣本用同一個相對主體框的位移：
//  - "x,y,w,h[,edge]"：矩形（edge ＝ 邊緣特徵）；
//  - "mask:cx,cy[,grow][,edge]"：和 M 工具點一下一樣——主體框內 Otsu、離 (cx,cy) 最近的亮連通塊、外擴 grow px。
// 沒有設定 ＝ 整個主體框、灰階（和使用者沒有按 M 一樣）。
MatchSpec matchFor(const ImageView& image, const Rect& body, const Rect& firstBody) {
    QString text = qEnvironmentVariable("AOI_MULTI_MATCH");
    const bool mask = text.startsWith(QLatin1String("mask:"), Qt::CaseInsensitive);
    if (mask) text = text.mid(5);
    const QStringList parts = text.split(QLatin1Char(','));
    MatchSpec spec;
    const bool edges = !parts.isEmpty() && parts.back().trimmed().compare(QLatin1String("edge"), Qt::CaseInsensitive) == 0;
    const int dx = static_cast<int>(std::lround(body.x - firstBody.x));
    const int dy = static_cast<int>(std::lround(body.y - firstBody.y));
    if (mask) {
        if (parts.size() < 2) return {};
        const Point2D click{parts[0].toDouble(), parts[1].toDouble()};
        const int grow = parts.size() > 2 ? parts[2].toInt() : 0;
        const std::optional<BlobPick> pick = pickBrightBlob(image, firstBody, click);
        if (!pick) qFatal("AOI_MULTI_MATCH: no bright blob near (%.0f, %.0f) inside the first sample", click.x, click.y);
        PixelMask m = growMask(pick->blob, grow, firstBody);
        static bool reported = false;
        if (!reported) {
            reported = true;
            qInfo("  match mask: Otsu %d, blob %zu px -> grown %d px: %zu px, bounds (%d,%d %dx%d)", pick->threshold8, pick->blob.count(),
                  grow, m.count(), m.x, m.y, m.width, m.height);
        }
        m.x += dx;
        m.y += dy;
        spec.rect = Rect{static_cast<double>(m.x), static_cast<double>(m.y), static_cast<double>(m.width), static_cast<double>(m.height)};
        spec.mask = std::move(m);
    } else {
        if (parts.size() < 4) return {};
        spec.rect = Rect{parts[0].toDouble() + dx, parts[1].toDouble() + dy, parts[2].toDouble(), parts[3].toDouble()};
    }
    if (edges) spec.mode = MatchMode::Edges;
    return spec;
}

// 從第一個樣本的結果裡挑下一個樣本：分數 ≥ minScore 的種子中，離已有樣本的最小距離最大的那一個。
std::optional<Point2D> farthestSeed(const SampleProposal& proposal, const std::vector<Rect>& existing, double minScore) {
    std::optional<Point2D> best;
    double bestDistance = -1.0;
    for (const Candidate& c : proposal.candidates) {
        if (c.source != CandidateSource::Seed || c.score < minScore) continue;
        double nearest = std::numeric_limits<double>::max();
        for (const Rect& r : existing) nearest = std::min(nearest, distance(c.pose.position(), r.center()));
        if (nearest > bestDistance) {
            bestDistance = nearest;
            best = c.pose.position();
        }
    }
    return best;
}

// 晶格的診斷：一組點用某個選項擬合，印出每一組晶格的基底、內點、索引範圍內的格子數與缺格數。
// 用來看「為什麼這塊板子的晶格只有一維」：整張的種子、只有一個區塊的種子、放寬的選項、完美的交錯陣列各試一次。
void diagnoseLattice(const char* label, std::span<const Point2D> points, const LatticeOptions& options, const Rect& bounds) {
    const LatticeFit fit = LatticeEstimator(options).fitAll(points);
    QString models;
    for (const LatticeModel& m : fit.models) {
        const std::size_t holes = LatticePredictor(m, bounds).holes().size();
        models += QStringLiteral("{%1 u(%2,%3) v(%4,%5) inliers %6 cells %7 holes %8 cov %9 conf %10} ")
                      .arg(m.hasV() ? "2D" : "1D")
                      .arg(m.basis.u.x, 0, 'f', 1).arg(m.basis.u.y, 0, 'f', 1).arg(m.basis.v.x, 0, 'f', 1).arg(m.basis.v.y, 0, 'f', 1)
                      .arg(m.inliers.size()).arg(m.cellCount()).arg(holes).arg(m.coverage, 0, 'f', 2).arg(m.confidence, 0, 'f', 2);
    }
    qInfo("  lattice[%s]: %zu points -> %zu model(s), reason %d: %s", label, points.size(), fit.models.size(), static_cast<int>(fit.reason),
          qPrintable(models));
}

void diagnoseLattices(const SampleProposal& proposal, const Rect& imageRect) {
    std::vector<Point2D> seeds;
    for (const Candidate& c : proposal.candidates) {
        if (c.source == CandidateSource::Sample || c.source == CandidateSource::Seed) seeds.push_back(c.pose.position());
    }
    const LatticeOptions defaults;
    diagnoseLattice("all seeds, defaults", seeds, defaults, imageRect);

    // 只有左上那一塊（沒有長槽在索引範圍裡）。
    std::vector<Point2D> block;
    for (const Point2D& p : seeds) {
        if (p.x > 560.0 && p.x < 3000.0 && p.y < 860.0) block.push_back(p);
    }
    diagnoseLattice("upper-left block, defaults", block, defaults, imageRect);

    LatticeOptions gaps = defaults;
    gaps.maxGapCells = 8;
    diagnoseLattice("all seeds, maxGapCells 8", seeds, gaps, imageRect);

    LatticeOptions rows = defaults;
    rows.minRowConsistency = 0.3;
    diagnoseLattice("all seeds, minRowConsistency 0.3", seeds, rows, imageRect);

    // 完美的交錯陣列的矩形區塊（和這塊板子一樣：欄距 266、列距 154、奇數欄差半個列距）：估計器本身做不做得到？
    std::vector<Point2D> perfect;
    for (int j = 0; j < 10; ++j) {
        for (int i = 0; i < 6; ++i) perfect.push_back({600.0 + 266.0 * j, 400.0 + 154.0 * i + 77.0 * (j % 2)});
    }
    diagnoseLattice("perfect staggered 10x6", perfect, defaults, imageRect);
    std::vector<Point2D> half;
    for (std::size_t k = 0; k < perfect.size(); ++k) {
        if ((k * 7919u) % 2u == 0u) half.push_back(perfect[k]);  // 固定的抽樣：約一半
    }
    diagnoseLattice("perfect staggered, half missing", half, defaults, imageRect);
}

void writeOverlay(const ImageView& image, const BoardProposal& proposal, const std::vector<Rect>& samples, double threshold,
                  const QString& path) {
    const infra::GrayRoi gray = infra::toGray8(image, cv::Rect(0, 0, image.width, image.height));
    cv::Mat color;
    cv::cvtColor(gray.mat, color, cv::COLOR_GRAY2BGR);
    for (const BoardCandidate& c : proposal.candidates) {
        // 紅：推算的缺格（Predicted）；灰：分數低於門檻（畫面上的「拒絕」）；綠：種子 ≥ 門檻；藍：視窗驗證找到、≥ 門檻。
        const cv::Scalar ink = c.source == CandidateSource::Predicted ? cv::Scalar(0, 0, 255)
                               : c.score < threshold                  ? cv::Scalar(160, 160, 160)
                               : c.source == CandidateSource::Verified ? cv::Scalar(255, 160, 0)
                                                                       : cv::Scalar(0, 220, 0);
        const cv::RotatedRect r(cv::Point2f(static_cast<float>(c.pose.x - 0.5), static_cast<float>(c.pose.y - 0.5)),
                                cv::Size2f(static_cast<float>(c.partSize.w), static_cast<float>(c.partSize.h)),
                                static_cast<float>(-c.pose.angleDeg));
        cv::Point2f corners[4];
        r.points(corners);
        for (int i = 0; i < 4; ++i) cv::line(color, corners[i], corners[(i + 1) % 4], ink, 3);
    }
    for (const Rect& s : samples) {  // 洋紅：樣本
        cv::rectangle(color, cv::Rect2d(s.x, s.y, s.w, s.h), cv::Scalar(255, 0, 255), 5);
    }
    cv::Mat small;
    cv::resize(color, small, cv::Size(), 0.25, 0.25, cv::INTER_AREA);
    cv::imwrite(path.toStdString(), small);
}

}  // namespace

class PerfMultiSampleReal : public QObject {
    Q_OBJECT

private:
    QDir dir_;
    QString file_;
    BackendRegistry registry_;

    // 一個樣本自己的流程（晶格的摘要只有這裡看得到）。
    void reportSingle(const app::CoreServices& core, const ImageView& view, std::size_t index, const Rect& sample) {
        SampleRequest request;
        request.image = view;
        request.sample = sample;
        request.match = matchFor(view, sample, firstSample());
        const Result<SampleProposal> r = core.engine->generate->run(request, CancelToken());
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        const SampleProposal& p = r.value();
        std::size_t bySource[4] = {0, 0, 0, 0};
        for (const Candidate& c : p.candidates) ++bySource[static_cast<int>(c.source)];
        qInfo("  sample %zu (%.0f,%.0f %.0fx%.0f): problem %s | candidates %zu: seed %zu, verified %zu, predicted %zu | lattice found %d, "
              "lattices %zu, u (%.1f, %.1f), v (%.1f, %.1f), rms %.2f px, coverage %.3f, confidence %.3f, holes %zu (unverified %zu) | %.0f ms",
              index + 1, sample.x, sample.y, sample.w, sample.h, p.sampleProblem ? "yes" : "no", p.candidates.size(), bySource[1],
              bySource[2], bySource[3], p.grid.found, p.grid.lattices, p.grid.u.x, p.grid.u.y, p.grid.v.x, p.grid.v.y, p.grid.rmsPx,
              p.grid.coverage, p.grid.confidence, p.grid.holes, p.grid.unverified, p.timings.totalMs);
    }

    void reportMerged(const char* backend, const app::CoreServices& core, const ImageView& view, const std::vector<Rect>& samples) {
        BoardSearchRequest request;
        request.image = view;
        for (std::size_t i = 0; i < samples.size(); ++i) {
            request.samples.push_back(SampleSearch{"t" + std::to_string(i + 1), samples[i], matchFor(view, samples[i], samples.front())});
        }
        const Result<BoardProposal> r = searchBoardWithSamples(*core.engine->generate, request, CancelToken());
        QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
        const BoardProposal& p = r.value();

        std::size_t bySource[4] = {0, 0, 0, 0};
        std::map<std::string, std::size_t> byType;
        std::array<int, 7> histogram{};  // 有分數的候選：0.30-0.40, …, 0.90-1.00
        std::size_t verifiedBelowSeedFloor = 0;  // Verified 而且分數 < 0.70：只因為驗證下限 0.30 才進來的
        std::size_t offLatticeScored = 0;
        for (const BoardCandidate& c : p.candidates) {
            ++bySource[static_cast<int>(c.source)];
            ++byType[c.typeId];
            if (!c.hasScore()) continue;
            ++histogram[std::clamp(static_cast<int>((c.score - 0.30) / 0.10), 0, 6)];
            if (c.source == CandidateSource::Verified && c.score < 0.70) ++verifiedBelowSeedFloor;
            if (!c.onLattice()) ++offLatticeScored;
        }
        QString types;
        for (const auto& [id, n] : byType) types += QStringLiteral("%1=%2 ").arg(QString::fromStdString(id)).arg(n);

        qInfo("[%s] %zu sample(s): searched %zu, candidates %zu (seed %zu, verified %zu, predicted %zu), merged %zu, excluded %zu | by type: %s| "
              "template %.0f, seed %.0f, fit %.0f, verify %.0f, total %.0f ms",
              backend, samples.size(), p.samplesSearched, p.candidates.size(), bySource[1], bySource[2], bySource[3], p.merged, p.excluded,
              qPrintable(types), p.timings.templateMs, p.timings.seedMs, p.timings.fitMs, p.timings.verifyMs, p.timings.totalMs);
        qInfo("[%s] %zu sample(s): scores 0.3-0.4 %d | 0.4-0.5 %d | 0.5-0.6 %d | 0.6-0.7 %d | 0.7-0.8 %d | 0.8-0.9 %d | 0.9-1.0 %d | "
              "verified < 0.70: %zu | scored off lattice: %zu",
              backend, samples.size(), histogram[0], histogram[1], histogram[2], histogram[3], histogram[4], histogram[5], histogram[6],
              verifiedBelowSeedFloor, offLatticeScored);
        for (const double threshold : {0.69, 0.87}) {
            const ProposalView v = classify(p, threshold);
            qInfo("[%s] %zu sample(s): threshold %.2f -> found %zu, rejected %zu, inferred %zu, off lattice %zu", backend, samples.size(),
                  threshold, v.found, v.rejected, v.inferred, v.offLattice);
        }
        // AOI_MULTI_DUMP=<檔案>：一個樣本時把所有候選倒成 CSV（x, y, score, source, onLattice），給外面的腳本分區統計。
        const QString dump = qEnvironmentVariable("AOI_MULTI_DUMP");
        if (!dump.isEmpty() && samples.size() == 1) {
            QFile f(dump);
            QVERIFY2(f.open(QIODevice::WriteOnly | QIODevice::Text), qPrintable(dump));
            QTextStream out(&f);
            out << "x,y,score,source,onLattice\n";
            for (const BoardCandidate& c : p.candidates) {
                out << c.pose.x << ',' << c.pose.y << ',' << (c.hasScore() ? c.score : -1.0) << ',' << static_cast<int>(c.source) << ','
                    << (c.onLattice() ? 1 : 0) << '\n';
            }
        }
        writeOverlay(view, p, samples, 0.87,
                     QDir::current().filePath(QStringLiteral("multi-%1-%2-%3.png").arg(QFileInfo(file_).baseName(), QString::fromLatin1(backend)).arg(samples.size())));
    }

private slots:
    void initTestCase() {
        const QString d = qEnvironmentVariable("AOI_REAL_IMAGE_DIR");
        if (d.isEmpty()) QSKIP("AOI_REAL_IMAGE_DIR is not set: the user's real images are not in the repository.");
        dir_ = QDir(d);
        QVERIFY2(dir_.exists(), qPrintable(d));
        file_ = qEnvironmentVariable("AOI_MULTI_FILE");
        if (file_.isEmpty()) file_ = QStringLiteral("0_0_HA.tif");
        registry_ = app::makeBackendRegistry();
    }

    void multiSample_data() {
        QTest::addColumn<QString>("backend");
        for (const BackendInfo& b : registry_.list()) {
            if (b.id == "fake") continue;  // 假 back end 不看影像
            QTest::newRow(b.id.c_str()) << QString::fromStdString(b.id);
        }
    }

    void multiSample() {
        QFETCH(QString, backend);
        const std::string id = backend.toStdString();
        for (const BackendInfo& b : registry_.list()) {
            if (b.id == id && !b.available) QSKIP(qPrintable(QString::fromStdString(b.unavailable.message)));
        }
        const QString path = dir_.filePath(file_);
        if (!QFileInfo::exists(path)) QSKIP(qPrintable(QStringLiteral("%1 is not there").arg(path)));

        Result<app::CoreServices> composed = app::composeCore(registry_, id, false, app::ExecutorKind::StdThreads);
        if (!composed.ok()) QSKIP(qPrintable(QString::fromStdString(composed.error().message)));
        const app::CoreServices& core = composed.value();

        infra::OpenCvImageSource source;
        const Result<ImageView> image = source.load(path.toStdString());
        QVERIFY2(image.ok(), image.ok() ? "" : image.error().message.c_str());
        const ImageView& view = image.value();
        qInfo("[%s] %s: %d x %d %s, %d bit (detected)", qPrintable(backend), qPrintable(file_), view.width, view.height,
              std::string(toString(view.format)).c_str(), effectiveBits(view));

        // 第一個樣本自己跑一次，從它的種子裡挑第二、第三個樣本。
        std::vector<Rect> samples{firstSample()};
        {
            SampleRequest request;
            request.image = view;
            request.sample = samples.front();
            request.match = matchFor(view, samples.front(), samples.front());
            const Result<SampleProposal> r = core.engine->generate->run(request, CancelToken());
            QVERIFY2(r.ok(), r.ok() ? "" : r.error().message.c_str());
            QVERIFY2(!r.value().sampleProblem, "the first sample has a problem; set AOI_MULTI_SAMPLE");
            diagnoseLattices(r.value(), Rect{0.0, 0.0, static_cast<double>(view.width), static_cast<double>(view.height)});
            for (int k = 0; k < 2; ++k) {
                const std::optional<Point2D> next = farthestSeed(r.value(), samples, 0.90);
                if (!next) break;
                samples.push_back(sampleAround(*next, samples.front()));
            }
        }
        QVERIFY2(samples.size() == 3, "could not pick three samples (not enough seeds with score >= 0.90)");
        for (std::size_t i = 0; i < samples.size(); ++i) reportSingle(core, view, i, samples[i]);
        for (std::size_t n = 1; n <= samples.size(); ++n) {
            reportMerged(qPrintable(backend), core, view, std::vector<Rect>(samples.begin(), samples.begin() + static_cast<std::ptrdiff_t>(n)));
        }
    }
};

QTEST_GUILESS_MAIN(PerfMultiSampleReal)
#include "perf_multi_sample_real.moc"
